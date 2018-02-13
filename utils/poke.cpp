/* Poke a memory location using /dev/mem
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
  // Arguments should be <ADDRESS> <DATA>
  if(argc != 3){
    printf("Usage: poke ADDRESS DATA\n");
    return(0);
  }

  u_int64_t addr = strtoll(argv[1], NULL, 0); // Auto-detect the base
  u_int32_t data = strtol(argv[2], NULL, 0); // but it's probably hex...

  // TODO: check that the address is in the address range
  if((addr & 0x3) != 0){
    printf("Error: address is not word-aligned.\n");
    return(0);
  }

  // Find the page-aligned offset for the address and map the next two pages
  // This is a quick and hacky way to handle anything that overlaps pages
  u_int64_t pageaddr = addr & 0xfffffffffffff000;
  u_int64_t pageoff = addr - pageaddr;
  //printf("Page address is 0x%lx, plus offset of 0x%lx\n", pageaddr, pageoff);

  int mem = open("/dev/mem", O_RDWR);
  u_int32_t* mptr = (u_int32_t*)mmap(NULL, MAP_LEN, PROT_WRITE, MAP_SHARED, mem, pageaddr);
  if(mptr == MAP_FAILED){
    printf("Failed to create memory mapping. Are you root?\n");
    return(0);
  }

  //printf("Current value at 0x%lx is 0x%x; writing 0x%x\n", addr, *(mptr + (pageoff>>2)), data);
  *(mptr + (pageoff>>2)) = data;

  munmap(mptr, MAP_LEN);

  return(0);
}

