// Pure domain rules: panel table, refresh policy, slideshow, calendar, period, battery, device name.
#include <string.h>
#include "check.h"
#include "domain/battery.h"
#include "domain/calendar.h"
#include "domain/device_name.h"
#include "domain/panel.h"
#include "domain/period.h"
#include "domain/refresh_policy.h"
#include "domain/slideshow.h"

static void test_panel(void)
{
    static const struct
    {
        uint8_t model;
        uint16_t width, height;
        uint8_t has_red;
    } expected[] = {
        {PANEL_MODEL_BW213, 250, 128, 0},     {PANEL_MODEL_BWR213, 250, 128, 1}, {PANEL_MODEL_BWR154, 200, 200, 1},
        {PANEL_MODEL_BW213_ICE, 212, 104, 0}, {PANEL_MODEL_BWR296, 296, 128, 1}, {PANEL_MODEL_BW296, 296, 128, 0},
    };
    unsigned i;

    CHECK(panel_find(PANEL_MODEL_AUTO) == NULL);
    CHECK(panel_find(PANEL_MODEL_COUNT) == NULL);

    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        const panel_t *panel = panel_find(expected[i].model);

        CHECK(panel != NULL);
        if (!panel)
            continue;
        CHECK_EQ(panel->model, expected[i].model);
        CHECK_EQ(panel->width, expected[i].width);
        CHECK_EQ(panel->height, expected[i].height);
        CHECK_EQ(panel->has_red, expected[i].has_red);
        CHECK(panel_plane_bytes(panel) <= PANEL_MAX_PLANE_BYTES);
    }
    CHECK_EQ(panel_plane_bytes(panel_find(PANEL_MODEL_BWR154)), PANEL_MAX_PLANE_BYTES);
}

static void test_refresh_policy(void)
{
    enum { SIZE = 64 };
    refresh_policy_t policy;
    uint8_t black[SIZE], red[SIZE];
    int i;

    memset(&policy, 0, sizeof(policy));
    memset(black, 0xFF, SIZE);
    memset(red, 0x00, SIZE);

    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_FULL);
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_SKIP);

    black[3] = 0x00;
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_PARTIAL);
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_SKIP);

    red[5] = 0x01;
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_FULL);

    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_SKIP);
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 1, 0), REFRESH_FULL);

    // The last full refresh reset the counter: FULL_INTERVAL partial refreshes, then a full one.
    for (i = 0; i < REFRESH_POLICY_FULL_INTERVAL; i++)
    {
        black[0] = (uint8_t)i;
        CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_PARTIAL);
    }
    black[0] = 0xAA;
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_FULL);

    refresh_policy_forget(&policy);
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_FULL);
}

// Fast mode: only an unknown panel content or a red change forces a full refresh.
static void test_refresh_policy_fast(void)
{
    enum { SIZE = 64 };
    refresh_policy_t policy;
    uint8_t black[SIZE], red[SIZE];
    int i;

    memset(&policy, 0, sizeof(policy));
    memset(black, 0xFF, SIZE);
    memset(red, 0x00, SIZE);

    // The first frame (panel content unknown) is full even in fast mode.
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 1), REFRESH_FULL);
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 1), REFRESH_SKIP);

    // A requested redraw is partial, changed or not.
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 1, 1), REFRESH_PARTIAL);
    black[3] = 0x00;
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 1, 1), REFRESH_PARTIAL);

    // No periodic anti-ghosting full refresh: twice the interval of changes stays partial.
    for (i = 0; i < 2 * REFRESH_POLICY_FULL_INTERVAL; i++)
    {
        black[0] = (uint8_t)i;
        CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 1), REFRESH_PARTIAL);
    }

    // A partial refresh cannot draw red.
    red[5] = 0x01;
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 1), REFRESH_FULL);

    // Unknown panel content is full in fast mode too.
    black[1] = 0x12;
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 1), REFRESH_PARTIAL);
    refresh_policy_forget(&policy);
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 1), REFRESH_FULL);

    // The partial counter saturates: after many fast partials, leaving fast mode is due a full refresh.
    for (i = 0; i < 300; i++)
    {
        black[0] = (uint8_t)(i + 1);
        CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 1), REFRESH_PARTIAL);
    }
    black[0] = 0x77;
    CHECK_EQ(refresh_policy_decide(&policy, black, red, SIZE, 0, 0), REFRESH_FULL);
}

static void test_slideshow(void)
{
    slideshow_t show;

    slideshow_restart(&show, 100);
    CHECK_EQ(show.index, 0);

    CHECK_EQ(slideshow_advance(&show, 109, 10, 3), 0);
    CHECK_EQ(show.index, 0);
    CHECK_EQ(slideshow_advance(&show, 110, 10, 3), 1);
    CHECK_EQ(show.index, 1);
    CHECK_EQ(slideshow_advance(&show, 120, 10, 3), 1);
    CHECK_EQ(show.index, 2);
    CHECK_EQ(slideshow_advance(&show, 130, 10, 3), 1);
    CHECK_EQ(show.index, 0);

    // Interval 0 means the default.
    slideshow_restart(&show, 1000);
    CHECK_EQ(slideshow_advance(&show, 1000 + SLIDESHOW_DEFAULT_INTERVAL_SECONDS - 1, 0, 2), 0);
    CHECK_EQ(slideshow_advance(&show, 1000 + SLIDESHOW_DEFAULT_INTERVAL_SECONDS, 0, 2), 1);
    CHECK_EQ(show.index, 1);

    // A clock set backwards restarts the wait from the new time.
    slideshow_restart(&show, 5000);
    CHECK_EQ(slideshow_advance(&show, 100, 10, 3), 0);
    CHECK_EQ(slideshow_advance(&show, 109, 10, 3), 0);
    CHECK_EQ(slideshow_advance(&show, 110, 10, 3), 1);
    CHECK_EQ(show.index, 1);

    // Nothing to show.
    slideshow_restart(&show, 0);
    CHECK_EQ(slideshow_advance(&show, 1000, 10, 0), 0);
    CHECK_EQ(show.index, 0);
}

static void test_calendar(void)
{
    calendar_t calendar;

    memset(&calendar, 0, sizeof(calendar));
    calendar_set(&calendar, 1791467127u, 2026, 10, 8, 4); // 2026-10-08 13:45:27 UTC
    CHECK_EQ(calendar.date.tm_hour, 13);
    CHECK_EQ(calendar.date.tm_min, 45);
    CHECK_EQ(calendar.date.tm_sec, 27);
    CHECK_EQ(calendar.date.tm_day, 8);

    // Leap year: 28 Feb 2024 23:59:59 (Wednesday) -> 29 Feb, Thursday.
    calendar_set(&calendar, 1709164799u, 2024, 2, 28, 3);
    calendar_advance_second(&calendar);
    CHECK_EQ(calendar.date.tm_year, 2024);
    CHECK_EQ(calendar.date.tm_month, 2);
    CHECK_EQ(calendar.date.tm_day, 29);
    CHECK_EQ(calendar.date.tm_week, 4);
    CHECK_EQ(calendar.date.tm_hour, 0);
    CHECK_EQ(calendar.date.tm_min, 0);
    CHECK_EQ(calendar.date.tm_sec, 0);

    // Not a leap year: 28 Feb 2023 23:59:59 -> 1 Mar.
    calendar_set(&calendar, 1677628799u, 2023, 2, 28, 2);
    calendar_advance_second(&calendar);
    CHECK_EQ(calendar.date.tm_month, 3);
    CHECK_EQ(calendar.date.tm_day, 1);
    CHECK_EQ(calendar.date.tm_week, 3);

    // New year: 31 Dec 2023 23:59:59 (Sunday) -> 1 Jan 2024, Monday.
    calendar_set(&calendar, 1704067199u, 2023, 12, 31, 0);
    calendar_advance_second(&calendar);
    CHECK_EQ(calendar.date.tm_year, 2024);
    CHECK_EQ(calendar.date.tm_month, 1);
    CHECK_EQ(calendar.date.tm_day, 1);
    CHECK_EQ(calendar.date.tm_week, 1);

    // Never set: the time of day runs but the date stays unset.
    memset(&calendar, 0, sizeof(calendar));
    {
        uint32_t i;
        for (i = 0; i < 86400 + 1; i++)
            calendar_advance_second(&calendar);
    }
    CHECK_EQ(calendar.date.tm_year, 0);
    CHECK_EQ(calendar.date.tm_month, 0);
    CHECK_EQ(calendar.date.tm_day, 0);
    CHECK_EQ(calendar.date.tm_sec, 1);
}

static void test_period(void)
{
    period_t period;

    memset(&period, 0, sizeof(period));
    CHECK_EQ(period_elapsed(&period, 1000, 10), 1);
    CHECK_EQ(period_elapsed(&period, 1009, 10), 0);
    CHECK_EQ(period_elapsed(&period, 1010, 10), 1);
    CHECK_EQ(period_elapsed(&period, 1019, 10), 0); // restarted at 1010
}

static void test_battery_and_name(void)
{
    static const uint8_t mac[6] = {0xAC, 0xF8, 0x3A, 0x11, 0x22, 0x33};
    char name[DEVICE_NAME_LENGTH + 1];

    CHECK_EQ(battery_percent(2000), 0);
    CHECK_EQ(battery_percent(2200), 0);
    CHECK_EQ(battery_percent(2650), 50);
    CHECK_EQ(battery_percent(3100), 100);
    CHECK_EQ(battery_percent(3300), 100);

    device_name_format(mac, name);
    CHECK(strcmp(name, "THX_3AF8AC") == 0);
}

int main(void)
{
    test_panel();
    test_refresh_policy();
    test_refresh_policy_fast();
    test_slideshow();
    test_calendar();
    test_period();
    test_battery_and_name();
    return check_report();
}
