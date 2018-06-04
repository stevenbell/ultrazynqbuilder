#include "dma_queue.h"

/* initialize the queue */
void dma_queue_init(struct dma_queue_t *queue)
{
	INIT_LIST_HEAD(&queue->list);
	sema_init(&queue->mutex, 1);
}

/* add item to the queue */
void dma_queue_enqueue(struct dma_queue_item_t *item, struct dma_queue_t *queue)
{
	INIT_LIST_HEAD(&item->list);
	while(down_interruptible(&queue->mutex) != 0);
	list_add_tail(&item->list, &queue->list);
	up(&queue->mutex);
}

/* dequeue the first element in the queue */
struct dma_queue_item_t *dma_queue_dequeue(struct dma_queue_t *queue)
{
	struct dma_queue_item_t *item = NULL;
	struct dma_queue_item_t *it;

	while(down_interruptible(&queue->mutex) != 0);
	list_for_each_entry(it, &queue->list, list) {
		item = it;
		break;
	}

	if(item) {
		list_del(&item->list);
	}
	up(&queue->mutex);

	return item;
}

/* get a queue item with given id */
struct dma_queue_item_t *dma_queue_get(u32 id, struct dma_queue_t *queue)
{
	struct dma_queue_item_t *item = NULL;
	struct dma_queue_item_t *it;

	while(down_interruptible(&queue->mutex) != 0);
	list_for_each_entry(it, &queue->list, list) {
		if(it->id == id) {
			item = it;
			break;
		}
	}
	up(&queue->mutex);

	return item;
}

/* removes the item with the given id from the queue */
struct dma_queue_item_t *dma_queue_delete(u32 id, struct dma_queue_t *queue)
{
	struct dma_queue_item_t *item = dma_queue_get(id, queue);
	if(item) {
		while(down_interruptible(&queue->mutex) != 0);
		list_del(&item->list);
		up(&queue->mutex);
	}
	return item;
}