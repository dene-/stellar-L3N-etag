#pragma once
#include <stdint.h>
#include "application/device_settings.h"

// Persistent settings record (infrastructure/storage/settings_flash).
// Returns 0 when nothing valid is stored.
uint8_t settings_storage_load(device_settings_t *settings);
void settings_storage_save(const device_settings_t *settings);
