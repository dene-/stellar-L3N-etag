#pragma once
#include <stdint.h>
#include "epd_canvas.h"
#include "etime.h"

// Everything a clock scene shows. Scenes only draw; they read no hardware state, so the same
// code renders on the tag and in the host preview tool (tools/scene_preview).
typedef struct
{
    struct date_time time;
    uint8_t time_valid; // 0 until the time has been set over BLE
    int8_t temperature_c;
    uint16_t battery_mv;
    uint8_t battery_percent;
    uint8_t ble_connected;
    char device_name[12];
} epd_scene_data_t;

typedef void (*epd_scene_draw_fn)(epd_canvas_t *c, const epd_scene_data_t *data);

// Scene 1: large time with a status line and a date band.
void epd_scene_draw_clock(epd_canvas_t *c, const epd_scene_data_t *data);
// Scene 2: header with tag name and status, time, temperature/battery panel and date band.
void epd_scene_draw_dashboard(epd_canvas_t *c, const epd_scene_data_t *data);
