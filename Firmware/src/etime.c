#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "drivers/8258/flash.h"
#include "etime.h"
#include "main.h"

RAM uint16_t time_trime = 5000; // The higher the number the slower the time runs!, -32,768 to 32,767
RAM uint32_t one_second_trimmed = CLOCK_16M_SYS_TIMER_CLK_1S;
RAM uint32_t current_unix_time;
RAM struct date_time current_date = {0};

RAM uint32_t last_clock_increase;
RAM uint32_t last_reached_period[10] = {0};
RAM uint8_t has_ever_reached[10] = {0};
RAM uint32_t next_midnight_unix = 0;

static uint8_t is_leap_year(uint16_t year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static uint8_t days_in_month(uint8_t month, uint16_t year)
{
    uint8_t map[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year))
        return 29;
    if (month >= 1 && month <= 12)
        return map[month - 1];
    return 30;
}

_attribute_ram_code_ void init_time(void)
{
    one_second_trimmed += time_trime;
    current_unix_time = 0;
}

_attribute_ram_code_ void handler_time(void)
{
    if (clock_time() - last_clock_increase >= one_second_trimmed)
    {
        last_clock_increase += one_second_trimmed;
        current_unix_time++;

        current_date.tm_min = (current_unix_time / 60) % 60;
        current_date.tm_hour = ((current_unix_time / 60) / 60) % 24;
        current_date.tm_sec = current_unix_time % 60;

        if (next_midnight_unix > 0 && current_unix_time >= next_midnight_unix)
        {
            next_midnight_unix += 86400;

            if (current_date.tm_day + 1 > days_in_month(current_date.tm_month, current_date.tm_year))
            {
                current_date.tm_day = 1;
                if (current_date.tm_month + 1 > 12)
                {
                    current_date.tm_month = 1;
                    current_date.tm_year += 1;
                }
                else
                {
                    current_date.tm_month += 1;
                }
            }
            else
            {
                current_date.tm_day = current_date.tm_day + 1;
            }

            current_date.tm_week = (current_date.tm_week + 1) % 7;
        }
    }
}

_attribute_ram_code_ uint8_t time_reached_period(timer_channel ch, uint32_t seconds)
{
    if (!has_ever_reached[ch])
    {
        has_ever_reached[ch] = 1;
        last_reached_period[ch] = current_unix_time;
        return 1;
    }
    if (current_unix_time - last_reached_period[ch] >= seconds)
    {
        last_reached_period[ch] = current_unix_time;
        return 1;
    }
    return 0;
}

_attribute_ram_code_ void set_time(uint32_t time_now, uint16_t time_year, uint8_t time_month, uint8_t time_day, uint8_t time_week)
{
    current_unix_time = time_now;
    current_date.tm_year = time_year;
    current_date.tm_month = time_month;
    current_date.tm_day = time_day;
    current_date.tm_week = time_week;

    // Compute next midnight: align to the next 86400 boundary from time_now
    uint32_t seconds_into_day = time_now % 86400;
    next_midnight_unix = time_now + (86400 - seconds_into_day);
}

_attribute_ram_code_ uint32_t get_unix_time(void)
{
    return current_unix_time;
}

_attribute_ram_code_ struct date_time get_time(void)
{
    return current_date;
}