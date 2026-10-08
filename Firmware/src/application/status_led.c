#include "application/status_led.h"
#include "application/device_settings.h"
#include "application/ports/wall_clock.h"
#include "application/ports/status_light.h"
#include "domain/period.h"
#include "sections.h"

static RAM period_t heartbeat_period;

void status_led_set_rainbow(uint8_t enabled)
{
    status_light_set_rainbow(enabled);
}

void status_led_update(uint8_t ble_connected)
{
    if (!device_settings_led_flashing_enabled())
    {
        status_light_off();
        return;
    }

    status_light_animate();
    if (period_elapsed(&heartbeat_period, wall_clock_uptime_seconds(), STATUS_LED_HEARTBEAT_SECONDS))
        status_light_blink(ble_connected ? STATUS_LIGHT_BLUE : STATUS_LIGHT_GREEN);
}
