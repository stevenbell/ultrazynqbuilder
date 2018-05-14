/* ttc_clock.c
 * Utilities for using one of the four "triple-timer-clocks" as a high-precision system clock.
 */

#ifndef TTC_CLOCK_H
#define TTC_CLOCK_H

#include "xil_types.h"

typedef u32 Time; // for now, might become a struct later

// Definitions for TTC1
// We only use one of the three counters, and don't touch the clock settings
// See https://www.xilinx.com/html_docs/registers/ug1087/mod___ttc.html for details
# define TTC_CTRL  0xFF11000C
# define TTC_COUNT 0xFF110018

#define TICKS_PER_US 100 // 100MHz = 100 ticks per microsecond

void ttc_clk_init(void);
u32 ttc_clock_now();
s32 ttc_clock_diff(u32 first, u32 second);

#endif /* TTC_CLOCK_H */
