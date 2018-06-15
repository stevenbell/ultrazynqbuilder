/* C includes */
#include <stdint.h>
#include <stdlib.h>

/* local includes */
#include "cma.h"

void *cma_map(struct UBuffer *buf, void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	uint32_t cma_buf_id;
	void *mapped_addr;

	if(buf == NULL) {
		return MAP_FAILED;
	}
	
	cma_buf_id = buf->id << 12;
	mapped_addr = mmap(addr, length, prot, flags, fd, (off_t) cma_buf_id);
	if(mapped_addr == MAP_FAILED) return MAP_FAILED;
	if((buf->stride * buf->height * buf->depth) < (length + offset)) {
		cma_unmap(mapped_addr, length);
		return MAP_FAILED;
	}
	return (void *) ((char *)mapped_addr + offset);
}

int cma_unmap(void *addr, size_t length)
{
	return munmap(addr, length);
}


int cma_get_buffer(int fd, struct UBuffer *buf)
{
	return ioctl(fd, GET_BUFFER, (void *) buf);
}

int cma_free_buffer(int fd, struct UBuffer *buf)
{
	return ioctl(fd, FREE_BUFFER, (void *) buf);
}
