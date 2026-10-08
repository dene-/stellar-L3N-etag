#pragma once
#include <stdint.h>

// Storing images uploaded over BLE (command E5) for the image and slideshow scenes.

// Prepares the store for image_count images drawn for the panel model; 0 if the model is unknown
// or the images don't fit.
uint8_t image_upload_begin(uint8_t model, uint16_t interval_seconds, uint8_t image_count);
uint8_t image_upload_write(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length);
// Commits the upload and shows it: the image scene for one image, the slideshow for several.
uint8_t image_upload_finish(void);
// Deletes the stored images and returns to the dashboard.
void image_upload_clear(void);
// Number of stored images, 0 if there are none.
uint8_t image_upload_stored_count(void);
// Seconds between slideshow images (0 = SLIDESHOW_DEFAULT_INTERVAL_SECONDS).
uint16_t image_upload_interval(void);
// Changes the slideshow interval of the stored images; 0 if there are none.
uint8_t image_upload_set_interval(uint16_t interval_seconds);
