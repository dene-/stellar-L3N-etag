#pragma once
#include <stdint.h>

// "THX_" followed by the last three bytes of the public MAC address in hex, e.g. "THX_3AF8AC".
#define DEVICE_NAME_LENGTH 10

// Writes the name and a terminating NUL into name (DEVICE_NAME_LENGTH + 1 bytes).
// mac is in the BLE stack's order (least significant byte first).
void device_name_format(const uint8_t mac[6], char *name);
