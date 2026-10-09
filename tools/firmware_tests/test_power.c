// power: voltage thresholds (flash writes, refresh pause with hysteresis), battery sampling, and
// how the display and telemetry follow from it.
#include "application/display.h"
#include "application/power.h"
#include "application/telemetry.h"
#include "domain/battery.h"
#include "check.h"
#include "fakes.h"

#define BWR296_PLANE_BYTES 4736

static void test_flash_write_threshold(void)
{
    CHECK(battery_flash_write_ok(3000));
    CHECK(battery_flash_write_ok(BATTERY_FLASH_MIN_MV));
    CHECK(!battery_flash_write_ok(BATTERY_FLASH_MIN_MV - 1));
    CHECK(!battery_flash_write_ok(BATTERY_PLAUSIBLE_MIN_MV));
    // Unknown or implausible readings do not block.
    CHECK(battery_flash_write_ok(0));
    CHECK(battery_flash_write_ok(BATTERY_PLAUSIBLE_MIN_MV - 1));
    CHECK(battery_flash_write_ok(BATTERY_PLAUSIBLE_MAX_MV + 1));
}

static void test_refresh_pause_has_hysteresis(void)
{
    // Running: pauses below the minimum only.
    CHECK(battery_refresh_ok(BATTERY_REFRESH_MIN_MV, 0));
    CHECK(!battery_refresh_ok(BATTERY_REFRESH_MIN_MV - 1, 0));
    // Paused: stays paused until the resume level, which is above the minimum.
    CHECK(!battery_refresh_ok(BATTERY_REFRESH_MIN_MV, 1));
    CHECK(!battery_refresh_ok(BATTERY_REFRESH_RESUME_MV - 1, 1));
    CHECK(battery_refresh_ok(BATTERY_REFRESH_RESUME_MV, 1));
    // An implausible reading keeps whatever state there is.
    CHECK(battery_refresh_ok(0, 0));
    CHECK(!battery_refresh_ok(0, 1));
    CHECK(!battery_refresh_ok(BATTERY_PLAUSIBLE_MAX_MV + 1, 1));
}

static void sample(uint16_t mv)
{
    fake_battery_mv = mv;
    power_sample_battery();
}

static void test_sampling_ignores_implausible_readings(void)
{
    fakes_reset();
    sample(3000);
    CHECK_EQ(power_battery_mv(), 3000);

    sample(0);
    CHECK_EQ(power_battery_mv(), 3000);
    sample(BATTERY_PLAUSIBLE_MIN_MV - 1);
    CHECK_EQ(power_battery_mv(), 3000);
    sample(BATTERY_PLAUSIBLE_MAX_MV + 1);
    CHECK_EQ(power_battery_mv(), 3000);
    CHECK(power_refresh_allowed());

    sample(2900);
    CHECK_EQ(power_battery_mv(), 2900);
}

// A failed reading must not pause refreshes, and the pause must survive one.
static void test_pause_follows_samples(void)
{
    fakes_reset();
    sample(3000);
    sample(0);
    CHECK(power_refresh_allowed());

    sample(2150);
    CHECK(!power_refresh_allowed());
    sample(0);
    CHECK(!power_refresh_allowed());
    sample(2250); // above the minimum, below the resume level
    CHECK(!power_refresh_allowed());
    sample(2300);
    CHECK(power_refresh_allowed());
}

static void test_flash_check_takes_a_fresh_reading(void)
{
    fakes_reset();
    sample(3000);

    fake_battery_mv = 2399;
    CHECK(!power_flash_write_allowed());
    fake_battery_mv = 2400;
    CHECK(power_flash_write_allowed());
    fake_battery_mv = 0;
    CHECK(power_flash_write_allowed());

    // Only the sampling path changes what the refresh decision is based on.
    fake_battery_mv = 2000;
    CHECK(!power_flash_write_allowed());
    CHECK_EQ(power_battery_mv(), 3000);
    CHECK(power_refresh_allowed());
}

static void finish_refresh(void)
{
    fake_panel_idle = 1;
    display_poll();
    fake_panel_idle = 0;
}

// Fresh fakes, a full battery, nothing refreshing and no deferred refresh left over.
static void setup_display(void)
{
    fake_panel_idle = 1;
    display_poll();
    fakes_reset();
    sample(3000);
    display_take_deferred_refresh();
    display_select_model(PANEL_MODEL_BWR296);
    display_fill(0xFF, 0x00);
}

static void test_no_refresh_starts_while_paused(void)
{
    setup_display();
    sample(2100);

    display_refresh(BWR296_PLANE_BYTES, 1);
    CHECK_EQ(display_refresh_if_changed(1, 0), 0);
    display_show_pattern(0x00);
    CHECK_EQ(fake_refresh_calls, 0);
    CHECK(!display_is_refreshing());
    // A pattern that was not shown is not drawn into the frame either.
    CHECK_EQ(display_plane(DISPLAY_PLANE_BLACK)[0], 0xFF);
}

static void test_skipped_refresh_is_reported_once_when_allowed(void)
{
    setup_display();
    CHECK(!display_take_deferred_refresh());

    sample(2100);
    display_refresh(BWR296_PLANE_BYTES, 1);
    CHECK(!display_take_deferred_refresh()); // still paused
    sample(2250);
    CHECK(!display_take_deferred_refresh());

    sample(2300);
    CHECK(display_take_deferred_refresh());
    CHECK(!display_take_deferred_refresh());
}

// The first frame after the pause is judged against what the panel really shows.
static void test_frame_is_shown_after_the_pause(void)
{
    setup_display();
    CHECK_EQ(display_refresh_if_changed(0, 0), 1);
    finish_refresh();

    sample(2100);
    display_plane(DISPLAY_PLANE_BLACK)[0] = 0x00;
    CHECK_EQ(display_refresh_if_changed(0, 0), 0);
    CHECK_EQ(fake_refresh_calls, 1);

    sample(2300);
    CHECK_EQ(display_refresh_if_changed(0, 0), 1);
    CHECK_EQ(fake_refresh_calls, 2);
}

static void test_running_refresh_is_not_cut_short(void)
{
    setup_display();
    display_refresh(BWR296_PLANE_BYTES, 1);
    CHECK(display_is_refreshing());

    sample(2100);
    CHECK(display_is_refreshing());
    finish_refresh();
    CHECK(!display_is_refreshing());
    CHECK_EQ(fake_sleep_calls, 1);
}

static void test_battery_is_not_sampled_during_a_refresh(void)
{
    setup_display();
    telemetry_update(0);
    CHECK_EQ(fake_publish_calls, 1);
    CHECK_EQ(power_battery_mv(), 3000);

    display_refresh(BWR296_PLANE_BYTES, 1);
    fake_battery_mv = 2100;
    fake_uptime += TELEMETRY_IDLE_INTERVAL;
    telemetry_update(0);
    CHECK_EQ(fake_publish_calls, 1);
    CHECK_EQ(power_battery_mv(), 3000);
    CHECK(power_refresh_allowed());

    // Right after the refresh, without waiting for the next interval.
    finish_refresh();
    fake_uptime += 1;
    telemetry_update(0);
    CHECK_EQ(fake_publish_calls, 2);
    CHECK_EQ(power_battery_mv(), 2100);
    CHECK_EQ(fake_publish_mv, 2100);
    CHECK(!power_refresh_allowed());

    // And only once.
    fake_uptime += 1;
    telemetry_update(0);
    CHECK_EQ(fake_publish_calls, 2);
}

int main(void)
{
    test_flash_write_threshold();
    test_refresh_pause_has_hysteresis();
    test_sampling_ignores_implausible_readings();
    test_pause_follows_samples();
    test_flash_check_takes_a_fresh_reading();
    test_no_refresh_starts_while_paused();
    test_skipped_refresh_is_reported_once_when_allowed();
    test_frame_is_shown_after_the_pause();
    test_running_refresh_is_not_cut_short();
    test_battery_is_not_sampled_during_a_refresh();
    return check_report();
}
