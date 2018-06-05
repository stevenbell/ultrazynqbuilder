#ifndef _IOCTL_CMDS_H_
#define _IOCTL_CMDS_H_

#include <linux/ioctl.h>

#define MAGIC			'Z'

/* cma driver */
#define GET_BUFFER 		_IOWR(MAGIC, 0x20, void *)	 // Get an unused buffer
#define FREE_BUFFER 	_IOWR(MAGIC, 0x21, void *)	 // Release buffer

/* dma driver */
#define ENROLL_BUFFER	_IOWR(MAGIC, 0x40, void *)
#define WAIT_COMPLETE	_IOWR(MAGIC, 0x41, void *)
#define STATUS_CHECK	_IOWR(MAGIC, 0x42, void *)
#define DMA_COMPLETED	2
#define DMA_IN_PROGRESS	1

/* hwacc */
#define GRAB_IMAGE 		_IOWR(MAGIC, 0x22, void *)	// Acquire image from camera
#define FREE_IMAGE		_IOWR(MAGIC, 0x23, void *)	// Release buffer
#define PROCESS_IMAGE	_IOWR(MAGIC, 0x24, void *)	// Push to stencil path
#define PEND_PROCESSED	_IOWR(MAGIC, 0x25, void *)	// Retreive from stencil path
#define READ_TIMER		_IOWR(MAGIC, 0x26, void *)	// Retreive hw timer count

/* stream generator */
#define RD_CONFIG		_IOWR(MAGIC, 0x30, void *)
#define RD_SIZE			_IOWR(MAGIC, 0x31, void *)
#define RD_STATUS		_IOWR(MAGIC, 0x32, void *)
#define WR_CONFIG		_IOWR(MAGIC, 0x33, void *)
#define WR_CTRL_SET		_IOWR(MAGIC, 0x34, void *)
#define WR_CTRL_CLR		_IOWR(MAGIC, 0x35, void *)
#define WR_SIZE			_IOWR(MAGIC, 0x36, void *)

#endif /* _IOCTL_CMDS_H_ */
