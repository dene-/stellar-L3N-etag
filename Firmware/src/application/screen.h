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
// Redraws the current scene with a full refresh on the next update.
void screen_request_redraw(void);
// Switches the panel model (see display_select_model) and redraws for the new resolution.
void screen_select_panel(uint8_t model);
// Main loop step: redraws the clock scenes when the minute changes, advances the slideshow and shows
// newly uploaded images.
void screen_update(uint8_t ble_connected, const char *device_name);
