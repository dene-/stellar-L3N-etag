#include <string.h>
#include "application/screen.h"
#include "application/device_settings.h"
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
// Redraw on the next update; also set at boot, when the panel content is unknown.
static RAM uint8_t redraw_requested = 1;
static RAM uint8_t drawn_minute = 100;
static RAM slideshow_t slideshow;

// Images are always shown with a full refresh: a partial one cannot draw red.
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
    display_refresh_if_changed(redraw_requested, device_settings_fast_refresh_enabled());
    redraw_requested = 0;
}

// Images stored for a panel of another size are not shown on this one.
static uint8_t stored_images_fit(void)
{
    return image_store_has_images() && image_store_get_plane_size() == panel_plane_bytes(display_panel());
}

static void update_image(void)
{
    uint8_t pending;

    if (display_is_refreshing() || !stored_images_fit())
        return;
    pending = image_store_take_display_pending();
    if (pending || redraw_requested)
    {
        redraw_requested = 0;
        show_stored_image(0);
    }
}

static void update_slideshow(void)
{
    uint8_t count = image_store_get_image_count();
    uint32_t now = wall_clock_uptime_seconds();

    if (display_is_refreshing() || !stored_images_fit())
        return;
    if (image_store_take_display_pending() || slideshow.index >= count)
    {
        slideshow_restart(&slideshow, now);
        redraw_requested = 1;
    }
    if (redraw_requested || slideshow_advance(&slideshow, now, image_store_get_interval_seconds(), count))
    {
        redraw_requested = 0;
        show_stored_image(slideshow.index);
    }
}

void screen_set_scene(uint8_t new_scene)
{
    scene = new_scene;
    screen_request_redraw();
}

void screen_hold_frame(void)
{
    scene = SCREEN_SCENE_IMAGE;
    redraw_requested = 0;
    image_store_take_display_pending();
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
        update_image();
        break;
    case SCREEN_SCENE_CLOCK:
        update_clock_scene(epd_scene_draw_clock, ble_connected, device_name);
        break;
    case SCREEN_SCENE_DASHBOARD:
        update_clock_scene(epd_scene_draw_dashboard, ble_connected, device_name);
        break;
    case SCREEN_SCENE_SLIDESHOW:
        update_slideshow();
        break;
    default:
        break;
    }
}
