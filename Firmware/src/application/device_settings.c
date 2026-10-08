#include "application/device_settings.h"
#include "application/ports/settings_storage.h"
#include "domain/panel.h"
#include "sections.h"

static RAM device_settings_t settings;
static RAM uint8_t settings_changed;

static void apply_defaults(void)
{
    settings.panel_model = PANEL_MODEL_AUTO;
    settings.fast_refresh_enabled = 0;
    settings.led_flashing_enabled = 1;
}

void device_settings_load(void)
{
    if (settings_storage_load(&settings))
        return;
    apply_defaults();
    device_settings_save();
}

void device_settings_reset(void)
{
    apply_defaults();
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
