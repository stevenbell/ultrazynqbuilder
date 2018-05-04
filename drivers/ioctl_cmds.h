#ifndef _IOCTL_CMDS_H_
#define _IOCTL_CMDS_H_

#include <linux/ioctl.h>
#include "ubuffer.h"

/* CMA BUFFER */
#define CMA_MAGIC		'Z'
#define GET_BUFFER 		_IOWR(CMA_MAGIC, 0x20, struct UBuffer *)	 // Get an unused buffer
#define GRAB_IMAGE 		_IOWR(CMA_MAGIC, 0x21, struct UBuffer *)	 // Acquire image from camera
#define FREE_IMAGE 		_IOWR(CMA_MAGIC, 0x22, struct UBuffer *)	 // Release buffer
#define PROCESS_IMAGE 	_IOWR(CMA_MAGIC, 0x23, struct UBuffer *)	 // Push to stencil path
#define PEND_PROCESSED 	_IOWR(CMA_MAGIC, 0x24, struct UBuffer *)	 // Retreive from stencil path
#define READ_TIMER		_IOWR(CMA_MAGIC, 0x25, struct UBuffer *)	 // Retreive hw timer count

/* DMA */
#define XILDMA_MAG		'Z'
#define GRAB_IMAGE		_IOWR(XILDMA_MAG, 0x40, unsigned long *)

#endif /* _IOCTL_CMDS_H */
