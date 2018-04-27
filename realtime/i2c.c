/* i2c.c
 * Board-wide I2C configuration and wrapper routines for Xilinx I2C functions
 */

#include "i2c.h"



//void i2c_write()
//{
//	XIic_Send(I2C_BASEADDR, I2C_MUX_ADDR, &muxval, 1, XIIC_STOP);
//}

// Set the value of the mux to a particular channel
void i2c_set_mux(u8 channel)
{
	u8 muxval = 0b00000100 | channel;
	XIic_Send(I2C_BASEADDR, I2C_MUX_ADDR, &muxval, 1, XIIC_STOP);
}


