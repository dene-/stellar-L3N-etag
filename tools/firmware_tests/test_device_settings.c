// device_settings: defaults on first boot, loading stored values, save-on-change.
#include "application/device_settings.h"
#include "check.h"
#include "domain/panel.h"
#include "fakes.h"

static void test_invalid_storage_applies_defaults(void)
{
    fakes_reset();
    device_settings_load();

    CHECK_EQ(device_settings_panel_model(), PANEL_MODEL_AUTO);
    CHECK_EQ(device_settings_fast_refresh_enabled(), 0);
    CHECK_EQ(device_settings_led_flashing_enabled(), 1);
    CHECK_EQ(fake_settings_save_calls, 1);
}

static void test_valid_storage_is_loaded_without_saving(void)
{
    fakes_reset();
    fake_settings_valid = 1;
    fake_settings_stored.panel_model = PANEL_MODEL_BWR154;
    fake_settings_stored.fast_refresh_enabled = 1;
    fake_settings_stored.led_flashing_enabled = 0;
    device_settings_load();

    CHECK_EQ(device_settings_panel_model(), PANEL_MODEL_BWR154);
    CHECK_EQ(device_settings_fast_refresh_enabled(), 1);
    CHECK_EQ(device_settings_led_flashing_enabled(), 0);
    CHECK_EQ(fake_settings_save_calls, 0);
}

static void test_save_if_changed(void)
{
    fakes_reset();
    fake_settings_valid = 1;
    fake_settings_stored.led_flashing_enabled = 1;
    device_settings_load();

    device_settings_save_if_changed();
    CHECK_EQ(fake_settings_save_calls, 0);

    device_settings_set_fast_refresh_enabled(1);
    device_settings_save_if_changed();
    CHECK_EQ(fake_settings_save_calls, 1);
    CHECK_EQ(fake_settings_stored.fast_refresh_enabled, 1);

    device_settings_save_if_changed();
    CHECK_EQ(fake_settings_save_calls, 1);
}

static void test_reset_restores_defaults_and_saves(void)
{
    fakes_reset();
    fake_settings_valid = 1;
    fake_settings_stored.panel_model = PANEL_MODEL_BW296;
    fake_settings_stored.fast_refresh_enabled = 1;
    fake_settings_stored.led_flashing_enabled = 0;
    device_settings_load();

    device_settings_reset();

    CHECK_EQ(device_settings_panel_model(), PANEL_MODEL_AUTO);
    CHECK_EQ(device_settings_fast_refresh_enabled(), 0);
    CHECK_EQ(device_settings_led_flashing_enabled(), 1);
    CHECK_EQ(fake_settings_save_calls, 1);
    CHECK_EQ(fake_settings_stored.panel_model, PANEL_MODEL_AUTO);
}

int main(void)
{
    test_invalid_storage_applies_defaults();
    test_valid_storage_is_loaded_without_saving();
    test_save_if_changed();
    test_reset_restores_defaults_and_saves();
    return check_report();
}
