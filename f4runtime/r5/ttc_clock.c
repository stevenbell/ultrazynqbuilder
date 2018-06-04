/* ttc_clock.c
 * Utilities for using the "triple-time-clock" as a system-wide,
 * hardware-independent clock.
 */

#include "ttc_clock.h"

inline void ttc_clock_init(void)
{
  // Disable and reset both counters
  *(u32*)TTC_CTRL1 = 0x11;
  *(u32*)TTC_CTRL2 = 0x11;

  // Enable counter 1 as our coarse clock
  // Runs at 100MHz / 2^16 ~= 1525 Hz (rolls over after 32 days)
  *(u32*)TTC_CLK1 = 0x1f; // Divide by 2^16, enable prescalar
  *(u32*)TTC_CTRL1 = 0x00; // Start and disable interrupt-on-match

  // Enable counter 2 as our high-precision clock
  // Runs at 100MHz by default (rolls over after 42 seconds)
  *(u32*)TTC_CTRL2 = 0x00; // Start and disable interrupt-on-match
}

// Returns the current Time in microseconds
// This currently takes 67 clock ticks, or 0.67us.
// Most of this is reading the TTC registers (~0.30us each)
Time ttc_clock_now()
{
  // Because our counters are 32 bits (and so is our processor), we have to be
  // careful about reading them together.
  // As long as the difference is small, we can just keep the fine bits
  // But when one or the other rolls beyond the 32-bit mark (every 42 seconds), we need to compensate
  u32 fine = *(u32*)TTC_COUNT2;
  u32 coarse = *(u32*)TTC_COUNT1;

  // Whichever bit is 0 is the clock that has rolled over
  u64 t = coarse;
  if(coarse >> 15 & 0x01){ // Coarse bit is 1
    if(fine >> 31 & 0x01){ // Fine bit is 1
      t = ((t & 0xffff0000) << 16) + fine; // Bits match, just combine the clocks
    }
    else{
      t = ((t & 0xffff0000) << 16) + (1 << 16) + fine; // Fine rolled over, need to increment coarse
    }
  }
  else{ // Coarse bit is 0
    if(fine >> 31 & 0x01){ // Fine bit is 1
      t = ((t & 0xffff0000) << 16) - (1 << 16) + fine; // Coarse rolled over; need to decrement coarse
    }
    else{
      t = ((t & 0xffff0000) << 16) + fine; // Bits match, just combine the clocks
    }
  }

  return t / TICKS_PER_US;
}

/* Return the difference between two timestamps.
 * @param first  The first (presumably earlier) timestamp
 * @param second  The second timestamp
 * If the difference is greater than half the rollover period, then the clock
 * is assumed to have rolled over.
 */
inline s64 ttc_clock_diff(Time first, Time second)
{
  // Integer overflow actually handles the wrap for us, as long as we treat
  // the result as a signed value.
  s64 difference = second - first;
  return(difference);
}



