#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "cma.h"

int main()
{
	int fd;
	struct UBuffer ubuf;
	uint32_t width, height, depth, stride, buf_size;	
	const uint32_t N_BUFFERS = 20;

	void *mapped_addrs[N_BUFFERS];
	struct UBuffer buffers[N_BUFFERS];
	uint32_t i, j;

	// open cma driver
	fd = open("/dev/cmabuffer0", O_RDWR);
	if(fd < 0) {
		printf("failed to open cma device driver\n");
		return -1;
	}
	printf("cma device opened successfully\n");
	
	// define buffer sizes
	width = 1024;
	height = 768;
	depth = 4;
	stride = width;
	buf_size = stride * height * depth;

	// initialize buffers
	printf("initializing buffer sizes\n");
	for (i = 0; i < N_BUFFERS; i++) {
		buffers[i].width = width;
		buffers[i].height = height;
		buffers[i].depth = depth;
		buffers[i].stride = stride;
	}
	printf("buffer size initialization completed\n");

	// get buffers
	printf("allocating buffers\n");
	for (i = 0; i < N_BUFFERS; i++) {
		if(cma_get_buffer(fd, &buffers[i]) == 0) {
			printf("buffer[%u] successfully allocated\n", buffers[i].id);
		} else {
			printf("failed to allocate buffer[%u]\n", i);
			for(j = i-1; j >= 0; j--) {
				cma_free_buffer(fd, &(buffers[j]));
				printf("de-allocating buffer [%u]\n", i);
			}
			goto deinit;
		}
	}
	printf("buffer allocation completed\n");


	// mmap the buffers
	printf("memory-mapping the buffers\n");
	for(i = 0; i < N_BUFFERS; i++) {
		mapped_addrs[i] = cma_map(&buffers[i], 0, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if(mapped_addrs[i] == MAP_FAILED) printf("mapping buffer [%u] failed\n", buffers[i].id);
	}
	printf("buffer mmap completed\n");

	// perform write/read tests on buffers
	printf("starting write/read tests\n");
	for(i = 0; i < N_BUFFERS; i++) {
		for(j = 0; j < buf_size; j++) {
			if(mapped_addrs[i] != MAP_FAILED)
				*((char *)mapped_addrs[i] + j) = (j + i*25) % 256;
		}
	}
	for(i = 0; i < N_BUFFERS; i++) {
		for(j = 0; j < buf_size; j++) {
			if(mapped_addrs[i] != MAP_FAILED && *((char *)mapped_addrs[i] + j) != (j + i*25) % 256) {
				printf("write/read test failed\n");
				break;
			}
		}
	}
	printf("write/read test completed\n");

	// unmap the buffers
	for(i = 0; i < N_BUFFERS; i++) {
		if(cma_unmap(mapped_addrs[i], buf_size)) printf("unmapping buffer [%u] failed\n", buffers[i].id);
	}	
	
	// release buffers
	buf_free: for(i = 0; i < N_BUFFERS; i++) {
		if(cma_free_buffer(fd, &buffers[i])) {
			printf("failed to free buffer [%u]\n", buffers[i].id);
		}
	}

	// close driver
	deinit: close(fd);
	printf("cma device closed\n");

	return 0;
}
