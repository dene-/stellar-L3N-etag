#pragma once
#include <stdint.h>

struct date_time
{
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_day;
  int tm_month;
  int tm_year; // 0 until the time has been set
  int tm_week; // 0 = Sunday
};

// Calendar date and time of day for a count of seconds since 1970-01-01 00:00 (local time when
// the time zone offset is already added).
struct date_time calendar_date(uint32_t seconds);
