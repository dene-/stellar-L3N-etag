#pragma once
#include <stdint.h>

// BLE stack setup, advertising and the connection.
void ble_init(void);
uint8_t ble_is_connected(void);
// Advertised name, see domain/device_name.h.
const char *ble_device_name(void);
// Requests a connection interval of speed * 1.25 ms (short for bulk transfers, long to save power).
void ble_set_connection_speed(uint16_t speed);
