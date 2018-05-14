/* mpu9250.h
 * Control routines for Invensense MPU-9250 9-axis IMU
 */

#ifndef MPU9250_H
#define MPU9250_H

#include "i2c.h"

#define I2C_IMU_ADDR	0x68

#define MPU_ACC_X 0x3b
#define MPU_ACC_Y 0x3d
#define MPU_ACC_Z 0x3f

#define MPU_GYRO_X 0x43
#define MPU_GYRO_Y 0x45
#define MPU_GYRO_Z 0x47

s16 mpu_read_reg16(u8 addr);

#endif /* MPU9250_H */
