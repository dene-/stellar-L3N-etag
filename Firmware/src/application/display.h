#pragma once
#include <stdint.h>
#include "domain/panel.h"

// The panel session: which panel is fitted, the frame planes drawn for it, refreshes and the
// controller's temperature sensor.

enum
{
    DISPLAY_PLANE_BLACK = 0, // 1 = white, 0 = black
    DISPLAY_PLANE_RED = 1,   // 1 = red
};

// Boot: uses the stored panel model; PANEL_MODEL_AUTO (or an unknown id) detects it on first use.
void display_init(uint8_t model);
// Switches to model (PANEL_MODEL_AUTO = detect again) and stores it in the settings.
void display_select_model(uint8_t model);
// The fitted panel; detects the controller family first when the model is AUTO.
const panel_t *display_panel(void);

// Plane buffers, PANEL_MAX_PLANE_BYTES each, laid out as described in domain/epd_canvas.h.
uint8_t *display_plane(uint8_t plane);
// Fills the whole black and red buffers.
void display_fill(uint8_t black, uint8_t red);
// Copies data into a plane of the current frame; 0 if it would run past the panel's plane size.
uint8_t display_write(uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length);

// Shows the first size bytes of both planes.
void display_refresh(uint16_t size, uint8_t full);
// Fills the black plane with pattern and shows it without red.
void display_show_pattern(uint8_t pattern);
// Shows the current frame as decided by domain/refresh_policy; returns 0 if it was unchanged.
uint8_t display_refresh_if_changed(uint8_t force_full);

uint8_t display_is_refreshing(void);
// Powers the panel down once a refresh has finished; returns whether one is still running.
uint8_t display_poll(void);

// Panel temperature in degrees C, re-measured at most every DISPLAY_TEMPERATURE_MAX_AGE seconds.
#define DISPLAY_TEMPERATURE_MAX_AGE 300
int8_t display_read_temperature(void);
// Last measured value, without touching the panel.
int8_t display_last_temperature(void);
