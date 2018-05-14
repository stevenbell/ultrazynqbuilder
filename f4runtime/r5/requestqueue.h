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

// WARNING: the size of an enum is not defined by the C standard, and may
// differ across platforms.
typedef enum {
  CAMERA0,
  CAMERA1,
  NONE
} RequestType;

typedef struct {
  u32 exposure;
} CameraParams;

typedef struct {
  RequestType type;
  Time time;
  CameraParams params;
} Request;

void requestqueue_init(void);
void requestqueue_close(void);
void requestqueue_check(void);

#endif /* REQUESTQUEUE_H */
