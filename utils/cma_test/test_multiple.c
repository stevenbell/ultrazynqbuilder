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

void test_multiple()
{
	int fd1, fd2, fd3;
	struct UBuffer b1, b2, b3;
	int status;

	/* open multiple cma file handles */
	fd1 = open("/dev/cmabuffer0", O_RDWR);
	if(fd1 < 0) {
		printf("failed to open /dev/cmabuffer0 for fd1\n");
		return;
	}
	fd2 = open("/dev/cmabuffer0", O_RDWR);
	if(fd2 < 0) {
		close(fd1);
		printf("failed to open /dev/cmabuffer0 for fd2\n");
		return;
	}
	fd3 = open("/dev/cmabuffer0", O_RDWR);
	if(fd3 < 0) {
		close(fd1);
		close(fd2);
		printf("failed to open /dev/cmabuffer0 for fd3\n");
		return;
	}
	printf("sucessfully opened all cma file handles\n");

	/* request a buffer from each handle */
	b1.width = b2.width = b3.width = WIDTH;
	b1.height = b2.height = b3.height = HEIGHT;
	b1.depth = b2.depth = b3.depth = DEPTH;
	b1.stride = b2.stride = b3.stride = STRIDE;

	status = cma_get_buffer(fd1, &b1);
	if(status < 0) {
		printf("failed to acquire buffer b1\n");
		goto close_fds;
	}
	status = cma_get_buffer(fd2, &b2);
	if(status < 0) {
		printf("failed to acquire buffer b2\n");
		cma_free_buffer(fd1, &b1);
		goto close_fds;
	}
	status = cma_get_buffer(fd3, &b3);
	if(status < 0) {
		printf("failed to acquire buffer b3\n");
		cma_free_buffer(fd1, &b1);
		cma_free_buffer(fd2, &b2);
		goto close_fds;
	}

	cma_free_buffer(fd1, &b1);
	cma_free_buffer(fd2, &b2);
	cma_free_buffer(fd3, &b3);

	printf("DONE!\n");

	close_fds:
	close(fd1);
	close(fd2);
	close(fd3);
}
