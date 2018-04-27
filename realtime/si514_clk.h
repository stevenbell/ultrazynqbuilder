/* si514_clk.h
 * Control routines for Silicon Labs Si514 programmable oscillator, which
 * is used as a reconfigurable clock for the camera.
 */

#ifndef SI514_CLK_H
#define SI514_CLK_H

#include "xil_types.h"

#define SI514_IIC_ADDR 0x55

// Set up the clock to produce a 24MHz signal
void si514_init();


#endif /* SI514_CLK_H */
