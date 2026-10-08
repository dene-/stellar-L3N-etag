#include "application/telemetry.h"
#include "application/display.h"
#include "application/ports/battery_sensor.h"
#include "application/ports/wall_clock.h"
#include "application/ports/telemetry_sink.h"
#include "domain/battery.h"
#include "domain/period.h"
#include "sections.h"

static RAM period_t sample_period;
static RAM uint16_t battery_mv;
static RAM int8_t temperature_c;

void telemetry_update(uint8_t ble_connected)
{
    uint32_t interval = ble_connected ? TELEMETRY_CONNECTED_INTERVAL : TELEMETRY_IDLE_INTERVAL;

    if (!period_elapsed(&sample_period, wall_clock_uptime_seconds(), interval))
        return;

    battery_mv = battery_sensor_read_mv();
    temperature_c = display_read_temperature(); // the panel controller has the only temperature sensor
    telemetry_sink_publish(temperature_c * 10, battery_percent(battery_mv), battery_mv);
}

uint16_t telemetry_battery_mv(void)
{
    return battery_mv;
}

int8_t telemetry_temperature_c(void)
{
    return temperature_c;
}
