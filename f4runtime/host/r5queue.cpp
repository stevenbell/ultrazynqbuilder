/* r5queue.cpp
 * Code which enqueues requests for execution by the R5 real-time process
 */

#include <metal/sys.h>
#include <metal/device.h>
#include <metal/io.h>
#include <metal/alloc.h>
#include "ttc_clock.h"
#include "mailbox.h"

#define BUS_NAME        "platform"
#define SHM_DEV_NAME    "3e800000.shm"

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

  uint32_t a2r_head = 0; // Our count (location of last Request sent)
  metal_io_write32(io, A2R_HEAD, a2r_head); // Initialize the a2r_head count

  uint32_t r2a_tail = 0; // Our count (id of last packet completed)
  //metal_io_write32(io, R2A_TAIL, r2a_tail);

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
    uint32_t param = (uint32_t)strtol(command, NULL, 10);

    uint32_t tail = metal_io_read32(io, A2R_TAIL);

    // Move to the next slot, and check that we're not going to overflow the
    // ring buffer
    a2r_head = (a2r_head + 1) % QUEUE_LEN;
    if(a2r_head == tail){
      printf("Message queue is full!\n");
      break; // Just die, for now
    }

    // Send a message to the R5
    Request req;

    req.device = SHUTTERBUTTON;
    req.camParams.exposure = param;
    req.time = ttc_clock_now() + 5e5; // 500ms from now
    printf("\nRequesting exposure %d on dev %d at time %ld\n", req.camParams.exposure, req.device, req.time);

/*
    req.device = FLASH0;
    req.flashParams.duration = param;
    req.time = ttc_clock_now() + 5e5; // 500ms from now
    printf("\nRequesting %d us long flash at time %ld\n", req.flashParams.duration, req.time);

*/
    metal_io_block_write(io, A2R_BUFFER_BASE + a2r_head*(sizeof(Request)), &req, sizeof(Request));
/*
    // HACK: make the same request on the other camera
    a2r_head = (a2r_head + 1) % QUEUE_LEN;
    if(a2r_head == tail){
      printf("Message queue is full!\n");
      break; // Just die, for now
    }
    req.device = CAMERA1;
    metal_io_block_write(io, A2R_BUFFER_BASE + a2r_head*(sizeof(Request)), &req, sizeof(Request));
    // END HACK
*/

    metal_io_write32(io, A2R_HEAD, a2r_head); // Update the a2r_head count


    // Now check for return metadata (completed requests)
    uint32_t head = metal_io_read32(io, R2A_HEAD);

    while(r2a_tail != head){
      r2a_tail = (r2a_tail + 1) % QUEUE_LEN;
      Request req;
      int ok = metal_io_block_read(io, R2A_BUFFER_BASE + r2a_tail*(sizeof(Request)), &req, sizeof(Request));
      if(ok){
        printf("received return %u on dev %d at time %lu\n", r2a_tail, req.device, req.time);
      }

      metal_io_write32(io, R2A_TAIL, r2a_tail);
    }
  }

  metal_device_close(shm_dev);
  metal_finish();
}

