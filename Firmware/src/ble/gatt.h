#pragma once
#include <stdint.h>

// GATT table: GAP, battery, temperature, OTA, RxTx command and raw EPD services.
// device_name (DEVICE_NAME_LENGTH chars) becomes the GAP Device Name characteristic.
void gatt_init(const char *device_name);
void gatt_notify_battery(uint8_t percent);
void gatt_notify_temperature(int16_t temperature_x10);
