#pragma once
#include <stdint.h>

// Uploaded images kept in MCU flash (infrastructure/storage/image_store): a header plus, per image,
// a black and a red plane of plane_size bytes.
#define IMAGE_STORE_MAX_COUNT 23

// Starts an upload of image_count images for a width x height panel, dropping the stored ones; 0 if
// they don't fit. Flash is erased as the chunks reach it, not up front. The stored header is gone
// from here on, so an upload that never finishes leaves no images, not a mix of old and new.
// interval_seconds goes into the header only for compatibility; the slideshow interval is a setting.
uint8_t image_store_prepare(uint8_t model, uint16_t width, uint16_t height, uint16_t plane_size,
                            uint16_t interval_seconds, uint8_t image_count);
// Only while an upload is active (from a successful prepare until finalize, clear or abort); 0 otherwise.
uint8_t image_store_write_chunk(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length);
uint8_t image_store_upload_active(void);

enum
{
    IMAGE_STORE_COMMIT_FAILED = 0,       // no upload active
    IMAGE_STORE_COMMIT_OK = 1,
    IMAGE_STORE_COMMIT_CRC_MISMATCH = 2, // the data in flash is not what the sender meant; nothing committed
};
// Commits the prepared header; the images become available and pending display. Bytes no chunk
// wrote read as 0xFF, so an upload may leave out chunks that are all 0xFF. With check_crc, the
// CRC-32 (domain/crc32.h) of all image bytes in flash, black then red plane of each image in turn,
// must equal crc before the header is written; otherwise the store stays empty.
uint8_t image_store_finalize(uint8_t check_crc, uint32_t crc);
// Ends an upload that will not finish (connection lost); the store stays empty.
void image_store_abort(void);
void image_store_clear(void);

uint8_t image_store_has_images(void);
uint8_t image_store_get_image_count(void);
// The interval the stored header carries, written by firmware before it became a setting; only read
// to carry that value over (see device_settings_load). 0 if no images are stored.
uint16_t image_store_get_interval_seconds(void);
uint16_t image_store_get_plane_size(void);
// Returns 1 once after new images were stored (or found at boot).
uint8_t image_store_take_display_pending(void);
void image_store_load_image(uint8_t image_index, uint8_t *black_buffer, uint8_t *red_buffer, uint16_t buffer_size);
