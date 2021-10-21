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
u32 mq_a2r_tail;
u32 mq_r2a_head;

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

  // Initialize our current pointer
  // Since we're running before the APU starts, set everything to zero
  metal_io_write32(io, A2R_HEAD, 0); // APU count
  mq_a2r_tail = 0;
  metal_io_write32(io, A2R_TAIL, mq_a2r_tail); // Our count (RPU)

  mq_r2a_head = 0;
  metal_io_write32(io, R2A_HEAD, mq_r2a_head); // Our count (RPU)
  metal_io_write32(io, R2A_TAIL, 0); // APU count

}

void masterqueue_close(void)
{
  metal_device_close(shm_dev);
  metal_finish();
}

/* layout:
 * 0x00: APU pointer
 * 0x04: RPU pointer
 * 0x08: APU->RPU buffers
 * ...
 * QUEUE_LEN + 0x08 : APU pointer
 * QUEUE_LEN + 0x0c : RPU pointer
 * QUEUE_LEN + 0x10 : RPU->APU buffers
 */
void masterqueue_check(void)
{
  // If the APU pointer (head) is ahead of the RPU pointer (tail), then we have
  // data to read. Because of wraparound, any inequality means we're behind.
  u32 head = metal_io_read32(io, A2R_HEAD);
  while(mq_a2r_tail != head){
    // Increment the RPU pointer (currently points to the last *processed*)
    mq_a2r_tail = (mq_a2r_tail + 1) % QUEUE_LEN;

    // Read the next request
    ZynqRequest req;
    int ok = metal_io_block_read(io, A2R_BUFFER_BASE + mq_a2r_tail*sizeof(ZynqRequest), &req, sizeof(ZynqRequest));
    if(ok){
      printf("received request %lu in slot %lu on dev %lu, for time %llu\n", req.reqId, mq_a2r_tail, req.device, req.time);
      requestqueue_push(req.device, req);
    }
  }
  metal_io_write32(io, A2R_TAIL, mq_a2r_tail); // TODO: only do this when we updated mq_a2r_tail
}

void masterqueue_push(const ZynqRequest* req)
{
  uint32_t tail = metal_io_read32(io, R2A_TAIL); // Where the APU currently is

  // Move to the next slot, and check that we're not going to overflow the ring buffer
  mq_r2a_head = (mq_r2a_head + 1) % QUEUE_LEN;
  if(mq_r2a_head == tail){
    printf("R2A message queue is full!\n");
    return; // Drop it on the floor
  }

  // Write the request
  metal_io_block_write(io, R2A_BUFFER_BASE + mq_r2a_head*(sizeof(ZynqRequest)), req, sizeof(ZynqRequest));

  // Update the head count
  metal_io_write32(io, R2A_HEAD, mq_r2a_head);

  printf("mq push: %lu in slot %lu at %llu\n", req->reqId, mq_r2a_head, req->time);
}

RequestQ* queues[NO_DEVICE]; // Assumes sequential enum numbering

void requestqueue_push(ZynqDevice dev, ZynqRequest req)
{
  // Allocate a new RequestQ node to hold this Request
  RequestQ* node = calloc(1, sizeof(RequestQ));
  node->req = req;

  if(dev >= NO_DEVICE){
    printf("Unknown device %ld\n", dev);
    return;
  }

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
ZynqRequest requestqueue_peek(ZynqDevice dev)
{
  // If the queue is not empty, return the first node
  if(queues[dev] != NULL){
    return queues[dev]->req;
  }
  else{ // If it is empty, return an empty struct with device set to NO_DEVICE
    ZynqRequest empty = {0};
    empty.device = NO_DEVICE;
    return empty;
  }
}

/* Removes the request at the head of the queue and returns it. */
ZynqRequest requestqueue_pop(ZynqDevice dev)
{
  ZynqRequest result;
  // If the queue is not empty, remove the first node and return it
  if(queues[dev] != NULL){
    result = queues[dev]->req; // Copy the request to return
    RequestQ* head = queues[dev]; // Save the head pointer so we can free it
    queues[dev] = head->next; // Move the head down one
    free(head); // Now delete the head
  }
  else{ // If it is empty, return an empty struct with device set to NO_DEVICE
    bzero(&result, sizeof(ZynqRequest));
    result.device = NO_DEVICE;
  }

  return result;
}

