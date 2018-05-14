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
  ttc_clk_init();

#ifdef USING_AMP
  requestqueue_init();
#endif
  init_platform(); // Interrupts and such

  // Blink for 5 seconds
  for(int i = 0; i < 20; i++){
    *leds = 0xaa;
    usleep(100000);
    *leds = 0x55;
    usleep(100000);
  }


  // Enable the cameras
  *leds = 0x01;
  *gpio_cam = 0xff; // TODO: move this into camera init too
  usleep(1000);

  IMX219_Config cam0 = {
    .i2c_channel = 3,
    .baseaddr = XPAR_CAM0_CSIRX_REGSPACE_S_AXI_BASEADDR,
    .irq = XPAR_FABRIC_CAM0_CSIRX_CSI_INTR_INTR,
  };
  imx219_cam_init(&cam0);

  IMX219_Config cam1 = {
    .i2c_channel = 2,
    .baseaddr = XPAR_CAM1_CSIRX_REGSPACE_S_AXI_BASEADDR,
    .irq = XPAR_FABRIC_CAM1_CSIRX_CSI_INTR_INTR,
  };
  imx219_cam_init(&cam1);

  // Start the csi receivers running
  imx219_cam_run(&cam0);
  imx219_cam_run(&cam1);

  // Main loop
  // Pick a target time (the start time of the next event from the queue)
  u32 target = ttc_clock_now() + 120*(1000*100); // 100MHz = 10ns increments
  int32_t inflight_time = 33327; // N-to-N+1 time of the frame currently in flight (us)

  u32 last_cam_time[2] = {0, 0};

  while(1){
    requestqueue_check();

    if(last_cam_time[0] != cam0.finished_time){
      // cam0 has a new frame, handle it
      last_cam_time[0] = cam0.finished_time;

    uint32_t now = cam0.finished_time;
    int32_t delta_us = (int32_t)(target/100) - (now / 100); // Time from now, microseconds

    if(delta_us < 15*1000){
      // Less than 15ms left; this round is over
      printf("%d\r\n", delta_us);

      // Pick a new target time between 100 and 200ms from now
      target = ttc_clock_now() + ((rand() % 200) + 100) *(1000*100); // 100MHz = 10ns increments
      delta_us = (int32_t)(target/100) - (now / 100);
    }

    // TODO: Wait for some configurable delay, up to the commit-point

    uint32_t exposure_lines; // Duration of the exposure in terms of image line times

    if(delta_us - inflight_time < 15*1000){
      // This round will be over after the next frame, so configure a
      // short shot for the beginning of the next round
      exposure_lines = 10; // Anything less than ~2000 will be 33ms
      inflight_time = 33327;
    }
    else{
      // This round isn't going to be over any time soon; configure the
      // exposure to hit the target time
      int32_t exposure = delta_us - inflight_time; // How much after frame N+1
      exposure_lines = (exposure - 80) / 18.90;
      inflight_time = exposure;

      // Probably better to just throw away lots of frames rather than one big one...
      if(exposure_lines > 65535){
        exposure_lines = 65535;
        inflight_time = exposure_lines * 19; // TODO: replace with calculated max
      }
      else if(exposure_lines < 4){
        exposure_lines = 4;
        inflight_time = 33327;
      }
    }

    imx219_cam_set_exposure(&cam0, exposure_lines);
    printf("target: %u  time remaining: %d (%d), exposure %d\r\n", target, delta_us, delta_us - inflight_time, exposure_lines);
    // Grab the next frame from the CSI (frame N+1)
    // Frame N+1 had its settings locked in a while ago, and is exposing
    imx219_cam_run(&cam0);
    }

    if(last_cam_time[1] != cam1.finished_time){
      // cam1 has a new frame, handle it
      imx219_cam_run(&cam1);
      last_cam_time[1] = cam1.finished_time;
    }
  }

#ifdef USING_AMP
  requestqueue_close();
#endif
  cleanup_platform();

  return 0;
}
