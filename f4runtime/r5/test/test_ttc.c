#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"

#include "i2c.h"
#include "ttc_clock.h"

#define NBINS 300
const int NTRIALS = 100000;
const u32 GPIO_LEDS = XPAR_AXI_GPIO_0_BASEADDR;

int main()
{
  u32* leds = (u32*)GPIO_LEDS;

  ttc_clock_init();

#ifdef USING_AMP
  masterqueue_init();
#endif
  init_platform(); // Interrupts and such

  while(1){


  printf("R5 loop initializing...\n");

  // Blink for 5 seconds
  for(int i = 0; i < 100; i++){
    *leds = 0xaa;
    usleep(100000);
    *leds = 0x55;
    usleep(100000);
  }

  // Now run the test
  int hist[NBINS] = {0};

  for(int i = 0; i < NTRIALS; i++){
    Time t1 = ttc_clock_now();
    Time t2 = ttc_clock_now();

    int64_t dt = ttc_clock_diff(t1, t2);
    dt = dt;
    if(dt < NBINS){
      hist[dt]++;
    }
    else{
      // Throw it in the largest bin
      printf("long wait: %lld\n", dt);
      hist[NBINS-1]++;
    }
  }

  printf("bin,count\n");
  for(int i = 0; i < NBINS; i++){
    printf("%d,%d\n", i, hist[i]);
  }

  }

}
