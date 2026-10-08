#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "infrastructure/wall_clock.h"
#include "domain/clock_calibration.h"
#include "sections.h"

typedef char nominal_ticks_check[(WALL_CLOCK_NOMINAL_TICKS == CLOCK_16M_SYS_TIMER_CLK_1S) ? 1 : -1];

static RAM uint32_t ticks_per_second;
static RAM uint32_t last_second_tick;
static RAM uint32_t uptime_seconds;
static RAM uint32_t utc_seconds;

_attribute_ram_code_ void wall_clock_init(void)
{
    ticks_per_second = WALL_CLOCK_NOMINAL_TICKS + CLOCK_TRIM_DEFAULT;
    last_second_tick = clock_time();
    utc_seconds = 0;
}

_attribute_ram_code_ void wall_clock_tick(void)
{
    // One second per call: a main loop held up for several seconds catches up over the next passes.
    if (clock_time() - last_second_tick >= ticks_per_second)
    {
        last_second_tick += ticks_per_second;
        uptime_seconds++;
        if (utc_seconds)
            utc_seconds++;
    }
}

_attribute_ram_code_ uint32_t wall_clock_utc(uint16_t *ms)
{
    if (ms)
    {
        uint32_t elapsed_ms = (clock_time() - last_second_tick) / (ticks_per_second / 1000);

        *ms = utc_seconds ? (elapsed_ms > 999 ? 999 : elapsed_ms) : 0; // a second not yet counted
    }
    return utc_seconds;
}

_attribute_ram_code_ void wall_clock_set_utc(uint32_t seconds, uint16_t ms)
{
    utc_seconds = seconds;
    // Shifts the uptime seconds by the same fraction; they stay one per second.
    last_second_tick = clock_time() - ms * (ticks_per_second / 1000);
}

_attribute_ram_code_ void wall_clock_set_trim(int16_t trim)
{
    ticks_per_second = WALL_CLOCK_NOMINAL_TICKS + trim;
}

_attribute_ram_code_ uint32_t wall_clock_uptime_seconds(void)
{
    return uptime_seconds;
}

_attribute_ram_code_ uint32_t wall_clock_uptime_ms(void)
{
    // Not clamped to the current second: seconds the main loop has yet to count are included.
    return uptime_seconds * 1000 + (clock_time() - last_second_tick) / (ticks_per_second / 1000);
}
