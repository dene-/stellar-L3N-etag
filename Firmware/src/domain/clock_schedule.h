#pragma once
#include <stdint.h>

// When the clock scenes show a new time: every interval minutes, counted from local midnight (every
// 5 minutes: :00, :05, :10, ...). An interval that does not divide a day ends the day early.
#define CLOCK_SCHEDULE_DEFAULT_MINUTES 1
#define CLOCK_SCHEDULE_MAX_MINUTES 60

// Whether minutes is a valid interval (1 to CLOCK_SCHEDULE_MAX_MINUTES).
uint8_t clock_schedule_valid(uint8_t minutes);
// Local time (seconds since 1970, on a whole minute) of the first frame after local_seconds.
uint32_t clock_schedule_next(uint32_t local_seconds, uint8_t minutes);
