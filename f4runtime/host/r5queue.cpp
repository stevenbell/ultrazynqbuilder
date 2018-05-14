/* r5queue.cpp
 * Code which enqueues requests for execution by the R5 real-time process
 */

#include <metal/sys.h>
#include <metal/device.h>
#include <metal/io.h>
#include <metal/alloc.h>

#define BUS_NAME        "platform"
#define SHM_DEV_NAME    "3e800000.shm"

// WARNING: the size of an enum is not defined by the C standard, and may
// differ across platforms.
typedef uint32_t Time;

typedef enum {
  CAMERA0,
  CAMERA1,
  NONE
} RequestType;

typedef struct {
  uint32_t exposure;
} CameraParams;

typedef struct {
  RequestType type;
  Time time;
  CameraParams params;
} Request;


const int QUEUE_LEN = 10;

int main(int argc, char* argv[])
{
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
  uint32_t master = 0; // Our count
  Request req;

  char c = 0;
  while(c != 'q'){
    uint32_t slave = metal_io_read32(io, 4); // Current slave pointer

    // Move to the next slot, and check that we're not going to overflow the
    // ring buffer
    master = (master + 1) % QUEUE_LEN;
    if(master == slave){
      printf("Message queue is full!\n");
      break; // Just die, for now
    }

    // Send a message to the R5
    printf("\nSending value 0x%2x ('%c') to R5\n", c, c);
    req.time = c;
    metal_io_block_write(io, 0x08 + master*(sizeof(Request)), &req, sizeof(Request));

    metal_io_write32(io, 0, master); // Update the master count
    do {
      c = getchar();
    } while (c == '\n'); // Eat newlines
  }

  metal_device_close(shm_dev);
  metal_finish();
}

