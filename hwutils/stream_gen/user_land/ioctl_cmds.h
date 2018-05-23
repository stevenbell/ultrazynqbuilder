#ifndef _STREAM_GEN_IOCTL_H_
#define _STREAM_GEN_IOCTL_H_

#include <linux/ioctl.h>

#define STREAM_MAGIC	'Z'
#define RD_CONFIG	_IOWR(STREAM_MAGIC, 0x30, unsigned int *)
#define RD_SIZE		_IOWR(STREAM_MAGIC, 0x31, unsigned int *)
#define RD_STATUS	_IOWR(STREAM_MAGIC, 0x32, unsigned int *)
#define WR_CONFIG	_IOWR(STREAM_MAGIC, 0x33, unsigned int *)
#define WR_CTRL_SET	_IOWR(STREAM_MAGIC, 0x34, unsigned int *)
#define WR_CTRL_CLR	_IOWR(STREAM_MAGIC, 0x35, unsigned int *)
#define WR_SIZE		_IOWR(STREAM_MAGIC, 0x36, unsigned int *)

#endif /* _STREAM_GEN_IOCTL_H_ */
