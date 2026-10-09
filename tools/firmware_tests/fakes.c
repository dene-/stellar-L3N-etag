#include <string.h>
#include "fakes.h"
#include "application/ports/battery_sensor.h"
#include "application/ports/epd_panel.h"
#include "application/ports/image_storage.h"
#include "application/ports/settings_storage.h"
#include "application/ports/status_light.h"
#include "application/ports/telemetry_sink.h"
#include "application/ports/wall_clock.h"
#include "domain/panel.h"

uint32_t fake_utc;
uint16_t fake_utc_ms;
uint32_t fake_uptime;
uint16_t fake_uptime_ms;
int16_t fake_clock_trim;

uint8_t fake_detect_model;
int16_t fake_panel_temperature;
uint8_t fake_panel_idle;
int fake_detect_calls;
int fake_read_temperature_calls;
int fake_refresh_calls;
int fake_sleep_calls;
uint8_t fake_sleep_model;
uint8_t fake_selected_model;
uint8_t fake_refresh_model;
uint16_t fake_refresh_size;
uint8_t fake_refresh_full;
int fake_refresh_red_null;
uint8_t fake_refresh_black0;

int fake_store_prepare_calls;
uint8_t fake_store_model;
uint16_t fake_store_width;
uint16_t fake_store_height;
uint16_t fake_store_plane_size;
uint16_t fake_store_interval;
uint8_t fake_store_count;
int fake_store_clear_calls;
int fake_store_load_calls;
int fake_store_abort_calls;
uint8_t fake_store_loaded_index;
uint32_t fake_store_data_crc;
uint32_t fake_store_finalize_crc;
uint8_t fake_store_finalize_checked;
static uint8_t store_prepared;
static uint8_t store_has_images;
static uint8_t store_pending;

uint8_t fake_settings_valid;
uint8_t fake_settings_legacy;
device_settings_t fake_settings_stored;
int fake_settings_save_calls;

uint16_t fake_battery_mv;

int fake_publish_calls;
int16_t fake_publish_temperature_x10;
uint8_t fake_publish_percent;
uint16_t fake_publish_mv;

int fake_light_off_calls;
int fake_light_blink_calls;
int fake_light_last_color;
uint8_t fake_light_rainbow;
int fake_light_animate_calls;

void fakes_reset(void)
{
    fake_utc = 0;
    fake_utc_ms = 0;
    fake_uptime = 0;
    fake_uptime_ms = 0;
    fake_clock_trim = 0;

    fake_detect_model = PANEL_MODEL_BWR296;
    fake_panel_temperature = 200;
    fake_panel_idle = 0;
    fake_detect_calls = fake_read_temperature_calls = fake_refresh_calls = fake_sleep_calls = 0;
    fake_sleep_model = 0;
    fake_selected_model = 0xFF;
    fake_refresh_model = 0;
    fake_refresh_size = 0;
    fake_refresh_full = 0;
    fake_refresh_red_null = 0;
    fake_refresh_black0 = 0;

    fake_store_prepare_calls = fake_store_clear_calls = fake_store_load_calls = fake_store_abort_calls = 0;
    fake_store_model = 0;
    fake_store_width = fake_store_height = fake_store_plane_size = fake_store_interval = 0;
    fake_store_count = 0;
    fake_store_loaded_index = 0;
    fake_store_data_crc = fake_store_finalize_crc = 0;
    fake_store_finalize_checked = 0;
    store_prepared = store_has_images = store_pending = 0;

    fake_settings_valid = 0;
    fake_settings_legacy = 0;
    memset(&fake_settings_stored, 0, sizeof(fake_settings_stored));
    fake_settings_save_calls = 0;

    fake_battery_mv = 3000;

    fake_publish_calls = 0;
    fake_publish_temperature_x10 = 0;
    fake_publish_percent = 0;
    fake_publish_mv = 0;

    fake_light_off_calls = fake_light_blink_calls = fake_light_animate_calls = 0;
    fake_light_last_color = -1;
    fake_light_rainbow = 0;
}

// wall_clock
uint32_t wall_clock_uptime_seconds(void)
{
    return fake_uptime;
}

uint32_t wall_clock_uptime_ms(void)
{
    return fake_uptime * 1000 + fake_uptime_ms;
}

uint32_t wall_clock_utc(uint16_t *ms)
{
    if (ms)
        *ms = fake_utc_ms;
    return fake_utc;
}

void wall_clock_set_utc(uint32_t seconds, uint16_t ms)
{
    fake_utc = seconds;
    fake_utc_ms = ms;
}

void wall_clock_set_trim(int16_t trim)
{
    fake_clock_trim = trim;
}

// epd_panel
void epd_panel_select(uint8_t model)
{
    fake_selected_model = model;
}

uint8_t epd_panel_detect(void)
{
    fake_detect_calls++;
    return fake_detect_model;
}

int16_t epd_panel_read_temperature(uint8_t model)
{
    fake_read_temperature_calls++;
    return fake_panel_temperature;
}

int16_t epd_panel_refresh(uint8_t model, uint8_t *black, uint8_t *red, uint16_t size, uint8_t full)
{
    fake_refresh_calls++;
    fake_refresh_model = model;
    fake_refresh_size = size;
    fake_refresh_full = full;
    fake_refresh_red_null = (red == NULL);
    fake_refresh_black0 = black[0];
    return fake_panel_temperature;
}

uint8_t epd_panel_is_idle(uint8_t model)
{
    return fake_panel_idle;
}

void epd_panel_sleep(uint8_t model)
{
    fake_sleep_calls++;
    fake_sleep_model = model;
}

// image_storage
uint8_t image_store_prepare(uint8_t model, uint16_t width, uint16_t height, uint16_t plane_size,
                            uint16_t interval_seconds, uint8_t image_count)
{
    fake_store_prepare_calls++;
    fake_store_model = model;
    fake_store_width = width;
    fake_store_height = height;
    fake_store_plane_size = plane_size;
    fake_store_interval = interval_seconds;
    fake_store_count = image_count;
    store_has_images = store_pending = 0;
    store_prepared = (image_count != 0 && image_count <= IMAGE_STORE_MAX_COUNT);
    return store_prepared;
}

uint8_t image_store_write_chunk(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length)
{
    return store_prepared && image_index < fake_store_count && plane < 2 &&
           (uint32_t)offset + length <= fake_store_plane_size;
}

uint8_t image_store_finalize(uint8_t check_crc, uint32_t crc)
{
    if (!store_prepared)
        return IMAGE_STORE_COMMIT_FAILED;
    fake_store_finalize_checked = check_crc;
    fake_store_finalize_crc = crc;
    store_prepared = 0;
    if (check_crc && crc != fake_store_data_crc)
        return IMAGE_STORE_COMMIT_CRC_MISMATCH;
    store_has_images = 1;
    store_pending = 1;
    return IMAGE_STORE_COMMIT_OK;
}

static void drop_store(void)
{
    store_prepared = store_has_images = store_pending = 0;
    fake_store_count = 0;
    fake_store_plane_size = 0;
}

void image_store_abort(void)
{
    fake_store_abort_calls++;
    drop_store();
}

uint8_t image_store_upload_active(void)
{
    return store_prepared;
}

void image_store_clear(void)
{
    fake_store_clear_calls++;
    drop_store();
}

uint8_t image_store_has_images(void)
{
    return store_has_images;
}

uint8_t image_store_get_image_count(void)
{
    return store_has_images ? fake_store_count : 0;
}

uint16_t image_store_get_interval_seconds(void)
{
    return fake_store_interval;
}

uint16_t image_store_get_plane_size(void)
{
    return store_has_images ? fake_store_plane_size : 0;
}

uint8_t image_store_take_display_pending(void)
{
    uint8_t pending = store_pending;

    store_pending = 0;
    return pending;
}

void image_store_load_image(uint8_t image_index, uint8_t *black_buffer, uint8_t *red_buffer, uint16_t buffer_size)
{
    fake_store_load_calls++;
    fake_store_loaded_index = image_index;
    memset(black_buffer, FAKE_IMAGE_BLACK_BASE + image_index, buffer_size);
    memset(red_buffer, FAKE_IMAGE_RED_BASE + image_index, buffer_size);
}

// settings_storage
uint8_t settings_storage_load(device_settings_t *settings)
{
    uint8_t scene = settings->scene;
    uint16_t interval = settings->slideshow_interval;

    if (!fake_settings_valid)
        return SETTINGS_STORAGE_NONE;
    *settings = fake_settings_stored;
    if (fake_settings_legacy)
    {
        settings->scene = scene;
        settings->slideshow_interval = interval;
        return SETTINGS_STORAGE_LEGACY;
    }
    return SETTINGS_STORAGE_CURRENT;
}

void settings_storage_save(const device_settings_t *settings)
{
    fake_settings_save_calls++;
    fake_settings_stored = *settings;
    fake_settings_valid = 1;
}

// battery_sensor
uint16_t battery_sensor_read_mv(void)
{
    return fake_battery_mv;
}

// telemetry_sink
void telemetry_sink_publish(int16_t temperature_x10, uint8_t battery_percent, uint16_t battery_mv)
{
    fake_publish_calls++;
    fake_publish_temperature_x10 = temperature_x10;
    fake_publish_percent = battery_percent;
    fake_publish_mv = battery_mv;
}

// status_light
void status_light_off(void)
{
    fake_light_off_calls++;
}

void status_light_blink(status_light_color_t color)
{
    fake_light_blink_calls++;
    fake_light_last_color = color;
}

void status_light_set_rainbow(uint8_t enabled)
{
    fake_light_rainbow = enabled;
    if (!enabled)
        fake_light_off_calls++;
}

void status_light_animate(void)
{
    fake_light_animate_calls++;
}
