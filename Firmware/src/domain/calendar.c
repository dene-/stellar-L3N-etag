#include "domain/calendar.h"

#define SECONDS_PER_DAY 86400

static uint8_t is_leap_year(int year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static uint8_t days_in_month(int month, int year)
{
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2 && is_leap_year(year))
        return 29;
    if (month >= 1 && month <= 12)
        return days[month - 1];
    return 30;
}

static void update_time_of_day(calendar_t *calendar)
{
    calendar->date.tm_sec = calendar->unix_time % 60;
    calendar->date.tm_min = (calendar->unix_time / 60) % 60;
    calendar->date.tm_hour = (calendar->unix_time / 3600) % 24;
}

static void advance_day(struct date_time *date)
{
    if (date->tm_day + 1 > days_in_month(date->tm_month, date->tm_year))
    {
        date->tm_day = 1;
        if (date->tm_month + 1 > 12)
        {
            date->tm_month = 1;
            date->tm_year += 1;
        }
        else
        {
            date->tm_month += 1;
        }
    }
    else
    {
        date->tm_day += 1;
    }
    date->tm_week = (date->tm_week + 1) % 7;
}

void calendar_set(calendar_t *calendar, uint32_t unix_time, uint16_t year, uint8_t month, uint8_t day, uint8_t weekday)
{
    calendar->unix_time = unix_time;
    calendar->date.tm_year = year;
    calendar->date.tm_month = month;
    calendar->date.tm_day = day;
    calendar->date.tm_week = weekday;
    calendar->next_midnight = unix_time + (SECONDS_PER_DAY - unix_time % SECONDS_PER_DAY);
    update_time_of_day(calendar);
}

void calendar_advance_second(calendar_t *calendar)
{
    calendar->unix_time++;
    update_time_of_day(calendar);

    if (calendar->next_midnight && calendar->unix_time >= calendar->next_midnight)
    {
        calendar->next_midnight += SECONDS_PER_DAY;
        advance_day(&calendar->date);
    }
}
