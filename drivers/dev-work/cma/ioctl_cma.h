#ifndef _IOCTL_CMDS_H_
#define _IOCTL_CMDS_H_

#include <linux/ioctl.h>
#include "ubuffer.h"

#define CMA_MAGIC		'Z'
#define GET_BUFFER 		_IOWR(CMA_MAGIC, 0x20, struct UBuffer *)	 // Get an unused buffer
#define FREE_BUFFER 	_IOWR(CMA_MAGIC, 0x22, struct UBuffer *)	 // Release buffer

#endif /* _IOCTL_CMDS_H */
