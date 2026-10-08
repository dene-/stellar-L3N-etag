#pragma once
#include <stdint.h>

// GATT table: GAP, battery, temperature, OTA, RxTx command and raw EPD services.
void gatt_init(void);
void gatt_notify_battery(uint8_t percent);
void gatt_notify_temperature(int16_t temperature_x10);
