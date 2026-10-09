#include "application/image_upload.h"
#include "application/device_settings.h"
#include "application/screen.h"
#include "application/ports/image_storage.h"
#include "domain/panel.h"

uint8_t image_upload_begin(uint8_t model, uint16_t interval_seconds, uint8_t image_count)
{
    const panel_t *panel = panel_find(model);

    if (!panel)
        return 0;
    if (!image_store_prepare(model, panel->width, panel->height, panel_plane_bytes(panel), interval_seconds,
                             image_count))
        return 0;
    device_settings_set_slideshow_interval(interval_seconds);
    return 1;
}

uint8_t image_upload_write(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length)
{
    return image_store_write_chunk(image_index, plane, offset, data, length);
}

uint8_t image_upload_finish(uint8_t check_crc, uint32_t crc)
{
    uint8_t result = image_store_finalize(check_crc, crc);

    if (result == IMAGE_STORE_COMMIT_OK)
        screen_set_scene(image_store_get_image_count() > 1 ? SCREEN_SCENE_SLIDESHOW : SCREEN_SCENE_IMAGE);
    else if (result == IMAGE_STORE_COMMIT_CRC_MISMATCH)
        screen_set_scene(SCREEN_SCENE_DASHBOARD); // the old images are gone too
    return result;
}

void image_upload_abort(void)
{
    if (!image_store_upload_active())
        return;
    image_store_abort();
    screen_set_scene(SCREEN_SCENE_DASHBOARD);
}

void image_upload_clear(void)
{
    image_store_clear();
    screen_set_scene(SCREEN_SCENE_DASHBOARD);
}

uint8_t image_upload_stored_count(void)
{
    return image_store_get_image_count();
}

uint16_t image_upload_interval(void)
{
    return device_settings_slideshow_interval();
}

void image_upload_set_interval(uint16_t interval_seconds)
{
    device_settings_set_slideshow_interval(interval_seconds);
}
