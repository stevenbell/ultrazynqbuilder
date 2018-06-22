/* Linux includes */
#include <linux/types.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/string.h>
#include <linux/of_device.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <asm/page.h>
#include <linux/slab.h>

/* local includes */
#include "kbuffer.h"
#include "common.h"

// debug level for debug outputs
// defined in including driver
extern const int debug_level;

// scheme for buffer memory allocations
static enum KBufferMode mode = DYNAMIC;

// tells whether the allocation system is initialized
static unsigned initialized = 0;

// ID counter
static atomic_t ID_counter;

// this struct adds fields to KBuffer
// that are used only here
struct KBufferRecord {
	struct KBuffer kbuf;
	u32 free; // keeps track of whether the related KBuffer is free or not
	struct list_head list; // allows lists of KBufferRecords to be created
};

// list of KBufferRecords allocated
static LIST_HEAD(KBufferRecords);
static struct mutex rec_lock;
#define MUTEX_LOCK(lock)	while(mutex_lock_interruptible(&lock))
#define MUTEX_UNLOCK(lock)	mutex_unlock(&lock)

// private helpers
static u64 page_aligned_size(u64 size);
static struct KBufferRecord *get_buffer_rec(u64 id);

// device handle
static struct device *cma_dev = NULL;

/* === public interface === */

int init_buffers(struct device *dev, enum KBufferMode alloc_mode, u64 static_block_size, u64 num_of_blocks)
{
	u64 n_blocks_allocated;
	struct KBufferRecord *record;
	void *kern_addr;
	u64 phys_addr;
	u64 i;
	u64 actual_size;
	u64 dma_mask;

	n_blocks_allocated = 0;
	atomic_set(&ID_counter, 0);

	// re-initialize if buffer management system is already initialized
	// this might be needed if any of the following parameters are to be changed on the fly
	// 		- mode of allocation --STATIC or DYNAMIC-- (alloc_mode)
	// 		- size of a block (static_block_size)
	// 		- number of blocks to pre-allocate (num_of_blocks)
	if(initialized) {
		DEBUG("buffer system re-initialization\n");
		cleanup_buffers(dev);
	} else {
		mutex_init(&rec_lock);
	}

	// configure the DMA masks
	// anything within actual RAM (up to 4GB) is fair game
	// returns 0 on success
	dma_mask = dma_get_required_mask(dev);
	if(dma_set_mask(dev, dma_mask)) {
		WARNING("Failed to configure DMA mask %llx\n", dma_mask);
	}
	if(dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		WARNING("Failed to configure DMA coherent mask %llx\n", (u64) DMA_BIT_MASK(32));
	}
	of_dma_configure(dev, NULL);

	DEBUG("Current mask is 0x%llx\n", (u64) dev->dma_mask);
	DEBUG("Current coherent mask is 0x%llx\n", (u64) dev->coherent_dma_mask);

	// set allocation mode
	mode = alloc_mode;

	// pre-allocate buffers for STATIC mode
	if(mode == STATIC) {

		actual_size = page_aligned_size(static_block_size);		

		DEBUG("pre-allocating [%llu] blocks\n", num_of_blocks);
		for(i = 0; i < num_of_blocks; i++) {

			// get a chunk of memory space reserved
			DEBUG("Allocationg DMA-able memory for a chunk of size %llu bytes\n", actual_size);
			kern_addr = dma_alloc_coherent(dev, (size_t) actual_size, (dma_addr_t *)&phys_addr, GFP_KERNEL);

			if(kern_addr == NULL) {

				WARNING("Failed to allocate DMA-able memory chunk of size %llu bytes\n", actual_size);
				break;

			} else {

				// allocate memory for a KBufferRecord
				DEBUG("Allocating memory for KBufferRecord to associate with previously allocated memory chunk\n");
				record = (struct KBufferRecord *) kmalloc(sizeof(struct KBufferRecord), GFP_KERNEL);

				// if allocation fails, free memory chunk
				if(record == NULL) {

					WARNING("Failed to allocate memory for KBufferRecord\n");
					dma_free_coherent(dev, (size_t) actual_size, kern_addr, phys_addr);
					DEBUG("Freed associated DMA-able memory chunk of size %llu\n", actual_size);
					break;
				} else {

					DEBUG("Successfully allocated memory for KBufferRecord\n");
					memset(record, 0, sizeof(struct KBufferRecord));

					INIT_LIST_HEAD(&record->list);
					record->free = 1;

					record->kbuf.size = actual_size;
					record->kbuf.phys_addr = phys_addr;
					record->kbuf.kern_addr = kern_addr;
					record->kbuf.parent_id = 0;

					record->kbuf.xdomain.id = (u32) atomic_inc_return(&ID_counter);
					
					MUTEX_LOCK(rec_lock);
					list_add(&record->list, &KBufferRecords);
					MUTEX_UNLOCK(rec_lock);

					DEBUG("KBufferRecord provisioned and added to list\n");

					n_blocks_allocated += 1;
				}
			}
		}
	}

	// if we're in STATIC mode and no memory chunks
	// were allocated, then return an error
	if(mode == STATIC && n_blocks_allocated == 0) {
		WARNING("buffer initialization failed\n");
		return -ENOMEM;
	}
	cma_dev = dev;
	initialized = 1;
	DEBUG("buffer initialization successfully completed\n");
	return 0;

}

void cleanup_buffers(struct device *dev)
{
	// used for root buffers
	struct KBufferRecord *record;

	// used for slices
	struct list_head *cur_pos, *next_pos;
	struct KBufferRecord *slice;

	// do nothing if buffer allocation system
	// is not yet initialized
	if(!initialized) {
		WARNING("cannot cleanup buffers from uninitialized system\n");
		return;
	}

	MUTEX_LOCK(rec_lock);
	record = list_first_entry_or_null(&KBufferRecords, struct KBufferRecord, list);
	MUTEX_UNLOCK(rec_lock);

	while (record != NULL) {
 
		// remove slices, if any
		MUTEX_LOCK(rec_lock);
		list_for_each_safe(cur_pos, next_pos, &KBufferRecords) {
			slice = list_entry(cur_pos, struct KBufferRecord, list);
			if(slice->kbuf.parent_id == record->kbuf.xdomain.id) {
				// remove slice from record list
				list_del(cur_pos);
				
				// free record
				kfree(slice);
			}
		}
		
		// remove record from list
		list_del(&record->list);
		MUTEX_UNLOCK(rec_lock);
		DEBUG("removed KBufferRecord from list\n");

		// free memory chunk associated with this KBufferRecord
		DEBUG("freeing memory chunk associated with buffer record\n");
		dma_free_coherent(dev, (size_t) record->kbuf.size, record->kbuf.kern_addr, record->kbuf.phys_addr);

		// free memory allocated for KBufferRecord 
		DEBUG("freeing memory allocated to KBufferRecord object\n");
		kfree(record);

		// get next record
		MUTEX_LOCK(rec_lock);
		record = list_first_entry_or_null(&KBufferRecords, struct KBufferRecord, list);
		MUTEX_UNLOCK(rec_lock);
	}

	DEBUG("buffer cleanup complete\n");
}

struct KBuffer *acquire_buffer(u32 width, u32 height, u32 depth, u32 stride)
{
	u32 found = 0;
	struct KBufferRecord *record;
	u64 block_size;
	void *kern_addr;
	u64 phys_addr;

	record = NULL;

	// do nothing if buffer allocation system is uninitialized
	if(!initialized) {
		WARNING("cannot acquire buffer from uninitialized system\n");
		return NULL;
	}

	// traverse the list of KBufferRecords to find 
	// a chunk of memory that fits and is available
	MUTEX_LOCK(rec_lock);
	list_for_each_entry(record, &KBufferRecords, list) {
		if(record && record->free && record->kbuf.size >= stride * height * depth &&
				record->kbuf.parent_id == 0) {
			record->free = 0;
			record->kbuf.xdomain.width = width;
			record->kbuf.xdomain.height = height;
			record->kbuf.xdomain.depth = depth;
			record->kbuf.xdomain.stride = stride;

			found = 1;
			break;
		}
	}
	MUTEX_UNLOCK(rec_lock);

	// if we're in STATIC mode and if no chunk that fits is available,
	// then return NULL
	if(!found && mode == STATIC) { 
		WARNING("failed to acquire buffer in STATIC mode\n");
		return NULL;
	}
	else if(found) {
		DEBUG("buffer successfully acquired\n");
		return &record->kbuf;
	}

	/* ============ DYNAMIC MODE ================= */
	/* in DYNAMIC mode, try to allocate a chunk on the fly */

	// find actual page-aligned size of the requested memory chunk
	block_size = page_aligned_size(stride * height * depth);

	// allocate memory chunk
	DEBUG("allocating memory chunk of side %llu bytes in DYNAMIC mode\n", block_size);
	kern_addr = dma_alloc_coherent(cma_dev, (size_t) block_size, (dma_addr_t *) &phys_addr, GFP_KERNEL);
	if(kern_addr == NULL) {
		WARNING("failed to acquire buffer in DYNAMIC mode\n");
		return NULL;
	}

	// allocate KBufferRecord
	DEBUG("allocating space for KBufferRecord object in DYNAMIC mode\n");
	record = kmalloc(sizeof(struct KBufferRecord), GFP_KERNEL);
	if(record == NULL) {
		DEBUG("failed to acquire space for KBufferRecord in DYNAMIC mode\n");
		DEBUG("failed to acquire buffer of size %llu in DYNAMIC mode\n", block_size);
		dma_free_coherent(cma_dev, (size_t) block_size, kern_addr, phys_addr);
		return NULL;
	}

	// provision KBufferRecord
	memset(record, 0, sizeof(struct KBufferRecord));

	INIT_LIST_HEAD(&record->list);
	record->free = 0;

	record->kbuf.size = block_size;
	record->kbuf.phys_addr = phys_addr;
	record->kbuf.kern_addr = kern_addr;
	record->kbuf.parent_id = 0;

	record->kbuf.xdomain.id = (u32) atomic_inc_return(&ID_counter);
	record->kbuf.xdomain.width = width;
	record->kbuf.xdomain.height = height;
	record->kbuf.xdomain.depth = depth;
	record->kbuf.xdomain.stride = stride;

	MUTEX_LOCK(rec_lock);
	list_add(&record->list, &KBufferRecords);
	MUTEX_UNLOCK(rec_lock);

	DEBUG("successfully aquired buffer in DYNAMIC mode\n");
	return &record->kbuf;
}

void release_buffer(u32 id)
{
	// used for root buffer
	struct KBufferRecord *target;

	// used for child (slice) buffers
	struct list_head *cur_pos, *next_pos;
	struct KBufferRecord *record;

	target = get_buffer_rec(id);

	if(target == NULL) return;

	// remove slices, if any
	MUTEX_LOCK(rec_lock);
	list_for_each_safe(cur_pos, next_pos, &KBufferRecords) {
		record = list_entry(cur_pos, struct KBufferRecord, list);
		if(record->kbuf.parent_id == id) {
			// remove slice from record list
			list_del(cur_pos);

			// free record
			kfree(record);
		}
	}
	MUTEX_UNLOCK(rec_lock);

	// in STATIC mode, just make the chunk free again
	if (mode == STATIC) {
		target->free = 1;
		target->kbuf.xdomain.width = 0;
		target->kbuf.xdomain.height = 0;
		target->kbuf.xdomain.depth = 0;
		target->kbuf.xdomain.stride = 0;
	} else if(mode == DYNAMIC) {
		// remove from list
		list_del(&target->list);

		// free memory chunk
		dma_free_coherent(cma_dev, (size_t) target->kbuf.size,
			target->kbuf.kern_addr, target->kbuf.phys_addr);

		// free KBufferRecord
		kfree(target);
	}
}

struct KBuffer *get_buffer_by_id(u32 id)
{
	struct KBufferRecord *record, *target;
	record = NULL;
	target = NULL;

	MUTEX_LOCK(rec_lock);
	list_for_each_entry(record, &KBufferRecords, list) {
		if(record && record->kbuf.xdomain.id == id) {
			target = record;
			break;
		}
	}
	MUTEX_UNLOCK(rec_lock);

	return (target ? &target->kbuf : NULL);
}

void zero_buffer(struct KBuffer *buf)
{
	if(buf == NULL) return;
	memset(buf, 0, sizeof(struct KBuffer));
}

static u64 page_aligned_size(u64 size)
{
	u64 offset;
	offset = ((size & ((1 << PAGE_SHIFT) - 1)) > 0) ? 1 : 0;
	return (((size >> PAGE_SHIFT) + offset) << PAGE_SHIFT);
}

static struct KBufferRecord *get_buffer_rec(u64 id)
{
	struct KBufferRecord *record, *target;
	record = NULL;
	target = NULL;

	MUTEX_LOCK(rec_lock);
	list_for_each_entry(record, &KBufferRecords, list) {
		if(record && record->kbuf.xdomain.id == id) {
			target = record;
			break;
		}
	}
	MUTEX_UNLOCK(rec_lock);

	return target;
}

/* for test purposes */
int is_empty_list(void) {
	int ret;
	MUTEX_LOCK(rec_lock);
	ret = list_empty(&KBufferRecords);
	MUTEX_UNLOCK(rec_lock);
	return ret;
}

struct KBuffer *slice_buffer(u32 id, u32 offset, u32 width, u32 height)
{
	struct KBuffer *root;
	struct KBufferRecord *slice;

	root = get_buffer_by_id(id);
	if(root == NULL) return NULL;

	// verify that the slice will fit in root buffer
	if(((offset % root->xdomain.stride) + width) > root->xdomain.width) {
		// horizontal fit test failed
		return NULL;
	} else if(((offset / root->xdomain.stride) + height) > root->xdomain.height) {
		// vertical fit test failed
		return NULL;
	}

	// create a record for the slice
	slice = kmalloc(sizeof(struct KBufferRecord), GFP_KERNEL);
	if(slice == NULL) {
		return NULL;
	}
	slice->kbuf.xdomain.id = (u32) atomic_inc_return(&ID_counter);
	slice->kbuf.xdomain.offset = offset;
	slice->kbuf.xdomain.width = width;
	slice->kbuf.xdomain.height = height;
	slice->kbuf.xdomain.depth = root->xdomain.depth;
	slice->kbuf.xdomain.stride = root->xdomain.stride;
	slice->kbuf.size = ((height - 1)*root->xdomain.stride + width) * root->xdomain.depth;
	slice->kbuf.phys_addr = root->phys_addr + offset * root->xdomain.depth;
	slice->kbuf.parent_id = id;

	// add the slice record to record list
	MUTEX_LOCK(rec_lock);
	list_add(&slice->list, &KBufferRecords);
	MUTEX_UNLOCK(rec_lock);

	return &slice->kbuf;
}


//EXPORT_SYMBOL(init_buffers);
EXPORT_SYMBOL(acquire_buffer);
EXPORT_SYMBOL(release_buffer);
EXPORT_SYMBOL(get_buffer_by_id);
//EXPORT_SYMBOL(cleanup_buffers);
//EXPORT_SYMBOL(is_empty_list);
