#pragma once
#include <stdint.h>

// Heartbeat blink every STATUS_LED_HEARTBEAT_SECONDS (blue while a phone is connected, green
// otherwise) and the optional rainbow animation, both only while LED flashing is enabled in the settings.
#define STATUS_LED_HEARTBEAT_SECONDS 10

void status_led_set_rainbow(uint8_t enabled);
void status_led_update(uint8_t ble_connected);
