/* imx219_cam.h
 * Control code for IMX219, as well as the CSI receiver.
 */
#ifndef IMX219_H
#define IMX219_H

#include "xil_types.h"

#define I2C_CAM_ADDR 0x10
#define I2C_FOCUS_ADDR 0x0c

#define CSIRX_CTRL_RUN_CONTINUOUS    0x01
#define CSIRX_CTRL_ENABLE_INTERRUPTS 0x02
#define CSIRX_CTRL_ENABLE_SOF_IRQ    0x04
#define CSIRX_CTRL_ENABLE_EOF_IRQ    0x08

typedef struct {
  unsigned int i2c_channel; // I2C mux channel, 0-3)
  u32 baseaddr; // CSI receiver base address
  unsigned int irq; // CSI receiver IRQ
  u32 finished_time; // Time of last interrupt
} IMX219_Config;


void set_focus(u16 raw_value);
void imx219_cam_init(IMX219_Config* config);
void imx219_cam_run(IMX219_Config* config);
void imx219_cam_set_exposure(IMX219_Config* config, u16 lines);

#endif
