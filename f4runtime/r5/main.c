/* main.c
 * Top-level application code for R5 runtime which controls the camera
 * and peripherals.
 * If this is built to run along with Linux, be sure to set the USING_AMP
 * define so that shared-memory communication works.
 */

#include <stdio.h>
#include <stdbool.h>
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

  // HACK: disable output
  *(u32*)(cam0.baseaddr) |= CSIRX_CFG_OUTPUT_DISABLE;
  // HACK: set the line height
  *(u32*)(cam0.baseaddr + 0x10) = 1232; // Coming off the image sensor

  // Start the csi receivers running
  imx219_cam_run(&cam0);
  imx219_cam_run(&cam1);

  // HACK: Start the HLS demosaicker
  uint32_t* demosaic = (uint32_t*)XPAR_DEMOSAIC0_S_AXI_CONFIG_BASEADDR;
  *demosaic = 0x81; // Start, auto-restart

  // Main loop
  Time last_cam_time[2] = {0, 0}; // Last time when we received a frame

  // Arrays to track images in flight
  Time cam0_inflight_sof[2] = {0, 0}; // When we expect to receive the frames in flight
  Time cam0_inflight_duration[2] = {0, 0}; // Expected duration (sof to sof) of frames in flight
  bool cam0_inflight_isdummy[2] = {true, true};

  while(1){
    masterqueue_check();

    if(last_cam_time[0] != cam0.finished_time){
      // cam0 has a new frame, handle it
      last_cam_time[0] = cam0.finished_time;

      if(!cam0_inflight_isdummy[0]){
        printf("received frame on camera0 at %llu (scheduled at %llu,  %+lld)\n",
             last_cam_time[0], cam0_inflight_sof[0], ttc_clock_diff(last_cam_time[0], cam0_inflight_sof[0]));
      }
      else{
        printf("rx dummy at %llu (expected at %llu,  %+lld)\n",
             last_cam_time[0], cam0_inflight_sof[0], ttc_clock_diff(last_cam_time[0], cam0_inflight_sof[0]));
      }

      // We just began to receive frame N; frame N+1 is already locked in (and
      // possibly exposing), so we'll be setting frame N+2.

      // Default values; we'll override them if we actually need a frame
      u32 exposure_lines = 4; // Length of the next exposure, in terms of image rows
      bool isDummy = true; // Whether this is a dummy (hidden) frame

      // Look at the next request
      Request req = requestqueue_peek(CAMERA0);
      if(req.device != NO_DEVICE){

        // Time in the request is the beginning of the exposure; calculate the
        // SOF time, which is what we can actually track.
        Time sof_target = req.time + req.params.exposure + IMX219_BLANKING;

        // Time from SOF of previous frame to SOF of this request
        Time interframe = req.params.exposure + IMX219_BLANKING;
        if(interframe < imx219_min_frame_time(&cam0)){
          interframe = imx219_min_frame_time(&cam0);
        }

        // Calculate the slack
        s64 slack = ttc_clock_diff((cam0_inflight_sof[1] + interframe), sof_target);

        // If the request is too late (slack < 0), just run it right away
        // If we've hit the time when the request should be scheduled, or if the
        // request does not have enough slack to be scheduled properly, just
        // schedule it right away. We might hit the timepoint more accurately if
        // we put in another dummy frame, but it is safer to schedule ASAP and
        // not push the schedule later, possibly bumping other things.
        if(slack < (s64)imx219_min_frame_time(&cam0)){
          exposure_lines = req.params.exposure / 18.90;
          if(exposure_lines < 1){
            exposure_lines = 1;
          }
          isDummy = false;
          requestqueue_pop(CAMERA0); // We're done with this Request
        }

        // If we can insert more than one dummy frame (slack > 2*min_frame_time),
        // then insert a minimal dummy frame.
        // Otherwise, calculate the required dummy frame exposure and insert it.
        else if(slack < 2*imx219_min_frame_time(&cam0)){
          exposure_lines = (slack - IMX219_BLANKING) / 18.90;
        }
        printf("sof target: %lld, slack: %lld  lines: %lu  dummy: %u\n", sof_target, slack, exposure_lines, isDummy);
      } // END if(camera has a request)

      // Configure the camera and CSI receiver with the calculated parameters
      imx219_cam_set_exposure(&cam0, exposure_lines);

      // Store the important values into the in-flight queue
      cam0_inflight_sof[0] = cam0_inflight_sof[1];
      cam0_inflight_duration[0] = cam0_inflight_duration[1];
      cam0_inflight_isdummy[0] = cam0_inflight_isdummy[1];

      if(exposure_lines > 1763){ // TODO: don't hardcode this
        cam0_inflight_duration[1] = (exposure_lines * 18.90) + 90;
      }
      else{
        cam0_inflight_duration[1] = imx219_min_frame_time(&cam0);
      }
      cam0_inflight_sof[1] = last_cam_time[0] + cam0_inflight_duration[0] + cam0_inflight_duration[1];
      cam0_inflight_isdummy[1] = isDummy;

      // CSI receiver has be configured for frame N+1, not N+2
      if(cam0_inflight_isdummy[0]){
        *(u32*)(cam0.baseaddr) |= CSIRX_CFG_OUTPUT_DISABLE;
      }
      else{
        *(u32*)(cam0.baseaddr) &= ~(CSIRX_CFG_OUTPUT_DISABLE); // Enable output (clear "disable" bit)
      }

      // Grab the next frame from the CSI (frame N+1)
      // Frame N+1 had its settings locked in a while ago, and is exposing
      imx219_cam_run(&cam0);
    } // END handling camera 0
  }

#ifdef USING_AMP
  masterqueue_close();
#endif
  cleanup_platform();

  return 0;
}
