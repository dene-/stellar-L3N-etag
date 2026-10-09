// image_upload together with screen: uploading, showing, cycling and clearing images.
#include "application/display.h"
#include "application/image_upload.h"
#include "application/ports/image_storage.h"
#include "application/screen.h"
#include "check.h"
#include "fakes.h"

#define BWR296_PLANE_BYTES 4736
#define INTERVAL 30

static void finish_refresh(void)
{
    fake_panel_idle = 1;
    display_poll();
}

// Chunks are only accepted between a successful begin and finish/abort/clear.
static void test_chunks_need_an_active_upload(void)
{
    static const uint8_t chunk[16] = {0};

    fakes_reset();
    fake_detect_model = PANEL_MODEL_BWR296;
    CHECK_EQ(image_upload_write(0, 0, 0, chunk, sizeof(chunk)), 0);

    CHECK_EQ(image_upload_begin(PANEL_MODEL_BWR296, INTERVAL, 1), 1);
    CHECK_EQ(image_upload_write(0, 0, 0, chunk, sizeof(chunk)), 1);
    CHECK_EQ(image_upload_finish(0, 0), IMAGE_STORE_COMMIT_OK);
    CHECK_EQ(image_upload_write(0, 0, 0, chunk, sizeof(chunk)), 0);

    CHECK_EQ(image_upload_begin(PANEL_MODEL_BWR296, INTERVAL, 1), 1);
    image_upload_clear();
    CHECK_EQ(image_upload_write(0, 0, 0, chunk, sizeof(chunk)), 0);
}

// A connection lost mid-upload leaves no images and the dashboard; without an upload it changes nothing.
static void test_abort_drops_the_upload_and_shows_the_dashboard(void)
{
    fakes_reset();
    fake_detect_model = PANEL_MODEL_BWR296;

    image_upload_abort();
    CHECK_EQ(fake_store_abort_calls, 0);

    screen_set_scene(SCREEN_SCENE_CLOCK);
    CHECK_EQ(image_upload_begin(PANEL_MODEL_BWR296, INTERVAL, 2), 1);
    image_upload_abort();
    CHECK_EQ(fake_store_abort_calls, 1);
    CHECK_EQ(image_upload_stored_count(), 0);
    CHECK_EQ(device_settings_scene(), SCREEN_SCENE_DASHBOARD);

    // A finished upload is not touched by a later disconnect.
    CHECK_EQ(image_upload_begin(PANEL_MODEL_BWR296, INTERVAL, 1), 1);
    CHECK_EQ(image_upload_finish(0, 0), IMAGE_STORE_COMMIT_OK);
    image_upload_abort();
    CHECK_EQ(fake_store_abort_calls, 1);
    CHECK_EQ(image_upload_stored_count(), 1);
    CHECK_EQ(device_settings_scene(), SCREEN_SCENE_IMAGE);
}

// The CRC given at commit is compared with the stored data; a mismatch commits nothing.
static void test_crc_mismatch_leaves_no_images_and_the_dashboard(void)
{
    fakes_reset();
    fake_detect_model = PANEL_MODEL_BWR296;
    fake_store_data_crc = 0xCBF43926u;

    CHECK_EQ(image_upload_begin(PANEL_MODEL_BWR296, INTERVAL, 1), 1);
    CHECK_EQ(image_upload_finish(1, 0xCBF43927u), IMAGE_STORE_COMMIT_CRC_MISMATCH);
    CHECK_EQ(fake_store_finalize_checked, 1);
    CHECK_EQ(image_upload_stored_count(), 0);
    CHECK_EQ(device_settings_scene(), SCREEN_SCENE_DASHBOARD);
    // The upload is over: more chunks are refused.
    CHECK_EQ(image_upload_write(0, 0, 0, (const uint8_t[]){0}, 1), 0);

    CHECK_EQ(image_upload_begin(PANEL_MODEL_BWR296, INTERVAL, 1), 1);
    CHECK_EQ(image_upload_finish(1, 0xCBF43926u), IMAGE_STORE_COMMIT_OK);
    CHECK_EQ(image_upload_stored_count(), 1);
    CHECK_EQ(device_settings_scene(), SCREEN_SCENE_IMAGE);
}

int main(void)
{
    static const uint8_t chunk[16] = {0};
    int calls;

    fakes_reset();
    fake_detect_model = PANEL_MODEL_BWR296;
    fake_utc = 1791467100u; // 2026-10-08 13:45 UTC
    fake_uptime = 1000;

    // Unknown models are rejected before the store is touched.
    CHECK_EQ(image_upload_begin(PANEL_MODEL_AUTO, INTERVAL, 2), 0);
    CHECK_EQ(image_upload_begin(PANEL_MODEL_COUNT, INTERVAL, 2), 0);
    CHECK_EQ(fake_store_prepare_calls, 0);

    CHECK_EQ(image_upload_begin(PANEL_MODEL_BWR296, INTERVAL, 2), 1);
    CHECK_EQ(fake_store_prepare_calls, 1);
    CHECK_EQ(fake_store_model, PANEL_MODEL_BWR296);
    CHECK_EQ(fake_store_width, 296);
    CHECK_EQ(fake_store_height, 128);
    CHECK_EQ(fake_store_plane_size, BWR296_PLANE_BYTES);
    CHECK_EQ(image_upload_interval(), INTERVAL);
    CHECK_EQ(fake_store_count, 2);

    CHECK_EQ(image_upload_write(0, 0, 0, chunk, sizeof(chunk)), 1);
    CHECK_EQ(image_upload_write(1, 1, 0, chunk, sizeof(chunk)), 1);
    CHECK_EQ(image_upload_finish(0, 0), IMAGE_STORE_COMMIT_OK);
    CHECK_EQ(device_settings_scene(), SCREEN_SCENE_SLIDESHOW);

    // The first image is shown in full as soon as the panel is idle.
    finish_refresh();
    calls = fake_refresh_calls;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls + 1);
    CHECK_EQ(fake_store_loaded_index, 0);
    CHECK_EQ(fake_refresh_black0, FAKE_IMAGE_BLACK_BASE + 0);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK_EQ(fake_refresh_size, BWR296_PLANE_BYTES);

    // Shown image stays until the interval passes, then the next one follows.
    finish_refresh();
    calls = fake_refresh_calls;
    fake_uptime += INTERVAL - 1;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls);

    fake_uptime += 1;
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls + 1);
    CHECK_EQ(fake_store_loaded_index, 1);
    CHECK_EQ(fake_refresh_black0, FAKE_IMAGE_BLACK_BASE + 1);
    CHECK_EQ(fake_refresh_full, 1);

    // Clearing deletes the images and brings the dashboard back with a full refresh.
    finish_refresh();
    calls = fake_refresh_calls;
    image_upload_clear();
    CHECK_EQ(fake_store_clear_calls, 1);
    screen_update(0, "THX_TEST");
    CHECK_EQ(fake_refresh_calls, calls + 1);
    CHECK_EQ(fake_refresh_full, 1);
    CHECK(fake_refresh_black0 != FAKE_IMAGE_BLACK_BASE + 0 && fake_refresh_black0 != FAKE_IMAGE_BLACK_BASE + 1);

    test_chunks_need_an_active_upload();
    test_abort_drops_the_upload_and_shows_the_dashboard();
    test_crc_mismatch_leaves_no_images_and_the_dashboard();

    return check_report();
}
