/* r5queue.cpp
 * Code which enqueues requests for execution by the R5 real-time process
 */

#include <metal/sys.h>
#include <metal/device.h>
#include <metal/io.h>
#include <metal/alloc.h>
#include "ttc_clock.h"

#define BUS_NAME        "platform"
#define SHM_DEV_NAME    "3e800000.shm"

// WARNING: the size of an enum is not defined by the C standard, and may
// differ across platforms.

typedef enum {
  CAMERA0,
  CAMERA1,
  NO_DEVICE
} ReqDevice;

typedef struct {
  uint32_t exposure;
} CameraParams;

typedef struct {
  ReqDevice device;
  Time time;
  CameraParams params;
} Request;


const int QUEUE_LEN = 10;

int main(int argc, char* argv[])
{
  ttc_clock_init();

  struct metal_init_params metal_param = METAL_INIT_DEFAULTS;
  metal_init(&metal_param);
  // We don't have to register anything; that should come from the devicetree

  struct metal_device* shm_dev = NULL;
  int err = metal_device_open(BUS_NAME, SHM_DEV_NAME, &shm_dev);
  if(err){
    printf("Failed to open metal device\n");
    return(err);
  }

	struct metal_io_region *io = NULL;
	io = metal_device_io_region(shm_dev, 0);
	if (!io) {
    printf("Failed to open I/O region\n");
    return(-ENODEV);
	}

  /* layout:
   * 0x00: master pointer
   * 0x04: slave pointer
   * 0x08: buffers
   */
  uint32_t master = 0; // Our count (id of last packet completed)
  metal_io_write32(io, 0, master); // Initialize the master count

  printf("Enter a number for the exposure, q to quit.\n");
  while(true){ // Loop until we break out
    // Get a string of input
    // If it begins with 'q', then quit
    // If it contains a number, use that as the exposure
    // Otherwise, print a usage message
    printf("> "); // command prompt
    char command[200];
    scanf("%s", command);
    if(command[0] == 'q'){
      break; // Quit
    }
    uint32_t exposure = (uint32_t)strtol(command, NULL, 10);

    uint32_t slave = metal_io_read32(io, 4); // Current slave pointer

    // Move to the next slot, and check that we're not going to overflow the
    // ring buffer
    master = (master + 1) % QUEUE_LEN;
    if(master == slave){
      printf("Message queue is full!\n");
      break; // Just die, for now
    }

    // Send a message to the R5
    Request req;
    req.device = CAMERA0;
    req.params.exposure = exposure;
    req.time = ttc_clock_now() + 5e5; // 500ms from now
    printf("\nRequesting exposure %d at time %ld\n", req.params.exposure, req.time);

    metal_io_block_write(io, 0x08 + master*(sizeof(Request)), &req, sizeof(Request));

    metal_io_write32(io, 0, master); // Update the master count
  }

  metal_device_close(shm_dev);
  metal_finish();
}

