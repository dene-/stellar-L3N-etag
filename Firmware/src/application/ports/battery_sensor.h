#pragma once
#include <stdint.h>

// Supply voltage measurement (infrastructure/battery). Each call takes a new sample and leaves the
// ADC powered down. Not to be called while the panel is transferring over SPI: on some boards the
// measurement pin is the panel's DC line.
uint16_t battery_sensor_read_mv(void);
