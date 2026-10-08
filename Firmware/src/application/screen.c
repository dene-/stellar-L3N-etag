#include <string.h>
#include "application/screen.h"
#include "application/display.h"
#include "application/telemetry.h"
#include "application/ports/wall_clock.h"
#include "application/ports/image_storage.h"
#include "domain/battery.h"
#include "domain/epd_canvas.h"
#include "domain/epd_scenes.h"
#include "domain/slideshow.h"
#include "sections.h"

static RAM uint8_t scene = SCREEN_SCENE_DASHBOARD;
// The first refresh after boot/OTA must be a full one: the panel RAM is blank and a partial
// waveform draws nothing useful on it.
static RAM uint8_t redraw_requested = 1;
static RAM uint8_t drawn_minute = 100;
static RAM slideshow_t slideshow;

static uint8_t is_image_scene(uint8_t id)
{
    return id == SCREEN_SCENE_IMAGE || id == SCREEN_SCENE_SLIDESHOW;
}

static void show_stored_image(uint8_t index)
{
    uint16_t size = image_store_get_plane_size();

    image_store_load_image(index, display_plane(DISPLAY_PLANE_BLACK), display_plane(DISPLAY_PLANE_RED), size);
    display_refresh(size, 1);
}

static void build_scene_data(epd_scene_data_t *data, struct date_time time, uint8_t ble_connected, const char *device_name)
{
    uint16_t battery_mv = telemetry_battery_mv();

    memset(data, 0, sizeof(*data));
    data->time = time;
    data->time_valid = (time.tm_year != 0);
    data->temperature_c = telemetry_temperature_c();
    data->battery_mv = battery_mv;
    data->battery_percent = battery_percent(battery_mv);
    data->ble_connected = ble_connected;
    strncpy(data->device_name, device_name, sizeof(data->device_name) - 1);
}

// Redraws a clock scene once a minute (or when requested); the panel refreshes only if the frame changed.
static void update_clock_scene(epd_scene_draw_fn draw, uint8_t ble_connected, const char *device_name)
{
    struct date_time time = wall_clock_date();
    const panel_t *panel;
    epd_scene_data_t data;
    epd_canvas_t canvas;

    if (display_is_refreshing())
        return;
    if (!redraw_requested && time.tm_min == drawn_minute)
        return;
    drawn_minute = time.tm_min;

    panel = display_panel();
    build_scene_data(&data, time, ble_connected, device_name);
    epd_canvas_init(&canvas, display_plane(DISPLAY_PLANE_BLACK), display_plane(DISPLAY_PLANE_RED), panel->width,
                    panel->height, panel->has_red);
    draw(&canvas, &data);
    display_refresh_if_changed(redraw_requested);
    redraw_requested = 0;
}

static void update_slideshow(void)
{
    uint8_t count = image_store_get_image_count();
    uint32_t now = wall_clock_unix_time();

    if (!count || !image_store_get_plane_size())
        return;
    if (image_store_take_display_pending())
    {
        slideshow_restart(&slideshow, now);
        show_stored_image(slideshow.index);
        return;
    }
    if (!display_is_refreshing() && slideshow_advance(&slideshow, now, image_store_get_interval_seconds(), count))
        show_stored_image(slideshow.index);
}

void screen_set_scene(uint8_t new_scene)
{
    // Entering an image scene from a clock scene: clear the panel first so the controller's
    // old-frame RAM doesn't ghost the clock into the image.
    if (is_image_scene(new_scene) && !is_image_scene(scene) && !display_is_refreshing())
    {
        display_fill(0xFF, 0x00);
        display_refresh(panel_plane_bytes(display_panel()), 1);
    }
    scene = new_scene;
    screen_request_redraw();
}

void screen_request_redraw(void)
{
    redraw_requested = 1;
}

void screen_select_panel(uint8_t model)
{
    display_select_model(model);
    screen_request_redraw();
}

void screen_update(uint8_t ble_connected, const char *device_name)
{
    switch (scene)
    {
    case SCREEN_SCENE_IMAGE:
        if (image_store_has_images() && image_store_take_display_pending())
            show_stored_image(0);
        break;
    case SCREEN_SCENE_CLOCK:
        update_clock_scene(epd_scene_draw_clock, ble_connected, device_name);
        break;
    case SCREEN_SCENE_DASHBOARD:
        update_clock_scene(epd_scene_draw_dashboard, ble_connected, device_name);
        break;
    case SCREEN_SCENE_SLIDESHOW:
        if (image_store_has_images())
            update_slideshow();
        break;
    default:
        break;
    }
}
