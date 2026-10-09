#pragma once
#include <stdint.h>
#include "application/device_settings.h"

// Persistent settings (infrastructure/storage/settings_flash). Saving never erases the only copy
// of the settings: a power cut during a save leaves the previous settings in place.
#define SETTINGS_STORAGE_NONE 0   // nothing valid stored
#define SETTINGS_STORAGE_CURRENT 1
#define SETTINGS_STORAGE_LEGACY 2 // the record of firmware before scene and slideshow interval were stored

// Fills `settings` with the stored values as they are (not range-checked); one of SETTINGS_STORAGE_*.
// For the legacy record, scene and slideshow_interval are left as they were in `settings`.
uint8_t settings_storage_load(device_settings_t *settings);
void settings_storage_save(const device_settings_t *settings);
