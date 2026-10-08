#pragma once
#include <stdint.h>

// A time zone as the phone sends it: the UTC offset when the clock was set, followed by the
// upcoming offset changes (daylight saving time) it knows of, in order.
#define TIME_ZONE_MAX_CHANGES 8

typedef struct
{
    uint32_t at;            // UTC unix time the offset changes
    int16_t offset_minutes; // UTC offset from then on
} time_zone_change_t;

typedef struct
{
    int16_t offset_minutes;
    uint8_t change_count;
    time_zone_change_t changes[TIME_ZONE_MAX_CHANGES];
} time_zone_t;

// UTC offset in seconds at utc.
int32_t time_zone_offset_seconds(const time_zone_t *zone, uint32_t utc);
