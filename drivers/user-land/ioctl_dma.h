#ifndef _XILDMA_IOCTL_H_
#define _XILDMA_IOCTL_H_

#include <linux/ioctl.h>

#define XILDMA_MAG		'Z'
#define GRAB_IMAGE		_IOWR(XILDMA_MAG, 0x40, unsigned long *)

#endif /* _XILDMA_IOCTL_H_ */
