// local_time: syncing the clock and time zone, and the drift calibration between syncs.
#include "application/device_settings.h"
#include "application/local_time.h"
#include "check.h"
#include "domain/clock_calibration.h"
#include "fakes.h"

#define SYNC_UTC 1791467127u // 2026-10-08 13:45:27 UTC
#define CET_STARTS 1792890000u // 2026-10-25 01:00 UTC
#define SIX_HOURS 21600u

static const time_zone_t berlin = {120, 1, {{CET_STARTS, 60}}};

static void test_local_date_follows_zone(void)
{
    struct date_time date = local_time_date();

    CHECK_EQ(date.tm_year, 0); // not set yet

    local_time_sync(SYNC_UTC, 0, &berlin);
    CHECK_EQ(fake_utc, SYNC_UTC);
    date = local_time_date();
    CHECK_EQ(date.tm_year, 2026);
    CHECK_EQ(date.tm_hour, 15);
    CHECK_EQ(date.tm_min, 45);

    // The clock goes back an hour at the change the phone sent.
    fake_utc = CET_STARTS - 1;
    CHECK_EQ(local_time_date().tm_hour, 2);
    CHECK_EQ(local_time_date().tm_day, 25);
    fake_utc = CET_STARTS;
    CHECK_EQ(local_time_date().tm_hour, 2);
    CHECK_EQ(local_time_date().tm_min, 0);
}

static void test_drift_is_calibrated_between_syncs(void)
{
    int16_t trim;

    CHECK_EQ(device_settings_clock_trim(), CLOCK_TRIM_DEFAULT);
    local_time_sync(SYNC_UTC, 0, &berlin);

    // Less than six hours later: too short to measure, only the time is set.
    fake_utc = SYNC_UTC + SIX_HOURS - 10 + 1;
    local_time_sync(SYNC_UTC + SIX_HOURS - 10, 0, &berlin);
    CHECK_EQ(device_settings_clock_trim(), CLOCK_TRIM_DEFAULT);
    CHECK_EQ(fake_utc, SYNC_UTC + SIX_HOURS - 10);

    // Six hours after that, the clock is 2.16 s ahead: it runs 100 ppm fast.
    fake_utc = SYNC_UTC + 2 * SIX_HOURS - 10 + 2;
    fake_utc_ms = 160;
    local_time_sync(SYNC_UTC + 2 * SIX_HOURS - 10, 0, &berlin);
    trim = device_settings_clock_trim();
    CHECK(trim >= 6599 && trim <= 6601);
    CHECK_EQ(fake_clock_trim, trim);
    CHECK_EQ(fake_utc, SYNC_UTC + 2 * SIX_HOURS - 10);

    // A phone whose time is behind the last sync: nothing to measure.
    fake_utc = SYNC_UTC + 3 * SIX_HOURS;
    local_time_sync(SYNC_UTC, 0, &berlin);
    CHECK_EQ(device_settings_clock_trim(), trim);
}

int main(void)
{
    fakes_reset();
    device_settings_load();
    local_time_init();
    CHECK_EQ(fake_clock_trim, CLOCK_TRIM_DEFAULT);

    test_local_date_follows_zone();
    test_drift_is_calibrated_between_syncs();
    return check_report();
}
