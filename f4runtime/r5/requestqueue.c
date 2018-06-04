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
u32 mq_slave_head;

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



void masterqueue_init(void)
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

  // Initialize to current master pointer
  mq_slave_head = metal_io_read32(io, 0);

}

void masterqueue_close(void)
{
  metal_device_close(shm_dev);
  metal_finish();
}

/* layout:
 * 0x00: master pointer
 * 0x04: slave pointer
 * 0x08: buffers
 */
void masterqueue_check(void)
{
  // If the master pointer is ahead of the slave pointer, then we have data to read
  // Because of wraparound, any inequality means we're behind.
  u32 master = metal_io_read32(io, 0);
  while(mq_slave_head != master){
    // Increment the slave pointer (currently points to the last *processed*)
    mq_slave_head = (mq_slave_head + 1) % QUEUE_LEN;

    // Read the next request
    Request req;
    int ok = metal_io_block_read(io, 0x08 + mq_slave_head*sizeof(Request), &req, sizeof(Request));
    if(ok){
      printf("received request %lu, passing it to the queue\n", mq_slave_head);
      requestqueue_push(req.device, req);
    }
  }
  metal_io_write32(io, 4, mq_slave_head);
}

RequestQ* queues[NO_DEVICE]; // Assumes sequential enum numbering

void requestqueue_push(ReqDevice dev, Request req)
{
  // Allocate a new RequestQ node to hold this Request
  RequestQ* node = calloc(1, sizeof(RequestQ));
  node->req = req;

  // Do a sequential search through the queue
  RequestQ** nodeptr = &(queues[dev]);
  while(*nodeptr != NULL && req.time > (*nodeptr)->req.time){
    nodeptr = &((*nodeptr)->next);
  }

  if(*nodeptr == NULL){ // Hit the end of the list
    node->next = NULL;
    *nodeptr = node;
  }
  else{ // Somewhere in the middle; insert ourselves
    node->next = (*nodeptr)->next;
    (*nodeptr)->next = node;
  }
}


/* Returns the request at the head of the queue without removing it. */
Request requestqueue_peek(ReqDevice dev)
{
  // If the queue is not empty, return the first node
  if(queues[dev] != NULL){
    return queues[dev]->req;
  }
  else{ // If it is empty, return an empty struct with device set to NO_DEVICE
    Request empty = {0};
    empty.device = NO_DEVICE;
    return empty;
  }
}

/* Removes the request at the head of the queue and returns it. */
Request requestqueue_pop(ReqDevice dev)
{
  Request result;
  // If the queue is not empty, remove the first node and return it
  if(queues[dev] != NULL){
    result = queues[dev]->req; // Copy the request to return
    RequestQ* head = queues[dev]; // Save the head pointer so we can free it
    queues[dev] = head->next; // Move the head down one
    free(head); // Now delete the head
  }
  else{ // If it is empty, return an empty struct with device set to NO_DEVICE
    bzero(&result, sizeof(Request));
    result.device = NO_DEVICE;
  }

  return result;
}

