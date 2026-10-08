#include <string.h>
#include "application/local_time.h"
#include "application/device_settings.h"
#include "application/ports/wall_clock.h"
#include "domain/clock_calibration.h"
#include "sections.h"

// Longest sync interval whose milliseconds fit 32 bits (49 days); longer ones are not measured.
#define MAX_MEASURED_SECONDS (0xFFFFFFFFUL / 1000)

static RAM time_zone_t zone;
static RAM uint32_t synced_seconds; // UTC of the last sync, 0 = none since boot
static RAM uint16_t synced_ms;

void local_time_init(void)
{
    wall_clock_set_trim(device_settings_clock_trim());
}

// Milliseconds from (from_s, from_ms) to (to_s, to_ms); 0 if negative or too long to measure.
static uint32_t elapsed_ms(uint32_t from_s, uint16_t from_ms, uint32_t to_s, uint16_t to_ms)
{
    uint32_t seconds;

    if (to_s < from_s || (to_s == from_s && to_ms < from_ms) || to_s - from_s >= MAX_MEASURED_SECONDS)
        return 0;
    seconds = to_s - from_s;
    return seconds * 1000 + to_ms - from_ms;
}

static void calibrate(uint32_t utc_seconds, uint16_t utc_ms)
{
    uint16_t device_ms;
    uint32_t device_seconds = wall_clock_utc(&device_ms);
    uint32_t true_elapsed = elapsed_ms(synced_seconds, synced_ms, utc_seconds, utc_ms);
    uint32_t device_elapsed = elapsed_ms(synced_seconds, synced_ms, device_seconds, device_ms);
    int16_t trim = device_settings_clock_trim();
    int16_t calibrated;

    if (!true_elapsed || !device_elapsed)
        return;
    calibrated = clock_calibrated_trim(trim, WALL_CLOCK_NOMINAL_TICKS, device_elapsed, true_elapsed);
    if (calibrated == trim)
        return;
    device_settings_set_clock_trim(calibrated);
    wall_clock_set_trim(calibrated);
}

void local_time_sync(uint32_t utc_seconds, uint16_t utc_ms, const time_zone_t *new_zone)
{
    if (synced_seconds)
        calibrate(utc_seconds, utc_ms);
    wall_clock_set_utc(utc_seconds, utc_ms);
    zone = *new_zone;
    synced_seconds = utc_seconds;
    synced_ms = utc_ms;
}

uint32_t local_time_seconds(uint16_t *ms)
{
    uint32_t utc = wall_clock_utc(ms);

    return utc ? utc + (uint32_t)time_zone_offset_seconds(&zone, utc) : 0;
}

struct date_time local_time_date(void)
{
    uint32_t local = local_time_seconds(NULL);
    struct date_time date;

    if (!local)
    {
        memset(&date, 0, sizeof(date));
        return date;
    }
    return calendar_date(local);
}
