#pragma once
#include <stdint.h>

// Sensor readings, sampled every TELEMETRY_CONNECTED_INTERVAL seconds while a phone is connected
// and every TELEMETRY_IDLE_INTERVAL seconds otherwise, and published over BLE. The temperature is
// re-measured at the same interval unless a refresh measured it more recently. A sample that falls
// due during a panel refresh is taken as soon as the refresh has ended.
#define TELEMETRY_CONNECTED_INTERVAL 30
#define TELEMETRY_IDLE_INTERVAL 300

void telemetry_update(uint8_t ble_connected);
uint16_t telemetry_battery_mv(void);
int16_t telemetry_temperature_x10(void);
