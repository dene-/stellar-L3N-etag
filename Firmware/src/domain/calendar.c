#include "domain/calendar.h"

#define SECONDS_PER_DAY 86400

struct date_time calendar_date(uint32_t seconds)
{
    struct date_time date;
    uint32_t days = seconds / SECONDS_PER_DAY;
    uint32_t time_of_day = seconds % SECONDS_PER_DAY;
    // Days to a civil date (Howard Hinnant's algorithm), counting eras of 400 years from 0000-03-01
    // so the leap day ends each year.
    uint32_t z = days + 719468;
    uint32_t era = z / 146097;
    uint32_t day_of_era = z - era * 146097;
    uint32_t year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    uint32_t day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    uint32_t month_from_march = (5 * day_of_year + 2) / 153;

    date.tm_day = (int)(day_of_year - (153 * month_from_march + 2) / 5 + 1);
    date.tm_month = (int)(month_from_march < 10 ? month_from_march + 3 : month_from_march - 9);
    date.tm_year = (int)(year_of_era + era * 400 + (date.tm_month <= 2 ? 1 : 0));
    date.tm_week = (int)((days + 4) % 7); // 1970-01-01 was a Thursday
    date.tm_hour = (int)(time_of_day / 3600);
    date.tm_min = (int)(time_of_day / 60 % 60);
    date.tm_sec = (int)(time_of_day % 60);
    return date;
}
