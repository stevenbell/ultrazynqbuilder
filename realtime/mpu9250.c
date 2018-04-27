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


  // Test the IMU
  /*
  while(1){
    s16 accX = mpu_read_reg16(MPU_ACC_X);
    s16 accY = mpu_read_reg16(MPU_ACC_Y);
    s16 accZ = mpu_read_reg16(MPU_ACC_Z);

    s16 gyroX = mpu_read_reg16(MPU_GYRO_X);
    s16 gyroY = mpu_read_reg16(MPU_GYRO_Y);
    s16 gyroZ = mpu_read_reg16(MPU_GYRO_Z);

    printf("Acc X %5d  Y %5d  Z %5d    Gyro X %5d  Y %5d  Z %5d\n",
           accX, accY, accZ, gyroX, gyroY, gyroZ);
  }
  */


