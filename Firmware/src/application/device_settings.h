#pragma once
#include <stdint.h>

typedef struct
{
    uint8_t panel_model;          // PANEL_MODEL_*, PANEL_MODEL_AUTO = detect
    uint8_t fast_refresh_enabled; // every panel update is a partial refresh
    uint8_t led_flashing_enabled; // status LED heartbeat and animation allowed
} device_settings_t;

// Loads the stored settings, or stores the defaults when there are none.
void device_settings_load(void);
// Restores and stores the defaults.
void device_settings_reset(void);
void device_settings_save(void);
// Stores the settings if a setter changed them since the last save (called on BLE disconnect).
void device_settings_save_if_changed(void);

uint8_t device_settings_panel_model(void);
void device_settings_set_panel_model(uint8_t model);
uint8_t device_settings_fast_refresh_enabled(void);
void device_settings_set_fast_refresh_enabled(uint8_t enabled);
uint8_t device_settings_led_flashing_enabled(void);
void device_settings_set_led_flashing_enabled(uint8_t enabled);
