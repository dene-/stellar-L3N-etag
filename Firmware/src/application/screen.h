#pragma once
#include <stdint.h>

// What the panel shows. Scene ids are part of the BLE protocol (command E1 <scene>).
enum
{
    SCREEN_SCENE_IMAGE = 0,     // last uploaded image
    SCREEN_SCENE_CLOCK = 1,     // large clock
    SCREEN_SCENE_DASHBOARD = 2, // clock with tag name, temperature and battery (default)
    SCREEN_SCENE_SLIDESHOW = 3, // cycle through uploaded images
    SCREEN_SCENE_COUNT
};

void screen_set_scene(uint8_t scene);
uint8_t screen_scene(void);
// A raw frame upload (EPD service) took over the panel: stop drawing scenes over it until the
// scene is changed again.
void screen_hold_frame(void);
// Redraws the current scene on the next update, always with a full refresh.
void screen_request_redraw(void);
// The clock interval or sync setting changed: plan the next clock frame again (no redraw now).
void screen_clock_schedule_changed(void);
// Switches the panel model (see display_select_model) and redraws for the new resolution.
void screen_select_panel(uint8_t model);
// Main loop step: shows the clock scenes' next frame when it is due (see device settings
// clock_interval and clock_sync), advances the slideshow and shows newly uploaded images.
void screen_update(uint8_t ble_connected, const char *device_name);
