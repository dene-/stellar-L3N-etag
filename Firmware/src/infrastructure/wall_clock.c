#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "infrastructure/wall_clock.h"
#include "sections.h"

#define CLOCK_TRIM 5000 // system timer ticks added per second; higher runs the clock slower

static RAM uint32_t ticks_per_second = CLOCK_16M_SYS_TIMER_CLK_1S;
static RAM uint32_t last_second_tick;
static RAM calendar_t calendar;

_attribute_ram_code_ void wall_clock_init(void)
{
    ticks_per_second += CLOCK_TRIM;
    calendar.unix_time = 0;
}

_attribute_ram_code_ void wall_clock_tick(void)
{
    if (clock_time() - last_second_tick >= ticks_per_second)
    {
        last_second_tick += ticks_per_second;
        calendar_advance_second(&calendar);
    }
}

_attribute_ram_code_ void wall_clock_set(uint32_t unix_time, uint16_t year, uint8_t month, uint8_t day, uint8_t weekday)
{
    calendar_set(&calendar, unix_time, year, month, day, weekday);
}

_attribute_ram_code_ uint32_t wall_clock_unix_time(void)
{
    return calendar.unix_time;
}

_attribute_ram_code_ struct date_time wall_clock_date(void)
{
    return calendar.date;
}
