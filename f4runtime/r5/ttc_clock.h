/* ttc_clock.c
 * Utilities for using one of the four "triple-timer-clocks" as a high-precision system clock.
 */

#ifndef TTC_CLOCK_H
#define TTC_CLOCK_H

#include "xil_types.h"

typedef u64 Time; // for now, might become a struct later

// Definitions for TTC1
// We only use two of the three counters, and don't touch the clock settings
// See https://www.xilinx.com/html_docs/registers/ug1087/mod___ttc.html for details
#define TTC_CLK1   0xFF110000
#define TTC_CTRL1  0xFF11000C
#define TTC_COUNT1 0xFF110018

#define TTC_CLK2   0xFF110004
#define TTC_CTRL2  0xFF110010
#define TTC_COUNT2 0xFF11001C

#define TICKS_PER_US 100 // 100MHz = 100 ticks per microsecond

void ttc_clock_init(void);
Time ttc_clock_now();
s64 ttc_clock_diff(Time first, Time second);

#endif /* TTC_CLOCK_H */
