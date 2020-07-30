#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"

#include "i2c.h"
#include "ttc_clock.h"

const u32 GPIO_CAMERA = XPAR_AXI_GPIO_0_BASEADDR + 0x8;

#include <metal/sys.h>
#include <metal/device.h>
#include <metal/io.h>
#include <metal/alloc.h>

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

int main()
{
  u32* gpio = (u32*)GPIO_CAMERA;

  ttc_clock_init();

#ifdef USING_AMP
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

  // Initialize our current pointer
  metal_io_write32(io, 0, 0);
#endif

  init_platform(); // Interrupts and such

  while(1){
    // Wait for shmem flag to go high 10 times
    for(int i = 0; i < 10; i++){
      while(metal_io_read32(io, 0) == 0){}
      // Reset the shmem flag
      metal_io_write32(io, 0, 0);
    }
    // Set the GPIO pin to let the logic analyzer know we're here
    *gpio = 0x7;
  }

}
