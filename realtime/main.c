/* main.c
 * Top-level application code for R5 runtime which controls the camera
 * and peripherals.
 * If this is built to run along with Linux, be sure to set the USING_AMP
 * define, or else it'll try to initialize and shutdown the platform (both of
 * which will make you sad).
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xscugic.h"
#include "sleep.h"

#include "i2c.h"
#include "si514_clk.h"
#include "mpu9250.h"
#include "imx219_cam.h"

const u32 GPIO_LEDS = XPAR_AXI_GPIO_0_BASEADDR;
const u32 GPIO_CAMERA = (XPAR_AXI_GPIO_0_BASEADDR + 0x8);

int main()
{
	u32* leds = (u32*)GPIO_LEDS;
	u32* gpio_cam = (u32*)GPIO_CAMERA;

#ifndef USING_AMP
  init_platform();
#endif

  // Blink for 5 seconds
  for(int i = 0; i < 20; i++){
  	*leds = 0xaa;
  	usleep(100000);
  	*leds = 0x55;
  	usleep(100000);
  }

  i2c_set_mux(3);

  // Set up the clock so it's running when the camera wakes up
  si514_init();

  // Enable the cameras
  *leds = 0x01;
  *gpio_cam = 0xff;
  usleep(1000);

  imx219_cam_init();

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

#ifndef USING_AMP
  cleanup_platform();
#endif
    return 0;
}
