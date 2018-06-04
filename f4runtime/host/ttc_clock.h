#ifndef TTC_CLOCK_H
#define TTC_CLOCK_H

typedef uint64_t Time;

// We don't control the clock, the realtime sets it up and it just runs
#define TTC_REG_BASE 0xFF110000
#define TTC_COUNT1 6 // 0x18, 7th 32-bit register
#define TTC_COUNT2 7 // 0x1c, 8th 32-bit register

#define TICKS_PER_US 100 // 100MHz = 100 ticks per microsecond

void ttc_clock_init(void);
Time ttc_clock_now(void);
int64_t ttc_clock_diff(Time first, Time second);

#endif /* TTC_CLOCK_H */
