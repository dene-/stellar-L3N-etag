#pragma once
#include <stdint.h>
#include "domain/panel.h"
#include "domain/refresh_policy.h"

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

// Shows the first size bytes (at most the panel's plane size) of both planes.
void display_refresh(uint16_t size, uint8_t full);
// Fills the black plane with pattern and shows it without red.
void display_show_pattern(uint8_t pattern);
// Shows the current frame as decided by domain/refresh_policy (redraw: even if unchanged; fast:
// fast refresh mode); returns 0 if it was skipped.
uint8_t display_refresh_if_changed(uint8_t redraw, uint8_t fast);
// None of the three starts a refresh while the battery is too low (application/power.h): the call
// does nothing, the panel keeps its image, and display_take_deferred_refresh reports it later.
// Returns 1 once, when a refresh was refused and refreshes are allowed again; the caller should
// redraw then.
uint8_t display_take_deferred_refresh(void);
// The refresh display_refresh_if_changed would do for the current frame, without doing it.
refresh_kind_t display_plan_refresh(uint8_t redraw, uint8_t fast);
// How long a refresh of kind (partial or full) takes on this panel, from starting it until the
// panel is idle: the last one measured, or the default until there is one.
#define DISPLAY_PARTIAL_REFRESH_DEFAULT_MS 3000
#define DISPLAY_FULL_REFRESH_DEFAULT_MS 20000
uint32_t display_refresh_duration_ms(refresh_kind_t kind);

uint8_t display_is_refreshing(void);
// Powers the panel down once a refresh has finished, or after DISPLAY_REFRESH_TIMEOUT seconds if
// the panel never reports idle; returns whether one is still running.
#define DISPLAY_REFRESH_TIMEOUT 60
uint8_t display_poll(void);

// Panel temperature in tenths of a degree C, re-measured once it is max_age seconds old and never
// during a refresh (reading resets the controller). Every refresh also measures it.
int16_t display_read_temperature(uint32_t max_age);
// Last measured value, without touching the panel.
int16_t display_last_temperature(void);
