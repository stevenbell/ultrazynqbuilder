#ifndef _UBUFFER_H_
#define _UBUFFER_H_

#ifdef __KERNEL__
	#include <linux/types.h>
	#define U32_TYPE	u32
#else
	#include <stdint.h>
	#define U32_TYPE	uint32_t
#endif /* __KERNEL__ */

/* user buffer declaration */
typedef struct UBuffer {
	U32_TYPE id;		// ID flag for internal use
	U32_TYPE width;		// width of the image
	U32_TYPE height;	// height of the image
	U32_TYPE stride;	// stride of the image
	U32_TYPE depth;		// byte-depth of the image
} UBuffer;

#endif /* _UBUFFER_H_ */
