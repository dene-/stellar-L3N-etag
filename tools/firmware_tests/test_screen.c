// screen: the dashboard scene (default) redraw rules, normal and fast refresh mode.
#include "application/device_settings.h"
#include "application/display.h"
#include "application/screen.h"
#include "check.h"
#include "domain/refresh_policy.h"
#include "fakes.h"

#define BWR296_PLANE_BYTES 4736
#define BWR213_PLANE_BYTES 4000

#define UTC_2026_10_08_1300 1791464400u

static void set_minute(int minute)
{
    fake_utc = UTC_2026_10_08_1300 + minute * 60;
}

static void finish_refresh(void)
{
    fake_panel_idle = 1;
    display_poll();
}

// Fresh fakes, an idle BWR296 panel with unknown content, the dashboard scene with a redraw requested.
static void setup(void)
{
    fake_panel_idle = 1;
    display_poll(); // ends a refresh left running by the previous test
    fakes_reset();
    device_settings_set_fast_refresh_enabled(0);
    display_select_model(PANEL_MODEL_BWR296);
    screen_set_scene(SCREEN_SCENE_DASHBOARD);
    set_minute(45);
}

static void test_dashboard_redraw_rules(void)
{
    int calls;

    setup();

    // First update after boot: full refresh of the whole plane.
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK_EQ(fake_refresh_size, BWR296_PLANE_BYTES);
    CHECK_EQ(fake_refresh_model, PANEL_MODEL_BWR296);

    // Same minute: nothing to redraw.
    finish_refresh();
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);

    // New minute: the changed frame is shown with a partial refresh.
    set_minute(46);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 2);
    CHECK_EQ(fake_refresh_full, 0);

    // While that refresh is running, even a new minute does nothing.
    set_minute(47);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 2);

    // Once it finished the pending minute is drawn.
    finish_refresh();
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 3);
    CHECK_EQ(fake_refresh_full, 0);

    // Switching the panel redraws in full for the new resolution, even within the same minute.
    finish_refresh();
    screen_select_panel(PANEL_MODEL_BWR213);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 4);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK_EQ(fake_refresh_model, PANEL_MODEL_BWR213);
    CHECK_EQ(fake_refresh_size, BWR213_PLANE_BYTES);

    // An explicit redraw request is a full refresh although the minute is unchanged.
    finish_refresh();
    calls = fake_refresh_calls;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls);
    screen_request_redraw();
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls + 1);
    CHECK_EQ(fake_refresh_full, 1);
}

// Fast refresh mode: after the first full frame, clock frames are partial refreshes for good; a
// scene switch or redraw and switching the panel (unknown content) are still full.
static void test_fast_refresh_keeps_clock_frames_partial(void)
{
    int i;
    int calls;

    setup();
    device_settings_set_fast_refresh_enabled(1);

    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    CHECK_EQ(fake_refresh_full, 1);
    finish_refresh();

    // More minutes than the anti-ghosting interval: no full refresh in between.
    for (i = 1; i <= 2 * REFRESH_POLICY_FULL_INTERVAL; i++)
    {
        set_minute(45 + i % 15);
        calls = fake_refresh_calls;
        screen_update(0, "THX_TEST");
        CHECK_EQ(fake_refresh_calls, calls + 1);
        CHECK_EQ(fake_refresh_full, 0);
        finish_refresh();
    }

    // Switching the scene is a full refresh: partially drawn, the old scene would show through.
    calls = fake_refresh_calls;
    screen_set_scene(SCREEN_SCENE_CLOCK);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls + 1);
    CHECK_EQ(fake_refresh_full, 1);
    finish_refresh();

    screen_select_panel(PANEL_MODEL_BWR213);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_full, 1);
    CHECK_EQ(fake_refresh_size, BWR213_PLANE_BYTES);
    device_settings_set_fast_refresh_enabled(0);
}

// Changing the scene draws the new scene directly, without a clearing refresh first.
static void test_scene_switch_is_a_single_refresh(void)
{
    int calls;

    setup();
    screen_update(0, "THX_TEST");
    finish_refresh();
    calls = fake_refresh_calls;

    screen_set_scene(SCREEN_SCENE_CLOCK);
    CHECK_EQ(fake_refresh_calls, calls);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls + 1);
    CHECK_EQ(fake_refresh_full, 1);
}

int main(void)
{
    test_dashboard_redraw_rules();
    test_fast_refresh_keeps_clock_frames_partial();
    test_scene_switch_is_a_single_refresh();
    return check_report();
}
