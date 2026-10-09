#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "application/ports/settings_storage.h"
#include "domain/settings_log.h"

// The log lives in the sector at 0x78000, after 0x100 bytes this firmware leaves alone (the SDK's
// pairing information, unused here; the old single record started here too). See
// domain/settings_log.h for the slot layout.
#define SETTINGS_SECTOR 0x78000
#define SETTINGS_ADDR 0x78100
#define SETTINGS_SECTOR_END 0x79000
#define SETTINGS_SLOT_COUNT ((SETTINGS_SECTOR_END - SETTINGS_ADDR) / SETTINGS_SLOT_SIZE)

static void scan_log(settings_log_scan_t *scan)
{
	uint8_t slot[SETTINGS_SLOT_SIZE];
	uint16_t index;

	settings_log_scan_begin(scan);
	for (index = 0; index < SETTINGS_SLOT_COUNT; index++)
	{
		flash_read_page(SETTINGS_ADDR + index * SETTINGS_SLOT_SIZE, SETTINGS_SLOT_SIZE, slot);
		settings_log_scan_slot(scan, index, slot);
	}
}

static void apply_record(device_settings_t *settings, const settings_log_record_t *record)
{
	settings->panel_model = record->panel_model;
	settings->fast_refresh_enabled = record->fast_refresh_enabled;
	settings->led_flashing_enabled = record->led_flashing_enabled;
	settings->clock_trim = record->clock_trim;
	settings->clock_interval = record->clock_interval;
	settings->clock_sync = record->clock_sync;
}

uint8_t settings_storage_load(device_settings_t *settings)
{
	settings_log_scan_t scan;

	scan_log(&scan);
	if (scan.newest != SETTINGS_SLOT_NONE)
	{
		apply_record(settings, &scan.record);
		settings->scene = scan.record.scene;
		settings->slideshow_interval = scan.record.slideshow_interval;
		return SETTINGS_STORAGE_CURRENT;
	}
	if (scan.has_legacy)
	{
		apply_record(settings, &scan.legacy);
		return SETTINGS_STORAGE_LEGACY;
	}
	return SETTINGS_STORAGE_NONE;
}

void settings_storage_save(const device_settings_t *settings)
{
	settings_log_scan_t scan;
	settings_log_record_t record;
	uint8_t slot[SETTINGS_SLOT_SIZE];
	uint8_t erase;
	uint16_t index;

	record.panel_model = settings->panel_model;
	record.fast_refresh_enabled = settings->fast_refresh_enabled;
	record.led_flashing_enabled = settings->led_flashing_enabled;
	record.clock_trim = settings->clock_trim;
	record.clock_interval = settings->clock_interval;
	record.clock_sync = settings->clock_sync;
	record.scene = settings->scene;
	record.slideshow_interval = settings->slideshow_interval;

	scan_log(&scan);
	index = settings_log_next_slot(&scan, &erase);
	settings_log_encode(slot, settings_log_next_sequence(&scan), &record);
	// Only a full log is erased, and the record goes into the empty sector at once; every other
	// save programs one empty slot and cannot damage an earlier record.
	if (erase)
		flash_erase_sector(SETTINGS_SECTOR);
	flash_write_page(SETTINGS_ADDR + index * SETTINGS_SLOT_SIZE, SETTINGS_SLOT_SIZE, slot);
}
