#include <string.h>
#include "application/display.h"
#include "application/device_settings.h"
#include "application/ports/wall_clock.h"
#include "application/ports/epd_panel.h"
#include "domain/refresh_policy.h"
#include "sections.h"

static RAM uint8_t model = PANEL_MODEL_AUTO;
static RAM uint8_t refreshing;
static RAM uint32_t refresh_started;
static RAM refresh_policy_t refresh_policy;

static RAM uint8_t temperature_valid;
static RAM int8_t temperature;
static RAM uint32_t temperature_time;

// The black plane survives deep-retention sleep so raw uploads can span connection events; the red
// plane is redrawn or reloaded before every refresh.
static RAM uint8_t black_plane[PANEL_MAX_PLANE_BYTES];
static uint8_t red_plane[PANEL_MAX_PLANE_BYTES];

static void remember_temperature(int8_t value)
{
    temperature = value;
    temperature_valid = 1;
    temperature_time = wall_clock_uptime_seconds();
}

static uint8_t resolved_model(void)
{
    if (model == PANEL_MODEL_AUTO)
        model = epd_panel_detect();
    return model;
}

_attribute_ram_code_ static void end_refresh(void)
{
    epd_panel_sleep(model);
    refreshing = 0;
}

_attribute_ram_code_ static void show(uint8_t *black, uint8_t *red, uint16_t size, uint8_t full)
{
    const panel_t *panel = display_panel();
    uint16_t panel_size = panel_plane_bytes(panel);

    if (size > panel_size)
        size = panel_size;
    // Black/white panels sharing a BWR driver must get a blank red RAM, whatever was drawn.
    if (!panel->has_red)
        red = 0;
    remember_temperature(epd_panel_refresh(panel->model, black, red, size, full));
    refreshing = 1;
    refresh_started = wall_clock_uptime_seconds();
}

void display_init(uint8_t stored_model)
{
    model = panel_find(stored_model) ? stored_model : PANEL_MODEL_AUTO;
}

void display_select_model(uint8_t new_model)
{
    if (refreshing)
        end_refresh(); // the sleep command must reach the controller that is refreshing
    display_init(new_model);
    device_settings_set_panel_model(model);
    refresh_policy_forget(&refresh_policy);
    temperature_valid = 0;
}

const panel_t *display_panel(void)
{
    return panel_find(resolved_model());
}

uint8_t *display_plane(uint8_t plane)
{
    return plane == DISPLAY_PLANE_RED ? red_plane : black_plane;
}

_attribute_ram_code_ void display_fill(uint8_t black, uint8_t red)
{
    memset(black_plane, black, sizeof(black_plane));
    memset(red_plane, red, sizeof(red_plane));
}

uint8_t display_write(uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length)
{
    if ((uint32_t)offset + length > panel_plane_bytes(display_panel()))
        return 0;
    memcpy(display_plane(plane) + offset, data, length);
    return 1;
}

_attribute_ram_code_ void display_refresh(uint16_t size, uint8_t full)
{
    refresh_policy_forget(&refresh_policy);
    show(black_plane, red_plane, size, full);
}

_attribute_ram_code_ void display_show_pattern(uint8_t pattern)
{
    uint16_t size = panel_plane_bytes(display_panel());

    memset(black_plane, pattern, size);
    refresh_policy_forget(&refresh_policy);
    show(black_plane, 0, size, 1);
}

uint8_t display_refresh_if_changed(uint8_t redraw, uint8_t fast)
{
    uint16_t size = panel_plane_bytes(display_panel());
    refresh_kind_t kind = refresh_policy_decide(&refresh_policy, black_plane, red_plane, size, redraw, fast);

    if (kind == REFRESH_SKIP)
        return 0;
    show(black_plane, red_plane, size, kind == REFRESH_FULL);
    return 1;
}

uint8_t display_is_refreshing(void)
{
    return refreshing;
}

_attribute_ram_code_ uint8_t display_poll(void)
{
    // A panel that never reports idle (wrong model selected, loose cable) must not keep the tag
    // awake and block every later update.
    if (refreshing && (epd_panel_is_idle(model) ||
                       wall_clock_uptime_seconds() - refresh_started >= DISPLAY_REFRESH_TIMEOUT))
        end_refresh();
    return refreshing;
}

_attribute_ram_code_ int8_t display_read_temperature(void)
{
    // Reading resets the controller, which would cut a running refresh short.
    if (refreshing ||
        (temperature_valid && wall_clock_uptime_seconds() - temperature_time < DISPLAY_TEMPERATURE_MAX_AGE))
        return temperature;

    remember_temperature(epd_panel_read_temperature(resolved_model()));
    return temperature;
}

int8_t display_last_temperature(void)
{
    return temperature;
}
