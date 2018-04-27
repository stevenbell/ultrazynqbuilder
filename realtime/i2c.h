/* i2c.h
 * Board-wide I2C configuration and wrapper routines for Xilinx I2C functions
 */

#include "xiic.h"
#include "xparameters.h"

#define IIC_DEVICE_ID	XPAR_CAMERA_IIC_DEVICE_ID
#define I2C_BASEADDR	XPAR_CAMERA_IIC_BASEADDR
#define IIC_BASEADDR	XPAR_CAMERA_IIC_BASEADDR

#define I2C_MUX_ADDR	0x70 // 0b1110000 [R/W]

void i2c_set_mux(u8 channel);

