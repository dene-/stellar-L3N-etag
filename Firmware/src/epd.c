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
#include "flash.h"
#include "image_store.h"
#include "etime.h"

#include "OneBitDisplay.h"
#include "TIFF_G4.h"
extern const uint8_t ucMirror[];
#include "font_60.h"
#include "font16.h"
#include "font16zh.h"
#include "font30.h"

#define LOG_UART(charP) uart_puts(charP)

RAM uint8_t epd_model = 0; // 0 = Undetected, 1 = BW213, 2 = BWR213_PRO, 3 = BWR154, 4 = BW213ICE, 5 = BWR290/BWR296
const char *epd_model_string[] = {"NC", "BW213", "BWR213", "BWR154", "213ICE", "BWR290"};
RAM uint8_t epd_update_state = 0;

RAM uint8_t epd_scene = 2;
RAM uint8_t epd_wait_update = 0;

RAM uint8_t hour_refresh = 100;
RAM uint8_t minute_refresh = 100;
RAM uint8_t partial_refresh_count = 0;
#define PARTIAL_REFRESH_FULL_INTERVAL 10 // Force full refresh every N partial updates

const char *BLE_conn_string[] = {"BLE 0", "BLE 1"};
RAM uint8_t epd_temperature_is_read = 0;
RAM int8_t epd_temperature = 0;
RAM uint32_t epd_temperature_read_time = 0;
#define EPD_TEMP_CACHE_SECONDS 300 // Cache temperature for 5 minutes

RAM uint8_t epd_buffer[epd_buffer_size];
uint8_t epd_buffer_red[epd_buffer_size];
uint8_t epd_temp[epd_buffer_size]; // for OneBitDisplay to draw into (scratch, no retention needed)
OBDISP obd;                        // virtual display structure
TIFFIMAGE tiff;
RAM uint8_t slideshow_index = 0;
RAM uint32_t slideshow_last_switch = 0;

extern settings_struct settings;

static uint8_t epd_is_fast_refresh_supported_model(uint8_t model_nr)
{
    switch (model_nr)
    {
    case 1:
    case 2:
    case 4:
    case 5:
        return 1;
    default:
        return 0;
    }
}

static uint8_t epd_resolve_refresh_mode(uint8_t full_or_partial)
{
    if (!epd_model)
    {
        EPD_detect_model();
    }

    if (!settings.fast_refresh_enabled)
    {
        return full_or_partial;
    }

    if (!epd_is_fast_refresh_supported_model(epd_model))
    {
        return full_or_partial;
    }

    return 0;
}

static void epd_get_resolution_for_model(uint8_t model_nr, uint16_t *width, uint16_t *height)
{
    if (width == NULL || height == NULL)
    {
        return;
    }

    switch (model_nr)
    {
    case 1:
    case 2:
        *width = 250;
        *height = 128;
        break;
    case 3:
        *width = 200;
        *height = 200;
        break;
    case 4:
        *width = 212;
        *height = 104;
        break;
    case 5:
        *width = 296;
        *height = 128;
        break;
    default:
        *width = epd_width;
        *height = epd_height;
        break;
    }
}

static uint16_t epd_get_model_buffer_size(uint8_t model_nr)
{
    uint16_t width = 0;
    uint16_t height = 0;

    epd_get_resolution_for_model(model_nr, &width, &height);
    return (width * height) / 8;
}

// With this we can force a display if it wasnt detected correctly
void set_EPD_model(uint8_t model_nr)
{
    epd_model = model_nr;
    epd_temperature_is_read = 0;
    epd_temperature_read_time = 0;
}

uint8_t get_EPD_model(void)
{
    return epd_model;
}

void set_EPD_fast_refresh_enabled(uint8_t enabled)
{
    settings.fast_refresh_enabled = enabled ? 1 : 0;
}

uint8_t get_EPD_fast_refresh_enabled(void)
{
    return settings.fast_refresh_enabled ? 1 : 0;
}

uint8_t get_EPD_fast_refresh_supported(void)
{
    if (!epd_model)
    {
        EPD_detect_model();
    }

    return epd_is_fast_refresh_supported_model(epd_model);
}

// With this we can force a display if it wasnt detected correctly
void set_EPD_scene(uint8_t scene)
{
    // When switching from a clock scene to an image scene, clear the display
    // first so the EPD controller's old-frame RAM doesn't ghost the previous scene.
    // Skip if the EPD is currently refreshing or if we're already on an image scene.
    if ((scene == 0 || scene == 3) && epd_scene != 0 && epd_scene != 3 && !epd_update_state)
    {
        uint16_t buffer_size = epd_get_current_buffer_size();
        epd_clear();
        EPD_Display(epd_buffer, epd_buffer_red, buffer_size, 1);
    }
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
    EPD_init();
    // system power
    uart_puts("EPD_detect_model\r\n");
    uart_puts("EPD_POWER_ON\r\n");
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

    uart_puts("Detected :");
    uart_puts(epd_model_string[epd_model]);
    uart_puts("\r\n");

    uart_puts("EPD_POWER_ON\r\n");
    EPD_POWER_OFF();
}

_attribute_ram_code_ int8_t EPD_read_temp(void)
{
    uint32_t now = get_unix_time();
    if (epd_temperature_is_read && (now - epd_temperature_read_time) < EPD_TEMP_CACHE_SECONDS)
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
    else if (epd_model == 4)
        epd_temperature = EPD_BW_213_ice_read_temp();
    else if (epd_model == 5)
        epd_temperature = EPD_BWR_296_read_temp();

    EPD_POWER_OFF();

    epd_temperature_is_read = 1;
    epd_temperature_read_time = get_unix_time();

    return epd_temperature;
}

_attribute_ram_code_ void EPD_Display(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial)
{
    full_or_partial = epd_resolve_refresh_mode(full_or_partial);

    if (!epd_model)
        EPD_detect_model();

    // uart_puts("Trying to update EPD\r\n");

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
    epd_temperature_read_time = get_unix_time();
    epd_update_state = 1;
}

_attribute_ram_code_ void epd_set_sleep(void)
{
    if (!epd_model)
        EPD_detect_model();

    if (epd_model == 1)
        EPD_BW_213_set_sleep();
    else if (epd_model == 2)
        EPD_BWR_213_set_sleep();
    //    else if (epd_model == 3)
    //        EPD_BWR_154_set_sleep();
    else if (epd_model == 4)
        EPD_BW_213_ice_set_sleep();
    else if (epd_model == 5)
        EPD_BWR_296_set_sleep();

    EPD_POWER_OFF();
    epd_update_state = 0;
}

_attribute_ram_code_ uint8_t epd_state_handler(void)
{
    switch (epd_update_state)
    {
    case 0:
        // Nothing todo
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
    EPD_Display(epd_buffer, NULL, epd_get_current_buffer_size(), 1);
}

extern uint8_t mac_public[6];

static int16_t epd_get_text_width(GFXfont *font, char *text)
{
    int width = 0;
    int top = 0;
    int bottom = 0;

    obdGetStringBox(font, text, &width, &top, &bottom);
    return (int16_t)width;
}

static int16_t epd_clamp_text_x(GFXfont *font, char *text, int16_t x, int16_t left, int16_t right)
{
    int16_t width = epd_get_text_width(font, text);
    int16_t max_x = right - width + 1;

    if (right < left)
    {
        return left;
    }

    if (width >= (right - left + 1))
    {
        return left;
    }

    if (x < left)
    {
        return left;
    }

    if (x > max_x)
    {
        return max_x;
    }

    return x;
}

static void epd_write_text_clamped(OBDISP *display, GFXfont *font, int16_t x, int16_t y, char *text, uint8_t color, int16_t left, int16_t right)
{
    x = epd_clamp_text_x(font, text, x, left, right);
    obdWriteStringCustom(display, font, x, y, text, color);
}

static void epd_write_text_centered(OBDISP *display, GFXfont *font, int16_t left, int16_t right, int16_t y, char *text, uint8_t color)
{
    int16_t width = epd_get_text_width(font, text);
    int16_t x = left + ((right - left + 1 - width) / 2);

    epd_write_text_clamped(display, font, x, y, text, color, left, right);
}

static void epd_write_text_right(OBDISP *display, GFXfont *font, int16_t left, int16_t right, int16_t y, char *text, uint8_t color)
{
    int16_t width = epd_get_text_width(font, text);
    int16_t x = right - width + 1;

    epd_write_text_clamped(display, font, x, y, text, color, left, right);
}

_attribute_ram_code_ void epd_display(struct date_time _time, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial)
{
    uint8_t battery_level;
    uint16_t resolution_w = epd_width;
    uint16_t resolution_h = epd_height;
    uint16_t header_right = 0;
    uint16_t conn_x = 0;
    uint16_t red_bottom = 0;

    if (epd_update_state)
        return;

    if (!epd_model)
    {
        EPD_detect_model();
    }

    epd_get_current_resolution(&resolution_w, &resolution_h);
    header_right = resolution_w - 1;
    conn_x = (resolution_w > 48) ? (resolution_w - 48) : 1;
    red_bottom = (resolution_h > 7) ? (resolution_h - 7) : (resolution_h - 1);

    epd_clear();

    // Draw BLACK layer
    obdCreateVirtualDisplay(&obd, resolution_w, resolution_h, epd_temp);
    obdFill(&obd, 0, 0); // fill with white

    char buff[100];
    battery_level = get_battery_level(battery_mv);
    sprintf(buff, "THX_%02X%02X%02X %s", mac_public[2], mac_public[1], mac_public[0], epd_model_string[epd_model]);
    epd_write_text_clamped(&obd, (GFXfont *)&Dialog_plain_16, 1, 17, (char *)buff, 1, 1, header_right);
    sprintf(buff, "%s", BLE_conn_string[ble_get_connected()]);
    epd_write_text_clamped(&obd, (GFXfont *)&Dialog_plain_16, conn_x, 20, (char *)buff, 1, 1, header_right);

    sprintf(buff, "-----%d'C-----", epd_temperature);
    epd_write_text_centered(&obd, (GFXfont *)&Special_Elite_Regular_30, 0, header_right, 95, (char *)buff, 1);
    sprintf(buff, "Battery %dmV  %d%%", battery_mv, battery_level);
    epd_write_text_clamped(&obd, (GFXfont *)&Dialog_plain_16, 10, 120, (char *)buff, 1, 0, header_right);

    FixBuffer(epd_temp, epd_buffer, resolution_w, resolution_h);

    // Draw RED layer
    obdFill(&obd, 0, 0); // fill with white

    obdRectangle(&obd, 0, 90, header_right, red_bottom, 1, 0);

    sprintf(buff, "%02d:%02d", _time.tm_hour, _time.tm_min);
    epd_write_text_centered(&obd, (GFXfont *)&DSEG14_Classic_Mini_Regular_40, 0, header_right, 65, (char *)buff, 1);

    FixBuffer(epd_temp, epd_buffer_red, resolution_w, resolution_h);
    EPD_Display(epd_buffer, epd_buffer_red, resolution_w * resolution_h / 8, full_or_partial);
}

_attribute_ram_code_ void epd_display_char(uint8_t data)
{
    uint16_t buffer_size = epd_get_current_buffer_size();
    int i;
    for (i = 0; i < buffer_size; i++)
    {
        epd_buffer[i] = data;
    }
    EPD_Display(epd_buffer, NULL, buffer_size, 1);
}

_attribute_ram_code_ void epd_clear(void)
{
    uint16_t sz = epd_get_current_buffer_size();
    if (!sz)
        sz = epd_buffer_size;
    memset(epd_buffer, 0x00, sz);
    memset(epd_buffer_red, 0x00, sz);
    memset(epd_temp, 0x00, sz);
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
        scene(_time, battery_mv, temperature, 1);
        epd_wait_update = 0;
        partial_refresh_count = 0;
    }

    else if (_time.tm_min != minute_refresh)
    {
        minute_refresh = _time.tm_min;
        if (partial_refresh_count >= PARTIAL_REFRESH_FULL_INTERVAL)
        {
            // Periodic full refresh to clear ghosting
            partial_refresh_count = 0;
            scene(_time, battery_mv, temperature, 1);
        }
        else
        {
            partial_refresh_count++;
            scene(_time, battery_mv, temperature, 0);
        }
    }
}

void epd_update(struct date_time _time, uint16_t battery_mv, int16_t temperature)
{
    switch (epd_scene)
    {
    case 0:
        if (image_store_has_images() && image_store_take_display_pending())
        {
            uint16_t buffer_size = image_store_get_plane_size();

            image_store_load_image(0, epd_buffer, epd_buffer_red, buffer_size);
            EPD_Display(epd_buffer, epd_buffer_red, buffer_size, 1);
        }
        break;
    case 1:
        update_time_scene(_time, battery_mv, temperature, epd_display);
        break;
    case 2:
        update_time_scene(_time, battery_mv, temperature, epd_display_time_with_date);
        break;
    case 3:
        if (image_store_has_images())
        {
            uint8_t count = image_store_get_image_count();
            uint16_t interval_seconds = image_store_get_interval_seconds();
            uint16_t buffer_size = image_store_get_plane_size();
            uint32_t now = get_unix_time();

            if (count == 0 || buffer_size == 0)
            {
                break;
            }

            if (interval_seconds == 0)
            {
                interval_seconds = 60;
            }

            if (image_store_take_display_pending())
            {
                slideshow_index = 0;
                slideshow_last_switch = now;
                image_store_load_image(slideshow_index, epd_buffer, epd_buffer_red, buffer_size);
                EPD_Display(epd_buffer, epd_buffer_red, buffer_size, 1);
                break;
            }

            if (now < slideshow_last_switch)
            {
                slideshow_last_switch = now;
            }

            if (!epd_update_state && (now - slideshow_last_switch) >= interval_seconds)
            {
                slideshow_last_switch = now;
                slideshow_index = (slideshow_index + 1) % count;
                image_store_load_image(slideshow_index, epd_buffer, epd_buffer_red, buffer_size);
                EPD_Display(epd_buffer, epd_buffer_red, buffer_size, 1);
            }
        }
        break;
    default:
        break;
    }
}

void epd_display_time_with_date(struct date_time _time, uint16_t battery_mv, int16_t temperature, uint8_t full_or_partial)
{
    uint16_t battery_level;
    uint16_t resolution_w = epd_width;
    uint16_t resolution_h = epd_height;
    uint16_t right = 0;
    uint16_t battery_left = 0;
    uint16_t battery_text_left = 0;
    uint16_t battery_text_right = 0;
    uint16_t info_x = 0;
    uint16_t info_width = 0;
    uint16_t divider_x = 0;
    uint16_t time_right = 0;

    if (!epd_model)
    {
        EPD_detect_model();
    }

    epd_get_current_resolution(&resolution_w, &resolution_h);

    if (resolution_h < 121)
    {
        epd_display(_time, battery_mv, temperature, full_or_partial);
        return;
    }

    right = resolution_w - 1;
    battery_left = (resolution_w > 44) ? (resolution_w - 44) : 0;
    battery_text_left = battery_left + 3;
    battery_text_right = (right > 3) ? (right - 3) : right;
    info_width = (resolution_w >= 296) ? 78 : 74;
    info_x = (resolution_w > info_width) ? (resolution_w - info_width) : 0;
    divider_x = (info_x > 4) ? (info_x - 4) : info_x;
    time_right = (divider_x > 4) ? (divider_x - 4) : right;

    // Clear all working buffers (black, red, temp)
    epd_clear();

    // Create a virtual monochrome drawing surface the size of the panel
    obdCreateVirtualDisplay(&obd, resolution_w, resolution_h, epd_temp);
    obdFill(&obd, 0, 0); // fill with white (1 = black pixel when finally inverted for panel)

    char buff[100];
    battery_level = get_battery_level(battery_mv);

    // Device identifier (partial MAC)
    sprintf(buff, "THX_%02X%02X%02X", mac_public[2], mac_public[1], mac_public[0]);
    epd_write_text_clamped(&obd, (GFXfont *)&Dialog_plain_16, 1, 17, (char *)buff, 1, 1, (battery_left > 5) ? (battery_left - 5) : right);

    // Battery icon rectangle
    obdRectangle(&obd, battery_left, 2, right, 22, 1, 1);

    // Battery percentage inside battery outline (drawn white on black fill)
    sprintf(buff, "%d", battery_level);
    epd_write_text_right(&obd, (GFXfont *)&Dialog_plain_16, battery_text_left, battery_text_right, 18, (char *)buff, 0);

    // Separator bar under header
    obdRectangle(&obd, 0, 25, right, 27, 1, 1);

    // Time (HH:MM) big segmented font
    sprintf(buff, "%02d:%02d", _time.tm_hour, _time.tm_min);
    epd_write_text_centered(&obd, (GFXfont *)&DSEG14_Classic_Mini_Regular_40, 0, time_right, 85, (char *)buff, 1);

    // Temperature (from EPD sensor, not the passed temperature param)
    sprintf(buff, "%d'C", epd_temperature);
    epd_write_text_right(&obd, (GFXfont *)&Dialog_plain_16, info_x, right, 50, (char *)buff, 1);

    // Small separator line under temperature
    obdRectangle(&obd, info_x, 60, right, 62, 1, 1);

    // Battery voltage in mV
    sprintf(buff, "%dmV", battery_mv);
    epd_write_text_right(&obd, (GFXfont *)&Dialog_plain_16, info_x, right, 84, (char *)buff, 1);

    // Vertical separator at right info block
    obdRectangle(&obd, divider_x, 27, divider_x + 2, 99, 1, 1);
    // Horizontal footer separator
    obdRectangle(&obd, 0, 97, right, 99, 1, 1);

    // Date (YYYY-MM-DD)
    sprintf(buff, "%d-%02d-%02d", _time.tm_year, _time.tm_month, _time.tm_day);
    epd_write_text_clamped(&obd, (GFXfont *)&Dialog_plain_16, 10, 120, (char *)buff, 1, 0, right);

    // Convert drawing buffer into panel memory layout
    FixBuffer(epd_temp, epd_buffer, resolution_w, resolution_h);

    // Send to panel (pass cleared red buffer to avoid ghosting on BWR panels)
    EPD_Display(epd_buffer, epd_buffer_red, (resolution_w * resolution_h) / 8, full_or_partial);
}

void epd_get_resolution(uint8_t model_nr, uint16_t *width, uint16_t *height)
{
    epd_get_resolution_for_model(model_nr, width, height);
}

void epd_get_current_resolution(uint16_t *width, uint16_t *height)
{
    epd_get_resolution_for_model(epd_model, width, height);
}

uint16_t epd_get_buffer_size_for_model(uint8_t model_nr)
{
    return epd_get_model_buffer_size(model_nr);
}

uint16_t epd_get_current_buffer_size(void)
{
    return epd_get_model_buffer_size(epd_model);
}
