#pragma once
#include <stdint.h>

// Uploaded images kept in MCU flash (infrastructure/storage/image_store): a header plus, per image,
// a black and a red plane of plane_size bytes.
#define IMAGE_STORE_MAX_COUNT 23

// Erases the store for image_count images for a width x height panel; 0 if they don't fit.
uint8_t image_store_prepare(uint8_t model, uint16_t width, uint16_t height, uint16_t plane_size,
                            uint16_t interval_seconds, uint8_t image_count);
uint8_t image_store_write_chunk(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length);
// Commits the prepared header; the images become available and pending display.
uint8_t image_store_finalize(void);
void image_store_clear(void);

uint8_t image_store_has_images(void);
uint8_t image_store_get_image_count(void);
uint16_t image_store_get_interval_seconds(void);
uint16_t image_store_get_plane_size(void);
// Returns 1 once after new images were stored (or found at boot).
uint8_t image_store_take_display_pending(void);
void image_store_load_image(uint8_t image_index, uint8_t *black_buffer, uint8_t *red_buffer, uint16_t buffer_size);
