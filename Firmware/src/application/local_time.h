#pragma once
#include <stdint.h>
#include "domain/calendar.h"
#include "domain/time_zone.h"

// Local time: the wall clock's UTC shifted by the phone's time zone, including the daylight saving
// changes it sent. Each sync also measures how far the clock drifted since the previous one (same
// boot, at least CLOCK_CALIBRATION_MIN_INTERVAL_MS apart) and stores the corrected trim.

// Boot: applies the stored clock trim (after device_settings_load).
void local_time_init(void);
void local_time_sync(uint32_t utc_seconds, uint16_t utc_ms, const time_zone_t *zone);
// tm_year is 0 until the first sync.
struct date_time local_time_date(void);
// Local time in seconds since 1970 (the calendar_date() input) and, if ms is not NULL, the
// milliseconds into that second; 0 until the first sync.
uint32_t local_time_seconds(uint16_t *ms);
