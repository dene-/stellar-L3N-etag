#include "domain/battery.h"

uint8_t battery_percent(uint16_t battery_mv)
{
    uint16_t percent;

    if (battery_mv < 2200)
        return 0;
    percent = (battery_mv - 2200) / 9;
    return percent > 100 ? 100 : (uint8_t)percent;
}
