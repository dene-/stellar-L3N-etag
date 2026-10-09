#pragma once
#include <stdint.h>

// Storing images uploaded over BLE (command E5) for the image and slideshow scenes.

// Prepares the store for image_count images drawn for the panel model and sets the slideshow
// interval; 0 if the model is unknown or the images don't fit. From here until finish, abort or
// clear the stored images are gone and chunks are accepted.
uint8_t image_upload_begin(uint8_t model, uint16_t interval_seconds, uint8_t image_count);
// Stores a chunk; 0 unless an upload is active (and the chunk is valid).
uint8_t image_upload_write(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length);
// Commits the upload and shows it: the image scene for one image, the slideshow for several. With
// check_crc the CRC-32 of the stored image bytes (see image_store_finalize) must equal crc.
// Returns IMAGE_STORE_COMMIT_*: on a CRC mismatch nothing is committed and the scene is the dashboard.
uint8_t image_upload_finish(uint8_t check_crc, uint32_t crc);
// Drops an upload that was cut off (connection lost) and returns to the dashboard; no effect
// without an active upload.
void image_upload_abort(void);
// Deletes the stored images and returns to the dashboard.
void image_upload_clear(void);
// Number of stored images, 0 if there are none.
uint8_t image_upload_stored_count(void);
// Seconds between slideshow images (0 = SLIDESHOW_DEFAULT_INTERVAL_SECONDS).
uint16_t image_upload_interval(void);
// Changes the slideshow interval (a setting, stored with the others).
void image_upload_set_interval(uint16_t interval_seconds);
