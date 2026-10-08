#include "application/image_upload.h"
#include "application/screen.h"
#include "application/ports/image_storage.h"
#include "domain/panel.h"

uint8_t image_upload_begin(uint8_t model, uint16_t interval_seconds, uint8_t image_count)
{
    const panel_t *panel = panel_find(model);

    if (!panel)
        return 0;
    return image_store_prepare(model, panel->width, panel->height, panel_plane_bytes(panel), interval_seconds,
                               image_count);
}

uint8_t image_upload_write(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length)
{
    return image_store_write_chunk(image_index, plane, offset, data, length);
}

uint8_t image_upload_finish(void)
{
    if (!image_store_finalize())
        return 0;
    screen_set_scene(image_store_get_image_count() > 1 ? SCREEN_SCENE_SLIDESHOW : SCREEN_SCENE_IMAGE);
    return 1;
}

void image_upload_clear(void)
{
    image_store_clear();
    screen_set_scene(SCREEN_SCENE_DASHBOARD);
}
