#include "domain/period.h"

uint8_t period_elapsed(period_t *period, uint32_t now, uint32_t seconds)
{
    if (period->started && now - period->last < seconds)
        return 0;

    period->started = 1;
    period->last = now;
    return 1;
}
