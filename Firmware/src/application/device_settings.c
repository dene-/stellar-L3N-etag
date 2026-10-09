#include "application/device_settings.h"
#include "application/screen.h"
#include "application/ports/image_storage.h"
#include "application/ports/settings_storage.h"
#include "domain/panel.h"
#include "domain/clock_calibration.h"
#include "domain/clock_schedule.h"
#include "sections.h"

static RAM device_settings_t settings;
static RAM uint8_t settings_changed;

static void apply_defaults(void)
{
    settings.panel_model = PANEL_MODEL_AUTO;
    settings.fast_refresh_enabled = 0;
    settings.led_flashing_enabled = 1;
    settings.clock_trim = CLOCK_TRIM_DEFAULT;
    settings.clock_interval = CLOCK_SCHEDULE_DEFAULT_MINUTES;
    settings.clock_sync = 0;
    settings.scene = DEVICE_SETTINGS_SCENE_UNSET;
    settings.slideshow_interval = 0;
}

// Puts a field back to its default if the stored value is out of range; 1 if anything was replaced.
static uint8_t sanitize_loaded(void)
{
    uint8_t replaced = 0;

    if (settings.panel_model >= PANEL_MODEL_COUNT)
    {
        settings.panel_model = PANEL_MODEL_AUTO;
        replaced = 1;
    }
    if (settings.fast_refresh_enabled > 1)
    {
        settings.fast_refresh_enabled = 0;
        replaced = 1;
    }
    if (settings.led_flashing_enabled > 1)
    {
        settings.led_flashing_enabled = 1;
        replaced = 1;
    }
    if (settings.clock_trim > CLOCK_TRIM_LIMIT || settings.clock_trim < -CLOCK_TRIM_LIMIT)
    {
        settings.clock_trim = CLOCK_TRIM_DEFAULT;
        replaced = 1;
    }
    if (!clock_schedule_valid(settings.clock_interval))
    {
        settings.clock_interval = CLOCK_SCHEDULE_DEFAULT_MINUTES;
        replaced = 1;
    }
    if (settings.clock_sync > 1)
    {
        settings.clock_sync = 0;
        replaced = 1;
    }
    if (settings.scene >= SCREEN_SCENE_COUNT && settings.scene != DEVICE_SETTINGS_SCENE_UNSET)
    {
        settings.scene = DEVICE_SETTINGS_SCENE_UNSET;
        replaced = 1;
    }
    return replaced;
}

void device_settings_load(void)
{
    uint8_t stored;

    apply_defaults();
    stored = settings_storage_load(&settings);
    if (stored == SETTINGS_STORAGE_NONE)
    {
        device_settings_save();
        return;
    }
    if (sanitize_loaded())
        settings_changed = 1;
    if (stored == SETTINGS_STORAGE_LEGACY)
    {
        // The interval used to live in the image header.
        settings.slideshow_interval = image_store_get_interval_seconds();
        settings_changed = 1;
    }
}

void device_settings_reset(void)
{
    // What is on the panel, and the images' slideshow interval, are not preferences to reset.
    uint8_t scene = settings.scene;
    uint16_t slideshow_interval = settings.slideshow_interval;

    apply_defaults();
    settings.scene = scene;
    settings.slideshow_interval = slideshow_interval;
    device_settings_save();
}

void device_settings_save(void)
{
    settings_changed = 0;
    settings_storage_save(&settings);
}

void device_settings_save_if_changed(void)
{
    if (settings_changed)
        device_settings_save();
}

uint8_t device_settings_panel_model(void)
{
    return settings.panel_model;
}

void device_settings_set_panel_model(uint8_t model)
{
    settings.panel_model = model;
    settings_changed = 1;
}

uint8_t device_settings_fast_refresh_enabled(void)
{
    return settings.fast_refresh_enabled;
}

void device_settings_set_fast_refresh_enabled(uint8_t enabled)
{
    settings.fast_refresh_enabled = enabled ? 1 : 0;
    settings_changed = 1;
}

uint8_t device_settings_led_flashing_enabled(void)
{
    return settings.led_flashing_enabled;
}

void device_settings_set_led_flashing_enabled(uint8_t enabled)
{
    settings.led_flashing_enabled = enabled ? 1 : 0;
    settings_changed = 1;
}

int16_t device_settings_clock_trim(void)
{
    return settings.clock_trim;
}

void device_settings_set_clock_trim(int16_t trim)
{
    settings.clock_trim = trim;
    settings_changed = 1;
}

uint8_t device_settings_clock_interval(void)
{
    return clock_schedule_valid(settings.clock_interval) ? settings.clock_interval : CLOCK_SCHEDULE_DEFAULT_MINUTES;
}

void device_settings_set_clock_interval(uint8_t minutes)
{
    if (!clock_schedule_valid(minutes))
        return;
    settings.clock_interval = minutes;
    settings_changed = 1;
}

uint8_t device_settings_clock_sync(void)
{
    return settings.clock_sync;
}

void device_settings_set_clock_sync(uint8_t enabled)
{
    settings.clock_sync = enabled ? 1 : 0;
    settings_changed = 1;
}

uint8_t device_settings_scene(void)
{
    return settings.scene;
}

void device_settings_set_scene(uint8_t scene)
{
    if (settings.scene == scene)
        return;
    settings.scene = scene;
    settings_changed = 1;
}

uint16_t device_settings_slideshow_interval(void)
{
    return settings.slideshow_interval;
}

void device_settings_set_slideshow_interval(uint16_t seconds)
{
    settings.slideshow_interval = seconds;
    settings_changed = 1;
}
