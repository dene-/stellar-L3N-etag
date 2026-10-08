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
#include "ble.h"
#include "flash.h"
#include "image_store.h"
#include "epd_canvas.h"
#include "epd_scenes.h"

#include "TIFF_G4.h"

RAM uint8_t epd_model = 0; // 0 = Undetected, 1 = BW213, 2 = BWR213_PRO, 3 = BWR154, 4 = BW213ICE, 5 = BWR290/BWR296
const char *epd_model_string[] = {"NC", "BW213", "BWR213", "BWR154", "213ICE", "BWR290"};
RAM uint8_t epd_update_state = 0;

RAM uint8_t epd_scene = EPD_SCENE_DASHBOARD;
RAM uint8_t epd_wait_update = 1; // first refresh after boot/OTA must be a full one: panel RAM is blank, a partial LUT draws nothing useful

RAM uint8_t minute_refresh = 100;
RAM uint8_t partial_refresh_count = 0;
#define PARTIAL_REFRESH_FULL_INTERVAL 10 // Force full refresh every N partial updates

// Fingerprint of the clock scene frame currently on the panel, so unchanged frames are skipped and
// red-plane changes (which partial refreshes cannot draw) get a full refresh. Any other display
// path clears epd_shown_valid.
RAM uint8_t epd_shown_valid = 0;
RAM uint32_t epd_shown_black_hash = 0;
RAM uint32_t epd_shown_red_hash = 0;

RAM uint8_t epd_temperature_is_read = 0;
RAM int8_t epd_temperature = 0;
RAM uint32_t epd_temperature_read_time = 0;
#define EPD_TEMP_CACHE_SECONDS 300 // Cache temperature for 5 minutes

RAM uint8_t epd_buffer[epd_buffer_size];
uint8_t epd_buffer_red[epd_buffer_size];
uint8_t epd_temp[epd_buffer_size]; // red plane staging for raw uploads over the EPD BLE service
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

// Selects the display driver and remembers it in settings (persisted by the caller);
// EPD_MODEL_AUTO detects the panel on next use.
void set_EPD_model(uint8_t model_nr)
{
    epd_model = model_nr;
    settings.epd_model = model_nr;
    epd_temperature_is_read = 0;
    epd_temperature_read_time = 0;
    set_EPD_wait_flush(); // new resolution: redraw the scene with a full refresh
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
    if ((scene == EPD_SCENE_IMAGE || scene == EPD_SCENE_SLIDESHOW) && epd_scene != EPD_SCENE_IMAGE && epd_scene != EPD_SCENE_SLIDESHOW && !epd_update_state)
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

// Auto-detection (model 0 only). It cannot identify SSD1680-based panels: the SSD1680 has no
// "read LUT" command (0x33), so both LUT tests below fail, and its status register (0x2F) reads
// 0x01 like the 2.13" ICE, so a 2.9" BWR296 comes out as model 4. Those tags need the model set
// explicitly; settings.epd_model defaults to EPD_DEFAULT_MODEL.
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
    epd_shown_valid = 0;

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

_attribute_ram_code_ void epd_display_char(uint8_t data)
{
    uint16_t buffer_size = epd_get_current_buffer_size();

    memset(epd_buffer, data, buffer_size);
    EPD_Display(epd_buffer, NULL, buffer_size, 1);
}

// Clears both planes to white.
_attribute_ram_code_ void epd_clear(void)
{
    memset(epd_buffer, 0xFF, epd_buffer_size);
    memset(epd_buffer_red, 0x00, epd_buffer_size);
}

static uint8_t epd_model_has_red(uint8_t model_nr)
{
    return model_nr == 2 || model_nr == 3 || model_nr == 5;
}

// FNV-1a
static uint32_t epd_hash(const uint8_t *data, uint16_t size)
{
    uint32_t hash = 2166136261u;

    while (size--)
    {
        hash ^= *data++;
        hash *= 16777619u;
    }
    return hash;
}

static void epd_fill_scene_data(epd_scene_data_t *data, struct date_time time, uint16_t battery_mv, int16_t temperature)
{
    memset(data, 0, sizeof(*data));
    data->time = time;
    data->time_valid = (time.tm_year != 0); // date_time stays zeroed until set_time()
    data->temperature_c = (int8_t)temperature;
    data->battery_mv = battery_mv;
    data->battery_percent = get_battery_level(battery_mv);
    data->ble_connected = ble_get_connected();
    sprintf(data->device_name, "THX_%02X%02X%02X", mac_public[2], mac_public[1], mac_public[0]);
}

// Redraws a clock scene once a minute (or when forced) and refreshes the panel only if the frame changed.
static void epd_update_clock_scene(epd_scene_draw_fn draw, struct date_time time, uint16_t battery_mv, int16_t temperature)
{
    epd_scene_data_t data;
    epd_canvas_t canvas;
    uint16_t width = epd_width;
    uint16_t height = epd_height;
    uint16_t size;
    uint32_t black_hash, red_hash;
    uint8_t full;

    if (epd_update_state)
        return;
    if (!epd_wait_update && time.tm_min == minute_refresh)
        return;
    minute_refresh = time.tm_min;

    if (!epd_model)
        EPD_detect_model();
    epd_get_current_resolution(&width, &height);
    size = (uint16_t)(width * height / 8);

    epd_fill_scene_data(&data, time, battery_mv, temperature);
    epd_canvas_init(&canvas, epd_buffer, epd_buffer_red, width, height, epd_model_has_red(epd_model));
    draw(&canvas, &data);

    black_hash = epd_hash(epd_buffer, size);
    red_hash = epd_hash(epd_buffer_red, size);
    if (!epd_wait_update && epd_shown_valid && black_hash == epd_shown_black_hash && red_hash == epd_shown_red_hash)
        return;

    // Red needs the full waveform; partial updates also need a periodic full one against ghosting.
    full = epd_wait_update || !epd_shown_valid || red_hash != epd_shown_red_hash ||
           partial_refresh_count >= PARTIAL_REFRESH_FULL_INTERVAL;
    partial_refresh_count = full ? 0 : partial_refresh_count + 1;
    epd_wait_update = 0;

    EPD_Display(epd_buffer, epd_buffer_red, size, full);
    epd_shown_valid = 1;
    epd_shown_black_hash = black_hash;
    epd_shown_red_hash = red_hash;
}

void epd_update(struct date_time _time, uint16_t battery_mv, int16_t temperature)
{
    switch (epd_scene)
    {
    case EPD_SCENE_IMAGE:
        if (image_store_has_images() && image_store_take_display_pending())
        {
            uint16_t buffer_size = image_store_get_plane_size();

            image_store_load_image(0, epd_buffer, epd_buffer_red, buffer_size);
            EPD_Display(epd_buffer, epd_buffer_red, buffer_size, 1);
        }
        break;
    case EPD_SCENE_CLOCK:
        epd_update_clock_scene(epd_scene_draw_clock, _time, battery_mv, temperature);
        break;
    case EPD_SCENE_DASHBOARD:
        epd_update_clock_scene(epd_scene_draw_dashboard, _time, battery_mv, temperature);
        break;
    case EPD_SCENE_SLIDESHOW:
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
