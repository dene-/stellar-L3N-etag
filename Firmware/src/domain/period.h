#pragma once
#include <stdint.h>

// A recurring deadline in clock seconds; zero-initialised it fires on the first check.
typedef struct
{
    uint8_t started;
    uint32_t last;
} period_t;

// Returns 1 (and restarts the period) on the first call and whenever seconds have passed since then.
uint8_t period_elapsed(period_t *period, uint32_t now, uint32_t seconds);
