#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "cma.h"

#define WIDTH	200
#define HEIGHT	200
#define DEPTH	2
#define STRIDE 	300

void test_slice()
{
	int fd;
	struct UBuffer root;
	struct UBuffer slice;
	int status;
	void *addr;

	fd = open("/dev/cmabuffer0", O_RDWR);
	if(fd < 0) {
		printf("failed to open /dev/cmabuffer0\n");
		return;
	}

	/* get root buffer */
	root.width = WIDTH;
	root.height = HEIGHT;
	root.depth = DEPTH;
	root.stride = STRIDE;
	status = cma_get_buffer(fd, &root);
	if(status < 0) {
		close(fd);
		printf("failed to acquire root buffer\n");
		return;
	}

	/* get a slice */
	slice.offset = 50*301;
	slice.width = WIDTH >> 2;
	slice.height = HEIGHT >> 2;
	status = cma_slice_buffer(fd, root.id, &slice);
	if(status < 0) {
		printf("failed to slice buffer\n");
		cma_free_buffer(fd, &root);
		close(fd);
		return;
	}
	printf("successfully got slice [id = %u] from root buffer [id = %u]\n", slice.id, root.id);
	
	/* mmap slice */
	unsigned slice_size = (STRIDE * (slice.height - 1) + slice.width) * DEPTH;
	printf("size of slice to mmap is %u\n", slice_size);
	addr = cma_map(&slice, 0, slice_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(addr == MAP_FAILED) {
		printf("failed to mmap the slice\n");
		cma_free_buffer(fd, &root);
		close(fd);
		return;
	}
	printf("successfully mmaped slice\n");

	/* write to slice */
	for(unsigned i = 0; i < slice_size; i++) {
		*((char *)addr + i) = i % 256;
	}
	printf("successfully wrote to entire slice\n");

	/* read back from slice */
	for(unsigned i = 0; i < slice_size; i++) {
		if(*((char *)addr + i) != i%256) {
			printf("verification failed\n");
			goto cleanup;
		}
	}
	printf("successfully read back from entire slice\n");

	printf("DONE!\n");

	cleanup:
		cma_unmap(addr, slice_size);
		cma_free_buffer(fd, &root);
		close(fd);
}
