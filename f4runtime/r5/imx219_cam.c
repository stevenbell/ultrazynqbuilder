/* imx219_cam.c
 * Control code for IMX219, as well as the CSI receiver.
 */

#include "xil_printf.h"
#include "i2c.h"
#include "si514_clk.h"
#include "ttc_clock.h"
#include "platform.h"
#include "imx219_cam.h"
#include "initcommands.h"

//#define VERBOSE 1

s32 read_registers(u16 address, u8 nregs, u8 registers[])
{
	// Force the 16-bit address into an MSB-first array of two bytes
	u8 addrbytes[2];
	addrbytes[0] = address >> 8;
	addrbytes[1] = address & 0xff;

	// Send the (two-byte) address
	unsigned int bytes_sent = XIic_Send(I2C_BASEADDR, I2C_CAM_ADDR, addrbytes, 2, XIIC_STOP);
	if (bytes_sent == 0) {
		// TODO: we could probably retry instead
		return XST_FAILURE;
	}
	usleep(50);

	// Receive the number of requested bytes
	unsigned int bytes_rxed = XIic_Recv(I2C_BASEADDR, I2C_CAM_ADDR, registers, 2, XIIC_STOP);
	if (bytes_rxed != nregs) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

void csi_irq_handler(void* val)
{
  IMX219_Config* cam = (IMX219_Config*)val;

  cam->finished_time = ttc_clock_now(); // Save the current time
  u32* ctrlreg = (u32*)cam->baseaddr;
  ctrlreg[1] = 0xe; // Ack interrupts
}

void imx219_cam_init(IMX219_Config* config)
{
  // Initialize the config struct
  config->inflight_sof[0] = config->inflight_sof[1] = 0;
  config->inflight_duration[0] = config->inflight_duration[1] = 0;
  config->inflight_isdummy[0] = config->inflight_isdummy[1] = true;

  // Set up the CSI receiver
  // Leave output disabled for now
  *(u32*)(config->baseaddr + 0x10) = 1232; // Coming off the image sensor TODO: make configurable

  // Set up the receive interrupt handler
  connect_interrupt(config->irq, csi_irq_handler, (void*)config);

  // Set up the camera clock
  i2c_set_mux(config->i2c_channel);
  // TODO: use a flag to determine whether we do this (our boards) or not (RasPi cameras)
  //si514_init();

  // Now write the camera registers
/* TODO: put these back, although I don't think they matter.
  u8 result[2];
  read_registers(0x0000, 2, result); // ID (should be 0x0219)
  usleep(1000);

  read_registers(0x0002, 1, result); // Who knows??
  usleep(1000);
*/

  // Set all the configuration registers
  // The sequence is terminated with an arry of all zeros
  for(int i = 0; byte_strings[i][0] > 0; i++){
    uint8_t* tx_bytes = byte_strings[i];
    int tx_len = tx_bytes[0]; // First byte is the length
    i2c_write(I2C_CAM_ADDR, tx_bytes+1, tx_len);
  }

  imx219_cam_set_gain(config, 10); // HACK
}

/* Sets the CSI receiver to capture frames */
void imx219_cam_run(IMX219_Config* config)
{
  u32* csirx = (u32*)config->baseaddr;

  csirx[0] |= CSIRX_CFG_RUN_CONTINUOUS | CSIRX_CFG_ENABLE_INTERRUPTS | CSIRX_CFG_ENABLE_SOF_IRQ;
  csirx[1] = CSIRX_CTRL_RUN | CSIRX_CTRL_ACK_ALL_IRQ; // Start running
}

void imx219_cam_set_exposure(IMX219_Config* config, u16 lines){
  // Set the COARSE_INTEGRATION_TIME register 0x015A
  uint8_t regvals[4] = {0x01,0x5A,0x00,0x00};

  regvals[2] = lines >> 8;
  regvals[3] = lines & 0xff;

  i2c_set_mux(config->i2c_channel);
  i2c_write(I2C_CAM_ADDR, regvals, 4);
}

// Set the gain value between x1 and x160 (16000 ISO)
void imx219_cam_set_gain(IMX219_Config* config, float gain)
{
  // Use as much analog gain as possible, then use digital to make up the difference
  float analog_gain;
  if(gain < 10.667){
    analog_gain = gain;
  }
  else{
    analog_gain = 10.667;
  }

  // Analog gain = 256 / (256 - x), where X has a max value of 232 (gain of 10.6 ~= 20dB)
  // X = 256 - (256 / gain)
  u8 x = 256.5f - (256.0f / analog_gain); // Plus 0.5 for rounding
  float true_analog_gain = 256.0f / (256.0f - x);

  u16 digital_gain = gain / true_analog_gain * 256;
  u8 upper = (digital_gain >> 8) & 0x0f; // Only 4 bits for integer part
  u8 lower = digital_gain & 0xff;

  // Set all the gains at once
  // 0x157 [analog gain] [digital upper] [digital lower]
  uint8_t regvals[5] = {0x01,0x57, x, upper, lower};

  printf("gain: %f  analog: %f  digital: %u\n", gain, true_analog_gain, digital_gain);
  printf("set gain: %d %d %d\n", x, upper, lower);

  i2c_set_mux(config->i2c_channel);
  i2c_write(I2C_CAM_ADDR, regvals, 5);
}

Time imx219_min_frame_time(IMX219_Config* config)
{
  Time t = 33327;
  return t;
}

// This function is called each time we have a new frame
void imx219_handle_requests(IMX219_Config* config)
{

  if(!config->inflight_isdummy[0]){
    // Pass the information on the received frame back to the host
    config->inflight_request[0].time = config->finished_time; // TODO: calculate SoF time here
    masterqueue_push(config->inflight_request);
#ifdef VERBOSE
    printf("[cam %d] received frame at %llu (scheduled at %llu,  %+lld)\n", config->i2c_channel,
         config->finished_time, config->inflight_sof[0], ttc_clock_diff(config->finished_time, config->inflight_sof[0]));
  }
  else{
    printf("[cam %d] rx dummy at %llu (expected at %llu,  %+lld)\n", config->i2c_channel,
         config->finished_time, config->inflight_sof[0], ttc_clock_diff(config->finished_time, config->inflight_sof[0]));
#endif
  }
  // We just began to receive frame N; frame N+1 is already locked in (and
  // possibly exposing), so we'll be setting frame N+2.

  // Default values; we'll override them if we actually need a frame
  u32 exposure_lines = 4; // Length of the next exposure, in terms of image rows
  bool isDummy = true; // Whether this is a dummy (hidden) frame

  // Look at the next request
  ZynqRequest req = requestqueue_peek(config->reqId);
  if(req.device != NO_DEVICE){

    // Time in the request is the beginning of the exposure; calculate the
    // SOF time, which is what we can actually track.
    Time sof_target = req.time + req.camParams.exposure; // TODO: tune this more precisely

    // Time from SOF of previous frame to SOF of this request
    Time interframe = req.camParams.exposure + IMX219_BLANKING;
    if(interframe < imx219_min_frame_time(config)){
      interframe = imx219_min_frame_time(config);
    }

    // Calculate the slack
    s64 slack = ttc_clock_diff((config->inflight_sof[1] + interframe), sof_target);

    // If the request is too late (slack < 0), just run it right away
    // If we've hit the time when the request should be scheduled, or if the
    // request does not have enough slack to be scheduled properly, just
    // schedule it right away. We might hit the timepoint more accurately if
    // we put in another dummy frame, but it is safer to schedule ASAP and
    // not push the schedule later, possibly bumping other things.
    if(slack < (s64)imx219_min_frame_time(config)){
      exposure_lines = req.camParams.exposure / IMX219_LINETIME;
      if(exposure_lines < 1){
        exposure_lines = 1;
      }
      isDummy = false;
      requestqueue_pop(config->reqId); // We're done with this Request

      printf("%lld,", sof_target);
    }

    // If we can insert more than one dummy frame (slack > 2*min_frame_time),
    // then insert a minimal dummy frame.
    // Otherwise, calculate the required dummy frame exposure and insert it.
    else if(slack < 2*imx219_min_frame_time(config)){
      exposure_lines = (slack - IMX219_BLANKING) / IMX219_LINETIME + 0.5f; // + 1/2 line for rounding
    }
//    printf("[cam %d] sof target: %lld, slack: %lld  lines: %lu  dummy: %u\n", config->i2c_channel, sof_target, slack, exposure_lines, isDummy);
  } // END if(camera has a request)

  // Configure the camera and CSI receiver with the calculated parameters
  imx219_cam_set_exposure(config, exposure_lines);

  // Store the important values into the in-flight queue
  config->inflight_sof[0] = config->inflight_sof[1];
  config->inflight_duration[0] = config->inflight_duration[1];
  config->inflight_isdummy[0] = config->inflight_isdummy[1];
  config->inflight_request[0] = config->inflight_request[1];

  if(exposure_lines > 1759){ // (imx219_min_frame_time - IMX219_BLANKING) / IMX219_LINETIME
    config->inflight_duration[1] = (exposure_lines * IMX219_LINETIME) + IMX219_BLANKING;
  }
  else{
    config->inflight_duration[1] = imx219_min_frame_time(config);
  }
  config->inflight_sof[1] = config->finished_time + config->inflight_duration[0] + config->inflight_duration[1];
  config->inflight_isdummy[1] = isDummy;

  // CSI receiver has be configured for frame N+1, not N+2
  if(config->inflight_isdummy[0]){
    *(u32*)(config->baseaddr) &= ~(CSIRX_CFG_OUTPUT_ENABLE); // Disable output (clear "enable" bit)
  }
  else{
    *(u32*)(config->baseaddr) |= CSIRX_CFG_OUTPUT_ENABLE;
  }

  // TODO: set the request parameters based on what was actually achieved
  config->inflight_request[1] = req;

  // Grab the next frame from the CSI (frame N+1)
  // Frame N+1 had its settings locked in a while ago, and is exposing
  imx219_cam_run(config);
}
