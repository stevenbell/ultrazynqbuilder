/* ttc_clock.c
 * Utilities for using the "triple-time-clock" as a system-wide,
 * hardware-independent clock.
 */

#include "ttc_clock.h"

inline void ttc_clk_init(void)
{
  // Enable counter 1 as our clock
  // Runs at 100MHz by default (rolls over after 42 seconds)
  *(u32*)TTC_CTRL = 0x00; // Start and disable interrupt-on-match
}

inline u32 ttc_clock_now()
{
  return *(u32*)TTC_COUNT;
}

/* Return the difference between two timestamps.
 * @param first  The first (presumably earlier) timestamp
 * @param second  The second timestamp
 * If the difference is greater than half the rollover period, then the clock
 * is assumed to have rolled over.
 */
inline s32 ttc_clock_diff(u32 first, u32 second)
{
  // Integer overflow actually handles the wrap for us, as long as we treat
  // the result as a signed value.
  s32 difference = second - first;
  return(difference);
}



