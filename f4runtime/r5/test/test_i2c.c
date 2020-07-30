/* main.c
 * Top-level application code for R5 runtime which controls the camera
 * and peripherals.
 * If this is built to run along with Linux, be sure to set the USING_AMP
 * define so that shared-memory communication works.
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"

#include "i2c.h"
#include "ttc_clock.h"
#include "mpu9250.h"
#include "imx219_cam.h"
#include "requestqueue.h"

const u32 GPIO_LEDS = XPAR_AXI_GPIO_0_BASEADDR;
const u32 GPIO_CAMERA = (XPAR_AXI_GPIO_0_BASEADDR + 0x8);

int main()
{
  u32* leds = (u32*)GPIO_LEDS;
  u32* gpio_cam = (u32*)GPIO_CAMERA;
  ttc_clock_init();

#ifdef USING_AMP
  masterqueue_init();
#endif
  init_platform(); // Interrupts and such

  printf("R5 loop initializing...\n");

  // Blink for 5 seconds
  for(int i = 0; i < 25; i++){
    *leds = 0xaa;
    usleep(100000);
    *leds = 0x55;
    usleep(100000);
  }

  i2c_init();

  while(1){
	  i2c_set_mux(1);
//	  for(int i = 0; i < 1000; i++){
//	    i2c_check();
//	    usleep(1);
//	  }
	  usleep(50);
	  i2c_set_mux(2);
    for(int i = 0; i < 1000; i++){
      i2c_check();
      usleep(1);
    }
  }

#ifdef USING_AMP
  masterqueue_close();
#endif
  cleanup_platform();

  return 0;
}
