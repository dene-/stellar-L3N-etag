#pragma once
#include <stdint.h>

// Sensor readings, sampled every TELEMETRY_CONNECTED_INTERVAL seconds while a phone is connected
// and every TELEMETRY_IDLE_INTERVAL seconds otherwise, and published over BLE.
#define TELEMETRY_CONNECTED_INTERVAL 30
#define TELEMETRY_IDLE_INTERVAL 300

void telemetry_update(uint8_t ble_connected);
uint16_t telemetry_battery_mv(void);
int8_t telemetry_temperature_c(void);
