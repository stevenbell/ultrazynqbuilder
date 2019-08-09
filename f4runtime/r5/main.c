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
#include "pmod_flash.h"
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

  // Blink for 5 seconds
  for(int i = 0; i < 10; i++){
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
    .reqId = CAMERA0,
    .i2c_channel = 3,
    .baseaddr = XPAR_CAM0_CSIRX_REGSPACE_S_AXI_BASEADDR,
    .irq = XPAR_FABRIC_CAM0_CSIRX_CSI_INTR_INTR,
  };
  imx219_cam_init(&cam0);

  IMX219_Config cam1 = {
    .reqId = CAMERA1,
    .i2c_channel = 2,
    .baseaddr = XPAR_CAM1_CSIRX_REGSPACE_S_AXI_BASEADDR,
    .irq = XPAR_FABRIC_CAM1_CSIRX_CSI_INTR_INTR,
  };
  imx219_cam_init(&cam1);

  // Start the csi receivers running
  imx219_cam_run(&cam0);
  imx219_cam_run(&cam1);

  // HACK: Start the HLS demosaicker
  uint32_t* demosaic0 = (uint32_t*)XPAR_DEMOSAIC0_S_AXI_CONFIG_BASEADDR;
  demosaic0[0] = 0x81; // Start, auto-restart

//  int16_t ccm[12] = { 563, -107, 20, -10142, -96, 385, 19, -6982, -27, -234, 773, -5408};
//  for(int i = 0; i < 12; i++){
//    demosaic0[2*i + 4] = ccm[i];
//  }
//
//  demosaic0[28] = 0x0; // Phase = 0b00

  uint32_t* demosaic1 = (uint32_t*)XPAR_DEMOSAIC1_S_AXI_CONFIG_BASEADDR;
  demosaic1[0] = 0x81;

  pmod_flash_config flash0 = {
    .reqId = FLASH0,
    .pin = 0,
  };
  pmod_flash_init(&flash0);

  pmod_flash_config flash1 = {
    .reqId = FLASH1,
    .pin = 1,
  };
  pmod_flash_init(&flash1);

  pmod_flash_config flash2 = {
    .reqId = FLASH2,
    .pin = 3, // Next to the previous two on PMOD, but non-sequential GPIO number
  };
  pmod_flash_init(&flash2);


  // Main loop
  Time last_cam_time[2] = {0, 0}; // Last time when we received a frame

  while(1){
    masterqueue_check();

    if(last_cam_time[0] != cam0.finished_time){
      // cam0 has a new frame, handle it
      last_cam_time[0] = cam0.finished_time;
      imx219_handle_requests(&cam0);
     }

    if(last_cam_time[1] != cam1.finished_time){
      // cam1 has a new frame, handle it
      last_cam_time[1] = cam1.finished_time;
      imx219_handle_requests(&cam1);
     }

    pmod_flash_handle_requests(&flash0);
    pmod_flash_handle_requests(&flash1);
    pmod_flash_handle_requests(&flash2);

  }

#ifdef USING_AMP
  masterqueue_close();
#endif
  cleanup_platform();

  return 0;
}
