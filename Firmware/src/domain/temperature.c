#include "domain/temperature.h"

// Division truncates towards zero, so offsetting by half the divisor away from zero rounds.
static int32_t divide_rounded(int32_t value, int32_t divisor)
{
    return (value >= 0 ? value + divisor / 2 : value - divisor / 2) / divisor;
}

int16_t temperature_x10_from_x256(int16_t x256)
{
    return (int16_t)divide_rounded((int32_t)x256 * 10, 256);
}

int16_t temperature_whole_c(int16_t x10)
{
    return (int16_t)divide_rounded(x10, 10);
}
