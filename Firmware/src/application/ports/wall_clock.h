#pragma once
#include <stdint.h>

// Wall clock (infrastructure/wall_clock): UTC counted on the system timer.
#define WALL_CLOCK_NOMINAL_TICKS 16000000 // system timer ticks per second

// Seconds since boot; unlike the UTC time it never jumps when the phone sets the clock.
// Use it for intervals and timeouts.
uint32_t wall_clock_uptime_seconds(void);
// Milliseconds since boot, wrapping after 49 days; for measuring short durations.
uint32_t wall_clock_uptime_ms(void);
// UTC unix time; 0 until set. ms (may be NULL) receives the milliseconds into that second.
uint32_t wall_clock_utc(uint16_t *ms);
void wall_clock_set_utc(uint32_t seconds, uint16_t ms);
// Ticks per second beyond WALL_CLOCK_NOMINAL_TICKS (domain/clock_calibration.h).
void wall_clock_set_trim(int16_t trim);
