#include <stdint.h>

#include "tl_common.h"
#include "app_config.h"

#include "epd.h"
#include "flash.h"
#include "image_store.h"
#include "drivers/8258/flash.h"

#define IMAGE_STORE_MAGIC 0x534C4453UL
#define IMAGE_STORE_VERSION 1
#define IMAGE_STORE_BASE_ADDR 0x40000
#define IMAGE_STORE_DATA_ADDR 0x41000
#define IMAGE_STORE_END_ADDR 0x78000
#define IMAGE_STORE_TOTAL_DATA_BYTES (IMAGE_STORE_END_ADDR - IMAGE_STORE_DATA_ADDR)

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint8_t model;
  uint8_t image_count;
  uint16_t width;
  uint16_t height;
  uint16_t plane_size;
  uint16_t interval_seconds;
  uint32_t total_data_bytes;
  uint8_t checksum;
} image_store_header_t;

static RAM image_store_header_t image_store_header;
static RAM uint8_t image_store_ready = 0;
static RAM uint8_t image_store_display_pending = 0;

static uint8_t image_store_checksum(const image_store_header_t *header)
{
  const uint8_t *bytes = (const uint8_t *)header;
  uint8_t checksum = 0;
  unsigned int index;

  for (index = 0; index < sizeof(image_store_header_t) - 1; index++)
  {
    checksum ^= bytes[index];
  }

  return checksum;
}

static uint32_t image_store_image_stride(void)
{
  return (uint32_t)image_store_header.plane_size * 2;
}

static uint32_t image_store_image_address(uint8_t image_index, uint8_t plane, uint16_t offset)
{
  return IMAGE_STORE_DATA_ADDR + ((uint32_t)image_index * image_store_image_stride()) + ((uint32_t)plane * image_store_header.plane_size) + offset;
}

static uint8_t image_store_is_header_valid(const image_store_header_t *header)
{
  if (header->magic != IMAGE_STORE_MAGIC)
  {
    return 0;
  }

  if (header->version != IMAGE_STORE_VERSION)
  {
    return 0;
  }

  if (header->image_count == 0 || header->image_count > IMAGE_STORE_MAX_COUNT)
  {
    return 0;
  }

  if (header->plane_size == 0 || header->plane_size > epd_buffer_size)
  {
    return 0;
  }

  if (header->total_data_bytes == 0 || header->total_data_bytes > IMAGE_STORE_TOTAL_DATA_BYTES)
  {
    return 0;
  }

  if (header->checksum != image_store_checksum(header))
  {
    return 0;
  }

  return 1;
}

static void image_store_erase_all_blocks(void)
{
  uint32_t address;

  for (address = IMAGE_STORE_BASE_ADDR; address < IMAGE_STORE_END_ADDR; address += 0x8000)
  {
    flash_erase_32kblock(address);
  }
}

static void image_store_write_bytes(uint32_t address, const uint8_t *data, uint16_t length)
{
  uint16_t remaining = length;
  uint16_t cursor = 0;

  while (remaining)
  {
    uint16_t page_space = 256 - (address & 0xff);
    uint16_t chunk = remaining < page_space ? remaining : page_space;

    flash_write_page(address, chunk, (unsigned char *)(data + cursor));
    address += chunk;
    cursor += chunk;
    remaining -= chunk;
  }
}

void image_store_init(void)
{
  flash_read_page(IMAGE_STORE_BASE_ADDR, sizeof(image_store_header_t), (uint8_t *)&image_store_header);

  if (image_store_is_header_valid(&image_store_header))
  {
    image_store_ready = 1;
    image_store_display_pending = 1;
  }
  else
  {
    memset(&image_store_header, 0, sizeof(image_store_header));
    image_store_ready = 0;
    image_store_display_pending = 0;
  }
}

void image_store_clear(void)
{
  image_store_erase_all_blocks();
  memset(&image_store_header, 0, sizeof(image_store_header));
  image_store_ready = 0;
  image_store_display_pending = 0;
}

uint8_t image_store_prepare(uint8_t model, uint16_t interval_seconds, uint8_t image_count)
{
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t plane_size = 0;
  uint32_t total_data_bytes = 0;

  if (image_count == 0 || image_count > IMAGE_STORE_MAX_COUNT)
  {
    return 0;
  }

  epd_get_resolution(model, &width, &height);
  plane_size = epd_get_buffer_size_for_model(model);

  if (!width || !height || !plane_size)
  {
    return 0;
  }

  total_data_bytes = (uint32_t)plane_size * 2 * image_count;
  if (total_data_bytes > IMAGE_STORE_TOTAL_DATA_BYTES)
  {
    return 0;
  }

  // Erase only after all validation passes, so we don't lose existing
  // images if the new configuration turns out to be invalid.
  image_store_erase_all_blocks();
  memset(&image_store_header, 0, sizeof(image_store_header));
  image_store_header.magic = IMAGE_STORE_MAGIC;
  image_store_header.version = IMAGE_STORE_VERSION;
  image_store_header.model = model;
  image_store_header.image_count = image_count;
  image_store_header.width = width;
  image_store_header.height = height;
  image_store_header.plane_size = plane_size;
  image_store_header.interval_seconds = interval_seconds;
  image_store_header.total_data_bytes = total_data_bytes;
  image_store_ready = 0;
  image_store_display_pending = 0;

  return 1;
}

uint8_t image_store_write_chunk(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length)
{
  uint32_t address;

  if (image_store_header.image_count == 0)
  {
    return 0;
  }

  if (image_index >= image_store_header.image_count || plane > 1)
  {
    return 0;
  }

  if ((uint32_t)offset + length > image_store_header.plane_size)
  {
    return 0;
  }

  address = image_store_image_address(image_index, plane, offset);
  if (address + length > IMAGE_STORE_END_ADDR)
  {
    return 0;
  }

  image_store_write_bytes(address, data, length);
  return 1;
}

uint8_t image_store_finalize(void)
{
  if (image_store_header.image_count == 0)
  {
    return 0;
  }

  image_store_header.checksum = image_store_checksum(&image_store_header);
  image_store_write_bytes(IMAGE_STORE_BASE_ADDR, (const uint8_t *)&image_store_header, sizeof(image_store_header));
  image_store_ready = 1;
  image_store_display_pending = 1;
  return 1;
}

uint8_t image_store_has_images(void)
{
  return image_store_ready;
}

uint8_t image_store_get_image_count(void)
{
  return image_store_ready ? image_store_header.image_count : 0;
}

uint16_t image_store_get_interval_seconds(void)
{
  return image_store_ready ? image_store_header.interval_seconds : 0;
}

uint8_t image_store_get_model(void)
{
  return image_store_ready ? image_store_header.model : 0;
}

uint16_t image_store_get_plane_size(void)
{
  return image_store_ready ? image_store_header.plane_size : 0;
}

uint8_t image_store_take_display_pending(void)
{
  if (!image_store_display_pending)
  {
    return 0;
  }

  image_store_display_pending = 0;
  return 1;
}

void image_store_load_image(uint8_t image_index, uint8_t *black_buffer, uint8_t *red_buffer, uint16_t buffer_size)
{
  if (!image_store_ready || image_index >= image_store_header.image_count)
  {
    return;
  }

  if (buffer_size > image_store_header.plane_size)
  {
    buffer_size = image_store_header.plane_size;
  }

  flash_read_page(image_store_image_address(image_index, 0, 0), buffer_size, black_buffer);
  flash_read_page(image_store_image_address(image_index, 1, 0), buffer_size, red_buffer);
}