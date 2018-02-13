/* Peek at a memory location using /dev/mem
 * This can be used as an interactive command-line utility, or in a script.
 * Steven Bell <sebell@stanford.edu>
 * 29 December 2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAP_LEN (4096*2)

int main(int argc, char* argv[])
{
  // Argument should be just a word-aligned address
  if(argc < 2 || argc > 3){
    printf("Usage: poke ADDRESS [COUNT]\n");
    return(0);
  }

  int count = 1; // By default, just print 1 value
  if(argc == 3){
    count = atoi(argv[2]);
  }

  u_int64_t addr = strtoll(argv[1], NULL, 0); // Auto-detect the base

  // TODO: check that the address is in the address range
  if((addr & 0x3) != 0){
    printf("Error: address is not word-aligned.\n");
    return(0);
  }

  // Find the page-aligned offset for the address and map the next two pages
  // This is a quick and hacky way to handle anything that overlaps pages
  // TODO: if the length is long, request more pages
  u_int64_t pageaddr = addr & 0xfffffffffffff000;
  u_int64_t pageoff = addr - pageaddr;
  //printf("Page address is 0x%lx, plus offset of 0x%lx\n", pageaddr, pageoff);

  int mem = open("/dev/mem", O_RDONLY);
  u_int32_t* mptr = (u_int32_t*)mmap(NULL, MAP_LEN, PROT_READ, MAP_SHARED, mem, pageaddr);
  if(mptr == MAP_FAILED){
    printf("Failed to create memory mapping. Are you root?\n");
    return(0);
  }

  for(int i = 0; i < count; i++){
    printf("0x%08lx : 0x%08x\n", addr+(4*i), *(mptr + (pageoff>>2) + i));
  }

  munmap(mptr, MAP_LEN);

  return(0);
}

