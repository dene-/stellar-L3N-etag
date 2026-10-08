#include <string.h>
#include "application/screen.h"
#include "application/device_settings.h"
#include "application/display.h"
#include "application/telemetry.h"
#include "application/local_time.h"
#include "application/ports/wall_clock.h"
#include "application/ports/image_storage.h"
#include "domain/battery.h"
#include "domain/calendar.h"
#include "domain/clock_schedule.h"
#include "domain/epd_canvas.h"
#include "domain/epd_scenes.h"
#include "domain/slideshow.h"
#include "domain/temperature.h"
#include "sections.h"

// The main loop runs at least once per advertising interval (about a second); a refresh planned to
// end on the minute starts up to this much early instead of late.
#define WAKE_MARGIN_MS 1100

static RAM uint8_t scene = SCREEN_SCENE_DASHBOARD;
// Redraw on the next update; also set at boot, when the panel content is unknown.
static RAM uint8_t redraw_requested = 1;
// Clock scenes: local time of the next frame (a whole minute); 0 = show the current time.
static RAM uint32_t next_frame;
// Sync mode: how long before next_frame its refresh starts, once planned; 0 = not planned yet.
static RAM uint32_t planned_lead_ms;
// The schedule settings changed: plan the next frame again.
static RAM uint8_t replan;
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
    data->temperature_c = (int8_t)temperature_whole_c(display_last_temperature()); // latest, often the last refresh's
    data->battery_mv = battery_mv;
    data->battery_percent = battery_percent(battery_mv);
    data->ble_connected = ble_connected;
    strncpy(data->device_name, device_name, sizeof(data->device_name) - 1);
}

// Draws a clock scene for local time `shown` (0 = clock not set) into the planes.
static void draw_clock_frame(epd_scene_draw_fn draw, uint32_t shown, uint8_t ble_connected, const char *device_name)
{
    const panel_t *panel = display_panel();
    struct date_time time;
    epd_scene_data_t data;
    epd_canvas_t canvas;

    if (shown)
        time = calendar_date(shown);
    else
        memset(&time, 0, sizeof(time));
    build_scene_data(&data, time, ble_connected, device_name);
    epd_canvas_init(&canvas, display_plane(DISPLAY_PLANE_BLACK), display_plane(DISPLAY_PLANE_RED), panel->width,
                    panel->height, panel->has_red);
    draw(&canvas, &data);
}

// Shows a new clock frame every clock interval (domain/clock_schedule.h), or when requested; the
// panel refreshes only if the frame changed. In sync mode the next frame is drawn ahead of its time
// and its refresh starts early by as long as that kind of refresh last took, so the panel shows the
// new time as the minute changes.
static void update_clock_scene(epd_scene_draw_fn draw, uint8_t ble_connected, const char *device_name)
{
    uint8_t minutes = device_settings_clock_interval();
    uint8_t fast = device_settings_fast_refresh_enabled();
    uint16_t ms;
    uint32_t now;
    int32_t remaining_ms;

    if (display_is_refreshing())
        return;
    now = local_time_seconds(&ms);
    if (replan && now && next_frame)
    {
        next_frame = clock_schedule_next(now, minutes);
        planned_lead_ms = 0;
    }
    replan = 0;

    // The current time, at once: when asked to, when the clock was first set, and when it jumped
    // (synced far off, time zone change) past the next frame or well before it.
    if (redraw_requested ||
        (now && (!next_frame || now >= next_frame + 60 || next_frame > now + minutes * 60u + 60)))
    {
        draw_clock_frame(draw, now, ble_connected, device_name);
        display_refresh_if_changed(redraw_requested, fast);
        redraw_requested = 0;
        next_frame = now ? clock_schedule_next(now, minutes) : 0;
        planned_lead_ms = 0;
        return;
    }
    if (!now)
        return; // without a clock, only requested redraws

    remaining_ms = (int32_t)(next_frame - now) * 1000 - ms;
    if (device_settings_clock_sync())
    {
        if (!planned_lead_ms)
        {
            refresh_kind_t kind;

            // Once even a full refresh would have to start, find out which kind the frame needs.
            if (remaining_ms > (int32_t)(display_refresh_duration_ms(REFRESH_FULL) + WAKE_MARGIN_MS))
                return;
            draw_clock_frame(draw, next_frame, ble_connected, device_name);
            kind = display_plan_refresh(0, fast);
            if (kind == REFRESH_SKIP)
            {
                next_frame = clock_schedule_next(next_frame, minutes);
                return;
            }
            planned_lead_ms = display_refresh_duration_ms(kind) + WAKE_MARGIN_MS;
        }
        if (remaining_ms > (int32_t)planned_lead_ms)
            return;
    }
    else if (remaining_ms > 0)
        return;

    draw_clock_frame(draw, next_frame, ble_connected, device_name);
    display_refresh_if_changed(0, fast);
    next_frame = clock_schedule_next(next_frame, minutes);
    planned_lead_ms = 0;
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

void screen_clock_schedule_changed(void)
{
    replan = 1;
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
