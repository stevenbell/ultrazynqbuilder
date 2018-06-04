/* imx219_cam.h
 * Control code for IMX219, as well as the CSI receiver.
 */
#ifndef IMX219_H
#define IMX219_H

#include "xil_types.h"

#define I2C_CAM_ADDR 0x10
#define I2C_FOCUS_ADDR 0x0c

/* CSIRX register map:
 * 0x00: Config register (not to be confused with control register)
 * 0x04: Atomic set for control register - writing 1 to a bit sets the
 *       corresponding bit in the control register.
 * 0x08: Atomic clear for control register - writing 1 to a bit clears the
 *       corresponding bit in the control register.
 * 0x0c: Status register
 * 0x10: Frame length - Used to send TLAST at the appropriate time.  Should
 *       match the number of lines coming in from the camera sensor (i.e., the
 *       height of the image).
 */

// Config register flags
#define CSIRX_CFG_RUN_CONTINUOUS    0x01
#define CSIRX_CFG_ENABLE_INTERRUPTS 0x02
#define CSIRX_CFG_ENABLE_SOF_IRQ    0x04
#define CSIRX_CFG_ENABLE_EOF_IRQ    0x08
#define CSIRX_CFG_OUTPUT_DISABLE    0x10

// Atomic register flags
#define CSIRX_CTRL_RUN         0x01
#define CSIRX_CTRL_ACK_ALL_IRQ 0x02
#define CSIRX_CTRL_ACK_SOF_IRQ 0x04
#define CSIRX_CTRL_ACK_EOF_IRQ 0x08

typedef struct {
  unsigned int i2c_channel; // I2C mux channel, 0-3)
  u32 baseaddr; // CSI receiver base address
  unsigned int irq; // CSI receiver IRQ
  Time finished_time; // Time of last interrupt
} IMX219_Config;

// Blanking time in microseconds
// This is actually a function of the image settings
#define IMX219_BLANKING 80

void set_focus(u16 raw_value);
void imx219_cam_init(IMX219_Config* config);
void imx219_cam_run(IMX219_Config* config);
void imx219_cam_set_exposure(IMX219_Config* config, u16 lines);
Time imx219_min_frame_time(IMX219_Config* config);

#endif
