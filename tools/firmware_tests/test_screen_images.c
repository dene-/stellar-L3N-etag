// screen: image and slideshow scenes (stored images, redraws, plane size mismatch, slideshow timing,
// holding a raw frame). Images arrive through image_upload, as they do over BLE.
#include "application/device_settings.h"
#include "application/display.h"
#include "application/image_upload.h"
#include "application/screen.h"
#include "check.h"
#include "fakes.h"

#define BWR296_PLANE_BYTES 4736
#define BWR213_PLANE_BYTES 4000
#define INTERVAL 30

static void finish_refresh(void)
{
    fake_panel_idle = 1;
    display_poll();
}

// Fresh fakes, an idle panel of the given model, the dashboard scene with a redraw requested.
static void setup(uint8_t model)
{
    fake_panel_idle = 1;
    display_poll(); // ends a refresh left running by the previous test
    fakes_reset();
    device_settings_set_fast_refresh_enabled(0);
    display_select_model(model);
    screen_set_scene(SCREEN_SCENE_DASHBOARD);
    fake_utc = 1791467100u; // 2026-10-08 13:45 UTC
}

static void upload(uint8_t model, uint8_t count)
{
    CHECK_EQ(image_upload_begin(model, INTERVAL, count), 1);
    CHECK_EQ(image_upload_finish(), 1);
}

static void test_dashboard_to_image_is_one_full_refresh(void)
{
    int calls;

    setup(PANEL_MODEL_BWR296);
    device_settings_set_fast_refresh_enabled(1); // images are shown in full whatever the setting
    screen_update(0, "THX_TEST");
    finish_refresh();
    calls = fake_refresh_calls;

    upload(PANEL_MODEL_BWR296, 1);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls + 1);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK_EQ(fake_refresh_size, BWR296_PLANE_BYTES);
    CHECK_EQ(fake_refresh_black0, FAKE_IMAGE_BLACK_BASE + 0);

    // Shown once: nothing more to do until a redraw is requested.
    finish_refresh();
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls + 1);
    device_settings_set_fast_refresh_enabled(0);
}

static void test_image_scene_redraws_image_zero(void)
{
    setup(PANEL_MODEL_BWR296);
    upload(PANEL_MODEL_BWR296, 1);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    finish_refresh();

    screen_request_redraw();
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 2);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK_EQ(fake_store_loaded_index, 0);
    CHECK_EQ(fake_refresh_black0, FAKE_IMAGE_BLACK_BASE + 0);
    finish_refresh();

    // Leaving the scene and coming back shows the image again.
    screen_set_scene(SCREEN_SCENE_DASHBOARD);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 3);
    CHECK(fake_refresh_black0 != FAKE_IMAGE_BLACK_BASE + 0);
    finish_refresh();

    screen_set_scene(SCREEN_SCENE_IMAGE);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 4);
    CHECK_EQ(fake_refresh_black0, FAKE_IMAGE_BLACK_BASE + 0);
}

// Images stored for another panel size must not be pushed through this panel.
static void test_images_for_other_plane_size_are_not_shown(uint8_t count)
{
    setup(PANEL_MODEL_BWR296);
    upload(PANEL_MODEL_BWR213, count);
    CHECK_EQ(fake_store_plane_size, BWR213_PLANE_BYTES);

    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 0);
    CHECK_EQ(fake_store_load_calls, 0);

    // Once the panel fits the images they show up.
    screen_select_panel(PANEL_MODEL_BWR213);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    CHECK_EQ(fake_refresh_model, PANEL_MODEL_BWR213);
    CHECK_EQ(fake_refresh_size, BWR213_PLANE_BYTES);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK_EQ(fake_refresh_black0, FAKE_IMAGE_BLACK_BASE + 0);
    finish_refresh();

    // And switching back to the other size stops showing them again.
    screen_select_panel(PANEL_MODEL_BWR296);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    CHECK_EQ(fake_store_load_calls, 1);
}

// While the panel refreshes the image waits (and stays pending).
static void test_images_wait_for_running_refresh(uint8_t count)
{
    setup(PANEL_MODEL_BWR296);
    upload(PANEL_MODEL_BWR296, count);
    fake_panel_idle = 0;
    display_refresh(BWR296_PLANE_BYTES, 1); // something else is on the panel right now
    CHECK_EQ(fake_refresh_calls, 1);

    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    CHECK_EQ(fake_store_load_calls, 0);

    finish_refresh();
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 2);
    CHECK_EQ(fake_store_load_calls, 1);
    CHECK_EQ(fake_store_loaded_index, 0);
}

// The slideshow runs on uptime: a clock set by the phone must neither advance nor stall it.
static void test_slideshow_runs_on_uptime(void)
{
    setup(PANEL_MODEL_BWR296);
    fake_uptime = 1000;
    upload(PANEL_MODEL_BWR296, 3);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    CHECK_EQ(fake_store_loaded_index, 0);
    finish_refresh();

    fake_utc = 1000000000;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);

    fake_uptime = 1000 + INTERVAL - 1;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);

    fake_uptime = 1000 + INTERVAL;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 2);
    CHECK_EQ(fake_store_loaded_index, 1);
    CHECK_EQ(fake_refresh_black0, FAKE_IMAGE_BLACK_BASE + 1);
    finish_refresh();

    // A redraw shows the current image, not the first one.
    screen_request_redraw();
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 3);
    CHECK_EQ(fake_store_loaded_index, 1);
    CHECK_EQ(fake_refresh_full, 1);
    finish_refresh();

    // A new upload restarts the show at image 0 with a full interval ahead.
    fake_uptime = 2000;
    upload(PANEL_MODEL_BWR296, 3);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 4);
    CHECK_EQ(fake_store_loaded_index, 0);
    finish_refresh();

    fake_uptime = 2000 + INTERVAL - 1;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 4);
    fake_uptime = 2000 + INTERVAL;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 5);
    CHECK_EQ(fake_store_loaded_index, 1);
}

// A raw frame upload owns the panel until the scene is chosen again.
static void test_hold_frame_keeps_stored_image_off_the_panel(void)
{
    setup(PANEL_MODEL_BWR296);
    upload(PANEL_MODEL_BWR296, 1);
    screen_hold_frame();

    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 0);
    CHECK_EQ(fake_store_load_calls, 0);

    display_refresh(BWR296_PLANE_BYTES, 1); // the raw frame
    finish_refresh();
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    CHECK_EQ(fake_store_load_calls, 0);

    screen_set_scene(SCREEN_SCENE_IMAGE);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 2);
    CHECK_EQ(fake_store_load_calls, 1);
    CHECK_EQ(fake_refresh_black0, FAKE_IMAGE_BLACK_BASE + 0);
}

static void test_hold_frame_stops_clock_scene(void)
{
    setup(PANEL_MODEL_BWR296);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);
    finish_refresh();

    screen_hold_frame();
    fake_utc += 60;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 1);

    screen_set_scene(SCREEN_SCENE_DASHBOARD);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, 2);
}

int main(void)
{
    test_dashboard_to_image_is_one_full_refresh();
    test_image_scene_redraws_image_zero();
    test_images_for_other_plane_size_are_not_shown(1);
    test_images_for_other_plane_size_are_not_shown(2);
    test_images_wait_for_running_refresh(1);
    test_images_wait_for_running_refresh(2);
    test_slideshow_runs_on_uptime();
    test_hold_frame_keeps_stored_image_off_the_panel();
    test_hold_frame_stops_clock_scene();
    return check_report();
}
