#pragma once
#include <stdint.h>

// Charge estimate for the coin cell: linear from 2.2 V (0 %) to 3.1 V (100 %).
uint8_t battery_percent(uint16_t battery_mv);
