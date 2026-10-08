#pragma once
#include <stdint.h>
#include "domain/calendar.h"

// Wall clock (infrastructure/wall_clock).
uint32_t wall_clock_unix_time(void);
struct date_time wall_clock_date(void);
// Seconds since boot; unlike the unix time it never jumps when the phone sets the clock.
// Use it for intervals and timeouts.
uint32_t wall_clock_uptime_seconds(void);
