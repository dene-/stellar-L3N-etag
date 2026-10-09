#include "application/power.h"
#include "application/display.h"
#include "application/ports/battery_sensor.h"
#include "domain/battery.h"
#include "sections.h"

static RAM uint16_t battery_mv;
static RAM uint8_t refresh_paused;

uint8_t power_flash_write_allowed(void)
{
    // During a refresh the supply sags and, on boards where the battery pin is also the panel's DC
    // line, sampling would reconfigure that pin before the panel is put to sleep: use the last idle
    // sample instead.
    if (display_is_refreshing())
        return battery_flash_write_ok(battery_mv);
    return battery_flash_write_ok(battery_sensor_read_mv());
}

void power_sample_battery(void)
{
    uint16_t mv = battery_sensor_read_mv();

    if (!battery_reading_plausible(mv))
        return;
    battery_mv = mv;
    refresh_paused = !battery_refresh_ok(mv, refresh_paused);
}

uint16_t power_battery_mv(void)
{
    return battery_mv;
}

uint8_t power_refresh_allowed(void)
{
    return !refresh_paused;
}
