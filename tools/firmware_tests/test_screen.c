// screen: the dashboard scene (default) redraw rules.
#include "application/display.h"
#include "application/screen.h"
#include "check.h"
#include "fakes.h"

#define BWR296_PLANE_BYTES 4736
#define BWR213_PLANE_BYTES 4000

static void set_minute(int minute)
{
    fake_date.tm_min = minute;
}

static void finish_refresh(void)
{
    fake_panel_idle = 1;
    display_poll();
}

int main(void)
{
    int calls;

    fakes_reset();
    fake_detect_model = PANEL_MODEL_BWR296;
    fake_date.tm_year = 2026;
    fake_date.tm_month = 10;
    fake_date.tm_day = 8;
    fake_date.tm_hour = 13;
    set_minute(45);

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

    return check_report();
}
