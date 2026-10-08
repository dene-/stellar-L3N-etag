// Pure domain rules: panel table, refresh policy, slideshow, calendar, time zone, clock calibration,
// firmware image, temperature, period, battery, device name.
#include <string.h>
#include "check.h"
#include "domain/battery.h"
#include "domain/calendar.h"
#include "domain/clock_calibration.h"
#include "domain/device_name.h"
#include "domain/firmware_image.h"
#include "domain/panel.h"
#include "domain/period.h"
#include "domain/refresh_policy.h"
#include "domain/slideshow.h"
#include "domain/temperature.h"
#include "domain/time_zone.h"

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

static void check_date(uint32_t seconds, int year, int month, int day, int week, int hour, int min, int sec)
{
    struct date_time date = calendar_date(seconds);

    CHECK_EQ(date.tm_year, year);
    CHECK_EQ(date.tm_month, month);
    CHECK_EQ(date.tm_day, day);
    CHECK_EQ(date.tm_week, week);
    CHECK_EQ(date.tm_hour, hour);
    CHECK_EQ(date.tm_min, min);
    CHECK_EQ(date.tm_sec, sec);
}

static void test_calendar(void)
{
    check_date(0, 1970, 1, 1, 4, 0, 0, 0);
    check_date(1791467127u, 2026, 10, 8, 4, 13, 45, 27);
    // Leap years: every 4th, except centuries not divisible by 400.
    check_date(1709164799u, 2024, 2, 28, 3, 23, 59, 59);
    check_date(1709164800u, 2024, 2, 29, 4, 0, 0, 0);
    check_date(1677628800u, 2023, 3, 1, 3, 0, 0, 0);
    check_date(951782400u, 2000, 2, 29, 2, 0, 0, 0);
    check_date(4107456000u + 86399, 2100, 2, 28, 0, 23, 59, 59);
    check_date(4107542400u, 2100, 3, 1, 1, 0, 0, 0);
    // New year.
    check_date(1704067199u, 2023, 12, 31, 0, 23, 59, 59);
    check_date(1704067200u, 2024, 1, 1, 1, 0, 0, 0);
    // Last day the 32-bit count reaches.
    check_date(4294944000u, 2106, 2, 7, 0, 0, 0, 0);
}

static void test_time_zone(void)
{
    // CEST until 2026-10-25 01:00 UTC, then CET, then CEST again from 2027-03-28 01:00 UTC.
    time_zone_t zone = {120, 2, {{1792890000u, 60}, {1806195600u, 120}}};
    time_zone_t west = {-300, 0, {{0, 0}}};

    CHECK_EQ(time_zone_offset_seconds(&zone, 1791467127u), 7200);
    CHECK_EQ(time_zone_offset_seconds(&zone, 1792890000u - 1), 7200);
    CHECK_EQ(time_zone_offset_seconds(&zone, 1792890000u), 3600);
    CHECK_EQ(time_zone_offset_seconds(&zone, 1806195600u), 7200);
    CHECK_EQ(time_zone_offset_seconds(&west, 1791467127u), -18000);
}

#define NOMINAL 16000000
#define SIX_HOURS_MS 21600000u

static void test_clock_calibration(void)
{
    int16_t trim;

    // 100 ppm fast with trim 5000: 16005000 * 1.0001 ticks per second.
    trim = clock_calibrated_trim(5000, NOMINAL, SIX_HOURS_MS + 2160, SIX_HOURS_MS);
    CHECK(trim >= 6599 && trim <= 6601);
    // 200 ppm slow: 16005000 * 0.9998.
    trim = clock_calibrated_trim(5000, NOMINAL, SIX_HOURS_MS - 4320, SIX_HOURS_MS);
    CHECK(trim >= 1798 && trim <= 1800);
    // Exact clock: unchanged.
    CHECK_EQ(clock_calibrated_trim(5000, NOMINAL, SIX_HOURS_MS, SIX_HOURS_MS), 5000);
    // 40 days, 50 ppm fast: 16005000 * 1.00005, within the precision the 32-bit math keeps.
    trim = clock_calibrated_trim(5000, NOMINAL, 3456000000u + 172800, 3456000000u);
    CHECK(trim >= 5797 && trim <= 5803);

    // Too short to measure.
    CHECK_EQ(clock_calibrated_trim(5000, NOMINAL, SIX_HOURS_MS - 1 + 2160, SIX_HOURS_MS - 1), 5000);
    // Drift beyond 0.2 %, and a result beyond the limit, are bad measurements.
    CHECK_EQ(clock_calibrated_trim(5000, NOMINAL, SIX_HOURS_MS + 43201, SIX_HOURS_MS), 5000);
    CHECK_EQ(clock_calibrated_trim(5000, NOMINAL, SIX_HOURS_MS - 43201, SIX_HOURS_MS), 5000);
    CHECK_EQ(clock_calibrated_trim(29000, NOMINAL, SIX_HOURS_MS + 4320, SIX_HOURS_MS), 29000);
    trim = clock_calibrated_trim(-29000, NOMINAL, SIX_HOURS_MS - 4320, SIX_HOURS_MS);
    CHECK_EQ(trim, -29000);
}

static void make_header(uint8_t *header, uint8_t flag, uint8_t dual_bank)
{
    static const uint8_t signature[4] = {0, 'N', 'L', 'T'};

    memset(header, 0, FIRMWARE_HEADER_SIZE);
    memcpy(&header[FIRMWARE_FLAG_OFFSET], signature, sizeof(signature));
    header[FIRMWARE_FLAG_OFFSET] = flag;
    if (dual_bank)
        memcpy(&header[FIRMWARE_DUAL_BANK_OFFSET], "2BNK", 4);
}

static void test_firmware_image(void)
{
    uint8_t header[FIRMWARE_HEADER_SIZE];

    // Running from bank 0 (its flag set): stage in 0x20000; else in bank 0.
    CHECK_EQ(firmware_spare_bank(FIRMWARE_FLAG_BOOTABLE), 0x20000);
    CHECK_EQ(firmware_spare_bank(0x00), 0);
    CHECK_EQ(firmware_spare_bank(0xFF), 0);

    make_header(header, FIRMWARE_FLAG_BOOTABLE, 1);
    CHECK_EQ(firmware_install_method(header, 0x20000), FIRMWARE_INSTALL_SWITCH_BANK);
    CHECK_EQ(firmware_install_method(header, 0), FIRMWARE_INSTALL_SWITCH_BANK);

    // Older images cannot run from 0x20000: copied over bank 0, or started in place from bank 0.
    make_header(header, FIRMWARE_FLAG_BOOTABLE, 0);
    CHECK_EQ(firmware_install_method(header, 0x20000), FIRMWARE_INSTALL_COPY_TO_BANK_0);
    CHECK_EQ(firmware_install_method(header, 0), FIRMWARE_INSTALL_SWITCH_BANK);

    // Not bootable: no boot flag, or no signature.
    make_header(header, 0xFF, 1);
    CHECK_EQ(firmware_install_method(header, 0x20000), FIRMWARE_INSTALL_REJECT);
    make_header(header, FIRMWARE_FLAG_BOOTABLE, 1);
    header[11] = 0;
    CHECK_EQ(firmware_install_method(header, 0), FIRMWARE_INSTALL_REJECT);
}

static void test_temperature(void)
{
    CHECK_EQ(temperature_x10_from_x256(21 * 256), 210);
    CHECK_EQ(temperature_x10_from_x256(0x1580), 215); // 21.5
    CHECK_EQ(temperature_x10_from_x256(0x15F0), 219); // 21.9375
    CHECK_EQ(temperature_x10_from_x256(-384), -15);   // -1.5
    CHECK_EQ(temperature_x10_from_x256(-16), -1);     // -0.0625

    CHECK_EQ(temperature_whole_c(215), 22);
    CHECK_EQ(temperature_whole_c(214), 21);
    CHECK_EQ(temperature_whole_c(-215), -22);
    CHECK_EQ(temperature_whole_c(-14), -1);
    CHECK_EQ(temperature_whole_c(-4), 0);
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

    // CR2032 discharge curve: flat near the top, steep towards empty.
    CHECK_EQ(battery_percent(3100), 100);
    CHECK_EQ(battery_percent(3000), 100);
    CHECK_EQ(battery_percent(2950), 71);
    CHECK_EQ(battery_percent(2900), 42);
    CHECK_EQ(battery_percent(2820), 30);
    CHECK_EQ(battery_percent(2740), 18);
    CHECK_EQ(battery_percent(2440), 6);
    CHECK_EQ(battery_percent(2270), 3);
    CHECK_EQ(battery_percent(2100), 0);
    CHECK_EQ(battery_percent(2000), 0);

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
    test_time_zone();
    test_clock_calibration();
    test_firmware_image();
    test_temperature();
    test_period();
    test_battery_and_name();
    return check_report();
}
