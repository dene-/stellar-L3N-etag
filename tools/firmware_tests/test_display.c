// display: panel detection, red plane handling, refresh sizes and kinds, polling and timeout, plane
// writes, temperature cache, model switching.
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

// Fresh fakes and a display with nothing refreshing, no cached temperature and an unknown shown frame.
static void setup(uint8_t model)
{
    fake_panel_idle = 1;
    display_poll(); // ends a refresh left running by the previous test
    fakes_reset();
    display_select_model(model);
}

static void test_detects_once(void)
{
    const panel_t *panel;

    setup(PANEL_MODEL_AUTO);
    fake_detect_model = PANEL_MODEL_BWR296;
    CHECK_EQ(fake_detect_calls, 0);

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
    setup(PANEL_MODEL_BW296);
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

// The fast refresh setting only matters to the callers that pass it as `fast`; explicit refreshes
// are shown exactly as asked.
static void test_explicit_refreshes_ignore_fast_setting(void)
{
    setup(PANEL_MODEL_BWR296);
    device_settings_set_fast_refresh_enabled(1);

    display_refresh(BW296_PLANE_BYTES, 1);
    CHECK_EQ(fake_refresh_full, 1);
    finish_refresh();

    display_refresh(BW296_PLANE_BYTES, 0);
    CHECK_EQ(fake_refresh_full, 0);
    finish_refresh();

    display_show_pattern(0xFF);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK(fake_refresh_red_null);
    finish_refresh();

    device_settings_set_fast_refresh_enabled(0);
}

static void test_refresh_if_changed_uses_policy(void)
{
    setup(PANEL_MODEL_BWR296);
    display_fill(0xFF, 0x00);

    // Unknown panel content: full, in fast mode too.
    CHECK_EQ(display_refresh_if_changed(0, 1), 1);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK_EQ(fake_refresh_size, BW296_PLANE_BYTES);
    finish_refresh();

    CHECK_EQ(display_refresh_if_changed(0, 1), 0);
    CHECK_EQ(fake_refresh_calls, 1);

    // A redraw of the same frame: full normally, partial in fast mode.
    CHECK_EQ(display_refresh_if_changed(1, 0), 1);
    CHECK_EQ(fake_refresh_full, 1);
    finish_refresh();
    CHECK_EQ(display_refresh_if_changed(1, 1), 1);
    CHECK_EQ(fake_refresh_full, 0);
    finish_refresh();

    // Anything drawn outside the policy (an explicit refresh) makes the next frame full again.
    display_refresh(BW296_PLANE_BYTES, 0);
    finish_refresh();
    CHECK_EQ(display_refresh_if_changed(0, 1), 1);
    CHECK_EQ(fake_refresh_full, 1);
}

static void test_refresh_size_is_clamped_to_panel_plane(void)
{
    setup(PANEL_MODEL_BW296);
    display_refresh(0xFFFF, 1);
    CHECK_EQ(fake_refresh_size, BW296_PLANE_BYTES);
    finish_refresh();

    display_init(PANEL_MODEL_BW213_ICE);
    display_refresh(PANEL_MAX_PLANE_BYTES, 1);
    CHECK_EQ(fake_refresh_size, panel_plane_bytes(panel_find(PANEL_MODEL_BW213_ICE)));
    CHECK(fake_refresh_size < PANEL_MAX_PLANE_BYTES);
    finish_refresh();

    // A smaller request is kept.
    display_refresh(100, 1);
    CHECK_EQ(fake_refresh_size, 100);
}

static void test_poll_sleeps_panel_once_idle(void)
{
    setup(PANEL_MODEL_BWR296);

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

// A panel that never reports idle must not block the display forever; the timeout runs on uptime.
static void test_poll_gives_up_on_stuck_panel(void)
{
    setup(PANEL_MODEL_BWR296);
    fake_uptime = 1000;
    display_refresh(BW296_PLANE_BYTES, 1);

    fake_uptime = 1000 + DISPLAY_REFRESH_TIMEOUT - 1;
    CHECK_EQ(display_poll(), 1);
    CHECK_EQ(fake_sleep_calls, 0);
    CHECK_EQ(display_is_refreshing(), 1);

    // The unix time jumping (phone sets the clock) must not end it.
    fake_utc = 5000000;
    CHECK_EQ(display_poll(), 1);

    fake_uptime = 1000 + DISPLAY_REFRESH_TIMEOUT;
    CHECK_EQ(display_poll(), 0);
    CHECK_EQ(fake_sleep_calls, 1);
    CHECK_EQ(fake_sleep_model, PANEL_MODEL_BWR296);
    CHECK_EQ(display_is_refreshing(), 0);

    // The display is usable again.
    display_refresh(BW296_PLANE_BYTES, 1);
    CHECK_EQ(display_is_refreshing(), 1);
    CHECK_EQ(fake_refresh_calls, 2);
}

static void test_write_is_bounded_by_plane_size(void)
{
    static const uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    setup(PANEL_MODEL_BW296);

    CHECK_EQ(display_write(DISPLAY_PLANE_BLACK, 0, data, 8), 1);
    CHECK_EQ(display_plane(DISPLAY_PLANE_BLACK)[7], 8);
    CHECK_EQ(display_write(DISPLAY_PLANE_BLACK, BW296_PLANE_BYTES - 8, data, 8), 1);
    CHECK_EQ(display_write(DISPLAY_PLANE_BLACK, BW296_PLANE_BYTES - 7, data, 8), 0);
    CHECK_EQ(display_write(DISPLAY_PLANE_RED, BW296_PLANE_BYTES, data, 1), 0);
}

#define MAX_AGE 300

static void test_temperature_is_cached(void)
{
    setup(PANEL_MODEL_BWR296);
    fake_uptime = 1000;
    fake_panel_temperature = 213;

    CHECK_EQ(display_read_temperature(MAX_AGE), 213);
    CHECK_EQ(fake_read_temperature_calls, 1);

    fake_panel_temperature = 250;
    fake_uptime = 1000 + MAX_AGE - 1;
    CHECK_EQ(display_read_temperature(MAX_AGE), 213);
    CHECK_EQ(fake_read_temperature_calls, 1);

    // The UTC time does not age the cache.
    fake_utc = 1000000;
    CHECK_EQ(display_read_temperature(MAX_AGE), 213);
    CHECK_EQ(fake_read_temperature_calls, 1);

    // A shorter maximum age (while connected) re-measures sooner.
    CHECK_EQ(display_read_temperature(MAX_AGE - 1), 250);
    CHECK_EQ(fake_read_temperature_calls, 2);
    CHECK_EQ(display_last_temperature(), 250);
}

// Reading the temperature resets the controller, so it must not happen while a refresh runs.
static void test_temperature_not_read_during_refresh(void)
{
    setup(PANEL_MODEL_BWR296);
    fake_uptime = 1000;
    fake_panel_temperature = 220;
    display_refresh(BW296_PLANE_BYTES, 1); // the refresh itself measures the temperature

    fake_panel_temperature = 300;
    fake_uptime = 1000 + MAX_AGE + 10;
    CHECK_EQ(display_read_temperature(MAX_AGE), 220);
    CHECK_EQ(fake_read_temperature_calls, 0);

    finish_refresh();
    CHECK_EQ(display_read_temperature(MAX_AGE), 300);
    CHECK_EQ(fake_read_temperature_calls, 1);
}

static void test_select_model_is_stored_in_settings(void)
{
    setup(PANEL_MODEL_AUTO);
    display_select_model(PANEL_MODEL_BW213_ICE);

    CHECK_EQ(device_settings_panel_model(), PANEL_MODEL_BW213_ICE);
    CHECK_EQ(display_panel()->model, PANEL_MODEL_BW213_ICE);
    CHECK_EQ(fake_detect_calls, 0);
    CHECK_EQ(fake_sleep_calls, 0);
}

// The sleep command must go to the controller that is refreshing, i.e. the old model.
static void test_select_model_during_refresh_sleeps_old_panel(void)
{
    setup(PANEL_MODEL_BWR296);
    display_refresh(BW296_PLANE_BYTES, 1);
    CHECK_EQ(display_is_refreshing(), 1);

    display_select_model(PANEL_MODEL_BWR213);
    CHECK_EQ(fake_sleep_calls, 1);
    CHECK_EQ(fake_sleep_model, PANEL_MODEL_BWR296);
    CHECK_EQ(display_is_refreshing(), 0);
    CHECK_EQ(display_panel()->model, PANEL_MODEL_BWR213);

    // Nothing is left to sleep later.
    CHECK_EQ(display_poll(), 0);
    CHECK_EQ(fake_sleep_calls, 1);
}

static void test_select_model_forgets_shown_frame(void)
{
    setup(PANEL_MODEL_BWR296);
    display_fill(0xFF, 0x00);
    CHECK_EQ(display_refresh_if_changed(0, 1), 1);
    finish_refresh();
    CHECK_EQ(display_refresh_if_changed(0, 1), 0);

    display_select_model(PANEL_MODEL_BWR296);
    CHECK_EQ(display_refresh_if_changed(0, 1), 1);
    CHECK_EQ(fake_refresh_full, 1);
}

int main(void)
{
    test_detects_once();
    test_red_plane_only_for_red_panels();
    test_explicit_refreshes_ignore_fast_setting();
    test_refresh_if_changed_uses_policy();
    test_refresh_size_is_clamped_to_panel_plane();
    test_poll_sleeps_panel_once_idle();
    test_poll_gives_up_on_stuck_panel();
    test_write_is_bounded_by_plane_size();
    test_temperature_is_cached();
    test_temperature_not_read_during_refresh();
    test_select_model_is_stored_in_settings();
    test_select_model_during_refresh_sleeps_old_panel();
    test_select_model_forgets_shown_frame();
    return check_report();
}
