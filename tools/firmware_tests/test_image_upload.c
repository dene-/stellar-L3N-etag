// image_upload together with screen: uploading, showing, cycling and clearing images.
#include "application/display.h"
#include "application/image_upload.h"
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

int main(void)
{
    static const uint8_t chunk[16] = {0};
    int calls;

    fakes_reset();
    fake_detect_model = PANEL_MODEL_BWR296;
    fake_date.tm_year = 2026;
    fake_date.tm_month = 10;
    fake_date.tm_day = 8;
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
    CHECK_EQ(fake_store_interval, INTERVAL);
    CHECK_EQ(fake_store_count, 2);

    CHECK_EQ(image_upload_write(0, 0, 0, chunk, sizeof(chunk)), 1);
    CHECK_EQ(image_upload_write(1, 1, 0, chunk, sizeof(chunk)), 1);
    CHECK_EQ(image_upload_finish(), 1);

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

    return check_report();
}
