/**
 * @file cmabuf.c
 * @authors:	Gedeon Nyengele <nyengele@stanford.edu>
 * 				Steven Bell <sebell@stanford.edu>
 * @date 2 April 2018
 * version 1.1
 * @brief driver to manage CMA buffer allocations
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <linux/mm.h>

#include "kbuffer.h"
#include "common.h"
#include "ioctl_cmds.h"

#define CLASS_NAME "cmabuffer"
#define DEVICE_NAME "cmabuffer0"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gedeon Nyengele, Steven Bell");
MODULE_DESCRIPTION("driver for managing CMA buffer allocations");
MODULE_VERSION("1.1");

const int debug_level = 4;

static int majorNumber;
static struct class *cmaBufClass = NULL;
static struct device *cmaBufDevice = NULL;

static atomic_t nOpens;

/* driver parameters */
static int mode = 1;				// by default, buffer management mode is DYNAMIC
module_param(mode, int, S_IRUGO);	// can be read/not changed
MODULE_PARM_DESC(mode, " DYNAMIC = 1 (default), STATIC = 0");

static unsigned long block_size = (1 << 23);	// size in bytes of a memory chunk given to one frame
module_param(block_size, ulong, S_IRUGO);
MODULE_PARM_DESC(block_size, " Size in bytes for a chunk containing a single frame (default = 8MB)");

static unsigned long block_count = 4;
module_param(block_count, ulong, S_IRUGO);
MODULE_PARM_DESC(block_count, " Number of chunks to allocate (default = 4)");

/** @brief callbacks for defined driver operations
 * implementation defined for open, release, ioctl, and mmap
 */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static long dev_ioctl(struct file *, unsigned int, unsigned long);
static int dev_mmap(struct file *, struct vm_area_struct *);

static struct file_operations fops = {
	.open 			= dev_open,
	.release 		= dev_release,
	.unlocked_ioctl = dev_ioctl,
	.mmap 			= dev_mmap,
};

static int __init dev_init(void)
{
	enum KBufferMode sys_mode;
	int status;

	DEBUG("[cmabuf]: initializing driver...\n");

	// log device driver invocation parameters
	DEBUG("[cmabuf]: device called with following parameters:\n");
	DEBUG("\t\tmode = %s\n",((mode == 0) ? "STATIC" : "DYNAMIC"));
	DEBUG("\t\tblock_size = %lu bytes\n", block_size);
	DEBUG("\t\tblock_count = %lu block(s)\n", block_count);

	// dynamically allocate a major number for the device
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if(majorNumber < 0) {
		WARNING("[cmabuf]: failed to register a major number for device [%s]\n", DEVICE_NAME);
		return majorNumber;
	}
	DEBUG("[cmabuf]: registered device [%s] with major number [%d]\n", DEVICE_NAME, majorNumber);

	// register the device class
	cmaBufClass = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(cmaBufClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		WARNING("[cmabuf]: failed to register device class [%s]\n", CLASS_NAME);
		return PTR_ERR(cmaBufClass);
	}
	DEBUG("[cmabuf]: device class [%s] registered successfully\n", CLASS_NAME);

	// register the device driver
	cmaBufDevice = device_create(cmaBufClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(cmaBufDevice)) {
		class_destroy(cmaBufClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		WARNING("[cmabuf]: failed to register device [%s]\n", DEVICE_NAME);
		return PTR_ERR(cmaBufDevice);
	}
	DEBUG("[cmabuf]: device driver registered successfully\n");

	DEBUG("[cmabuf]: initializing buffers...\n");
	if(mode == 0) { sys_mode = STATIC; }
	else { sys_mode = DYNAMIC; }
	status = init_buffers(cmaBufDevice, sys_mode, block_size, block_count);
	if(status) { 
		DEBUG("[cmabuf] failed to initialize buffers\n");
	} else {
		DEBUG("[cmabuf] successfully initialized buffers\n");
	}

	atomic_set(&nOpens, 0);

	return status;
}

static void __exit dev_exit(void)
{
	DEBUG("[cmabuf]: exiting driver ...\n");
	cleanup_buffers(cmaBufDevice);
	device_destroy(cmaBufClass, MKDEV(majorNumber, 0)); 	// remove the device driver
	class_unregister(cmaBufClass); 							// unregister the device class
	class_destroy(cmaBufClass); 							// remove the device class
	unregister_chrdev(majorNumber, DEVICE_NAME); 			// unregister the major number
	DEBUG("[cmabuf]: device driver exited successfully\n");
}

// device open callback
// TODO: allow multiple device openings and closings
static int dev_open(struct inode *inodep, struct file *filep)
{
	DEBUG("[cmabuf]: dev_open entry\n");
	atomic_inc(&nOpens);
	DEBUG("[cmabuf]: dev_open exit\n");
	return 0;
}

// device close callback
// TODO: allow multiple device openings and closings
static int dev_release(struct inode *inodep, struct file *filep)
{
	DEBUG("[cmabuf]: dev_release entry\n");
	atomic_dec(&nOpens);
	DEBUG("[cmabuf]: dev_release exit\n");
	return 0;
}

// device ioctl callback
static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct UBuffer ubuf;
	struct KBuffer *kbufPtr;
	
	DEBUG("[dmabuf]: dev_ioctl entry\n");
	DEBUG("[cmabuf]: ioctl cmd 0x%08x | %lu (0x%lx)\n", cmd, arg, arg);

	switch(cmd) {

		case GET_BUFFER:
			DEBUG("[cmabuf]: ioctl [GET_BUFFER]\n");

			// get buffer parameters from user's buffer
			if(access_ok(VERIFY_READ, (void *)arg, sizeof(struct UBuffer)) &&
					(copy_from_user(&ubuf, (void *)arg, sizeof(struct UBuffer)) == 0)) {

				kbufPtr = acquire_buffer(ubuf.width, ubuf.height, ubuf.depth, ubuf.stride);
				if(kbufPtr == NULL) { return -ENOBUFS; }
			} else { return(-EIO); }

			// copy the retrieved buffer back to the user
			if(access_ok(VERIFY_WRITE, (void *)arg, sizeof(struct UBuffer)) &&
					(copy_to_user((void *)arg, &kbufPtr->xdomain, sizeof(struct UBuffer)) == 0)) {

				DEBUG("[cmabuf]: ioctl [GET_BUFFER] successful\n");
			} else { return(-EIO); }
		break; // GET_BUFFER

		case FREE_BUFFER:
			DEBUG("[cmabuf]: ioctl [FREE_BUFFER]\n");

			// copy user buffer and request to free buffer
			if(access_ok(VERIFY_READ, (void *)arg, sizeof(struct UBuffer)) &&
					(copy_from_user(&ubuf, (void *)arg, sizeof(struct UBuffer))) == 0) {

				// release buffer
				DEBUG("[cmabuf]: releasing buffer [id = %u]\n", ubuf.id);
				release_buffer(ubuf.id);
			} else { return(-EIO); }
		break; // FREE_BUFFER

		case SLICE_BUFFER:
			DEBUG("[cmabuf]: ioctl [SLICE_BUFFER]\n");

			// copy user buffer
			if(access_ok(VERIFY_READ, (void *)arg, sizeof(struct UBuffer)) &&
					(copy_from_user(&ubuf, (void *)arg, sizeof(struct UBuffer))) == 0) {

				kbufPtr = slice_buffer(ubuf.id, ubuf.offset, ubuf.width, ubuf.height);
				if(kbufPtr == NULL) {
					DEBUG("[cmabuf] failed to slice buffer [id = %u]\n", ubuf.id);
					return -EINVAL;
				}
				if(copy_to_user((void *)arg, &kbufPtr->xdomain, sizeof(struct UBuffer)) != 0) {
					DEBUG("[cmabuf] failed to copy slice to user\n");
					return -EIO;
				}
			} else { return(-EIO); }
		break; // SLICE_BUFFER

		default:
			//unknown command
			return(-EINVAL);
		break; // default
	}
	
	DEBUG("[cmabuf]: dev_ioctl exit\n");
	return 0;
}

/*
 * @brief This implementation puts the buffer ID in the offset field
 * and lets the user-space helper function to apply the offset from
 * user space
 */
static int dev_mmap(struct file *filep, struct vm_area_struct *vma)
{
	u64 buffer_id, offset, req_size;
	unsigned long start_pfn;
	struct KBuffer *kbuf;

	DEBUG("[cmabuf]: dev_mmap entry\n");

	// get buffer ID
	// buffer id is stored in lower 20 bits of offset field
	// Note: the last 12 bits of the user-supplied offset field
	// are dropped by the kernel by the time that value gets here.
	// that is because the kernel re-computes the offset in "pages"
	buffer_id = vma->vm_pgoff & ((1 << 20) - 1);
	// here offset is always 0 because user-land helper will take of
	// actual offset
	offset = 0;
	req_size = vma->vm_end - vma->vm_start;
	DEBUG("[cmabuf] vma->vm_start = 0x%lx\n", vma->vm_start);
	DEBUG("[cmabuf] vma->vm_end = 0x%lx\n", vma->vm_end);

	// get corresponding buffer
	kbuf = get_buffer_by_id(buffer_id);
	if(kbuf == NULL) {
		WARNING("[cmabuf]: dev_mmap: couldn't find a corresponding buffer to mmap\n");
		return(-EINVAL);
	}

	// check for overflow on requested size and offset
	DEBUG("[cmabuf]: buffer ID = %llu\n", buffer_id);
	DEBUG("[cmabuf]: requested region size to mmap = %llu\n", req_size);
	DEBUG("[cmabuf]: pre-allocated buffer size = %llu\n", kbuf->size);
	DEBUG("[cmabuf]: req_size + (offset << PAGE_SHIFT) = %llu\n", (req_size + (offset << PAGE_SHIFT)));
	DEBUG("[cmabuf]: offset = %llu pages\n", offset);
	DEBUG("[cmabuf]: vma->vm_pgoff = 0x%08lx\n", vma->vm_pgoff);
	if(kbuf->size < req_size) {
		WARNING("[cmabuf]: dev_mmap : mapping cannot overflow buffer boundaries\n");
		return(-EINVAL);
	}

	// get starting pfn
	start_pfn = kbuf->phys_addr >> PAGE_SHIFT;
	if(!pfn_valid(start_pfn)) {
		WARNING("[cmabuf]: dev_mmap : invalid pfn\n");
		return(-EINVAL);
	}	

	// everything is good, so now do the mapping
	// disable caching
	// vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	// set VM_IO flag
	// vma->vm_flags |= VM_IO;
	// prevent the VMA from swapping out
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	if(remap_pfn_range(vma, vma->vm_start, start_pfn, req_size, vma->vm_page_prot)) {
		WARNING("[cmabuf] dev_mmap : failed to map buffer, try again\n");
		return(-EAGAIN);
	}
	DEBUG("[cmabuf]: dev_mmap exit\n");
	return 0;
}

/* alternate mmap implementation
 * @brief This implementation always maps an entire buffer
 * to user space. 
 * changes to user parameters semantics:
 * 		size : ignored
 * 		offset: offset from beginning of buffer
 */
/*
static int dev_mmap_alternate(struct file *filep, struct vm_area_struct *vma)
{
	u64 buffer_id;
	unsigned long start_pfn;
	struct KBuffer *kbuf;

	DEBUG("[cmabuf]: dev_mmap\n");

	// get buffer id (hidden within page offset field)
	buffer_id = vma->vm_pgoff;

	// find corresponding buffer
	kbuf = get_buffer_by_id(buffer_id);
	if(kbuf == NULL) {
		WARNING("[cmabuf]: dev_mmap: couldn't find a corresponding buffer to mmap\n");
		return(-EINVAL);
	}

	// calculate the starting pfn from buffer's
	start_pfn = kbuf->phys_addr >> PAGE_SHIFT;
	if(!pfn_valid(start_pfn)) {
		WARNING("[cmabuf]: dev_mmap : invalid pfn\n");
		return(-EINVAL);
	}

	// map the entire buffer
	// it is up to the user-facing wrapper API to apply proper offset
	// (in this implementation, the user's size parameter never matters)
	// since the entire block gets mapped
	if(remap_pfn_range(vma, vma->vm_start, start_pfn, kbuf->size, vma->vm_page_prot)) {
		WARNING("[cmabuf] dev_mmap : failed to map buffer, try again\n");
		return(-EAGAIN);
	}

	return 0;
}
*/

module_init(dev_init);
module_exit(dev_exit);
