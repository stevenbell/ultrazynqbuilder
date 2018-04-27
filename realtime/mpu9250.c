/* mpu9250.c
 * Control routines for Invensense MPU-9250 9-axis IMU
 */

#include "mpu9250.h"

// Read two sequential 8-bit registers and return the 16-bit value
// Assumes that the high byte is in the lower register (first to be read out)
s16 mpu_read_reg16(u8 addr)
{
	// Send the address
	XIic_Send(I2C_BASEADDR, I2C_IMU_ADDR, &addr, 1, XIIC_STOP);

	// Receive the data
	u8 bytes[2];
	XIic_Recv(I2C_BASEADDR, I2C_IMU_ADDR, bytes, 2, XIIC_STOP);

	// Return a 16-bit value
	s16 val = (s16)bytes[0] << 8;
	return(val | bytes[1]);
}




