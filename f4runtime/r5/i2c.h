/* i2c.h
 * Board-wide I2C configuration and wrapper routines for Xilinx I2C functions
 */

#ifndef I2C_H
#define I2C_H

#include "xiic.h"
#include "xparameters.h"

#define IIC_DEVICE_ID	XPAR_CAMERA_IIC_DEVICE_ID
#define I2C_BASEADDR	XPAR_CAMERA_IIC_BASEADDR
#define IIC_BASEADDR	XPAR_CAMERA_IIC_BASEADDR

#define I2C_MUX_ADDR	0x70 // 0b1110000 [R/W]

int i2c_init(void);
unsigned int i2c_write(u8 addr, u8 data[], unsigned int len);
void i2c_set_mux(u8 channel);

#endif
