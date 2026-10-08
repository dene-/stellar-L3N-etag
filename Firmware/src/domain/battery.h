#pragma once
#include <stdint.h>

// Charge estimate from the voltage of a lithium coin cell (CR2032/CR2450 chemistry), following its
// discharge curve rather than a straight line.
uint8_t battery_percent(uint16_t battery_mv);
