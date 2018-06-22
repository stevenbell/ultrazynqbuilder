#ifndef _KBUFFER_H_
#define _KBUFFER_H_

#ifndef __KERNEL__
#define __KERNEL__
#endif /* __KERNEL__ */

#define __PIXEL_VAL		// annotation to tell that the specified value is in pixels
#define __BYTE_VAL		// annotation to tell that the specified value is in bytes

/* Linux includes */
#include <linux/types.h>

/* local includes */
#include "ubuffer.h"

/* kernel buffer declaration */
typedef struct KBuffer {
	struct UBuffer xdomain; // cross-domain part of the buffer (user buffer part)
	u64 size; // actual size of memory chunk allocated
	u64 phys_addr; // physical address of memory chunk allocated
	void *kern_addr; // kernel virtual address of memory chunk allocated
	u32	parent_id;	// used for slices. This is the ID of parent (root) buffer
	struct mMap *cvals;
	u32 mmap_offset;
} KBuffer;

/* kernel buffer allocation mode */
enum KBufferMode {
	STATIC = 0,		// pre-allocate a fixed-size pool of buffers
	DYNAMIC		// on-demand buffer management
};

/* forward declarations */
struct device;

/* initializes the buffer memory allocation system
 * @param dev The device managed by the driver
 * @param alloc_mode The allocation mode to use
 * @param static_block_size The size in bytes of a single memory chunk
 * 			This is used in STATIC mode only
 * @param num_of_blocks The maximum number of chunks to pre-allocate for STATIC allocation mode
 * @return 0 for success, negative error otherwise (-ENOMEM)
 */
int init_buffers(struct device *dev, enum KBufferMode alloc_mode, u64 static_block_size, u64 num_of_blocks);

/* returns all buffers to OS
 * @param dev The device managed by the driver
 * @return N/A
 */
void cleanup_buffers(struct device *dev);

/* gets a buffer of the requested size
 * @param width The width of the image frame
 * @param height The height of the image frame
 * @param depth The byte-depth of the image frame
 * @param stride The stride between successive rows of the image frame
 * @return The buffer of requested size, or NULL for failure
 */
struct KBuffer *acquire_buffer(u32 __PIXEL_VAL width, u32 __PIXEL_VAL height,
					u32 __BYTE_VAL depth, u32 __PIXEL_VAL stride);

/* releases the buffer for reallocation
 * @param id The id of the buffer to release
 * @return N/A
 */
void release_buffer(u32 id);

/* slice a buffer
 * @param id The id of the buffer to get a slice from
 * @param offset The offset in pixels to the start of the slice
 *        !!! when computing the offset, use "stride" instead of "width"
 * @param width The width of the slice
 * @param height The height of the slice
 * @return a slice buffer or NULL if failed to get slice
 */
struct KBuffer *slice_buffer(u32 id, u32 __PIXEL_VAL offset,
					u32 __PIXEL_VAL width, u32 __PIXEL_VAL height);

/* gets a buffer from the list of allocated buffer using the id field
 * @param id The id of the buffer to retrieve
 * @return Pointer to buffer matching the id, NULL otherwise
 */
struct KBuffer *get_buffer_by_id(u32 id);

/* clear buffer
 * @param buf The buffer to clear
 * @return N/A
 */
void zero_buffer(struct KBuffer *buf);

/* gets a new buffer object which points to the same memory, but which has
 * a smaller shape
 * @param orig The original buffer
 * @param x The x-axis offset
 * @param y The y-axis offset
 * @param width The new width
 * @param height The new height
 * @return reshaped buffer, or NULL if error
 */
// TO DO: enable slice_buffer when proper implementation is defined
//struct KBuffer *slice_buffer(struct KBuffer *orig, u32 x, u32 y, u32 width, u32 height);

int is_empty_list(void);

#endif /* _KBUFFER_H_ */
