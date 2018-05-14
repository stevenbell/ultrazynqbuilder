/* requestqueue.c
 */

#include <metal/sys.h>
#include <metal/device.h>
#include <metal/io.h>
#include <metal/alloc.h>

#include "requestqueue.h"

// Set up the shared memory


#define BUS_NAME        "generic"
#define SHM_DEV_NAME    "3e800000.shm"

#define SHM_BASE_ADDR   0x3E800000

/* Each I/O region can contain multiple pages.
 * In baremetal system, the memory mapping is flat, there is no
 * virtual memory.
 * We can assume there is only one page in the whole baremetal system.
 */
#define DEFAULT_PAGE_SHIFT (-1UL)
#define DEFAULT_PAGE_MASK  (-1UL)

// TODO: move these into their own instance struct so they aren't global?
struct metal_device* shm_dev;
struct metal_io_region *io = NULL;

const metal_phys_addr_t phys_bases[1] = {SHM_BASE_ADDR};

// Shared memory device
const struct metal_device dev = {
  .name = SHM_DEV_NAME,
  .bus = NULL,
  .num_regions = 1,
  .regions = {
    {
      .virt = (void *)SHM_BASE_ADDR,
      .physmap = (metal_phys_addr_t*)phys_bases,
      .size = 0x800000,
      .page_shift = DEFAULT_PAGE_SHIFT,
      .page_mask = DEFAULT_PAGE_MASK,
      .mem_flags = NORM_SHARED_NCACHE |
          PRIV_RW_USER_RW,
      .ops = {NULL},
    }
  },
  .node = {NULL},
  .irq_num = 0,
  .irq_info = NULL,
};



void requestqueue_init(void)
{
  struct metal_init_params metal_param = METAL_INIT_DEFAULTS;
  metal_init(&metal_param);

  int err = metal_register_generic_device(&dev);
  if(err){
    print("Failed to register device\n");
  }

  err = metal_device_open(BUS_NAME, SHM_DEV_NAME, &shm_dev);
  if(err){
    print("Failed to open device\n");
  }

  // Now get the I/O region
  io = metal_device_io_region(shm_dev, 0);
  if(!io){
    print("Failed to get I/O region\n");
  }
}

void requestqueue_close(void)
{
  metal_device_close(shm_dev);
  metal_finish();
}

/* layout:
 * 0x00: master pointer
 * 0x04: slave pointer
 * 0x08: buffers
 */
void requestqueue_check(void)
{
  static u32 slave = 0;

  // If the master pointer is ahead of the slave pointer, then we have data to read
  // Because of wraparound, any inequality means we're behind.
  u32 master = metal_io_read32(io, 0);
  while(master != slave){
    // Read the next request
    Request req;
    int ok = metal_io_block_read(io, 0x08 + slave*sizeof(Request), &req, sizeof(Request));
    if(ok){
      printf("received request %lu (just gonna ignore it...)\n", slave);
    }

    // Update the slave pointer
    slave = (slave + 1) % QUEUE_LEN;
  }
  metal_io_write32(io, 4, slave);

}
