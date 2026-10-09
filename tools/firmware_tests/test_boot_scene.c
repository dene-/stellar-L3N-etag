// screen_restore_scene: which scene the tag shows after a reset.
#include "application/device_settings.h"
#include "application/image_upload.h"
#include "application/ports/image_storage.h"
#include "application/screen.h"
#include "check.h"
#include "domain/panel.h"
#include "fakes.h"

// A boot with `stored` in the settings and `images` images in the store (0 = none).
static uint8_t boot(uint8_t stored, uint8_t images)
{
    fakes_reset();
    if (images)
    {
        CHECK_EQ(image_upload_begin(PANEL_MODEL_BWR296, 30, images), 1);
        CHECK_EQ(image_upload_finish(0, 0), IMAGE_STORE_COMMIT_OK);
    }
    device_settings_set_scene(stored);
    screen_restore_scene();
    return screen_scene();
}

int main(void)
{
    // The stored scene comes back.
    CHECK_EQ(boot(SCREEN_SCENE_CLOCK, 0), SCREEN_SCENE_CLOCK);
    CHECK_EQ(boot(SCREEN_SCENE_DASHBOARD, 2), SCREEN_SCENE_DASHBOARD);
    CHECK_EQ(boot(SCREEN_SCENE_IMAGE, 2), SCREEN_SCENE_IMAGE);
    CHECK_EQ(boot(SCREEN_SCENE_SLIDESHOW, 3), SCREEN_SCENE_SLIDESHOW);

    // Image scenes without images cannot be shown.
    CHECK_EQ(boot(SCREEN_SCENE_IMAGE, 0), SCREEN_SCENE_DASHBOARD);
    CHECK_EQ(boot(SCREEN_SCENE_SLIDESHOW, 0), SCREEN_SCENE_DASHBOARD);

    // Settings of older firmware have no scene: it follows the images.
    CHECK_EQ(boot(DEVICE_SETTINGS_SCENE_UNSET, 0), SCREEN_SCENE_DASHBOARD);
    CHECK_EQ(boot(DEVICE_SETTINGS_SCENE_UNSET, 1), SCREEN_SCENE_IMAGE);
    CHECK_EQ(boot(DEVICE_SETTINGS_SCENE_UNSET, 2), SCREEN_SCENE_SLIDESHOW);

    // The chosen scene is stored, so the next boot does not repeat the guess.
    CHECK_EQ(device_settings_scene(), SCREEN_SCENE_SLIDESHOW);

    return check_report();
}
