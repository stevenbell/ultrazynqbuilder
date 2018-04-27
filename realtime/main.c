/* main.c
 * Top-level application code for R5 runtime which controls the camera
 * and peripherals.
 * If this is built to run along with Linux, be sure to set the USING_AMP
 * define, or else it'll try to initialize and shutdown the platform (both of
 * which will make you sad).
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xscugic.h"
#include "sleep.h"

#include "i2c.h"
#include "ttc_clock.h"
#include "si514_clk.h"
#include "mpu9250.h"
#include "imx219_cam.h"

const u32 GPIO_LEDS = XPAR_AXI_GPIO_0_BASEADDR;
const u32 GPIO_CAMERA = (XPAR_AXI_GPIO_0_BASEADDR + 0x8);

#define CSIRX0  XPAR_CAM0_CSIRX_REGSPACE_S_AXI_BASEADDR

u32 irqs_received = 0;
u32 csirx0_irq_time;

void csi_irq_handler(void* val)
{
  csirx0_irq_time = ttc_clock_now();
  uint32_t* csirx0 = (uint32_t*)CSIRX0;
  csirx0[1] = 0xe; // Ack interrupts

  irqs_received++;
}


int main()
{
  u32* leds = (u32*)GPIO_LEDS;
  u32* gpio_cam = (u32*)GPIO_CAMERA;
  ttc_clk_init();

#ifndef USING_AMP
  init_platform();
#endif

  // Blink for 5 seconds
  for(int i = 0; i < 20; i++){
    *leds = 0xaa;
    usleep(100000);
    *leds = 0x55;
    usleep(100000);
  }

  i2c_set_mux(3);

  // Set up the clock so it's running when the camera wakes up
  si514_init();

  // Enable the cameras
  *leds = 0x01;
  *gpio_cam = 0xff;
  usleep(1000);

  imx219_cam_init();

  // Set up interrupts
  const int IRQ_NUM = XPAR_FABRIC_CAM0_CSIRX_CSI_INTR_INTR;

  XScuGic_Config* gic_cfg = XScuGic_LookupConfig(XPAR_PSU_RCPU_GIC_DEVICE_ID);
  XScuGic gic;
  int32_t ok;
  ok = XScuGic_CfgInitialize(&gic, gic_cfg, gic_cfg->CpuBaseAddress);
  if(ok != XST_SUCCESS){
    printf("Failed to initialize gic\r\n");
  }

  // Connect up the R5 GIC to receive interrupts
  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
      (Xil_ExceptionHandler) XScuGic_InterruptHandler,
      &gic);
  Xil_ExceptionEnable();

  // Priority is 8 (just below the max of 0), trigger is rising-edge
  XScuGic_SetPriorityTriggerType(&gic, IRQ_NUM, 0x08, 0b11);

  ok = XScuGic_Connect(&gic, IRQ_NUM, csi_irq_handler, NULL);
  if(ok != XST_SUCCESS){
    printf("Failed to connect irq\r\n");
  }

  XScuGic_Enable(&gic, IRQ_NUM);

  // Set up the CSI receiver
  uint32_t* csirx0 = (uint32_t*)CSIRX0;
  const u32 CSIRX_CTRL_RUN_CONTINUOUS = 0x01;
  const u32 CSIRX_CTRL_ENABLE_INTERRUPTS = 0x02;
  const u32 CSIRX_CTRL_ENABLE_SOF_IRQ = 0x04;
  const u32 CSIRX_CTRL_ENABLE_EOF_IRQ = 0x08;

  csirx0[0] = CSIRX_CTRL_RUN_CONTINUOUS | CSIRX_CTRL_ENABLE_INTERRUPTS | CSIRX_CTRL_ENABLE_SOF_IRQ;
  csirx0[1] = 0xf; // Start running


  // Main loop
  // Pick a target time (the start time of the next event from the queue)
  u32 target = ttc_clock_now() + 120*(1000*100); // 100MHz = 10ns increments
  int32_t inflight_time = 33327; // N-to-N+1 time of the frame currently in flight (us)

  u32 last_irqs_received = 0;
  while(1){
    // Get the current time
    // Check if any events need to be handled and dispatch them

    // Wait until the csirx finishes receiving a frame (frame N)
    while(last_irqs_received == irqs_received){}
    last_irqs_received = irqs_received;

    uint32_t now = csirx0_irq_time;
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
        inflight_time = 33000;
      }
    }

    // Set the COARSE_INTEGRATION_TIME register 0x015A
    uint8_t regvals[4] = {0x01,0x5A,0x00,0x00};

    regvals[2] = exposure_lines >> 8;
    regvals[3] = exposure_lines & 0xff;

    XIic_Send(IIC_BASEADDR, I2C_CAM_ADDR, regvals, 4, XIIC_STOP);

    //printf("target: %d  time remaining: %d (%d), exposure %d\r\n", target, delta_us, delta_us - inflight_time, exposure_lines);

    // Grab the next frame from the CSI (frame N+1)
    // Frame N+1 had its settings locked in a while ago, and is exposing
    csirx0[1] = 0xf; // Ack any old interrupts, and run once
  }


#ifndef USING_AMP
  cleanup_platform();
#endif

  return 0;
}
