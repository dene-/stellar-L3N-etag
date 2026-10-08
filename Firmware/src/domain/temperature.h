#pragma once
#include <stdint.h>

// Temperatures are carried in tenths of a degree C (x10).

// From the 1/256 degree fixed point the panel controllers report, rounded to the nearest tenth.
int16_t temperature_x10_from_x256(int16_t x256);
// Whole degrees, rounded to the nearest (halves away from zero).
int16_t temperature_whole_c(int16_t x10);
