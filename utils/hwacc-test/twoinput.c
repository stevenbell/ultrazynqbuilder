#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "ubuffer.h"
#include "ioctl_cmds.h"
#include "cma.h"

int main(int argc, char* argv[])
{
  // Open the buffer allocation device
  int cma = open("/dev/cmabuffer0", O_RDWR);
  if(cma == -1){
    printf("Failed to open cma provider!\n");
    return(0);
  }

  // Open the hardware device
  int hwacc = open("/dev/hwacc0", O_RDWR);
  if(hwacc == -1){
    printf("Failed to open hardware device!\n");
    return(0);
  }

  // Allocate input and output buffers
  UBuffer bufs[3];
  bufs[0].width = 540;
  bufs[1].width = 540;
  bufs[0].height = 480;
  bufs[1].height = 480;
  bufs[0].depth = 3;
  bufs[1].depth = 3;
  bufs[0].stride = 640;
  bufs[1].stride = 640;

  bufs[2].width = bufs[2].stride = 640;
  bufs[2].height = 480;
  bufs[2].depth = 3;


  int ok = cma_get_buffer(cma, &bufs[0]);
  if(ok < 0){
    printf("Failed to allocate buffer 0!\n");
    return(0);
  }

  ok = cma_get_buffer(cma, &bufs[1]);
  if(ok < 0){
    printf("Failed to allocate buffer 1!\n");
    return(0);
  }

  ok = cma_get_buffer(cma, &bufs[2]);
  if(ok < 0){
    printf("Failed to allocate buffer 2!\n");
    return(0);
  }


  // Fill the first buffer with vertical stripes
  char* data = (char*) cma_map(&bufs[0], 0,  bufs[0].stride * bufs[0].height * bufs[0].depth,
                            PROT_WRITE, MAP_SHARED, cma, 0);
  if(data == MAP_FAILED){
    printf("mmap 0 failed!\n");
    return(0);
  }

  for(int i = 0; i < bufs[0].height; i++){
    for(int j = 0; j < bufs[0].width; j += 1){
      data[i * bufs[0].stride + j] = 20;
    }
  }
  cma_unmap((void*)data, bufs[0].stride * bufs[0].height * bufs[0].depth);

  printf("we are here\n");
  // Fill the second buffer with horizonal stripes
  data = (char*) cma_map(&bufs[1], 0, bufs[1].stride * bufs[1].height * bufs[1].depth,
                      PROT_WRITE, MAP_SHARED, cma, 0);
  if(data == MAP_FAILED){
    printf("mmap 1 failed!\n");
    return(0);
  }

  for(int i = 0; i < bufs[1].height; i += 1){
    for(int j = 0; j < bufs[1].width; j++){
      data[i * bufs[1].stride + j] = 20;
    }
  }
  cma_unmap((void*)data, bufs[1].stride * bufs[1].height * bufs[1].depth);

  // Run the processing operation
  int id = ioctl(hwacc, PROCESS_IMAGE, (long unsigned int)bufs);
  //printf("Processing ID %d\n", id);
  printf("Processing ID %d\n", id);
  ioctl(hwacc, PEND_PROCESSED, id);

  // Print out the result buffer
  data = (char*) cma_map(&bufs[2], 0, bufs[2].stride * bufs[2].height * bufs[2].depth,
                      PROT_READ, MAP_SHARED, cma, 0);
  if(data == MAP_FAILED){
    printf("mmap 2 failed!\n");
    return(0);
  }

  for(int i = 0; i < 10; i++){
    for(int j = 0; j < 10; j++){
      printf("%x, ", data[i * bufs[2].stride + j]);
    }
    printf("\n");
  }
  cma_unmap((void*)data, bufs[2].stride * bufs[2].height * bufs[2].depth);

  ok = cma_free_buffer(cma, &bufs[0]);
  ok = cma_free_buffer(cma, &bufs[1]);
  ok = cma_free_buffer(cma, &bufs[2]);

  close(hwacc);
  close(cma);
  return(0);
}

