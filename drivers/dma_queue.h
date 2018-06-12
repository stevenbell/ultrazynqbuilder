#ifndef __MY_DMA_QUEUE_H__
#define __MY_DMA_QUEUE_H__

#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/types.h>

struct dma_queue_t {
	struct list_head list;
	struct semaphore mutex;
};

struct dma_queue_item_t {
	u32 id;
	struct list_head list;
};

/* initialize the queue */
void dma_queue_init(struct dma_queue_t *queue);

/* add item to the queue */
void dma_queue_enqueue(struct dma_queue_item_t *item, struct dma_queue_t *queue);

/* dequeue the first element in the queue */
struct dma_queue_item_t *dma_queue_dequeue(struct dma_queue_t *queue);

/* return the first element in the queue without removing it*/
struct dma_queue_item_t *dma_queue_peek(struct dma_queue_t *queue);

/* get a queue item with given id */
struct dma_queue_item_t *dma_queue_get(u32 id, struct dma_queue_t *queue);

/* removes the item wtih given id from the queue */
struct dma_queue_item_t *dma_queue_delete(u32 id, struct dma_queue_t *queue);

#endif /* __MY_DMA_QUEUE_H__ */
