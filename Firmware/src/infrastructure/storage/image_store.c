#include <stdint.h>

#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/blt_config.h"
#include "infrastructure/storage/image_store.h"
#include "domain/crc32.h"
#include "domain/panel.h"
#include "sections.h"

#define IMAGE_STORE_MAGIC 0x534C4453UL
#define IMAGE_STORE_VERSION 1
#define IMAGE_STORE_BASE_ADDR 0x40000
#define IMAGE_STORE_DATA_ADDR 0x41000
// Ends below the SDK's MAC address and calibration sectors (0x76000, 0x77000); settings are at 0x78100.
#define IMAGE_STORE_END_ADDR CFG_ADR_MAC
#define IMAGE_STORE_TOTAL_DATA_BYTES (IMAGE_STORE_END_ADDR - IMAGE_STORE_DATA_ADDR)
#define IMAGE_STORE_SECTOR_SIZE 0x1000
#define IMAGE_STORE_DATA_SECTORS (IMAGE_STORE_TOTAL_DATA_BYTES / IMAGE_STORE_SECTOR_SIZE)

typedef char image_store_end_check[(CFG_ADR_MAC == 0x76000 && CUST_CAP_INFO_ADDR == 0x77000) ? 1 : -1];

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
// Between a successful image_store_prepare() and the commit or abort of that upload.
static RAM uint8_t image_store_upload_active_flag = 0;
// Data sectors erased since image_store_prepare(), one bit each. An upload erases a sector when its
// first chunk arrives instead of erasing the whole store up front, which took seconds.
static RAM uint32_t image_store_erased_sectors[(IMAGE_STORE_DATA_SECTORS + 31) / 32];

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

  if (header->plane_size == 0 || header->plane_size > PANEL_MAX_PLANE_BYTES)
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

  for (address = IMAGE_STORE_BASE_ADDR; address < IMAGE_STORE_END_ADDR;)
  {
    if (address % 0x8000 == 0 && address + 0x8000 <= IMAGE_STORE_END_ADDR)
    {
      flash_erase_32kblock(address);
      address += 0x8000;
    }
    else
    {
      flash_erase_sector(address);
      address += 0x1000;
    }
  }
}

static void image_store_erase_data_sector_once(uint32_t sector)
{
  uint32_t bit = 1UL << (sector % 32);

  if (image_store_erased_sectors[sector / 32] & bit)
  {
    return;
  }
  flash_erase_sector(IMAGE_STORE_DATA_ADDR + sector * IMAGE_STORE_SECTOR_SIZE);
  image_store_erased_sectors[sector / 32] |= bit;
}

// Erases the data sectors in [address, address + length) this upload has not erased yet.
static void image_store_erase_data_once(uint32_t address, uint32_t length)
{
  uint32_t sector;

  if (!length)
  {
    return;
  }
  for (sector = (address - IMAGE_STORE_DATA_ADDR) / IMAGE_STORE_SECTOR_SIZE;
       sector <= (address + length - 1 - IMAGE_STORE_DATA_ADDR) / IMAGE_STORE_SECTOR_SIZE; sector++)
  {
    image_store_erase_data_sector_once(sector);
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

// CRC-32 of the image bytes as stored (domain/crc32.h), read back in pages.
static uint32_t image_store_data_crc(void)
{
  uint8_t page[256];
  uint32_t crc = 0;
  uint32_t address = IMAGE_STORE_DATA_ADDR;
  uint32_t remaining = image_store_header.total_data_bytes;

  while (remaining)
  {
    uint16_t chunk = remaining < sizeof(page) ? (uint16_t)remaining : (uint16_t)sizeof(page);

    flash_read_page(address, chunk, page);
    crc = crc32_update(crc, page, chunk);
    address += chunk;
    remaining -= chunk;
  }
  return crc;
}

// Firmware before this layout erased the store up to 0x78000, through the SDK's MAC address
// (CFG_ADR_MAC, 0x76000) and crystal calibration (CUST_CAP_INFO_ADDR, 0x77000) sectors, and a
// large upload wrote image bytes there. Such stores are dropped, and the two sectors erased so the
// SDK falls back to a generated MAC and the default calibration instead of image data.
// Runs before the SDK reads those sectors.
void image_store_repair_legacy_overlap(void)
{
  image_store_header_t header;

  flash_read_page(IMAGE_STORE_BASE_ADDR, sizeof(header), (uint8_t *)&header);
  if (header.magic != IMAGE_STORE_MAGIC || header.checksum != image_store_checksum(&header) ||
      header.total_data_bytes <= IMAGE_STORE_TOTAL_DATA_BYTES)
    return;

  // The header goes last, so a reset part-way repeats the repair on the next boot.
  flash_erase_sector(CFG_ADR_MAC);
  flash_erase_sector(CUST_CAP_INFO_ADDR);
  flash_erase_sector(IMAGE_STORE_BASE_ADDR);
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
  image_store_abort();
}

uint8_t image_store_prepare(uint8_t model, uint16_t width, uint16_t height, uint16_t plane_size,
                            uint16_t interval_seconds, uint8_t image_count)
{
  uint32_t total_data_bytes = 0;

  if (image_count == 0 || image_count > IMAGE_STORE_MAX_COUNT)
  {
    return 0;
  }

  if (!width || !height || !plane_size || plane_size > PANEL_MAX_PLANE_BYTES)
  {
    return 0;
  }

  total_data_bytes = (uint32_t)plane_size * 2 * image_count;
  if (total_data_bytes > IMAGE_STORE_TOTAL_DATA_BYTES)
  {
    return 0;
  }

  // Validate before touching flash, so invalid parameters keep the stored images. Erasing the
  // header sector drops them; the data sectors are erased as the chunks arrive.
  flash_erase_sector(IMAGE_STORE_BASE_ADDR);
  memset(image_store_erased_sectors, 0, sizeof(image_store_erased_sectors));
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
  image_store_upload_active_flag = 1;

  return 1;
}

uint8_t image_store_write_chunk(uint8_t image_index, uint8_t plane, uint16_t offset, const uint8_t *data, uint16_t length)
{
  uint32_t address;

  if (!image_store_upload_active_flag)
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

  image_store_erase_data_once(address, length);
  image_store_write_bytes(address, data, length);
  return 1;
}

uint8_t image_store_finalize(uint8_t check_crc, uint32_t crc)
{
  if (!image_store_upload_active_flag)
  {
    return IMAGE_STORE_COMMIT_FAILED;
  }

  // Chunks left out (all 0xFF) may have skipped whole sectors that still hold old images.
  image_store_erase_data_once(IMAGE_STORE_DATA_ADDR, image_store_header.total_data_bytes);
  if (check_crc && image_store_data_crc() != crc)
  {
    // The header is still erased from image_store_prepare(), so the store stays empty.
    image_store_abort();
    return IMAGE_STORE_COMMIT_CRC_MISMATCH;
  }
  image_store_header.checksum = image_store_checksum(&image_store_header);
  image_store_write_bytes(IMAGE_STORE_BASE_ADDR, (const uint8_t *)&image_store_header, sizeof(image_store_header));
  image_store_upload_active_flag = 0;
  image_store_ready = 1;
  image_store_display_pending = 1;
  return IMAGE_STORE_COMMIT_OK;
}

void image_store_abort(void)
{
  memset(&image_store_header, 0, sizeof(image_store_header));
  image_store_upload_active_flag = 0;
  image_store_ready = 0;
  image_store_display_pending = 0;
}

uint8_t image_store_upload_active(void)
{
  return image_store_upload_active_flag;
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