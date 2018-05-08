#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "ubuffer.h"
#include "ioctl_dma.h"
#include "stream-gen.h"
#include "cma.h"

int main(int argc, char **argv)
{
	int dma_fd, stream_fd, cma_fd;
	int status;
	unsigned int val;
	struct UBuffer buf;
	uint16_t *buf_addr;
	int i;

    printf("======= DMA TEST PROG =========\n");

	// open cmabuffer driver
	printf("opening the cma driver...\n");
	cma_fd = open("/dev/cmabuffer0", O_RDWR);
	if(cma_fd < 0) {
		printf("failed to open the cmabuffer driver!\n");
		printf("\tMake sure the driver is loaded\n");
		return -1;
	}
	printf("successfully opened the cmabuffer driver\n");

	// open the stream generator driver
	printf("opening the stream-gen driver...\n");
	stream_fd = open("/dev/streamgen0", O_RDWR);
	if(stream_fd < 0) {
		printf("failed to open the stream generator driver!\n");
		printf("\tMake sure the driver is loaded\n");
		close(cma_fd);
		return -1;		
	}
	printf("successfully opened the stream generator driver\n");

	// open the dma driver
	printf("opening the dma driver...\n");
	dma_fd = open("/dev/xilcam0", O_RDWR);
	if(dma_fd < 0) {
		printf("failed to open the xilcam DMA driver!\n");
		printf("\tMake sure the driver is loaded\n");
		close(cma_fd);
		close(stream_fd);
		return -1;		
	}
	printf("successfully opened the xilcam DMA driver\n");

	/* ==== setup the stream generator === */
	// set MODE = run/stop, no interrupts, TLAST enabled
	val = 0;
	status = wr_reg(stream_fd, CONFIG_REG, &val);
	if(status) {
		printf("failed to configure stream generator\n");
		goto close_fds;
	}

	// set stream generation SIZE
	val = 1920 * 1080;
	status = wr_reg(stream_fd, SIZE_REG, &val);
	if(status) {
		printf("failed to set SIZE for stream generator\n");
		goto close_fds;
	}

	// start the stream_generator 
	val = 0x01;
	status = wr_reg(stream_fd, CTRL_SET_REG, &val);
	if(status) {
		printf("failed to start the stream generator\n");
		goto close_fds;
	}

	// get a buffer
	buf.width = 1920;
	buf.height = 1080;
	buf.depth = 2;
	buf.stride = buf.width;

	status = cma_get_buffer(cma_fd, &buf);
	if(status) {
		printf("failed to get a buffer from cmabuffer driver\n");
		goto close_fds;
	}

	// grab an image from DMA
	status = ioctl(dma_fd, GRAB_IMAGE, &buf);
	if(status) {
		printf("failed to grab an image from DMA\n");
		cma_free_buffer(cma_fd, &buf);
		goto close_fds;
	}

	// mmap the buffer
	buf_addr = (uint16_t *) cma_map(&buf, 0, buf.stride * buf.height * buf.depth, PROT_READ | PROT_WRITE, MAP_SHARED, cma_fd, 0);
	if(buf_addr == MAP_FAILED) {
		printf("failed to mmap the buffer\n");
		cma_free_buffer(cma_fd, &buf);
		goto close_fds;
	}

	// verify the content of the buffer 
	status = buf.stride * buf.height;
	for(i = 0; i < status; i++) {
		if(buf_addr[i] != (i + 1)) {
			printf("test failed [index = %d, val @ index = %u]\n", i, buf_addr[i]);
			cma_unmap(buf_addr, buf.stride * buf.height * buf.depth);
			cma_free_buffer(cma_fd, &buf);
			goto close_fds;
		}
	}

	cma_unmap(buf_addr, buf.stride * buf.height * buf.depth);
	cma_free_buffer(cma_fd, &buf);
	printf("TEST DONE!\n");
	
	close_fds:
		close(dma_fd);
		close(stream_fd);
		close(cma_fd);
    return 0;
}
