#include "domain/clock_schedule.h"

#define MINUTES_PER_DAY 1440u

uint8_t clock_schedule_valid(uint8_t minutes)
{
    return minutes >= 1 && minutes <= CLOCK_SCHEDULE_MAX_MINUTES;
}

uint32_t clock_schedule_next(uint32_t local_seconds, uint8_t minutes)
{
    uint32_t minute = local_seconds / 60 + 1; // first whole minute after local_seconds
    uint32_t of_day = minute % MINUTES_PER_DAY;

    if (!clock_schedule_valid(minutes))
        minutes = CLOCK_SCHEDULE_DEFAULT_MINUTES;
    if (of_day % minutes)
    {
        uint32_t step = minutes - of_day % minutes;

        // The last frame of the day is followed by midnight when the interval does not divide the day.
        if (of_day + step > MINUTES_PER_DAY)
            step = MINUTES_PER_DAY - of_day;
        minute += step;
    }
    return minute * 60;
}
