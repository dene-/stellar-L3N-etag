#pragma once
#include <stdint.h>

// Where sensor readings go (ble/: advertising data and the battery/temperature characteristics).
void telemetry_sink_publish(int16_t temperature_x10, uint8_t battery_percent, uint16_t battery_mv);
