
#include <cstdint>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>


#include <stdio.h> // REMOVE ME
#include "ttc_clock.h"

int memhandle = 0;
uint32_t* ttcregs;

void ttc_clock_init(void)
{
  memhandle = open("/dev/mem", O_RDONLY);
  ttcregs = (uint32_t*)mmap(NULL, 16, PROT_READ, MAP_SHARED, memhandle, TTC_REG_BASE);
  if(ttcregs == MAP_FAILED){
    printf("Error opening /dev/mem.  Are you root?\n");
  }
}

Time ttc_clock_now(void)
{
  if(ttcregs == MAP_FAILED){
    return 0;
  }

  uint32_t fine = ttcregs[TTC_COUNT2];
  uint32_t coarse = ttcregs[TTC_COUNT1];

  // Whichever bit is 0 is the clock that has rolled over
  uint64_t t = coarse;
  if(coarse >> 15 & 0x01){ // Coarse bit is 1
    if(fine >> 31 & 0x01){ // Fine bit is 1
      t = ((t & 0xffff0000) << 16) + fine; // Bits match, just combine the clocks
    }
    else{
      t = ((t & 0xffff0000) << 16) + (1 << 16) + fine; // Fine rolled over, need to increment coarse
    }
  }
  else{ // Coarse bit is 0
    if(fine >> 31 & 0x01){ // Fine bit is 1
      t = ((t & 0xffff0000) << 16) - (1 << 16) + fine; // Coarse rolled over; need to decrement coarse
    }
    else{
      t = ((t & 0xffff0000) << 16) + fine; // Bits match, just combine the clocks
    }
  }

  return t / TICKS_PER_US;
}

int64_t ttc_clock_diff(Time first, Time second)
{
  // Integer overflow actually handles the wrap for us, as long as we treat
  // the result as a signed value.
  int64_t difference = second - first;
  return(difference);
}

/* Quick standalone test
int main(int argc, char* argv[])
{
  ttc_clk_init();
  Time last = ttc_clk_now();
  while(1){
    Time now = ttc_clk_now();
    printf("time: %lu   dt: %ld\n", now, ttc_clock_diff(last, now));
    last = now;
  }

  return 0;
}
*/
