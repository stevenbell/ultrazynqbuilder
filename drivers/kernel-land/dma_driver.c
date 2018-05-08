/**
 * @file driver.c
 * @authors:	Gedeon Nyengele <nyengele@stanford.edu>
 * 				
 * @date April 25 2018
 * version 1.0
 * @brief DMA S2MM Driver (base on Steven Bell's VDMA driver)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <asm/page.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include "kbuffer.h"
#include "ioctl_cmds.h"
#include "common.h"
#include "dma.h"

#define CLASS_NAME "xilcam"
#define DEVICE_NAME "xilcam0"

#define N_BUFFERS	3
#define IMG_WIDTH	1920
#define IMG_HEIGHT	1080
#define IMG_DEPTH	2
#define IMG_STRIDE	(IMG_WIDTH)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gedeon Nyengele");
MODULE_DESCRIPTION("DMA S2MM Driver");
MODULE_VERSION("1.0");

const int debug_level = 4;

static int majorNumber;
static struct class *drvClass = NULL;
static struct device *drvDevice = NULL;

static unsigned nOpens = 0;

// driver internal state variables
struct resource* irq_res = NULL; // interrupt
struct resource* reg_res = NULL; // register space
static char *dev_base_addr = NULL; // base address for register space
static char *sg_base_addr = NULL; // base address for SG descritor table
static struct KBuffer *dma_buffers[N_BUFFERS];

// external functions defined in our custom buffer allocator (KBuffer)
extern struct KBuffer *acquire_buffer(u32, u32, u32, u32);
extern void release_buffer(u32);

// wait queue
// for in-progess frames.
// Threads waiting on this are woken up
// each time anew frame is finished and an interrupt occurs
DECLARE_WAIT_QUEUE_HEAD(wq_frame);
atomic_t new_frame; // do we have a new unread frame or not

static int dev_open(struct inode *inodep, struct file *filep)
{
	int i;
	unsigned long temp_val;
	char *sg;
	u32 addr;

	DEBUG("[dma-mod] dev_open entry\n");

	// enfore single device opening
	// we do not support multiple device openings yet
	if(nOpens) {
		DEBUG("[dma-mod] driver supports only one device opening at a time\n");
		return -EBUSY;
	}
	nOpens++;

	// allocate space for SG descriptor table
	DEBUG("[dma-mod] allocating space for SG descriptor table...\n");
	sg_base_addr = kmalloc((N_BUFFERS + 1) * DESC_SIZE, GFP_KERNEL);
	if(sg_base_addr == NULL) {
		ERROR("[dma-mod] could not allocate memory for SG Descriptor Table\n");
		return -ENOMEM;
	}
	DEBUG("[dma-mod] sg_base_addr = 0x%08x\n", (u32)sg_base_addr);
	DEBUG("[dma-mod] touching the SG desc table memory region so kernel can do mappings...\n");
	memset(sg_base_addr, 0, (N_BUFFERS + 1) * DESC_SIZE);
	DEBUG("[dma-mod] SG desc table memory touch completed\n");
	DEBUG("[dma-mod] successfully allocated memory for the SG descriptor tbale\n");

	// allocate N_BUFFERS KBuffers
	DEBUG("[dma-mod] allocating KBuffers for dma ring buffer...\n");
	temp_val = 0;
	for(i = 0; i < N_BUFFERS; i++) {
		DEBUG("\tacquiring buffer [%d]...\n", i);
		dma_buffers[i] = acquire_buffer(IMG_WIDTH, IMG_HEIGHT, IMG_DEPTH, IMG_STRIDE);
		if(dma_buffers[i] == NULL) {
			DEBUG("\tfailed to acquire ring buffer [%d]\n", i);
			break;
		}
		DEBUG("\tring buffer [%d] successfully acquired\n", i);
		temp_val++;
	}
	if(temp_val != N_BUFFERS) {
		DEBUG("[dma-mod] failed to acquire [%d] ring buffers\n", N_BUFFERS);
		DEBUG("[dma-mod] releasing the [%d] acquired ring buffers...\n", temp_val);
		for(i = temp_val - 1; i >= 0; i--) {
			release_buffer(dma_buffers[i]->xdomain.id);
		}
		DEBUG("dma-mod] ring buffer release completed\n");
		kfree(sg_base_addr);
		ERROR("[dma-mod] could not allocate KBuffers for the %d ring buffers\n", N_BUFFERS);
		return -ENOMEM;
	}
	DEBUG("[dma-mod] successfully acquired all %d ring buffers\n", N_BUFFERS);
	// set up the SG descriptor table
	DEBUG("[dma-mod] setting up the SG descriptor table...\n");
	sg = sg_base_addr;
	DEBUG("[dma-mod] getting SG decriptor table physical address...\n");
	addr = (u32) virt_to_phys(sg_base_addr);
	DEBUG("[dma-mod] SG desc table physical address obtained\n");
	for(i = 0; i < N_BUFFERS; i++) {
		DEBUG("[dma-mod] setting up descriptor for buffer [%d]...\n", i);
		*(volatile u32 *)(sg + 0x00) = (u32)(addr + ((i+1) % N_BUFFERS) * 0x40); // next descriptor address 
		*(volatile u32 *)(sg + 0x04) = 0; // NEXTDESC_MSB = 0 for 32-bit addresses
		*(volatile u32 *)(sg + 0x08) = (u32)(dma_buffers[i]->phys_addr); // buffer address (lower 32 bits)
		*(volatile u32 *)(sg + 0x0C) = 0; // buffer address MSB = 0 for 4GB memory size
		*(volatile u32 *)(sg + 0x10) = 0x0f << AWCACHE_OFFSET;
		*(volatile u32 *)(sg + 0x14) = (IMG_HEIGHT << VSIZE_OFFSET) | ((IMG_STRIDE * IMG_DEPTH) << STRIDE_OFFSET);
		*(volatile u32 *)(sg + 0x18) = (IMG_WIDTH * IMG_DEPTH) << HSIZE_OFFSET;
		*(volatile u32 *)(sg + 0x1C) = 0;
		sg += 0x40;
		DEBUG("[dma-mod] finished setting up descriptor for buffer [%d]\n", i);
	}
	DEBUG("[dma-mod] SG descriptor table setup is complete\n");

	DEBUG("[dma-mod] setting up the DMA engine (register access)...\n");
	// enable coherency for SG transactions
	iowrite32(0x0000000f, (void *)(dev_base_addr + 0x2C));

	// enable dma,  set IRQThreshold to 1, and enable IOC_irq
	iowrite32((1 << 0) | (1 << 16) | (1 << 12), (void *)(dev_base_addr + 0x30));

	// set current descriptor pointer
	iowrite32(addr, (void *)(dev_base_addr + 0x38));
	iowrite32(0, (void *)(dev_base_addr + 0x3C));

	// set the tail descritor pointer to kickstart dma operations
	iowrite32(addr + 0x40 * (N_BUFFERS-1), (void *)(dev_base_addr + 0x40));
	DEBUG("[dma-mod] DMA engine setup complete\n");

	DEBUG("[dma-mod] dev_open exit\n");
	return 0;
}

static int dev_close(struct inode *inodep, struct file *filep)
{
	int i;
	unsigned int reg_val;
	DEBUG("[dma-mod] dev_close entry\n");

	// stop dma
	reg_val = ioread32((const volatile void *)(dev_base_addr + 0x30));
	iowrite32(reg_val & (-2), (void *)(dev_base_addr + 0x30));

	// free KBuffers
	DEBUG("[dma-mod] releasing KBuffersfor ring buffers\n");
	for(i = 0; i < N_BUFFERS; i++) {
		release_buffer(dma_buffers[i]->xdomain.id);
	}

	// free SG descriptor table
	DEBUG("[dma-mod] releasing SG descriptor table memory\n");
	kfree(sg_base_addr);

	// adjust nOpens
	nOpens--;

	DEBUG("[dma-mod] dev_close exit\n");
	return 0;
}

static long grab_image(struct UBuffer *buf)
{	
	int i;
	unsigned int temp_val;
	char *sg;
	struct KBuffer *rep_buf;

	DEBUG("[dma-mod] grab_image entry\n");

	// if the DMA is idle, reset SG descriptor flags and run
	// this is because we don't know how old the frames are
	temp_val = ioread32((const volatile void *)(dev_base_addr + 0x34));
	sg = sg_base_addr;
	if(temp_val & 0x02) {
		// reset descriptor flags
		for(i = 0; i < N_BUFFERS; i++) {
			*(volatile u32 *)(sg + 0x1C) = 0;
			sg += 0x40;
		}

		// reset new_frame flag
		atomic_set(&new_frame, 0);

		// kickstart the DMA by writing TAIL DESC
		iowrite32((u32)virt_to_phys(sg_base_addr) + 0x40 * (N_BUFFERS-1), (void *)(dev_base_addr + 0x40));
	}

	// wait until we have a new image
	wait_event_interruptible(wq_frame, atomic_read(&new_frame) == 1);
	atomic_set(&new_frame, 0);

	// get index of completed descriptor
	sg = (char *) ioread32((const volatile void *)(dev_base_addr + 0x38)); // get current descriptor pointer
	temp_val = ((unsigned)sg - (unsigned) virt_to_phys(sg_base_addr)) >> 6; // get current descriptor index
	temp_val = temp_val ? temp_val - 1 : (N_BUFFERS - 1); // get index of previous descriptor (the actual completed one)
	
	// get a replacement buffer
	rep_buf = acquire_buffer(IMG_WIDTH, IMG_HEIGHT, IMG_DEPTH, IMG_STRIDE);
	if(rep_buf == NULL) {
		ERROR("[dma-mod] failed to acquire a replacement buffer for ring buffers\n");
		return -ENOMEM;
	}

	// copy finished buffer to user and replace in SG descriptor table
	*buf = dma_buffers[temp_val]->xdomain;
	*(volatile u32 *)(sg_base_addr + 0x40*temp_val + 0x08) = (u32) rep_buf->phys_addr;
	dma_buffers[temp_val] = rep_buf;
	
	DEBUG("[dma-mod] grab_image exit\n");
	return 0;
}

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long user_arg)
{
	struct UBuffer tmp;

	DEBUG("[dma-mod] dev_ioctl entry\n");

	switch(cmd) {
		case GRAB_IMAGE:
			DEBUG("[dma-mod] ioctl [GRAB_IMAGE]\n");
			if(grab_image(&tmp) == 0 && access_ok(VERIFY_WRITE, (void*)user_arg, sizeof(struct UBuffer))) {
				if(copy_to_user((void*)user_arg, &tmp, sizeof(struct UBuffer) != 0)) {
					return -EIO;
				}
			} else {
				return -ENOMEM;
			}
		break;
		default:
			DEBUG("[dma-mod] un-supported ioctl [0x%08x]\n", cmd);
			return -EINVAL;
		break;
	}

	DEBUG("[dma-mod] dev_ioctl exit\n");
	return 0;
}


// interrupt handler for when a frame finishes
irqreturn_t frame_finished_handler(int irq, void *dev_id)
{
	// ackowledge interrupt
	iowrite32((1<< 12), (void *)(dev_base_addr + 0x34));

	// dispath "new_frame" event
	atomic_set(&new_frame, 1);
	wake_up_interruptible(&wq_frame);
	return (IRQ_HANDLED);
}

// device operations callbacks
static struct file_operations fops = {
	.open			= dev_open,
	.release		= dev_close,
	.unlocked_ioctl	= dev_ioctl,
};

// device probe callback
static int dev_probe(struct platform_device *pdev)
{
	int status;
	DEBUG("[dma-mod] dev_probe entry\n");

	// get device interrupt and register interrupt callback
	DEBUG("[dma-mod] setting up irq handler\n");
	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if(irq_res == NULL) {
		ERROR("[dma-mod] IRQ lookup failed. Check device tree\n");
		return -ENODEV;
	}
	status = request_irq(irq_res->start, frame_finished_handler, 0, "xilcam", NULL);
	DEBUG("[dma-mod] IRQ handler registered, device IRQ number is %d\n", irq_res->start);

	// dynamically get device's register space info
	DEBUG("[dma-mod] getting device register space\n");
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(reg_res == NULL) {
		if(irq_res) {
			free_irq(irq_res->start, NULL);
			irq_res = NULL;
		}
		ERROR("[dma-mod] Register space lookup failed. Check device tree\n");
		return -ENODEV;
	}

	// we only need to ioremap a small chunk
	// so we'll remap a page
	dev_base_addr = ioremap(reg_res->start, PAGE_SIZE);
	if(dev_base_addr == NULL) {
		if(irq_res) {
			free_irq(irq_res->start, NULL);
			irq_res = NULL;
		}
		reg_res = NULL;
		ERROR("[dma-mod] failed to ioremap device register space\n");
		return -ENODEV;
	}
	DEBUG("[dma-mod] device register space remapped successfully\n");

	// dynamically allocate a major number for the device
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if(majorNumber < 0) {
		WARNING("[dma-mod]: failed to register a major number for device [%s]\n", DEVICE_NAME);
		return majorNumber;
	}
	DEBUG("[dma-mod]: registered device [%s] with major number [%d]\n", DEVICE_NAME, majorNumber);

	// register the device class
	drvClass = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(drvClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		WARNING("[dma-mod]: failed to register device class [%s]\n", CLASS_NAME);
		return PTR_ERR(drvClass);
	}
	DEBUG("[dma-mod]: device class [%s] registered successfully\n", CLASS_NAME);

	// register the device driver
	drvDevice = device_create(drvClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(drvDevice)) {
		class_destroy(drvClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		WARNING("[dma-mod]: failed to register device [%s]\n", DEVICE_NAME);
		return PTR_ERR(drvDevice);
	}
	
	DEBUG("[dma-mod]: device driver registered successfully\n");
	DEBUG("[dma-mod] dev_probe exit\n");
	return 0;
}

static int dev_remove(struct platform_device *pdev)
{
	DEBUG("[dma-mod] dev_remove entry\n");
	if(irq_res) {
		free_irq(irq_res->start, NULL);
		irq_res = NULL;
	}
	if(dev_base_addr) {
		iounmap(dev_base_addr);
		dev_base_addr = NULL;
		reg_res = NULL;
	}

	device_destroy(drvClass, MKDEV(majorNumber, 0)); 	// remove the device driver
	class_unregister(drvClass); 							// unregister the device class
	class_destroy(drvClass); 							// remove the device class
	unregister_chrdev(majorNumber, DEVICE_NAME); 			// unregister the major number
	DEBUG("[dma-mod]: device driver exited successfully\n");
	DEBUG("[dma-mod] dev_remove exit \n");
	return 0;
}

// driver match strings
static struct of_device_id dma_of_match[] = {
	{.compatible = "xilcam", },
	{},
};

// dma driver
static struct platform_driver dma_driver =  {
	.driver = {
		.name = "xilcam",
		.of_match_table = dma_of_match,
	},
	.probe = dev_probe,
	.remove = dev_remove,
};

// register platform DMA driver
module_platform_driver(dma_driver);
