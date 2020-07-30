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

const u32 SHUTTER_IRQ = 123U;

u32 shutter_count = 0;

// Initialize this globally for maximum speed access
u32* gpio_cam = (u32*)(XPAR_AXI_GPIO_0_BASEADDR + 0x8);

void shutter_irq_handler(void* val)
{
  *gpio_cam = 0x08; // Turn on the pin we're watching
  usleep(10);
  *gpio_cam = 0x00; // Reset for next time
  shutter_count++;
}

int main()
{
  u32* leds = (u32*)GPIO_LEDS;

  ttc_clock_init();

#ifdef USING_AMP
  masterqueue_init();
#endif
  init_platform(); // Interrupts and such

  printf("R5 loop initializing...\n");

  // Blink for 5 seconds
  for(int i = 0; i < 20; i++){
    *leds = 0xaa;
    usleep(100000);
    *leds = 0x55;
    usleep(100000);
  }

  int ok = connect_interrupt(SHUTTER_IRQ, shutter_irq_handler, NULL);
  if(ok != XST_SUCCESS){
	  printf("IRQ init failed: %d\n", ok);
	  *leds = 0xff;
	  while(1){} // stop here
  }

  while(1){
	  //printf("irq count: %ld\n", shutter_count);
	  //*leds = shutter_count;
  }


#ifdef USING_AMP
  masterqueue_close();
#endif
  cleanup_platform();

  return 0;
}
