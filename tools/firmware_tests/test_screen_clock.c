// screen: when the clock scenes show a new frame (clock interval) and when its refresh starts in
// sync mode (early by the measured refresh time, so it ends as the minute changes).
#include "application/device_settings.h"
#include "application/display.h"
#include "application/screen.h"
#include "check.h"
#include "fakes.h"

#define DAY 1791417600u // 2026-10-08 00:00 UTC; no time zone is set, so also local time
#define WAKE_MARGIN_MS 1100

static uint32_t uptime_ms;

// Local time of day; the clock runs in whole seconds plus ms.
static void at(int hour, int minute, int second, int ms)
{
    fake_utc = DAY + hour * 3600 + minute * 60 + second;
    fake_utc_ms = ms;
}

static void set_uptime_ms(uint32_t ms)
{
    uptime_ms = ms;
    fake_uptime = ms / 1000;
    fake_uptime_ms = ms % 1000;
}

// The running refresh ends after ms.
static void finish_after(uint32_t ms)
{
    set_uptime_ms(uptime_ms + ms);
    fake_panel_idle = 1;
    display_poll();
    fake_panel_idle = 0;
}

static int refreshes_after_update(void)
{
    int calls = fake_refresh_calls;

    screen_update(0, "THX_TEST");
    return fake_refresh_calls - calls;
}

// Fresh fakes, BWR296, the dashboard drawn at boot (time) with a full refresh lasting full_ms.
static void setup(int hour, int minute, int second, uint32_t full_ms)
{
    fake_panel_idle = 1;
    display_poll(); // ends a refresh left running by the previous test
    fakes_reset();
    set_uptime_ms(100000);
    device_settings_set_fast_refresh_enabled(0);
    device_settings_set_clock_interval(1);
    device_settings_set_clock_sync(0);
    display_select_model(PANEL_MODEL_BWR296);
    screen_set_scene(SCREEN_SCENE_DASHBOARD);
    at(hour, minute, second, 0);
    CHECK_EQ(refreshes_after_update(), 1);
    CHECK_EQ(fake_refresh_full, 1);
    finish_after(full_ms);
}

static void test_interval_aligned_to_the_clock(void)
{
    setup(13, 46, 10, 15000);
    device_settings_set_clock_interval(5);
    screen_clock_schedule_changed();

    // Every 5 minutes: nothing until 13:50, without a redraw for the setting change itself.
    at(13, 46, 30, 0);
    CHECK_EQ(refreshes_after_update(), 0);
    at(13, 49, 59, 999);
    CHECK_EQ(refreshes_after_update(), 0);
    at(13, 50, 0, 0);
    CHECK_EQ(refreshes_after_update(), 1);
    finish_after(1000);
    at(13, 54, 0, 0);
    CHECK_EQ(refreshes_after_update(), 0);
    at(13, 55, 0, 0);
    CHECK_EQ(refreshes_after_update(), 1);
    finish_after(1000);

    // Back to every minute from the next whole minute.
    device_settings_set_clock_interval(1);
    screen_clock_schedule_changed();
    at(13, 55, 30, 0);
    CHECK_EQ(refreshes_after_update(), 0);
    at(13, 56, 0, 0);
    CHECK_EQ(refreshes_after_update(), 1);
    finish_after(1000);

    // A clock that jumped (synced an hour ahead) shows the current time at once.
    at(14, 56, 20, 0);
    CHECK_EQ(refreshes_after_update(), 1);
    finish_after(1000);
    at(14, 56, 40, 0);
    CHECK_EQ(refreshes_after_update(), 0);
}

static void test_sync_ends_refreshes_on_the_minute(void)
{
    const uint32_t full_ms = 15500;
    const uint32_t partial_ms = 1200;

    setup(13, 45, 0, full_ms);
    device_settings_set_clock_sync(1);
    screen_clock_schedule_changed();

    // 13:46 needs a partial refresh; none measured yet, so it starts the default 3 s plus the 1.1 s
    // wake-up margin ahead: at 13:45:55.900.
    at(13, 45, 20, 0);
    CHECK_EQ(refreshes_after_update(), 0);
    at(13, 45, 55, 800);
    CHECK_EQ(refreshes_after_update(), 0);
    at(13, 45, 55, 900);
    CHECK_EQ(refreshes_after_update(), 1);
    CHECK_EQ(fake_refresh_full, 0);
    finish_after(partial_ms);

    // The minute changes on the frame already shown: nothing more for 13:46.
    at(13, 46, 0, 0);
    CHECK_EQ(refreshes_after_update(), 0);

    // From now on the measured 1.2 s (plus the wake-up margin): starts at 13:46:57.700.
    at(13, 46, 57, 600);
    CHECK_EQ(refreshes_after_update(), 0);
    at(13, 46, 57, 700);
    CHECK_EQ(refreshes_after_update(), 1);
    CHECK_EQ(fake_refresh_full, 0);
    finish_after(partial_ms);
}

static void test_sync_plans_full_refreshes_by_their_duration(void)
{
    const uint32_t full_ms = 15500;

    setup(23, 58, 0, full_ms);
    device_settings_set_clock_sync(1);
    screen_clock_schedule_changed();

    at(23, 58, 55, 900); // 23:59, partial, default lead 4.1 s
    CHECK_EQ(refreshes_after_update(), 1);
    CHECK_EQ(fake_refresh_full, 0);
    finish_after(1200);

    // Midnight changes the red date band, so it needs a full refresh: 15.5 + 1.1 s ahead.
    at(23, 59, 43, 300);
    CHECK_EQ(refreshes_after_update(), 0);
    at(23, 59, 43, 400);
    CHECK_EQ(refreshes_after_update(), 1);
    CHECK_EQ(fake_refresh_full, 1);
    finish_after(full_ms);
    at(0, 0, 0, 0);
    fake_utc += 24 * 3600;
    CHECK_EQ(refreshes_after_update(), 0);
}

int main(void)
{
    test_interval_aligned_to_the_clock();
    test_sync_ends_refreshes_on_the_minute();
    test_sync_plans_full_refreshes_by_their_duration();
    return check_report();
}
