/* Generated automatically by parameterize.py on Mon Apr 30 19:55:24 2018
 * Combined VDMA/DMA driver to run the camera and an image pipeline
 *
 * Like other drivers, the interface is a character driver with a single file.
 * However, all operations are handled via ioctl() and mmap(), rather than with
 * read() and write().  This is so that we can pass the data around and access
 * it without having to do any copying.  The other zero-copy method is scatter-
 * gather to userspace, but then we have to worry about the SG table and cache
 * flush/invalidate.
 *
 *
 * Steven Bell <sebell@stanford.edu>
 * 28 February 2014
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h> // Character device
#include <linux/slab.h> // kmalloc
#include <asm/io.h> // ioremap and friends
#include <asm/uaccess.h> // Copy to/from userspace pointers
#include <asm/cacheflush.h> // flush L1 cache
#include <linux/sched.h> // current task struct
#include <linux/fs.h> // File node numbers
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/types.h>

#include "common.h"
#include "kbuffer.h"
#include "dma_bufferset.h"
#include "ioctl_cmds.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MH Students");
MODULE_DESCRIPTION("Driver for halide hw nodes");
MODULE_VERSION("1.2");

#define CLASSNAME "hwacc" // Shows up in /sys/class
#define DEVNAME "hwacc" // Shows up in /dev

#define ACC_CONTROLLER_PAGES 1

#define MAX_HWACC_MODULE 8

#define N_DMA_BUFFERSETS 16 // Number of "buffer set" objects for passing through the queues

#define SG_DESC_SIZE 16 // Size of each SG descriptor, in 32-byte words
#define SG_DESC_BYTES (SG_DESC_SIZE * 4)  // Size of each descriptor in bytes

#define DMA_MAX_CHANS_PER_DEVICE	0x20

#define TAP_DATA_BASE 0x10

/* these values are copied from the official implementation */
#define XILINX_DMA_MM2S_CTRL_OFFSET 0x0000
#define XILINX_DMA_S2MM_CTRL_OFFSET 0x0030

// Each scanline is a separate scatter-gather block so that we can handle striding.
// For example, the VDMA engine writes data in with a 2048-pixel stride, where
// the last 128 pixels are empty; this lets us grab the valid parts and stream
// them all together.

// For input, we have one SG block per image row, which gives 1080 * (16*4),
// a little over 64kb. The output might be stripped, so we should do the same.
// So page order has to be over 64kb/4kb = 16, -> page order 5.
#define SG_PAGEORDER 5

// cma driver utils
extern struct KBuffer *get_buffer_by_id(u32 id);


// Forward declarations of the work functions
void dma_launch_work(struct work_struct*);
void dma_finished_work(struct work_struct*);

struct class *pipe_class;
int device_usage[MAX_HWACC_MODULE]; /* controls the char dev creation */

static int debug_level = 4;
module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level,
                 "0 is errors only, increasing numbers print more stuff");

// If the DMAs are configured in the multichannel mode,
// // use this flag to enable 2D transfer mode
static bool use_2D_mode = true;
module_param(use_2D_mode, bool, 0644);
MODULE_PARM_DESC(use_2D_mode, "enable 2D transfer for multichannel mode");

// If the device use the accelerator coherency port (ACP) of PS,
// // use this flag to enforce cache coherency and to avoid cache flushing
static bool use_acp = true;
module_param(use_acp, bool, 0644);
MODULE_PARM_DESC(use_acp,
                 "enforce cache coherency, if the device uses ACP");

/**
 * this is number of different taps feeding into the acclerator,
 * not size of a single tap array.
 */
static int max_tap_count = 2;
module_param(max_tap_count, int, 0644);
MODULE_PARM_DESC(max_tap_count,
                "maximum allowed tap count. increase if needed before load");

/**
 * This value makes sure there is only 1 GPIO gets set, when there is multiple hls_target
 */
static int has_gpio = 0;

struct dma_chan {
        struct hwacc_drvdata *drvdata;
        struct device *dev;

        void __iomem *controller;
        int irq;

        int id;
        bool input_chan;
        wait_queue_head_t wq;

        int index;
};

struct hwacc_drvdata {
        struct platform_device  *pdev;
        struct cdev cdev; /* char device structure */
        struct dma_chan *chan[DMA_MAX_CHANS_PER_DEVICE];
        void __iomem *hls_controller;
        void __iomem *chan_controller;
        void __iomem *gpio_controller;

        u32 nr_channels;
        int chan_id;

        /* only allow one process accessing hwacc at a time */
        atomic_t usage_count;

        /* work queue */
        struct work_struct launch_work;
        struct work_struct finished_work;
        atomic_t finished_count; /* won't be used for inputs */

        /*
         * Wait queues to pend on the various DMA operations.
         * Things waiting on these are woken up when the interrupt fires.
         */
        wait_queue_head_t processing_finished;
        /* Writes waiting for a free buffer */
        wait_queue_head_t buffer_free_queue;
        /*
         * A BufferSet is attached to one of the following lists, which
         * determines its state All of the lists maintain order (i.e, FIFO).
         * FREE - Input buffer is ready to be filled with data
         */
        BufferList free_list;
        /*
         * QUEUED - Has data (flushed to RAM) and is ready to be streamed
         */
        BufferList queued_list;
        /*  PROCESSING - Input DMA has started, and output DMA has not yet
         *  finished. This implies that some part of the buffer's contents
         *  are in the stencil path. If more than one buffer is in this state,
         *  something is probably very wrong.
         */
        BufferList processing_list;
        /*
         * COMPLETE - DMA has finished, and output data is ready to be handed
         * back to the user
         */
        BufferList complete_list;
        BufferSet buffer_pool[N_DMA_BUFFERSETS];

        /* char dev */
        struct device *pipe_dev;
        dev_t device_num;
        int dev_index;
};

/**
 * Probes the DMA engine to confirm that it exists at the assigned memory
 * location and that scatter-gather DMA is built-in.  The only reason
 * this would fail is if the image in the FPGA doesn't match the software
 * build.
 * Return: 0 on success, -1 on failure
 */
int check_dma_engine(unsigned char* dma_controller)
{
  unsigned long status;

  status = ioread32((void*)dma_controller + 0x04);
  // The SG bit should be set - see page 29 of the DMA datasheet (PG021)
  if((status & 0x00000008) == 0){
    ERROR("ERROR: Scatter-gather DMA not built in!\n");
    return(-1);
  }
  if((status & 0x00000770) != 0){
    // Some error occurred; try to reset the engine (bit 3 of DMACR)
    iowrite32(0x00000004, (void*)dma_controller + 0x00);
    // Spin and wait until the reset finishes
    while(ioread32((void*)dma_controller + 0x00) & 0x00000004) {}
  }

  // setup SG_CTL register
  // SG_CACHE: 1111 defines memory type as 'write-back read and write-allocate'
  // SG_USER: tie off high to enable coherency when allowed by SG_CACHE
  //
  // Note that S2MM and MM2S share the same SG_CTL, so the write w.r.t the
  // output DMA is unnecessary, but the out-of-range write looks fine
  iowrite32(0x00000f0f, (void*)dma_controller + 0x2c);
  return(0);
}

static int dev_open(struct inode *inode, struct file *file)
{
        int i, j;
        struct dma_chan *chan;
        struct hwacc_drvdata *drvdata = container_of(inode->i_cdev,
                                                     struct hwacc_drvdata,
                                                     cdev);

        BufferSet *buffer_pool = drvdata->buffer_pool;
        file->private_data = drvdata;

        /* allocate pages */
        for (i = 0; i < N_DMA_BUFFERSETS; i++) {
                for (j = 0; j < drvdata->nr_channels; j++) {
                        chan = drvdata->chan[j];
                        buffer_pool[i].chan_buf_list[j].sg = (unsigned long*) \
                                  __get_free_pages(GFP_KERNEL, SG_PAGEORDER);
                        if (!buffer_pool[i].chan_buf_list[j].sg) {
                                ERROR("failed to allocate memory for SG table"
                                      "chan %d\n", chan->id);
                                return -ENOMEM;
                        }
                        // TODO: add fail case
                }
        }


        /* make sure the device is not busy */
        if (atomic_read(&drvdata->usage_count))
                return -EBUSY;
        atomic_set(&drvdata->finished_count, 0); // No buffers finished yet
        atomic_set(&drvdata->usage_count, 1);
  return(0);
}

static int dev_close(struct inode *inode, struct file *file)
{
        int i, j;
        struct hwacc_drvdata *drvdata = file->private_data;

        /* make sure the queue is empty */
        if (waitqueue_active(&drvdata->processing_finished)
        || waitqueue_active(&drvdata->buffer_free_queue)) {
                dev_err(drvdata->pipe_dev,
                "closing device before clearing out wait queue!\n");
        }

        /* free all the pages */
        /*
         * TODO: Wait until the DMA engine is done,
         * so we don't write to memory after freeing it
         */
        for (i = 0; i < N_DMA_BUFFERSETS; i++) {
                /* becase no consumers are deleting nodes,
                 * we are safe to traverse without deleting the nodes
                */
                for (j = 0; j < drvdata->buffer_pool[i].nr_channels; j++) {
                        free_pages((unsigned long) drvdata->buffer_pool[i]
                                   .chan_buf_list[j].sg,
                                   SG_PAGEORDER);
                }
        }

        atomic_set(&drvdata->usage_count, 0);
        return(0);
}

static int setup_buffer_list(struct hwacc_drvdata *drvdata)
{
        int i;
        BufferSet *buffer_pool = drvdata->buffer_pool;

        /* set up the image buffer set */
        buffer_initlist(&drvdata->free_list);
        buffer_initlist(&drvdata->queued_list);
        buffer_initlist(&drvdata->processing_list);
        buffer_initlist(&drvdata->complete_list);

        /**
         * Allocate memory for the scatter-gather tables in the buffer set and
         * put them on the free list.
         * The actual data will be attached when needed.
         */
        for (i = 0; i < N_DMA_BUFFERSETS; i++) {
                buffer_pool[i].id = i;
                buffer_pool[i].nr_channels = drvdata->nr_channels;

                buffer_pool[i].chan_buf_list = \
                        devm_kzalloc(&drvdata->pdev->dev,
                        sizeof (struct chan_buf) * drvdata->nr_channels,
                        GFP_KERNEL);

                DEBUG("enqueing buffer set %d\n", i);
                buffer_enqueue(&drvdata->free_list, buffer_pool + i);
        }
        return 0;
}

/* Builds a scatter-gather descriptor chain for a buffer.
 * SG is particularly useful for automatically handling strided images, such as
 * blocks of a larger image.
 * Each image row is a new sg slice, and starts on a new memory location,
 * which may not be adjacent to the last one.
 */
void build_sg_chain(const KBuffer buf, unsigned long* sg_ptr_base, unsigned long* sg_phys)
{
  int sg; // Descriptor count
  unsigned int* sg_ptr = (int*)sg_ptr_base; // Pointer which will be incremented along the chain
  TRACE("build_sg_chain: sg_ptr 0x%lx, sg_phys 0x%lx\n", (unsigned long)sg_ptr, (unsigned long)sg_phys);

  for(sg = 0; sg < buf.xdomain.height; sg++){
    sg_ptr[0] = virt_to_phys(sg_ptr + SG_DESC_SIZE); // Pointer to next descriptor
    sg_ptr[1] = 0; // Upper 32 bits of descriptor pointer (unused)
    sg_ptr[2] = buf.phys_addr + (sg * buf.xdomain.stride * buf.xdomain.depth); // Address where the data lives
    sg_ptr[3] = 0; // Upper 32 bits of data address (unused)
    // Next 2 words are reserved
    sg_ptr[6] = (buf.xdomain.width * buf.xdomain.depth); // Width of data is width*depth of image
    if(sg == 0){
      sg_ptr[6] |= 0x08000000; // Start of frame flag
    }
    if(sg == buf.xdomain.height-1){
      sg_ptr[6] |= 0x04000000; // End of frame flag
    }
    sg_ptr[7] = 0; // Clear the status; the DMA engine will set this

    sg_ptr += SG_DESC_SIZE; // Move to next descriptor
  }
  TRACE("build_sg_chain: built SG table\n");

  // This is always mapped DMA_TO_DEVICE, since the DMA engine is reading the SG table
  // regardless of the data direction.
  *sg_phys = virt_to_phys(sg_ptr_base);
  //*sg_phys = dma_map_single(pipe_dev, sg_ptr_base, SG_DESC_BYTES * buf.height, DMA_TO_DEVICE);
}

// Use 2-D transfer feature in Xilinx AXI DMA.
// To enable this feature, DMA IP needs to be configured with Multichannel mode enabled.
// With this feature, a transfer of a sub-block of 2-D data requires only one descriptor.
void build_sg_chain_2D(const KBuffer buf, unsigned long* sg_ptr_base, unsigned long* sg_phys)
{
  unsigned int* sg_ptr = (int*)sg_ptr_base; // Pointer which will be incremented along the chain (2/18/18 WC updated from long to int)

  // the sg chain only has one descriptor
  sg_ptr[0] = virt_to_phys(sg_ptr + SG_DESC_SIZE); // Pointer to next descriptor
  sg_ptr[1] = 0; // Upper 32 bits of descriptor pointer (unused)
  sg_ptr[2] = buf.phys_addr; // Address where the data lives
  sg_ptr[3] = 0; // Upper 32 bits of data address (unused)

  // 0x10: AxUSER|AxCACHE|Rsvd|TUSER|Rsvd|TID|Rsvd|TDEST
  if (use_acp) {
    // AxCACHE: 0011 defines memory type as 'normal non-cacheable bufferable'
    // AxCACHE: 1111 defines memory type as 'write-back read and write-allocate'
    // AxUSER: tie off high to enable coherency when allowed by AxCACHE
    sg_ptr[4] = 0xff000000; // values for AxCACHE (1111) and AxUSER (1111)
  } else {
    // for some reason, AxCACHE (1111) with dma_map/unmap doesn't
    // work correctly on Linux 4.0 kernel, so we use 0011 for AxCACHE field
    sg_ptr[4] = 0x03000000; // values for AxCACHE (0011) and AxUSER (0000)
  }

  // 0x14: VSIZE|Rsvd|Stride
  sg_ptr[5] = buf.xdomain.stride * buf.xdomain.depth;  // Stride of the 2D block
  sg_ptr[5] |= (buf.xdomain.height << 19);     // VSize, i.e. the height of the block

  // 0x18: SOP|EOP|Rsvd|HSIZE
  sg_ptr[6] = (buf.xdomain.width * buf.xdomain.depth); // Width of data is width*depth of image
  sg_ptr[6] |= 0x08000000; // Start of frame flag
  sg_ptr[6] |= 0x04000000; // End of frame flag

  sg_ptr[7] = 0; // Clear the status; the DMA engine will set this


  // This is always mapped DMA_TO_DEVICE, since the DMA engine is reading the SG table
  // regardless of the data direction.
  *sg_phys = virt_to_phys(sg_ptr_base);
  //TRACE("build_sg_chain_2D: call dma_map_single()\n");
  //*sg_phys = dma_map_single(pipe_dev, sg_ptr_base, SG_DESC_BYTES * 1, DMA_TO_DEVICE);

  TRACE("build_sg_chain_2D: sg_ptr 0x%lx, sg_phys 0x%lx, *sg_ptr 0x%lx, *sg_phys 0x%lx\n", (unsigned long)sg_ptr, (unsigned long)sg_phys, (unsigned long)*sg_ptr, (unsigned long)*sg_phys);

}

void write_tap_values(struct hwacc_drvdata *drvdata, UBuffer *buf, void *tap)
{
        uint32_t data_size = buf->width * buf->depth;
        uint8_t *data = kzalloc(data_size, GFP_KERNEL);
        int retval, i;

        retval = copy_from_user(data, tap, data_size);
        if (retval) {
                ERROR("cannot copy data from user addr: %p\n", tap);
                return;
        }

        for (i = 0; i < buf->width; i++) {
                uint32_t value;
                switch (buf->depth) {
                        case 1: {
                                        value = data[i];
                                        break;
                                }
                        case 2: {
                                        uint16_t *d = (uint16_t*)data;
                                        value = d[i];
                                        break;
                                }
                        case 4: {
                                        uint32_t *d = (uint32_t*)data;
                                        value = d[i];
                                        break;
                                }
                        default: {
                                        DEBUG("Unknown tap size: %d\n",
                                              buf->depth);
                                        value = 0;
                                        break;
                                 }
                }
                // each value is stored in 8-byte register space
                iowrite32(value, drvdata->hls_controller
                                 + TAP_DATA_BASE
                                 + i * 0x08);
                DEBUG("write %d to hwacc hls_controller: 0x%x\n", value,
                      TAP_DATA_BASE + i * 0x08);
        }

        // clean up
        kfree(data);
}

/* Sets up a buffer for processing through the stencil path.  This drops the
 * image buffers into a BufferSet object, builds the scatter-gather tables,
 * and flushes the cache. Then it drops the BufferSet into the
 * queue to be pushed to the stencil path DMA engine as soon as it's free.
 */
int process_image(struct hwacc_drvdata *drvdata, KBuffer *buf_list)
{
  BufferSet* src;
  int i, flag, tap_count;
  KBuffer *buf;
  struct chan_buf *chan_buf;
  int retval;

  TRACE("process_image: begin\n");
  // Acquire a bufferset to pass through the processing chain
  wait_event_interruptible(drvdata->buffer_free_queue,
                           !buffer_listempty(&drvdata->free_list));
  src = buffer_dequeue(&drvdata->free_list);
  TRACE("src id is %d\n", src->id);
  TRACE("process_image: got BufferSet\n");
  /* copy buffer address */
  tap_count = 0;
  for (i = 0; i < drvdata->nr_channels + tap_count; i++) {
    /**
    * Keyi: height == 1 means tap value, which is an 1D array.
    * set the tap value here directly and then skip the buf,
    * since we don't have DMA channel on the fabric
    */
    if (buf_list[i].xdomain.height != 1) {
      DEBUG("src->chan_buf_list[%d]\n", i - tap_count);
      src->chan_buf_list[i - tap_count].buf = buf_list[i];
    } else {
      /* set the tap value */
      // TODO
      uint64_t addr = 0;
      uint64_t high = buf_list[i].xdomain.id;
      addr |= high << 32;
      addr |= buf_list[i].xdomain.stride;
      tap_count++;
      write_tap_values(drvdata, &buf_list[i].xdomain, (void *)addr);
    }
  }

  for (i = 0; i < drvdata->nr_channels; i++) {
    buf = &src->chan_buf_list[i].buf;
    DEBUG("buffer width: %d height: %d depth: %d\n",
           buf->xdomain.width,
           buf->xdomain.height,
           buf->xdomain.depth);
  }

  // Set up the scatter-gather descriptor chains
  for (i = 0; i < drvdata->nr_channels; i++) {
    chan_buf = &src->chan_buf_list[i];
    if (use_2D_mode) {
      build_sg_chain_2D(chan_buf->buf, chan_buf->sg, &chan_buf->sg_phys);
    } else {
      build_sg_chain(chan_buf->buf, chan_buf->sg, &chan_buf->sg_phys);
    }
  }

  // Map the buffers for DMA
  // This causes cache flushes for the source buffer(s)
  // The invalidate for the result happens on the unmap
  if (!use_acp) {
    for (i = 0; i < drvdata->nr_channels; i++) {
    chan_buf = &src->chan_buf_list[i];
    flag = drvdata->chan[i]->input_chan ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
    dma_map_single(drvdata->pipe_dev, chan_buf->buf.kern_addr,
                   chan_buf->buf.xdomain.height * chan_buf->buf.xdomain.stride
                                        * chan_buf->buf.xdomain.depth,
                   flag);
    }

    //flush_cache_all(); // flush l1 cache
    //outer_flush_all(); // flush l2 cache, kernel dump "bus error"
    TRACE("process_image: dma_map_single() finished.\n");
  }

  // Now throw this whole thing into the queue.
  // When the DMA engine is free, it will get pulled off and run.
  buffer_enqueue(&drvdata->queued_list, src);

  // Launch a work queue task to write this to the DMA
  retval = schedule_work(&drvdata->launch_work);

  TRACE("process_image: return\n");
  return(src->id);
}


void dma_launch_work(struct work_struct* ws)
{
  BufferSet* buf;
  int i;
  struct dma_chan *chan;
  struct chan_buf *chan_buf;
  struct hwacc_drvdata *drvdata = container_of(ws, struct hwacc_drvdata,
                                             launch_work);

  TRACE("dma_launch_work: begin\n");
  while(!buffer_listempty(&drvdata->queued_list)){
    buf = buffer_dequeue(&drvdata->queued_list);

    // Sleep until all the DMA engines are idle or halted (bits 0 and 1)
    // Write should always be idle first, but do all of them to be safe
    for (i = 0; i < drvdata->nr_channels; i++) {
      chan = drvdata->chan[i];
      wait_event_interruptible(chan->wq,
                               (ioread32(chan->controller + 0x04) & 0x00000003)
                               != 0);
    }

    // Kick off the DMA write operations
    TRACE("dma_launch_work: writing DMA registers\n");
    for (i = 0; i < drvdata->nr_channels; i++) {
      chan_buf = &(buf->chan_buf_list[i]);
      chan = drvdata->chan[i];
      DEBUG("dma_launch_work: sg_phys 0x%lx\n",
            (unsigned long)chan_buf->sg_phys);
      /* stop, so we can set the head ptr */
      iowrite32(0x00010002, chan->controller + 0x00);
      /* pointer to the fisrt descriptor */
      iowrite32(chan_buf->sg_phys, chan->controller + 0x08);
      /* run and enable interrupts */
      iowrite32(0x00011003, chan->controller + 0x00);

      if (use_2D_mode)
        /* 2D mode: last descriptor, starts transfer */
        iowrite32(chan_buf->sg_phys, chan->controller + 0x10);
      else
        /* last descriptor, starts transfer */
        iowrite32(chan_buf->sg_phys + (chan_buf->buf.xdomain.height - 1) * SG_DESC_BYTES,
                  chan->controller + 0x10);
    }

    // Start the stencil engine running
    // control register [7: auto_restart, --- 3: ap_ready, 2: ap_idle, 1: ap_done, 0: ap_start]
    iowrite32(0x00000001, drvdata->hls_controller + 0);
    TRACE("dma_launch_work: Transfers started\n");

    // Move the buffer to the processing list
    buffer_enqueue(&drvdata->processing_list, buf);
  } // END while(buffers in QUEUED list)
}

void dma_finished_work(struct work_struct* ws)
{
  BufferSet* buf;
  struct hwacc_drvdata *drvdata = container_of(ws, struct hwacc_drvdata,
                                             finished_work);

  TRACE("dma_finished_work: begin\n");

  // Check that all of the output DMAs have completed their work
  // TODO: bugs may lurk here if there are multiple outputs and the primary
  // finishes first.
  TRACE("finished_count: %d memory address: %p\n",
         atomic_read(&drvdata->finished_count), &drvdata->finished_count);

  while(atomic_read(&drvdata->finished_count) > 0) {

    // Decrement the completion count
    // This is the only part of the driver that will decrement these counts,
    // so we can be sure that if they're all >0, we have a frame.
    atomic_dec(&drvdata->finished_count);

    buf = buffer_dequeue(&drvdata->processing_list);
    DEBUG("dma_finished_work: buf: %lx\n", (unsigned long)buf);

    // Unmap each of the SG buffers
    //dma_unmap_single(pipe_dev, buf->input0_sg_phys, SG_DESC_BYTES * buf->input0.height, DMA_TO_DEVICE);
    //dma_unmap_single(pipe_dev, buf->output_sg_phys, SG_DESC_BYTES * buf->output.height, DMA_TO_DEVICE);

    // Unmap all the input and output buffers (with appropriate direction)
    // For the source buffers, this should do nothing.  For the result buffers,
    // it should cause a cache invalidate.
    if (!use_acp) {
      //dma_unmap_single(pipe_dev, buf->input0.phys_addr,
      //buf->input0.height * buf->input0.stride * buf->input0.depth, DMA_TO_DEVICE);
      //dma_unmap_single(pipe_dev, buf->output.phys_addr,
      //buf->output.height * buf->output.stride * buf->output.depth, DMA_FROM_DEVICE);
    TRACE("dma_finished_work: dma_unmap_single() finished.\n");
    }
    TRACE("we got it? %d\n", buffer_hasid(&drvdata->complete_list, buf->id));
    buffer_enqueue(&drvdata->complete_list, buf);
  }

  // Both the DMA launcher (which starts new DMA transactions) and the read()
  // operation (which waits for new data) want to know this is finished.
  TRACE("dma_finished_work: DMA read finished\n");
  wake_up_interruptible_all(&drvdata->processing_finished);
}

/* Blocks until a result is complete, and removes the buffer set from the queue.
 * This should be called once for each process_image call.
 */
void pend_processed(struct hwacc_drvdata *drvdata, int id)
{
  BufferSet* resultSet;

  TRACE("pend_processed: begin for bufferset %d\n", id);

  // Block until a completed buffer becomes available
  wait_event_interruptible(drvdata->processing_finished,
                           buffer_hasid(&drvdata->complete_list, id));

  // Remove the buffer
  resultSet = buffer_dequeueid(&drvdata->complete_list, id);
  if(resultSet == NULL){
    ERROR("buffer_dequeue for id %d failed!\n", id);
    return;
  }

  // Put the buffer set back on the free list
  buffer_enqueue(&drvdata->free_list, resultSet);
  wake_up_interruptible(&drvdata->buffer_free_queue);

  TRACE("pend_processed: return for bufferset %d\n", id);
}

long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct hwacc_drvdata *drvdata = filp->private_data;
        struct dma_chan *chan;
        size_t bsize = sizeof(UBuffer);
        int retval, i, tap_count = 0;
        UBuffer tmp_ubuf;
        KBuffer *tmp_kbuf;
        KBuffer tmp_buf[drvdata->nr_channels + max_tap_count];

        DEBUG("ioctl cmd %d | %lu (%lx) \n", cmd, arg, arg);
        switch (cmd) {
                case PROCESS_IMAGE:
                        TRACE("ioctl: PROCESS_IMAGE\n");
                        if (access_ok(VERIFY_READ,
                                     (void *)arg,
                                     drvdata->nr_channels * bsize)) {
                                for (i = 0;
                                     i < drvdata->nr_channels + tap_count;
                                     i++) {
                                        chan = drvdata->chan[i];
                                        /* copy_from_user return non-zero
                                         * upon error
                                         */
                                        if (copy_from_user(&tmp_ubuf,
                                                           (void*)(arg +
                                                                   i * bsize),
                                                           bsize)) {
                                                retval = -EIO;
                                                goto failed;
                                        }
                                        /* test if it's tap value */
                                        if (tmp_ubuf.height == 1) {
                                            tap_count++;
                                            /* only update the xdomain part */
                                            // TODO: fix this part
                                            tmp_buf[i].xdomain = tmp_ubuf;
                                        } else {
                                            tmp_kbuf = get_buffer_by_id(tmp_ubuf.id);
                                            if(tmp_kbuf == NULL) {
                                                retval = -ENOMEM;
                                            }
                                            tmp_buf[i] = *tmp_kbuf;
                                        }
                                }
                                return process_image(drvdata, tmp_buf);
                        }
                        /* cannot read or copy */
                        retval = -EIO;
                        goto failed;
                case PEND_PROCESSED:
                        TRACE("ioctl: PEND_PROCESSED\n");
                        pend_processed(drvdata, arg);
                        return 0;
                default:
                        DEBUG("Unknown cmd\n");
                        retval = -EINVAL; /* unknown command, return an error */
                        goto failed;
        }
failed:
        return retval;
}

int dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
  unsigned long physical_pfn, vsize;
  struct hwacc_drvdata *drvdata = filp->private_data;
  uintptr_t controller = (uintptr_t)drvdata->hls_controller;

  physical_pfn = (controller >> PAGE_SHIFT) + vma->vm_pgoff; // Physical page
  vsize = vma->vm_end - vma->vm_start; // Requested virtual size (in bytes)

  // Check that the user isn't asking to map too much (or too high)
  if(vsize > ((ACC_CONTROLLER_PAGES - vma->vm_pgoff) << PAGE_SHIFT)){
    return -EINVAL;
  }

  io_remap_pfn_range(vma, vma->vm_start, physical_pfn, vsize, vma->vm_page_prot);

  TRACE("dev_mmap virt: %lx, phys: %lx, size: %lx\n", vma->vm_start, physical_pfn << PAGE_SHIFT, vsize);
  return(0);
}

struct file_operations fops = {
  // No read/write; everything is handled by ioctl and mmap
  .open = dev_open,
  .release = dev_close,
  .unlocked_ioctl = dev_ioctl,
  .mmap = dev_mmap,
};

static irqreturn_t dma_irq_handler(int irq, void *data)
{
        struct dma_chan *chan = data;
        struct hwacc_drvdata *drvdata = chan->drvdata;

        /* we need to distinguish between input and output channel */
        if (chan->input_chan) {
                /* acknowledge/clear interrupt */
                iowrite32(0x00007000, chan->controller + 0x04);
                wake_up_interruptible(&chan->wq);
                //DEBUG("irq: DMA channel %d finished.\n", chan->id);

                return IRQ_HANDLED;
        } else {
                iowrite32(0x00007000, chan->controller + 0x04);
                /* the next processing action can now start */
                wake_up_interruptible(&chan->wq);
                TRACE("irq: DMA chan: %d finished.\n", chan->id);

                /* keep an explicit count of the number of buffers, to cover
                 * the rare(hopefully impossible) case where a second buffer
                 * finished before the work queue task actually executes. This
                 * would cause dma_finished_work to only execute once, when it
                 * was queued twice.
                 */
                atomic_inc(&drvdata->finished_count);
                /* delegate the work of moving the buffer from "PROCESSING" to
                 * "FINISHED".
                 * We can't do it here, since we're in an atomic context and
                 * trying to lock access to the list could cause us to block.
                 */
                DEBUG("irq: Launching workqueue to complete processing\n");
                /* launch a work queue task to write this to the DMA */
                schedule_work(&drvdata->finished_work);
                return 0;
        }
}

static int dma_chan_probe(struct device_node *node,
                          struct hwacc_drvdata *drvdata,
                          int chan_id)
{
        struct dma_chan *chan;
        struct device *dev = &drvdata->pdev->dev;
        int retval;

        chan = devm_kzalloc(dev, sizeof(*chan), GFP_KERNEL);

        if (!chan)
                return -ENOMEM;

        chan->drvdata = drvdata;
        chan->dev = dev;
        chan->id = chan_id;

        /* based on the naming to determine data flow direction */
	    if (of_device_is_compatible(node, "xlnx,axi-dma-mm2s-channel")) {
                chan->input_chan = true;
        } else if(of_device_is_compatible(node, "xlnx,axi-dma-s2mm-channel")) {
                chan->input_chan = false;
        } else {
                dev_err(dev, "unknown dma channel\n");
                retval = -EINVAL;
                goto failed0;
        }

        /* request IRQ */
        chan->irq = irq_of_parse_and_map(node, 0);
        retval = request_irq(chan->irq, dma_irq_handler, IRQF_SHARED,
                             "dma-irq-handler", chan);
        DEBUG("chan: %d irq->%d input?: %d\n", chan_id, chan->irq,
              chan->input_chan);
        if (retval < 0) {
                dev_err(dev, "request_irq() failed for chann id: %d\n",
                        chan_id);
                goto failed0;
        }

        /*
         * we don't do ioremap for individual channels. instead we calculate
         * the relative address based on the parent dma ioremap address
         */
        if (chan->input_chan) /* mm2s */
                chan->controller = XILINX_DMA_MM2S_CTRL_OFFSET
                                 + drvdata->chan_controller;
        else
                chan->controller = XILINX_DMA_S2MM_CTRL_OFFSET
                                 + drvdata->chan_controller;
        dev_notice(dev, "chan: %d controller: 0x%p\n", chan->id,
                   chan->controller);

        /* validate dma channel */
        if (check_dma_engine(chan->controller)) {
                dev_err(dev, "dma chan %d misconfigured or hung!\n", chan_id);
                retval = -ENODEV;
                goto failed1;
        }

        /* set up wait queue using macro */
        chan->wq = (wait_queue_head_t)__WAIT_QUEUE_HEAD_INITIALIZER(chan->wq);

        /* save to drvdata chan list */
        drvdata->chan[chan->id] = chan;

        return 0;

failed1:
        free_irq(chan->irq, chan);

failed0:
        return retval;
}

static int dma_chan_remove(struct dma_chan *chan)
{
        DEBUG("remove chan: %d irq: %d\n", chan->id, chan->irq);
        /* free irq */
        free_irq(chan->irq, chan);

        /* no need to free memory since chan is allocated based on dev */
        return 0;
}


static int dma_child_probe(struct device_node *node,
                           struct hwacc_drvdata *drvdata,
                           int index)
{
        int i, retval, nr_channels = 1;
        struct device *dev = &drvdata->pdev->dev;

        retval = of_property_read_u32(node, "dma-channels", &nr_channels);
        if (retval < 0) {
                dev_err(dev,  "dma-channels not found\n");
                return retval;
        }
 
        /* loop through the dma channels */
        for (i = 0; i < nr_channels; i++) {
                retval = dma_chan_probe(node, drvdata, drvdata->chan_id++);
                /* set the dma channel index */
                drvdata->chan[drvdata->nr_channels + i]->index = index;
        }

        drvdata->nr_channels += nr_channels;

        return 0;
}

static void swap_dma_channel(struct hwacc_drvdata *drvdata, int index1, int index2)
{
        struct dma_chan *temp;
        temp = drvdata->chan[index2];
        drvdata->chan[index1]->id = temp->id;
        temp->id = index1;
        drvdata->chan[index2] = drvdata->chan[index1];
        drvdata->chan[index1] = temp;
        DEBUG("swap between chan: %d and chan %d\n", index1, index2);
}

static int init_dma(struct hwacc_drvdata *drvdata)
{
        struct platform_device *pdev = drvdata->pdev, *dma;
        struct resource *io;
        struct device_node *child, *dma_node;
        int i, retval, entry_index, index;
        const char *dma_name;
        /*
         * find the phandle for dma. we need to read both dmas-in and
         * we don't need to actually distinguish the difference between
         * in or out as a single read/write dma will only have one channel.
         */
        /* simply iterate through the list */
        i = 0;
        while ((dma_node = of_parse_phandle(pdev->dev.of_node,
                                            "dmas", i++))) {
                dma = of_find_device_by_node(dma_node);
                if (!dma) {
                        retval = -ENODEV;
                        goto failed0;
                }
                /* request and map I/O memory  for dma */
                io = platform_get_resource(dma, IORESOURCE_MEM, 0);
                drvdata->chan_controller =
                        devm_ioremap_resource(&pdev->dev, io);

                if (IS_ERR(drvdata->chan_controller)) {
                        retval = -ENOMEM;
                        goto failed0;
                }


                retval = of_property_read_u32(dma_node, "dma-index", &index);
                if (retval < 0) {
                    /* no dma-index info present, set to -1 to ignore */
                    index = -1;
                }

                retval = of_property_read_string(dma_node, "hw-name", &dma_name);
                if (!retval)
                    DEBUG("dma name: %s\n", dma_name);

                /* initialize channels */
                for_each_child_of_node(dma_node, child) {
                        retval = dma_child_probe(child, drvdata, index);
                        if (retval < 0)
                                goto failed1;
                }

                of_node_put(dma_node);
        }

        /*
         * because we only have one output, we can simply make the rule that
         * the last channel/buffer is the output channel. If we're heading
         * to multi-output in the future, we need to have annother ioctl
         * command to reorder the channels and additional entry in the device
         * tree overlay to indicate the dma index.
         */
        for (i = 0; i < drvdata->nr_channels; i++) {
            if ((!drvdata->chan[i]->input_chan)
                && (i != drvdata->nr_channels - 1)) {
                /*
                 * we've found output data channel and it's not the last
                 * one. swap them
                 */
                swap_dma_channel(drvdata, i, drvdata->nr_channels - 1);
                break;
            }
        }
        /* double check */
        if (drvdata->chan[drvdata->nr_channels - 1]->input_chan) {
            ERROR("last channel is not output channel\n");
            retval = -ENXIO;
            goto failed0;
        }

        /* then we re-arrange channel again */
        for (i = 0; i < drvdata->nr_channels - 1; i++) {
            entry_index = drvdata->chan[i]->index;
            if (entry_index != -1 && entry_index != i) {
                /* we need to swap them as we go */
                swap_dma_channel(drvdata, i, entry_index);
            }
        }

        of_node_put(dma_node);
        return 0;

failed1:
        for (i = 0; i < drvdata->nr_channels; i++) {
                if (drvdata->chan[i])
                        dma_chan_remove(drvdata->chan[i]);
        }

failed0:
        of_node_put(dma_node);
        return retval;
}

static int find_set_gpio(struct hwacc_drvdata *drvdata)
{
        struct platform_device *pdev = drvdata->pdev, *gpio;
        struct device_node *node = NULL;
        struct resource *io;
        int retval;

        if (has_gpio)
                return 0;

        /* we foudn the actual gpio node */
        node = of_parse_phandle(pdev->dev.of_node, "gpio", 0);;
        if (!node) {
            dev_err(&pdev->dev, "gpio phandle not found\n");
            retval = -ENODEV;
            goto failed;
        }
        gpio = of_find_device_by_node(node);
        io = platform_get_resource(gpio, IORESOURCE_MEM, 0);
        drvdata->gpio_controller = devm_ioremap_resource(&pdev->dev, io);

        of_node_put(node); /* decrease the refcount */
        /*
         * no need to clean the memory since it's managed by kernel
         * we only need to write the 0x02 to the register
         */
        iowrite32(0x00000002, (void*)drvdata->gpio_controller);
        
        /* we've set GPIO */
        has_gpio = true;
        return 0;

failed:
        return retval;

}

static int hwacc_probe(struct platform_device *pdev)
{
        struct hwacc_drvdata *drvdata;
        struct resource *io;

        int retval, i;

		DEBUG("[hwacc] probe function entry\n");

        /* alocate drvdata */
        drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);

        if (!drvdata) {
                dev_err(&pdev->dev, "devm_kzalloc() failed\n");
                return -ENOMEM;
        }

        /* set up drvdata */
        drvdata->pdev = pdev;
        atomic_set(&drvdata->usage_count, 0);
        init_waitqueue_head(&drvdata->processing_finished);
        init_waitqueue_head(&drvdata->buffer_free_queue);

        /* request and map I/O memory  for hwacc*/
        io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        drvdata->hls_controller = devm_ioremap_resource(&pdev->dev, io);

        if (!drvdata->hls_controller) {
                dev_err(&pdev->dev, "ioremap() failed for hls");
                retval = -ENOMEM;
                goto failed0;
        }

        // TODO:
        // MAY NEED TO ADD BACK AS NEEDED
        /* ignore list:
         * include-sg
         * xlnx,addrwidth
         */

        platform_set_drvdata(pdev, drvdata);

        retval = find_set_gpio(drvdata);
        if (retval < 0)
                goto failed0;

        /* setup dma */
        retval = init_dma(drvdata);
        if (retval < 0)
                goto failed0;

        /* set up buffer list */
        retval = setup_buffer_list(drvdata);
        if (retval < 0)
                goto failed0;

        /* manually setup the work queue because we want to pass drvdata
         * into the callback.
         * corresponding structures that get put in the queues.
         * we only need one instance, since we're simply launching work,
         * not queuing up a bunch of tasks.
         */

        INIT_WORK(&drvdata->launch_work, dma_launch_work);
        INIT_WORK(&drvdata->finished_work, dma_finished_work);

        /* Get a single character device number */
        alloc_chrdev_region(&drvdata->device_num, 0, 1, DEVNAME);
        DEBUG("Device registered with major %d, minor: %d\n",
              MAJOR(drvdata->device_num), MINOR(drvdata->device_num));

        /* set up the device so we show up in sysfs,
         * and so we have a device we can hand to the DMA request
         */

        for (drvdata->dev_index = 0; drvdata->dev_index < MAX_HWACC_MODULE;
             drvdata->dev_index++) {
                if (!device_usage[drvdata->dev_index]) {
                        device_usage[drvdata->dev_index] = true;
                        break; /* we've found a free device slot */
                }
        }
        if (drvdata->dev_index == MAX_HWACC_MODULE) {
                /* exceeds the max number of devices */
                dev_err(&pdev->dev, "exceeds maximum number of char devices\n");
                retval = -ENOMEM;
                goto failed1;
        }

        drvdata->pipe_dev = device_create(pipe_class, &pdev->dev,
                                          drvdata->device_num, 0,
                                          DEVNAME "%d", drvdata->dev_index);

        /* register the driver with the kernel */
        cdev_init(&drvdata->cdev, &fops);
        drvdata->cdev.owner = THIS_MODULE;
        cdev_add(&drvdata->cdev, drvdata->device_num, 1);

        /* set drvdata to everything */
        platform_set_drvdata(pdev, drvdata);
        dev_set_drvdata(drvdata->pipe_dev, drvdata);

        /* print out the channel order, just to be sure */
        for (i = 0; i < drvdata->nr_channels; i++) {
                DEBUG("[%d] dma chan id %d: irq: %d\n", i,
                      drvdata->chan[i]->id, drvdata->chan[i]->irq);
        }


        DEBUG("[hwacc] probe function exit\n");

        return(0);

failed1:
        /* remove the allocated region */
        unregister_chrdev_region(drvdata->device_num, 1);

failed0:
        /* drvdata is allocated to pdev->dev, no need to clean up */
        return retval;
}

static int hwacc_remove(struct platform_device *pdev)
{
        int i;
        struct hwacc_drvdata *drvdata = platform_get_drvdata(pdev);
		
		DEBUG("[hwacc] hwacc_remove entry\n");

		DEBUG("[hwacc] hwacc_remove entry\n");
        /* clear each channel */
        for (i = 0; i < drvdata->nr_channels; i++) {
                if (drvdata->chan[i])
                        dma_chan_remove(drvdata->chan[i]);
        }

        /* all ioremap are call using devm_, hence no manual cleanup needed */

        device_unregister(drvdata->pipe_dev);
        cdev_del(&drvdata->cdev);
        unregister_chrdev_region(drvdata->device_num, 1);

        device_usage[drvdata->dev_index] = false;

        dev_notice(&pdev->dev, "hwacc removed");
        DEBUG("[hwacc] hwacc_remove exit\n");
        return(0);
}

static struct of_device_id hwacc_of_match[] = {
        /* bind to hls_module */
        { .compatible = "hls-target", },
        {}
};

static struct platform_driver hwacc_driver = {
	.driver = {
		.name = "hwacc",
		.owner = THIS_MODULE,
		.of_match_table = hwacc_of_match,
	},
	.probe = hwacc_probe,
	.remove = hwacc_remove,
};


static int uevent(struct device *dev, struct kobj_uevent_env *env)
{
        add_uevent_var(env, "DEVMODE=%#o", 0766);
        return 0;
}

static int __init hwacc_init(void)
{
		int ret = 0;
		DEBUG("[hwacc] hwacc_init entry\n");
        pipe_class = class_create(THIS_MODULE, CLASSNAME);
        /* allow non-root access */
        pipe_class->dev_uevent = uevent;
        ret = platform_driver_register(&hwacc_driver);
        DEBUG("[hwacc] hwacc_init exit\n");
        return ret;
}

static void __exit hwacc_exit(void)
{
		DEBUG("[hwacc] hwacc_exit entry\n");
        platform_driver_unregister(&hwacc_driver);
        /* class destory has to happen after unregister */
        class_destroy(pipe_class);
        DEBUG("[hwacc] hwacc_exit exit \n");
}

/*
 * because we need to make sure the device class created first, we need to
 * manually set the init and exit code instead of using built-in macro
 */
module_init(hwacc_init);
module_exit(hwacc_exit);
