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
#include "mailbox.h"


// Linked list node used to create a time-ordered queue of requests
typedef struct rq RequestQ;
struct rq {
  ZynqRequest req;
  RequestQ* next;
};

// Methods to handle the shared memory connection to the master processor
void masterqueue_init(void);
void masterqueue_close(void);
void masterqueue_check(void);
void masterqueue_push(const ZynqRequest* req);

void requestqueue_push(ZynqDevice dev, ZynqRequest req);
ZynqRequest requestqueue_peek(ZynqDevice dev);
ZynqRequest requestqueue_pop(ZynqDevice dev);

#endif /* REQUESTQUEUE_H */
