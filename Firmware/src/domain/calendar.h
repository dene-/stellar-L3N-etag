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
  int tm_week;
};

// Wall clock kept as unix seconds plus the calendar date the phone sent with them; the date rolls
// over at each midnight (UTC day boundary of the unix time).
typedef struct
{
  uint32_t unix_time;
  uint32_t next_midnight; // 0 until set
  struct date_time date;
} calendar_t;

void calendar_set(calendar_t *calendar, uint32_t unix_time, uint16_t year, uint8_t month, uint8_t day, uint8_t weekday);
void calendar_advance_second(calendar_t *calendar);
