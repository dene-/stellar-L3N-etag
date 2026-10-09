// device_settings: defaults on first boot, loading stored values, save-on-change.
#include "application/device_settings.h"
#include "application/screen.h"
#include "check.h"
#include "domain/clock_calibration.h"
#include "domain/clock_schedule.h"
#include "domain/panel.h"
#include "fakes.h"

// A stored record with every field valid.
static void stored_defaults(void)
{
    fake_settings_valid = 1;
    fake_settings_stored.led_flashing_enabled = 1;
    fake_settings_stored.clock_trim = CLOCK_TRIM_DEFAULT;
    fake_settings_stored.clock_interval = CLOCK_SCHEDULE_DEFAULT_MINUTES;
    fake_settings_stored.scene = DEVICE_SETTINGS_SCENE_UNSET;
}

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
    stored_defaults();
    fake_settings_stored.panel_model = PANEL_MODEL_BWR154;
    fake_settings_stored.fast_refresh_enabled = 1;
    fake_settings_stored.led_flashing_enabled = 0;
    fake_settings_stored.clock_trim = -1234;
    fake_settings_stored.clock_interval = 5;
    fake_settings_stored.clock_sync = 1;
    fake_settings_stored.scene = SCREEN_SCENE_SLIDESHOW;
    fake_settings_stored.slideshow_interval = 600;
    device_settings_load();

    CHECK_EQ(device_settings_panel_model(), PANEL_MODEL_BWR154);
    CHECK_EQ(device_settings_fast_refresh_enabled(), 1);
    CHECK_EQ(device_settings_led_flashing_enabled(), 0);
    CHECK_EQ(device_settings_clock_trim(), -1234);
    CHECK_EQ(device_settings_clock_interval(), 5);
    CHECK_EQ(device_settings_clock_sync(), 1);
    CHECK_EQ(device_settings_scene(), SCREEN_SCENE_SLIDESHOW);
    CHECK_EQ(device_settings_slideshow_interval(), 600);
    CHECK_EQ(fake_settings_save_calls, 0);
}

static void test_save_if_changed(void)
{
    fakes_reset();
    stored_defaults();
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
    stored_defaults();
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

static void test_out_of_range_values_fall_back_per_field(void)
{
    fakes_reset();
    stored_defaults();
    fake_settings_stored.panel_model = PANEL_MODEL_COUNT;
    fake_settings_stored.fast_refresh_enabled = 1;
    fake_settings_stored.clock_trim = CLOCK_TRIM_LIMIT + 1;
    fake_settings_stored.clock_interval = 61;
    fake_settings_stored.scene = SCREEN_SCENE_COUNT;
    fake_settings_stored.slideshow_interval = 45;
    device_settings_load();

    CHECK_EQ(device_settings_panel_model(), PANEL_MODEL_AUTO);
    CHECK_EQ(device_settings_clock_trim(), CLOCK_TRIM_DEFAULT);
    CHECK_EQ(device_settings_clock_interval(), CLOCK_SCHEDULE_DEFAULT_MINUTES);
    CHECK_EQ(device_settings_scene(), DEVICE_SETTINGS_SCENE_UNSET);
    // Valid fields stay as stored.
    CHECK_EQ(device_settings_fast_refresh_enabled(), 1);
    CHECK_EQ(device_settings_slideshow_interval(), 45);

    // The corrected values are stored by the next save.
    CHECK_EQ(fake_settings_save_calls, 0);
    device_settings_save_if_changed();
    CHECK_EQ(fake_settings_save_calls, 1);
    CHECK_EQ(fake_settings_stored.panel_model, PANEL_MODEL_AUTO);
}

static void test_trim_at_the_limit_is_kept(void)
{
    fakes_reset();
    stored_defaults();
    fake_settings_stored.clock_trim = -CLOCK_TRIM_LIMIT;
    device_settings_load();

    CHECK_EQ(device_settings_clock_trim(), -CLOCK_TRIM_LIMIT);
    device_settings_save_if_changed();
    CHECK_EQ(fake_settings_save_calls, 0);
}

// A record of firmware before scene and slideshow interval were stored: the interval comes from
// the stored images' header, and the scene stays unset for the boot choice.
static void test_legacy_record_takes_interval_from_images(void)
{
    fakes_reset();
    stored_defaults();
    fake_settings_legacy = 1;
    fake_settings_stored.panel_model = PANEL_MODEL_BWR296;
    fake_store_interval = 90;
    device_settings_load();

    CHECK_EQ(device_settings_panel_model(), PANEL_MODEL_BWR296);
    CHECK_EQ(device_settings_slideshow_interval(), 90);
    CHECK_EQ(device_settings_scene(), DEVICE_SETTINGS_SCENE_UNSET);
    device_settings_save_if_changed();
    CHECK_EQ(fake_settings_save_calls, 1);
    CHECK_EQ(fake_settings_stored.slideshow_interval, 90);
}

static void test_scene_marks_settings_changed_only_when_it_differs(void)
{
    fakes_reset();
    stored_defaults();
    fake_settings_stored.scene = SCREEN_SCENE_CLOCK;
    device_settings_load();

    device_settings_set_scene(SCREEN_SCENE_CLOCK);
    device_settings_save_if_changed();
    CHECK_EQ(fake_settings_save_calls, 0);

    device_settings_set_scene(SCREEN_SCENE_DASHBOARD);
    device_settings_save_if_changed();
    CHECK_EQ(fake_settings_save_calls, 1);
    CHECK_EQ(fake_settings_stored.scene, SCREEN_SCENE_DASHBOARD);
}

static void test_reset_keeps_scene_and_slideshow_interval(void)
{
    fakes_reset();
    stored_defaults();
    fake_settings_stored.scene = SCREEN_SCENE_SLIDESHOW;
    fake_settings_stored.slideshow_interval = 20;
    fake_settings_stored.fast_refresh_enabled = 1;
    device_settings_load();

    device_settings_reset();

    CHECK_EQ(device_settings_fast_refresh_enabled(), 0);
    CHECK_EQ(fake_settings_stored.scene, SCREEN_SCENE_SLIDESHOW);
    CHECK_EQ(fake_settings_stored.slideshow_interval, 20);
}

int main(void)
{
    test_invalid_storage_applies_defaults();
    test_valid_storage_is_loaded_without_saving();
    test_save_if_changed();
    test_reset_restores_defaults_and_saves();
    test_out_of_range_values_fall_back_per_field();
    test_trim_at_the_limit_is_kept();
    test_legacy_record_takes_interval_from_images();
    test_scene_marks_settings_changed_only_when_it_differs();
    test_reset_keeps_scene_and_slideshow_interval();
    return check_report();
}
