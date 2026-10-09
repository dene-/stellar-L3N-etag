#include "domain/battery.h"

// Light-load discharge curve of a CR2032 lithium coin cell, as used by Nordic's nRF5 SDK
// (battery_level_in_percent): the voltage stays near 2.9 V for most of the cell's life, then drops.
static const struct
{
    uint16_t mv;
    uint8_t percent;
} curve[] = {{3000, 100}, {2900, 42}, {2740, 18}, {2440, 6}, {2100, 0}};

uint8_t battery_percent(uint16_t battery_mv)
{
    unsigned int i;

    if (battery_mv >= curve[0].mv)
        return 100;
    for (i = 1; i < sizeof(curve) / sizeof(curve[0]); i++)
    {
        if (battery_mv > curve[i].mv)
            return curve[i].percent + (uint32_t)(curve[i - 1].percent - curve[i].percent) * (battery_mv - curve[i].mv) /
                                          (curve[i - 1].mv - curve[i].mv);
    }
    return 0;
}

uint8_t battery_reading_plausible(uint16_t battery_mv)
{
    return battery_mv >= BATTERY_PLAUSIBLE_MIN_MV && battery_mv <= BATTERY_PLAUSIBLE_MAX_MV;
}

uint8_t battery_flash_write_ok(uint16_t battery_mv)
{
    return !battery_reading_plausible(battery_mv) || battery_mv >= BATTERY_FLASH_MIN_MV;
}

uint8_t battery_refresh_ok(uint16_t battery_mv, uint8_t paused)
{
    if (!battery_reading_plausible(battery_mv))
        return !paused;
    return paused ? battery_mv >= BATTERY_REFRESH_RESUME_MV : battery_mv >= BATTERY_REFRESH_MIN_MV;
}
