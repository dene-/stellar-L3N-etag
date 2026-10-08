// display: panel detection, red plane handling, fast refresh, polling, plane writes, temperature cache.
#include "application/device_settings.h"
#include "application/display.h"
#include "check.h"
#include "fakes.h"

#define BW296_PLANE_BYTES 4736

static void finish_refresh(void)
{
    fake_panel_idle = 1;
    display_poll();
}

static void test_detects_once(void)
{
    const panel_t *panel;

    fakes_reset();
    fake_detect_model = PANEL_MODEL_BWR296;
    display_init(PANEL_MODEL_AUTO);

    panel = display_panel();
    CHECK(panel != NULL);
    CHECK_EQ(panel->model, PANEL_MODEL_BWR296);
    CHECK_EQ(panel->width, 296);
    CHECK_EQ(panel->height, 128);
    CHECK_EQ(fake_detect_calls, 1);

    display_panel();
    CHECK_EQ(fake_detect_calls, 1);
}

static void test_red_plane_only_for_red_panels(void)
{
    fakes_reset();
    display_init(PANEL_MODEL_BW296);
    display_refresh(BW296_PLANE_BYTES, 1);
    CHECK_EQ(fake_refresh_model, PANEL_MODEL_BW296);
    CHECK(fake_refresh_red_null);
    finish_refresh();

    display_init(PANEL_MODEL_BWR296);
    display_refresh(BW296_PLANE_BYTES, 1);
    CHECK_EQ(fake_refresh_model, PANEL_MODEL_BWR296);
    CHECK(!fake_refresh_red_null);
    finish_refresh();
}

static void test_fast_refresh_makes_refreshes_partial(void)
{
    fakes_reset();
    display_init(PANEL_MODEL_BWR296);

    display_refresh(BW296_PLANE_BYTES, 1);
    CHECK_EQ(fake_refresh_full, 1);
    finish_refresh();

    device_settings_set_fast_refresh_enabled(1);
    display_refresh(BW296_PLANE_BYTES, 1);
    CHECK_EQ(fake_refresh_full, 0);
    finish_refresh();
    device_settings_set_fast_refresh_enabled(0);
}

static void test_poll_sleeps_panel_once_idle(void)
{
    fakes_reset();
    display_init(PANEL_MODEL_BWR296);

    CHECK_EQ(display_is_refreshing(), 0);
    display_refresh(BW296_PLANE_BYTES, 1);
    CHECK_EQ(display_is_refreshing(), 1);

    fake_panel_idle = 0;
    CHECK_EQ(display_poll(), 1);
    CHECK_EQ(fake_sleep_calls, 0);

    fake_panel_idle = 1;
    CHECK_EQ(display_poll(), 0);
    CHECK_EQ(fake_sleep_calls, 1);
    CHECK_EQ(display_is_refreshing(), 0);

    CHECK_EQ(display_poll(), 0);
    CHECK_EQ(fake_sleep_calls, 1);
}

static void test_write_is_bounded_by_plane_size(void)
{
    static const uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    fakes_reset();
    display_init(PANEL_MODEL_BW296);

    CHECK_EQ(display_write(DISPLAY_PLANE_BLACK, 0, data, 8), 1);
    CHECK_EQ(display_plane(DISPLAY_PLANE_BLACK)[7], 8);
    CHECK_EQ(display_write(DISPLAY_PLANE_BLACK, BW296_PLANE_BYTES - 8, data, 8), 1);
    CHECK_EQ(display_write(DISPLAY_PLANE_BLACK, BW296_PLANE_BYTES - 7, data, 8), 0);
    CHECK_EQ(display_write(DISPLAY_PLANE_RED, BW296_PLANE_BYTES, data, 1), 0);
}

static void test_temperature_is_cached(void)
{
    fakes_reset();
    display_init(PANEL_MODEL_BWR296);
    fake_now = 1000;
    fake_panel_temperature = 21;

    CHECK_EQ(display_read_temperature(), 21);
    CHECK_EQ(fake_read_temperature_calls, 1);

    fake_panel_temperature = 25;
    fake_now = 1000 + DISPLAY_TEMPERATURE_MAX_AGE - 1;
    CHECK_EQ(display_read_temperature(), 21);
    CHECK_EQ(fake_read_temperature_calls, 1);

    fake_now = 1000 + DISPLAY_TEMPERATURE_MAX_AGE;
    CHECK_EQ(display_read_temperature(), 25);
    CHECK_EQ(fake_read_temperature_calls, 2);
}

static void test_select_model_is_stored_in_settings(void)
{
    fakes_reset();
    display_select_model(PANEL_MODEL_BW213_ICE);

    CHECK_EQ(device_settings_panel_model(), PANEL_MODEL_BW213_ICE);
    CHECK_EQ(display_panel()->model, PANEL_MODEL_BW213_ICE);
    CHECK_EQ(fake_detect_calls, 0);
}

int main(void)
{
    test_detects_once();
    test_red_plane_only_for_red_panels();
    test_fast_refresh_makes_refreshes_partial();
    test_poll_sleeps_panel_once_idle();
    test_write_is_bounded_by_plane_size();
    test_temperature_is_cached();
    test_select_model_is_stored_in_settings();
    return check_report();
}
