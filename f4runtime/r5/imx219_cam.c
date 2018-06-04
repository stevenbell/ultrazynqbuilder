/* imx219_cam.c
 * Control code for IMX219, as well as the CSI receiver.
 */

#include "i2c.h"
#include "si514_clk.h"
#include "ttc_clock.h"
#include "platform.h"
#include "imx219_cam.h"
#include "initcommands.h"


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

/* Set the focus position of the lens
 * @param raw_value: a 10-bit number which translates to the lens position
 */
void set_focus(u16 raw_value)
{
  // VCM controller has only two configuration bytes
  // [ Power down | FLAG | data (10 bits) | slope control (2 bits) | step period (2 bits) ]
  u8 control[2];
  control[0] = (raw_value >> 4) & 0x3F; // Leave PD and FLAG as 0, write 6/10 bits
  control[1] = (raw_value << 4) & 0xF0; // Bottom 4/10 bits, leave control bits 0
  i2c_write(I2C_FOCUS_ADDR, control, 2);
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
  // Set up the receive interrupt handler
  connect_interrupt(config->irq, csi_irq_handler, (void*)config);

  // Set up the camera clock
  i2c_set_mux(config->i2c_channel);
  si514_init();

  // Now write the camera registers
  u8 result[2];
  read_registers(0x0000, 2, result); // ID (should be 0x0219)
  usleep(1000);

  read_registers(0x0002, 1, result); // Who knows??
  usleep(1000);

  // Set all the configuration registers
  // The sequence is terminated with an arry of all zeros
  for(int i = 0; byte_strings[i][0] > 0; i++){
    uint8_t* tx_bytes = byte_strings[i];
    int tx_len = tx_bytes[0]; // First byte is the length
    XIic_Send(I2C_BASEADDR, I2C_CAM_ADDR, tx_bytes+1, tx_len, XIIC_STOP);
  }
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
  XIic_Send(IIC_BASEADDR, I2C_CAM_ADDR, regvals, 4, XIIC_STOP);
}

Time imx219_min_frame_time(IMX219_Config* config)
{
  Time t = 33327;
  return t;
}
