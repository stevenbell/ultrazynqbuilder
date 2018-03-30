/* Run an S2MM DMA transaction using raw writes to /dev/mem
 * This should normally be encapsulated in a driver, but this is a good way to
 * do quick development tests.
 * Steven Bell <sebell@stanford.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

const int N_BUFFERS = 5;

// Each pixel is 16 bits
const u_int32_t WIDTH = 1640;
const u_int32_t HEIGHT = 1232;

const u_int64_t DMA0_BASE =   0x80020000; // Base address of DMA engine (must be page aligned)
const u_int64_t DMA_MEM =     0x70000000; // Base address of the memory we're going to commandeer (page aligned)
const u_int32_t DMA0_SG_OFF = 0x1000; // Offset to scatter-gather table
const u_int32_t DMA0_DATA =   0x100000; // Offset to memory for data

#define MAP_LEN (DMA0_DATA * 2)

void configure_dmarx(u_int32_t* dma_base, u_int32_t* sg_base, u_int32_t sg_phys, u_int32_t data_phys)
{
  // Create 1-byte pointers just to make the addressing more explicit below
  u_int8_t* dma = (u_int8_t*)dma_base;
  u_int8_t* sg = (u_int8_t*)sg_base;

  /* For 2D DMA transfer (uses scatter-gather) */
  u_int32_t hsize = WIDTH*2;
  u_int32_t vsize = HEIGHT;

  // Create the buffer ring
  for(int i = 0; i < N_BUFFERS; i++){
    *(u_int32_t*)(sg + 0x00) = (u_int32_t)(sg_phys + ((i+1) % N_BUFFERS) * 0x40); // Next descriptor (must be 16-word aligned)
    *(u_int32_t*)(sg + 0x04) = 0; // Upper 32 bits of next descriptor addr are zero (for < 4GB RAM!)
    *(u_int32_t*)(sg + 0x08) = data_phys + i*hsize*vsize; // Data address
    *(u_int32_t*)(sg + 0x0C) = 0; // Upper 32 bits
    *(u_int32_t*)(sg + 0x10) = 0xff000000; // AWUSER = 0b1111, AWCACHE = 0b1111 to enable coherency
    *(u_int32_t*)(sg + 0x14) = vsize << 19 | hsize; // [32 -- VSIZE -- 19 | 18 - reserved - 16 | 15 -- STRIDE -- 0]
    *(u_int32_t*)(sg + 0x18) = hsize; // HSIZE
    *(u_int32_t*)(sg + 0x1c) = 0; // SOP/EOP flags are set by DMA
    sg += 0x40;
  }

  // Launch the DMA
  *(u_int32_t*)(dma + 0x2C) = 0x00000f0f; // Enable coherency for SG transactions (ARUSER is 0b1111, dunno if it matters)
  *(u_int32_t*)(dma + 0x30) = 0x00010001; // Enable DMA engine
  *(u_int32_t*)(dma + 0x38) = sg_phys; // Target address
  *(u_int32_t*)(dma + 0x3c) = 0; // Upper 32 bits
  *(u_int32_t*)(dma + 0x40) = sg_phys + 0x40*(N_BUFFERS-1); // Last descriptor

  // Now babysit the DMA
  // This is easier in the driver, because we get interrupts

  sg = (u_int8_t*)sg_base; // Reset sg to beginning of chain
  u_int32_t curdesc_off = 0; // Offset to the next-to-be-finished descriptor

  // This really dumb loop spins forever, clearing the completed bits and
  // bumping the tail pointer so the DMA keeps running.
  // Note that the DMA curdesc register may change quite some time before
  // the CMPLT bit gets set and the data is actually free to use.
  printf("DMA initialized; now babysitting.\n");
  while(1){
    // Spin until the current descriptor is marked done
    while(*(u_int32_t*)(sg + curdesc_off + 0x1c) == 0){}

    // This is where we'd actually *use* the data

    // Reset the "done" bit (and everything else...)
    *(u_int32_t*)(sg + curdesc_off + 0x1c) = 0;

    // Move the tail pointer to this descriptor
    *(u_int32_t*)(dma + 0x40) = sg_phys + curdesc_off;

    // Update our current descriptor reference
    curdesc_off = (curdesc_off + 0x40) % (0x40 * N_BUFFERS);
    printf("moved curdesc to %x\n", curdesc_off);
  }
}


int main(int argc, char* argv[])
{
  int mem = open("/dev/mem", O_RDWR);
  // Map the memory in CMA space
  u_int8_t* mptr = (u_int8_t*)mmap(NULL, MAP_LEN, PROT_WRITE, MAP_SHARED, mem, DMA_MEM);
  if(mptr == MAP_FAILED){
    printf("Failed to create memory mapping. Are you root?\n");
    return(0);
  }

  // Map the DMA engine, 1 page is enough
  u_int32_t* dmaptr = (u_int32_t*)mmap(NULL, 0x1000, PROT_WRITE, MAP_SHARED, mem, DMA0_BASE);
  if(mptr == MAP_FAILED){
    printf("Failed to create memory mapping. Are you root?\n");
    return(0);
  }

  memset(mptr, 0xee, MAP_LEN); // Wipe memory, but not with 0

  // Set up a DMA transaction
  configure_dmarx(dmaptr, (u_int32_t*)(mptr + DMA0_SG_OFF), (DMA_MEM + DMA0_SG_OFF), (DMA_MEM + DMA0_DATA));

  // We'll never get here...
  munmap(mptr, MAP_LEN);
  return(0);
}
