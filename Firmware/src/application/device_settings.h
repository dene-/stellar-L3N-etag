#pragma once
#include <stdint.h>

typedef struct
{
    uint8_t panel_model;          // PANEL_MODEL_*, PANEL_MODEL_AUTO = detect
    uint8_t fast_refresh_enabled; // clock scenes skip the optional full refreshes, see domain/refresh_policy.h
    uint8_t led_flashing_enabled; // status LED heartbeat and animation allowed
    int16_t clock_trim;           // wall clock correction, see domain/clock_calibration.h
    uint8_t clock_interval;       // minutes between clock scene frames, see domain/clock_schedule.h
    uint8_t clock_sync;           // start clock refreshes early so they finish as the minute changes
    uint8_t scene;                // SCREEN_SCENE_* shown at boot, DEVICE_SETTINGS_SCENE_UNSET if never stored
    uint16_t slideshow_interval;  // seconds between slideshow images, 0 = SLIDESHOW_DEFAULT_INTERVAL_SECONDS
} device_settings_t;

#define DEVICE_SETTINGS_SCENE_UNSET 0xFF

// Loads the stored settings, or stores the defaults when there are none. Stored values out of range
// are replaced by their defaults, settings the stored record predates keep their defaults; the
// slideshow interval of such a record comes from the stored images. The changes are saved by
// device_settings_save_if_changed().
void device_settings_load(void);
// Restores and stores the defaults.
void device_settings_reset(void);
void device_settings_save(void);
// Stores the settings if a setter changed them since the last save. The main loop calls this when
// no BLE connection is open, outside a panel refresh.
void device_settings_save_if_changed(void);

uint8_t device_settings_panel_model(void);
void device_settings_set_panel_model(uint8_t model);
uint8_t device_settings_fast_refresh_enabled(void);
void device_settings_set_fast_refresh_enabled(uint8_t enabled);
uint8_t device_settings_led_flashing_enabled(void);
void device_settings_set_led_flashing_enabled(uint8_t enabled);
int16_t device_settings_clock_trim(void);
void device_settings_set_clock_trim(int16_t trim);
uint8_t device_settings_clock_interval(void);
// Ignores intervals clock_schedule_valid() rejects.
void device_settings_set_clock_interval(uint8_t minutes);
uint8_t device_settings_clock_sync(void);
void device_settings_set_clock_sync(uint8_t enabled);
// The scene to show at boot (DEVICE_SETTINGS_SCENE_UNSET if none was stored). Stored with the others.
uint8_t device_settings_scene(void);
void device_settings_set_scene(uint8_t scene);
uint16_t device_settings_slideshow_interval(void);
void device_settings_set_slideshow_interval(uint16_t seconds);
