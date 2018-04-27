/* imx219_cam.c
 *
 */

#include "imx219_cam.h"
#include "i2c.h"
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

void focus_init(void)
{
    u8 control[2];
    control[0] = 0x3f; // Leave top two bits zero
    control[1] = 0; // Direct drive mode
    XIic_Send(I2C_BASEADDR, I2C_FOCUS_ADDR, control, 2, XIIC_STOP);
}

void imx219_cam_init(void)
{
  u8 result[2];
  read_registers(0x0000, 2, result); // ID (should be 0x0219)
  usleep(1000);

  read_registers(0x0002, 1, result); // Who knows??
  usleep(1000);

  // Set all the configuration registers
  for(int i = 0; i < COMMANDS_LEN; i++){
    uint8_t* tx_bytes = byte_strings[i];
    int tx_len = tx_bytes[0]; // First byte is the length
    XIic_Send(I2C_BASEADDR, I2C_CAM_ADDR, tx_bytes+1, tx_len, XIIC_STOP);
  }
}

