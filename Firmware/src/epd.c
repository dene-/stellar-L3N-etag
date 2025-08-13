#include <stdint.h>
#include "etime.h"
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bw_213.h"
#include "epd_bwr_213.h"
#include "epd_bw_213_ice.h"
// #include "epd_bwr_154.h"
#include "epd_bwr_296.h"
#include "drivers.h"
#include "stack/ble/ble.h"

#include "battery.h"

#include "OneBitDisplay.h"
#include "TIFF_G4.h"
extern const uint8_t ucMirror[];
#include "font_60.h"
#include "font16.h"
#include "font16zh.h"
#include "font30.h"

#define LOG_UART(charP) puts(charP)

RAM uint8_t epd_model = 2; // 0 = Undetected, 1 = BW213, 2 = BWR213_PRO, 3 = BWR154, 4 = BW213ICE, 5 BWR296
const char *epd_model_string[] = {"NC", "BW213", "BWR213", "BWR154", "213ICE", "BWR296"};
RAM uint8_t epd_update_state = 0;

RAM uint8_t epd_scene = 2;
RAM uint8_t epd_wait_update = 0;

RAM uint8_t hour_refresh = 100;
RAM uint8_t minute_refresh = 100;

// Track partial updates to periodically force a full refresh for ghosting mitigation
RAM static uint16_t epd_partial_count = 0; // number of consecutive partials
// Force a full refresh after this many partials if an hour boundary didn't already do it
#define EPD_MAX_PARTIAL_BEFORE_FULL 30
// Timestamp (minutes) of last full refresh to ensure at least one per hour
RAM static int last_full_refresh_hour = -1;
// Keep last rendered buffers for simple change detection (dirty heuristic)
// Note: size equals epd_buffer_size; we only allocate if memory allows.
// Large frame history buffers don't need to survive deep retention sleep; omit RAM retention attribute
static uint8_t epd_prev_black[epd_buffer_size];
static uint8_t epd_prev_red[epd_buffer_size];

// Adaptive panel power hold (ms) and telemetry
static uint16_t epd_power_hold_ms = 15000;     // initial hold window
static unsigned long epd_last_power_on_ts = 0; // clock_time() of last power on
static uint8_t epd_powered = 0;
static unsigned long epd_last_update_ts = 0;        // for interval averaging
static uint32_t epd_update_interval_avg_ms = 60000; // start with 60s
static uint16_t epd_last_battery_mv = 0;
static uint32_t epd_partial_area_accum = 0; // cumulative partial updated area in pixels
// Row-level hashes for fast dirty detection (supports up to 128 rows for current panels)
static uint8_t epd_row_hash[128];

// Forward state variables needed by early helper functions
RAM uint8_t epd_temperature_is_read = 0; // moved up to avoid implicit use
RAM uint8_t epd_temperature = 0;

// Forward declaration for get_battery_level used in adaptation
uint8_t get_battery_level(uint16_t mv);
// Forward declaration for puts if not provided by headers
int puts(const char *s);

static int epd_can_reuse_power(void)
{
    if (!epd_powered)
        return 0;
    unsigned long now = clock_time();
    unsigned long elapsed_ms = (now - epd_last_power_on_ts) / CLOCK_16M_SYS_TIMER_CLK_1MS;
    return (elapsed_ms < epd_power_hold_ms);
}

static void epd_power_on_if_needed(void)
{
    if (epd_can_reuse_power())
        return; // already powered and within hold window
    EPD_init();
    EPD_POWER_ON();
    WaitMs(2);
    // Panel reset only if cold start
    gpio_write(EPD_RESET, 0);
    WaitMs(2);
    gpio_write(EPD_RESET, 1);
    WaitMs(2);
    epd_last_power_on_ts = clock_time();
    epd_powered = 1;
}

static void epd_power_maybe_off(void)
{
    if (!epd_powered)
        return;
    if (!epd_can_reuse_power())
    {
        epd_set_sleep();
        epd_powered = 0;
    }
}

// Adapt hold window using EMA of update intervals and battery level
static void epd_adapt_hold(void)
{
    unsigned long now = clock_time();
    if (epd_last_update_ts)
    {
        unsigned long delta_ms = (now - epd_last_update_ts) / CLOCK_16M_SYS_TIMER_CLK_1MS;
        // EMA alpha=0.2: new = 0.8*old + 0.2*delta
        epd_update_interval_avg_ms = (epd_update_interval_avg_ms * 8 + delta_ms * 2) / 10;
    }
    epd_last_update_ts = now;
    uint8_t batt_pct = get_battery_level(epd_last_battery_mv);
    uint16_t target;
    if (epd_update_interval_avg_ms > 120000)
        target = 2000; // very sparse updates
    else if (epd_update_interval_avg_ms > 60000)
        target = 4000;
    else if (epd_update_interval_avg_ms > 30000)
        target = 8000;
    else
        target = 15000; // frequent updates
    if (batt_pct < 20 && target > 3000)
        target = 3000; // conserve when low battery
    epd_power_hold_ms = target;
}

// Sampled diff: returns 1 if sampled diff count exceeds threshold (meaning we should force full refresh)
static int epd_sample_diff_exceeds(const uint8_t *new_b, const uint8_t *prev_b,
                                   const uint8_t *new_r, const uint8_t *prev_r,
                                   int size, int has_red)
{
    int diff = 0;
    for (int i = 0; i < size; i += 8)
    {
        if (new_b[i] != prev_b[i])
        {
            diff++;
            if (diff > 32)
                return 1;
        }
        if (has_red && new_r && prev_r && new_r[i] != prev_r[i])
        {
            diff++;
            if (diff > 32)
                return 1;
        }
    }
    return 0;
}

// Unified frame send with decision logic (hourly full, partial count, diff sampling)
static void epd_send_frame(uint8_t *black, uint8_t *red, uint16_t w, uint16_t h, uint8_t requested_full, int current_hour, int has_red)
{
    int size = (w * h) / 8;
    int force_full = 0;
    if (!requested_full)
    {
        if (current_hour != last_full_refresh_hour)
            force_full = 1; // ensure hourly full
        else if (epd_partial_count >= EPD_MAX_PARTIAL_BEFORE_FULL)
            force_full = 1; // periodic ghosting mitigation
        else if (epd_sample_diff_exceeds(black, epd_prev_black, red, epd_prev_red, size, has_red))
            force_full = 1;
    }
    uint8_t final_full = (requested_full || force_full) ? 1 : 0;
    if (!final_full && epd_model == 2) // BWR 2.13 specific partial window handling
    {
        // Compute bounding box of changed bytes (coarse): each row = w pixels => w/8 bytes
        int row_bytes = w / 8;
        int first_row = h, last_row = -1;
        int first_col = w, last_col = -1;
        for (int y = 0; y < h && y < (int)sizeof(epd_row_hash); y++)
        {
            int base = y * row_bytes;
            uint8_t hash = 0;
            for (int xb = 0; xb < row_bytes; xb++)
            {
                hash ^= black[base + xb];
                if (has_red && red)
                    hash ^= red[base + xb];
            }
            if (hash == epd_row_hash[y])
                continue;           // unchanged row
            epd_row_hash[y] = hash; // mark new hash
            // Row changed: refine columns
            for (int xb = 0; xb < row_bytes; xb++)
            {
                int idx = base + xb;
                uint8_t cur_b = black[idx];
                uint8_t prev_b = epd_prev_black[idx];
                uint8_t cur_r = has_red ? red[idx] : 0;
                uint8_t prev_r = has_red ? epd_prev_red[idx] : 0;
                if (cur_b != prev_b || cur_r != prev_r)
                {
                    int x_start_px = xb * 8;
                    int x_end_px = x_start_px + 7;
                    if (x_start_px < first_col)
                        first_col = x_start_px;
                    if (x_end_px > last_col)
                        last_col = x_end_px;
                }
            }
            if (y < first_row)
                first_row = y;
            if (y > last_row)
                last_row = y;
        }
        // Fallback to full if nothing detected or box too large
        if (last_row < 0 || (last_row - first_row > h * 3 / 4))
        {
            final_full = 1;
            EPD_Display(black, has_red ? red : NULL, size, final_full);
        }
        else
        {
            // Ensure panel powered
            epd_power_on_if_needed();
            // Enter partial and set window
            if (first_col >= w)
                first_col = 0;
            if (last_col >= w)
                last_col = w - 1;
            EPD_BWR_213_enter_partial();
            EPD_BWR_213_set_partial_area(first_col, first_row, last_col, last_row);

            int width_px = (last_col - first_col + 1);
            int width_bytes = (width_px + 7) / 8;
            int x_byte_start = first_col / 8;

            // Black layer (0x10)
            EPD_WriteCmd(0x10);
            for (int y = first_row; y <= last_row; y++)
            {
                int base = y * row_bytes + x_byte_start;
                EPD_WriteDataStream(&black[base], width_bytes);
            }
            if (has_red && red)
            {
                EPD_WriteCmd(0x13);
                for (int y = first_row; y <= last_row; y++)
                {
                    int base = y * row_bytes + x_byte_start;
                    EPD_WriteDataStream(&red[base], width_bytes);
                }
            }
            // Trigger refresh
            EPD_WriteCmd(0x12);
            EPD_BWR_213_exit_partial();

            // Update temperature/read flag and update state bookkeeping similar to EPD_Display
            epd_temperature_is_read = 1;
            epd_update_state = 1;
            // Partial count bookkeeping
            if (requested_full)
            {
                epd_partial_count = 0;
                last_full_refresh_hour = hour_refresh;
                epd_partial_area_accum = 0;
                memset(epd_row_hash, 0, sizeof(epd_row_hash));
            }
            else
            {
                if (epd_partial_count < 0xFFFF)
                    epd_partial_count++;
                uint32_t area = (uint32_t)width_px * (uint32_t)(last_row - first_row + 1);
                epd_partial_area_accum += area;
                uint32_t full_area = (uint32_t)w * (uint32_t)h;
                if (epd_partial_area_accum > full_area + full_area / 2)
                {
                    epd_partial_count = EPD_MAX_PARTIAL_BEFORE_FULL; // force soon
                }
            }
        }
    }
    else
    {
        EPD_Display(black, has_red ? red : NULL, size, final_full);
        if (final_full)
        {
            epd_partial_area_accum = 0;
            memset(epd_row_hash, 0, sizeof(epd_row_hash));
        }
    }
    // Persist current frame for next diff (store even if we forced full)
    memcpy(epd_prev_black, black, size);
    if (has_red && red)
        memcpy(epd_prev_red, red, size);
    else
        memset(epd_prev_red, 0x00, size); // keep stable zeros for black-only scenes
    // Adapt power hold after sending frame
    epd_adapt_hold();
}

const char *BLE_conn_string[] = {"BLE 0", "BLE 1"};

RAM uint8_t epd_buffer[epd_buffer_size];
uint8_t epd_buffer_red[epd_buffer_size];
RAM uint8_t epd_temp[epd_buffer_size]; // for OneBitDisplay to draw into
OBDISP obd;                            // virtual display structure
TIFFIMAGE tiff;

// With this we can force a display if it wasnt detected correctly
void set_EPD_model(uint8_t model_nr)
{
    epd_model = model_nr;
}

// With this we can force a display if it wasnt detected correctly
void set_EPD_scene(uint8_t scene)
{
    epd_scene = scene;
    set_EPD_wait_flush();
}

void set_EPD_wait_flush()
{
    epd_wait_update = 1;
}

// Here we detect what E-Paper display is connected
_attribute_ram_code_ void EPD_detect_model(void)
{
    epd_model = 2;
    return;

    EPD_init();
    // system power
    puts("EPD_detect_model\r\n");
    puts("EPD_POWER_ON\r\n");
    EPD_POWER_ON();

    WaitMs(10);
    // Reset the EPD driver IC
    gpio_write(EPD_RESET, 0);
    WaitMs(10);
    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    // Here we neeed to detect it
    if (EPD_BWR_296_detect())
    {
        epd_model = 5;
    }
    else if (EPD_BWR_213_detect())
    {
        epd_model = 2;
    }
    //    else if (EPD_BWR_154_detect())// Right now this will never trigger, the 154 is same to 213BWR right now.
    //    {
    //        epd_model = 3;
    //    }
    else if (EPD_BW_213_ice_detect())
    {
        epd_model = 4;
    }
    else
    {
        epd_model = 1;
    }

    puts("Detected :");
    puts(epd_model_string[epd_model]);
    puts("\r\n");

    puts("EPD_POWER_ON\r\n");
    EPD_POWER_OFF();
}

_attribute_ram_code_ uint8_t EPD_read_temp(void)
{
    if (epd_temperature_is_read)
        return epd_temperature;

    if (!epd_model)
        EPD_detect_model();

    EPD_init();
    // system power
    EPD_POWER_ON();

    WaitMs(5);

    // Reset the EPD driver IC
    gpio_write(EPD_RESET, 0);
    WaitMs(10);

    gpio_write(EPD_RESET, 1);
    WaitMs(10);

    if (epd_model == 1)
        epd_temperature = EPD_BW_213_read_temp();
    else if (epd_model == 2)
        epd_temperature = EPD_BWR_213_read_temp();
    else if (epd_model == 4 || epd_model == 5)
        epd_temperature = EPD_BW_213_ice_read_temp();

    EPD_POWER_OFF();

    epd_temperature_is_read = 1;

    return epd_temperature;
}

_attribute_ram_code_ void EPD_Display(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial)
{
    if (!epd_model)
        EPD_detect_model();

    // puts("Trying to update EPD\r\n");
    epd_power_on_if_needed();

    if (epd_model == 1)
        epd_temperature = EPD_BW_213_Display(image, size, full_or_partial);
    else if (epd_model == 2)
        epd_temperature = EPD_BWR_213_Display_BWR(image, red_image, size, full_or_partial);
    // else if (epd_model == 3)
    //     epd_temperature = EPD_BWR_154_Display(image, size, full_or_partial);
    else if (epd_model == 4)
        epd_temperature = EPD_BW_213_ice_Display(image, size, full_or_partial);
    else if (epd_model == 5)
        epd_temperature = EPD_BWR_296_Display_BWR(image, red_image, size, full_or_partial);
    // epd_temperature = EPD_BWR_296_Display(image, size, full_or_partial);

    epd_temperature_is_read = 1;
    epd_update_state = 1;

    // Update partial/full counters
    if (full_or_partial)
    {
        epd_partial_count = 0;
        last_full_refresh_hour = hour_refresh; // hour_refresh updated elsewhere
    }
    else
    {
        if (epd_partial_count < 0xFFFF)
            epd_partial_count++;
    }
}

_attribute_ram_code_ void epd_set_sleep(void)
{
    if (!epd_model)
        EPD_detect_model();

    if (epd_model == 1)
        EPD_BW_213_set_sleep();
    else if (epd_model == 2)
        EPD_BWR_213_set_sleep();
    else if (epd_model == 4 || epd_model == 5)
        EPD_BW_213_ice_set_sleep();

    EPD_POWER_OFF();
    epd_update_state = 0;
    epd_powered = 0;
}

_attribute_ram_code_ uint8_t epd_state_handler(void)
{
    switch (epd_update_state)
    {
    case 0:
        // If idle, decide whether to power down after hold window
        epd_power_maybe_off();
        break;
    case 1: // check if refresh is done and sleep epd if so
        if (epd_model == 1)
        {
            if (!EPD_IS_BUSY())
                epd_set_sleep();
        }
        else
        {
            if (EPD_IS_BUSY())
                epd_set_sleep();
        }
        break;
    }
    return epd_update_state;
}

_attribute_ram_code_ void FixBuffer(uint8_t *pSrc, uint8_t *pDst, uint16_t width, uint16_t height)
{
    int x, y;
    uint8_t *s, *d;
    for (y = 0; y < (height / 8); y++)
    { // byte rows
        d = &pDst[y];
        s = &pSrc[y * width];
        for (x = 0; x < width; x++)
        {
            d[x * (height / 8)] = ~ucMirror[s[width - 1 - x]]; // invert and flip
        } // for x
    } // for y
}

_attribute_ram_code_ void TIFFDraw(TIFFDRAW *pDraw)
{
    uint8_t uc = 0, ucSrcMask, ucDstMask, *s, *d;
    int x, y;

    s = pDraw->pPixels;
    y = pDraw->y;                          // current line
    d = &epd_buffer[(249 * 16) + (y / 8)]; // rotated 90 deg clockwise
    ucDstMask = 0x80 >> (y & 7);           // destination mask
    ucSrcMask = 0;                         // src mask
    for (x = 0; x < pDraw->iWidth; x++)
    {
        // Slower to draw this way, but it allows us to use a single buffer
        // instead of drawing and then converting the pixels to be the EPD format
        if (ucSrcMask == 0)
        { // load next source byte
            ucSrcMask = 0x80;
            uc = *s++;
        }
        if (!(uc & ucSrcMask))
        { // black pixel
            d[-(x * 16)] &= ~ucDstMask;
        }
        ucSrcMask >>= 1;
    }
}

_attribute_ram_code_ void epd_display_tiff(uint8_t *pData, int iSize)
{
    // test G4 decoder
    epd_clear();
    TIFF_openRAW(&tiff, 250, 122, BITDIR_MSB_FIRST, pData, iSize, TIFFDraw);
    TIFF_setDrawParameters(&tiff, 65536, TIFF_PIXEL_1BPP, 0, 0, 250, 122, NULL);
    TIFF_decode(&tiff);
    TIFF_close(&tiff);
    EPD_Display(epd_buffer, NULL, epd_buffer_size, 1);
}

extern uint8_t mac_public[6];
_attribute_ram_code_ void epd_display(struct date_time _time, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial)
{
    uint8_t battery_level;

    if (epd_update_state)
        return;

    if (!epd_model)
    {
        EPD_detect_model();
    }
    uint16_t resolution_w = 250;
    uint16_t resolution_h = 128; // 122 real pixel, but needed to have a full byte
    if (epd_model == 1)
    {
        resolution_w = 250;
        resolution_h = 128; // 122 real pixel, but needed to have a full byte
    }
    else if (epd_model == 2)
    {
        resolution_w = 250;
        resolution_h = 128; // 122 real pixel, but needed to have a full byte
    }
    else if (epd_model == 3)
    {
        resolution_w = 200;
        resolution_h = 200;
    }
    else if (epd_model == 4)
    {
        resolution_w = 212;
        resolution_h = 104;
    }
    else if (epd_model == 5)
    {
        resolution_w = 296;
        resolution_h = 128;
    }

    epd_clear();

    // Draw BLACK layer
    obdCreateVirtualDisplay(&obd, resolution_w, resolution_h, epd_temp);
    obdFill(&obd, 0, 0); // fill with white

    char buff[100];
    battery_level = get_battery_level(battery_mv);
    sprintf(buff, "THX_%02X%02X%02X %s", mac_public[2], mac_public[1], mac_public[0], epd_model_string[epd_model]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 1, 17, (char *)buff, 1);
    sprintf(buff, "%s", BLE_conn_string[ble_get_connected()]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 232, 20, (char *)buff, 1);

    sprintf(buff, "-----%d'C-----", EPD_read_temp());
    obdWriteStringCustom(&obd, (GFXfont *)&Special_Elite_Regular_30, 10, 95, (char *)buff, 1);
    sprintf(buff, "Battery %dmV  %d%%", battery_mv, battery_level);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 10, 120, (char *)buff, 1);

    FixBuffer(epd_temp, epd_buffer, resolution_w, resolution_h);

    // Draw RED layer
    obdFill(&obd, 0, 0); // fill with white

    obdRectangle(&obd, 0, 90, 249, 121, 1, 0);

    sprintf(buff, "%02d:%02d", _time.tm_hour, _time.tm_min);
    obdWriteStringCustom(&obd, (GFXfont *)&DSEG14_Classic_Mini_Regular_40, 75, 65, (char *)buff, 1);

    FixBuffer(epd_temp, epd_buffer_red, resolution_w, resolution_h);

    epd_send_frame(epd_buffer, epd_buffer_red, resolution_w, resolution_h, full_or_partial, _time.tm_hour, 1);
}

_attribute_ram_code_ void epd_display_char(uint8_t data)
{
    int i;
    for (i = 0; i < epd_buffer_size; i++)
    {
        epd_buffer[i] = data;
    }
    EPD_Display(epd_buffer, NULL, epd_buffer_size, 1);
}

_attribute_ram_code_ void epd_clear(void)
{
    memset(epd_buffer, 0x00, epd_buffer_size);
    memset(epd_buffer_red, 0x00, epd_buffer_size);
    memset(epd_temp, 0x00, epd_buffer_size);
}

void update_time_scene(struct date_time _time, uint16_t battery_mv, int16_t temperature, void (*scene)(struct date_time, uint16_t, int16_t, uint8_t))
{
    // default scene: show default time, battery, ble address, temperature
    if (epd_update_state)
    {
        return;
    }

    if (!epd_model)
    {
        EPD_detect_model();
    }

    if (epd_wait_update)
    {
        scene(_time, battery_mv, temperature, 1); // force full
        epd_wait_update = 0;
        return;
    }

    if (_time.tm_min != minute_refresh)
    {
        minute_refresh = _time.tm_min;
        if (_time.tm_hour != hour_refresh)
        {
            hour_refresh = _time.tm_hour;
            scene(_time, battery_mv, temperature, 1); // hourly full
        }
        else
        {
            scene(_time, battery_mv, temperature, 0); // attempt partial
        }
    }
}

void epd_update(struct date_time _time, uint16_t battery_mv, int16_t temperature)
{
    switch (epd_scene)
    {
    case 1:
        update_time_scene(_time, battery_mv, temperature, epd_display);
        break;
    case 2:
        update_time_scene(_time, battery_mv, temperature, epd_display_time_with_date);
        break;
    default:
        break;
    }
}

void epd_display_time_with_date(struct date_time _time, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial)
{
    uint16_t battery_level;

    // Clear all working buffers (black, red, temp)
    epd_clear();

    // Create a virtual monochrome drawing surface the size of the panel
    obdCreateVirtualDisplay(&obd, epd_width, epd_height, epd_temp);
    obdFill(&obd, 0, 0); // fill with white (1 = black pixel when finally inverted for panel)

    char buff[100];
    battery_level = get_battery_level(battery_mv);

    // Device identifier (partial MAC)
    sprintf(buff, "THX_%02X%02X%02X", mac_public[2], mac_public[1], mac_public[0]);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 1, 17, (char *)buff, 1);

    // BLE connection state (appears to map to two hard‑coded string endings)
    if (ble_get_connected())
    {
        sprintf(buff, "78%s", "234");
    }
    else
    {
        sprintf(buff, "78%s", "56");
    }
    // Write BLE state (Chinese font variant)
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16_zh, 120, 21, (char *)buff, 1);

    // Battery icon rectangle
    obdRectangle(&obd, 235, 2, 249, 22, 1, 1);

    // Battery percentage inside battery outline (drawn white on black fill)
    sprintf(buff, "%d", battery_level);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 219, 18, (char *)buff, 0);

    // Separator bar under header
    obdRectangle(&obd, 0, 25, 249, 27, 1, 1);

    // Time (HH:MM) big segmented font
    sprintf(buff, "%02d:%02d", _time.tm_hour, _time.tm_min);
    obdWriteStringCustom(&obd, (GFXfont *)&DSEG14_Classic_Mini_Regular_40, 35, 85, (char *)buff, 1);

    // Temperature (from EPD sensor, not the passed temperature param)
    sprintf(buff, "%d'C", epd_temperature);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 216, 50, (char *)buff, 1);

    // Small separator line under temperature
    obdRectangle(&obd, 216, 60, 249, 62, 1, 1);

    // Battery voltage in mV
    sprintf(buff, " %dmV", battery_mv);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 216, 84, (char *)buff, 1);

    // Vertical separator at right info block
    obdRectangle(&obd, 214, 27, 216, 99, 1, 1);
    // Horizontal footer separator
    obdRectangle(&obd, 0, 97, 249, 99, 1, 1);

    // Date (YYYY-MM-DD)
    sprintf(buff, "%d-%02d-%02d", _time.tm_year, _time.tm_month, _time.tm_day);
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 10, 120, (char *)buff, 1);

    // Weekday display: seems to offset char code; special case for week == 7
    if (_time.tm_week == 7)
    {
        sprintf(buff, "9:%c", _time.tm_week + 0x20 + 6);
    }
    else
    {
        sprintf(buff, "9:%c", _time.tm_week + 0x20);
    }
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 120, 122, (char *)buff, 1);

    // Day/Night indicator (hard-coded strings based on hour range)
    if (_time.tm_hour > 7 && _time.tm_hour < 20)
    {
        sprintf(buff, "%s", "EFGH");
    }
    else
    {
        sprintf(buff, "%s", "ABCD");
    }
    obdWriteStringCustom(&obd, (GFXfont *)&Dialog_plain_16, 200, 122, (char *)buff, 1);

    // Convert drawing buffer into panel memory layout then send (black only)
    FixBuffer(epd_temp, epd_buffer, epd_width, epd_height);
    epd_send_frame(epd_buffer, NULL, epd_width, epd_height, full_or_partial, _time.tm_hour, 0);
}
