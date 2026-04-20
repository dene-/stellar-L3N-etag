#pragma once

#include <stdint.h>

#define IMAGE_STORE_MAX_COUNT 23

void image_store_init(void);
void image_store_clear(void);
uint8_t image_store_prepare(uint8_t model, uint16_t interval_seconds, uint8_t image_count);
uint8_t image_store_write_chunk(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length);
uint8_t image_store_finalize(void);

uint8_t image_store_has_images(void);
uint8_t image_store_get_image_count(void);
uint16_t image_store_get_interval_seconds(void);
uint8_t image_store_get_model(void);
uint16_t image_store_get_plane_size(void);
uint8_t image_store_take_display_pending(void);
void image_store_load_image(uint8_t image_index, uint8_t *black_buffer, uint8_t *red_buffer, uint16_t buffer_size);