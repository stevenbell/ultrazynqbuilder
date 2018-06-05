#ifndef _IOCTL_DMA_H_
#define _IOCTL_DMA_H_

#include <linux/ioctl.h>

/* DMA */
#define XILDMA_MAG		'Z'
#define ENROLL_BUFFER	_IOWR(XILDMA_MAG, 0x40, void *)
#define WAIT_COMPLETE	_IOWR(XILDMA_MAG, 0x41, void *)
#define STATUS_CHECK	_IOWR(XILDMA_MAG, 0x42, void *)

/* DMA status return Codes */
#define DMA_COMPLETED	2
#define DMA_IN_PROGRESS	1

#endif /* _IOCTL_DMA_H_*/
