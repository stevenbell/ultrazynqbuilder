/* requestqueue.h
 * Implementation of the request queue which is shared between the R5 and A53.
 *
 * For now this is just a polling-based implementation which uses shared memory
 * (libmetal).  If we want to avoid polling, we could add IPI, and/or switch to
 * a full-blown message-passing system with OpenAMP.
 *
 */

#ifndef REQUESTQUEUE_H
#define REQUESTQUEUE_H

#include "ttc_clock.h"
#include "xil_types.h"

#define QUEUE_LEN 10

// WARNING: the byte size of an enum is not defined by the C standard, and may
// differ across platforms.
typedef enum {
  CAMERA0,
  CAMERA1,
  CAMERA2,
  CAMERA3,
  FLASH0,
  FLASH1,
  FLASH2,
  FLASH3,
  LENS0,
  LENS1,
  LENS2,
  LENS3,
 NO_DEVICE
} ReqDevice;

typedef struct {
  u32 exposure;
} CameraParams;

typedef struct {
  u32 duration;
} FlashParams;

typedef struct {
  u32 position;
} LensParams;

typedef struct {
  ReqDevice device;
  Time time;
  union{
    CameraParams camParams;
    FlashParams flashParams;
    LensParams lensParams;
  };
} Request;

// Linked list node used to create a time-ordered queue of requests
typedef struct rq RequestQ;
struct rq {
  Request req;
  RequestQ* next;
};

// Methods to handle the shared memory connection to the master processor
void masterqueue_init(void);
void masterqueue_close(void);
void masterqueue_check(void);

void requestqueue_push(ReqDevice dev, Request req);
Request requestqueue_peek(ReqDevice dev);
Request requestqueue_pop(ReqDevice dev);

#endif /* REQUESTQUEUE_H */
