/* i2c.c
 * Board-wide I2C configuration and wrapper routines for Xilinx I2C functions
 */

#include "i2c.h"



unsigned int i2c_write(u8 addr, u8 data[], unsigned int len)
{
  return XIic_Send(I2C_BASEADDR, addr, data, len, XIIC_STOP);
}


/* Set the value of the mux to a particular channel
 * If the mux is already set to the requested channel, does nothing.
 */
void i2c_set_mux(u8 channel)
{
  static u8 current_channel = 0; // Startup value of the mux
  if(channel != current_channel){
    u8 muxval = 0b00000100 | channel;
    i2c_write(I2C_MUX_ADDR, &muxval, 1);
    current_channel = channel;
  }
}

