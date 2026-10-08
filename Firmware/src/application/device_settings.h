#pragma once
#include <stdint.h>

typedef struct
{
    uint8_t panel_model;          // PANEL_MODEL_*, PANEL_MODEL_AUTO = detect
    uint8_t fast_refresh_enabled; // clock scenes skip the optional full refreshes, see domain/refresh_policy.h
    uint8_t led_flashing_enabled; // status LED heartbeat and animation allowed
    int16_t clock_trim;           // wall clock correction, see domain/clock_calibration.h
} device_settings_t;

// Loads the stored settings, or stores the defaults when there are none. Settings the stored record
// predates keep their defaults.
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
int16_t device_settings_clock_trim(void);
void device_settings_set_clock_trim(int16_t trim);
