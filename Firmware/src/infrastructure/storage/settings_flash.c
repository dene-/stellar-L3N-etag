#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "application/ports/settings_storage.h"
#include "sections.h"

#define SETTINGS_ADDR 0x78100
#define SETTINGS_MAGIC 0xABCFF124 // bump when defaults must replace saved settings

// Flash record. The layout is fixed so settings survive firmware updates; reserved bytes held
// settings of earlier firmware and are kept as they are. New fields go before the CRC: records of
// earlier firmware are shorter (len) and leave the newer settings at their defaults.
typedef struct __attribute__((packed))
{
	uint32_t magic;
	uint32_t len;
	uint8_t reserved_a[5];
	uint8_t led_flashing_enabled;
	uint8_t fast_refresh_enabled;
	uint8_t reserved_b[4];
	uint8_t panel_model;
	int16_t clock_trim;
	uint8_t crc; // XOR of the len - 1 bytes before it; must stay last
} settings_record_t;

#define RECORD_LEN_BEFORE_CLOCK_TRIM 21
typedef char settings_record_size_check[(sizeof(settings_record_t) == 23) ? 1 : -1];

static RAM settings_record_t record;

static uint8_t record_crc(uint32_t len)
{
	const uint8_t *bytes = (const uint8_t *)&record;
	uint8_t crc = 0;
	unsigned int i;

	for (i = 0; i < len - 1; i++)
		crc ^= bytes[i];
	return crc;
}

uint8_t settings_storage_load(device_settings_t *settings)
{
	const uint8_t *bytes = (const uint8_t *)&record;

	flash_read_page(SETTINGS_ADDR, sizeof(record), (uint8_t *)&record);
	if (record.magic != SETTINGS_MAGIC ||
		(record.len != sizeof(record) && record.len != RECORD_LEN_BEFORE_CLOCK_TRIM) ||
		bytes[record.len - 1] != record_crc(record.len))
	{
		memset(&record, 0, sizeof(record));
		return 0;
	}

	settings->panel_model = record.panel_model;
	settings->fast_refresh_enabled = record.fast_refresh_enabled;
	settings->led_flashing_enabled = record.led_flashing_enabled;
	if (record.len >= sizeof(record))
		settings->clock_trim = record.clock_trim;
	return 1;
}

void settings_storage_save(const device_settings_t *settings)
{
	record.magic = SETTINGS_MAGIC;
	record.len = sizeof(record);
	record.panel_model = settings->panel_model;
	record.fast_refresh_enabled = settings->fast_refresh_enabled;
	record.led_flashing_enabled = settings->led_flashing_enabled;
	record.clock_trim = settings->clock_trim;
	record.crc = record_crc(sizeof(record));
	flash_erase_sector(SETTINGS_ADDR);
	flash_write_page(SETTINGS_ADDR, sizeof(record), (uint8_t *)&record);
}
