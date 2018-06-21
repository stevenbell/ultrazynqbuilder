#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "ubuffer.h"
#include "ioctl_cmds.h"
#include "cma.h"

// Each pixel is 16 bits
const uint32_t WIDTH = 1638;
const uint32_t HEIGHT = 1230;
const uint32_t DEPTH = 3;

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


  while(1){
	  // get a buffer
	  struct UBuffer buf;
	  buf.width = WIDTH;
	  buf.height = HEIGHT;
	  buf.depth = DEPTH;
	  buf.stride = buf.width;

	  printf("requesting a buffer from CMA driver...\n");
	  status = cma_get_buffer(cma_fd, &buf);
	  if(status) {
	  	printf("failed to get a buffer from cmabuffer driver\n");
	  	break; // Quit and clean up
	  }
	  printf("successfully obtained buffer from CMA driver\n");

	  // enroll the buffer with DMA
	  printf("enrolling buffer with DMA\n");
	  status = ioctl(dma_fd, ENROLL_BUFFER, &buf);
	  if(status) {
	  	printf("failed to enroll buffer with DMA\n");
	  	cma_free_buffer(cma_fd, &buf);
	  	break; // Quit and clean up
	  }

	  // wait until the frame is ready and retrieve it
	  printf("grabbing image from DMA\n");
	  status = ioctl(dma_fd, WAIT_COMPLETE, &buf);
	  if(status) {
	  	printf("failed to grab an image from DMA\n");
	  	cma_free_buffer(cma_fd, &buf);
      break;
	  }
	  printf("successfully grabbed image from DMA\n");

	  cma_free_buffer(cma_fd, &buf);
	  printf("Freed buffer\n");
  }

}
