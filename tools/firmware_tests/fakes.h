#pragma once
// Controllable, inspectable fakes for every port in application/ports/. State is plain globals;
// call fakes_reset() first thing in main() to set the defaults noted below.
#include <stdint.h>
#include "application/device_settings.h"

// wall_clock (default: UTC 0 = not set, uptime 0, trim 0). Intervals and timeouts run on
// fake_uptime; fake_utc only feeds the local time. fake_clock_trim is the last trim set.
extern uint32_t fake_utc;
extern uint16_t fake_utc_ms;
extern uint32_t fake_uptime;
extern int16_t fake_clock_trim;

// epd_panel (default: detects PANEL_MODEL_BWR296, busy, temperature 200 = 20.0 C)
extern uint8_t fake_detect_model;
extern int16_t fake_panel_temperature; // x10, returned by read_temperature and refresh
extern uint8_t fake_panel_idle;       // returned by epd_panel_is_idle
extern int fake_detect_calls;
extern int fake_read_temperature_calls;
extern int fake_refresh_calls;
extern int fake_sleep_calls;
extern uint8_t fake_sleep_model; // model of the last sleep
// Arguments of the last refresh.
extern uint8_t fake_refresh_model;
extern uint16_t fake_refresh_size;
extern uint8_t fake_refresh_full;
extern int fake_refresh_red_null;
extern uint8_t fake_refresh_black0; // first byte of the black plane handed to the panel

// image_storage: an in-memory store. load_image fills black with FAKE_IMAGE_BLACK_BASE + index and
// red with FAKE_IMAGE_RED_BASE + index.
#define FAKE_IMAGE_BLACK_BASE 0xA0
#define FAKE_IMAGE_RED_BASE 0xB0
extern int fake_store_prepare_calls;
extern uint8_t fake_store_model;
extern uint16_t fake_store_width;
extern uint16_t fake_store_height;
extern uint16_t fake_store_plane_size;
extern uint16_t fake_store_interval;
extern uint8_t fake_store_count;
extern int fake_store_clear_calls;
extern int fake_store_load_calls;
extern uint8_t fake_store_loaded_index; // image of the last load_image

// settings_storage (default: nothing valid stored)
extern uint8_t fake_settings_valid;
extern device_settings_t fake_settings_stored; // what load returns / the last save wrote
extern int fake_settings_save_calls;

// battery_sensor (default 3000)
extern uint16_t fake_battery_mv;

// telemetry_sink
extern int fake_publish_calls;
extern int16_t fake_publish_temperature_x10;
extern uint8_t fake_publish_percent;
extern uint16_t fake_publish_mv;

// status_light
extern int fake_light_off_calls;
extern int fake_light_blink_calls;
extern int fake_light_last_color; // -1 before any blink
extern uint8_t fake_light_rainbow;
extern int fake_light_animate_calls;

void fakes_reset(void);
