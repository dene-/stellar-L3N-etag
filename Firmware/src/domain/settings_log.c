#include <string.h>
#include "domain/settings_log.h"
#include "domain/clock_calibration.h"
#include "domain/clock_schedule.h"
#include "domain/crc32.h"

#define SLOT_OFFSET_SEQUENCE 4
#define SLOT_OFFSET_LENGTH 8
#define SLOT_OFFSET_PAYLOAD 9
#define SLOT_OFFSET_CRC 28
#define SLOT_PAYLOAD_MAX (SLOT_OFFSET_CRC - SLOT_OFFSET_PAYLOAD)

#define LEGACY_MAGIC 0xABCFF124UL
#define LEGACY_LEN_BEFORE_CLOCK_TRIM 21
#define LEGACY_LEN_BEFORE_CLOCK_INTERVAL 23
#define LEGACY_LEN 25
#define LEGACY_OFFSET_LED 13
#define LEGACY_OFFSET_FAST_REFRESH 14
#define LEGACY_OFFSET_PANEL_MODEL 19
#define LEGACY_OFFSET_CLOCK_TRIM 20
#define LEGACY_OFFSET_CLOCK_INTERVAL 22
#define LEGACY_OFFSET_CLOCK_SYNC 23

typedef char payload_fits_check[(SETTINGS_PAYLOAD_LEN <= SLOT_PAYLOAD_MAX) ? 1 : -1];

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

static void write_le32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = value >> 24;
}

void settings_log_encode(uint8_t *slot, uint32_t sequence, const settings_log_record_t *record)
{
    uint8_t *payload = &slot[SLOT_OFFSET_PAYLOAD];

    memset(slot, 0, SETTINGS_SLOT_SIZE);
    write_le32(slot, SETTINGS_SLOT_MAGIC);
    write_le32(&slot[SLOT_OFFSET_SEQUENCE], sequence);
    slot[SLOT_OFFSET_LENGTH] = SETTINGS_PAYLOAD_LEN;
    payload[0] = record->panel_model;
    payload[1] = record->fast_refresh_enabled;
    payload[2] = record->led_flashing_enabled;
    payload[3] = (uint16_t)record->clock_trim & 0xFF;
    payload[4] = (uint16_t)record->clock_trim >> 8;
    payload[5] = record->clock_interval;
    payload[6] = record->clock_sync;
    payload[7] = record->scene;
    payload[8] = record->slideshow_interval & 0xFF;
    payload[9] = record->slideshow_interval >> 8;
    write_le32(&slot[SLOT_OFFSET_CRC], crc32_update(0, slot, SLOT_OFFSET_CRC));
}

uint8_t settings_log_decode(const uint8_t *slot, uint32_t *sequence, settings_log_record_t *record)
{
    const uint8_t *payload = &slot[SLOT_OFFSET_PAYLOAD];

    if (read_le32(slot) != SETTINGS_SLOT_MAGIC || slot[SLOT_OFFSET_LENGTH] < SETTINGS_PAYLOAD_LEN ||
        slot[SLOT_OFFSET_LENGTH] > SLOT_PAYLOAD_MAX ||
        read_le32(&slot[SLOT_OFFSET_CRC]) != crc32_update(0, slot, SLOT_OFFSET_CRC))
        return 0;
    *sequence = read_le32(&slot[SLOT_OFFSET_SEQUENCE]);
    record->panel_model = payload[0];
    record->fast_refresh_enabled = payload[1];
    record->led_flashing_enabled = payload[2];
    record->clock_trim = (int16_t)read_le16(&payload[3]);
    record->clock_interval = payload[5];
    record->clock_sync = payload[6];
    record->scene = payload[7];
    record->slideshow_interval = read_le16(&payload[8]);
    return 1;
}

uint8_t settings_log_decode_legacy(const uint8_t *slot, settings_log_record_t *record)
{
    uint32_t length;
    uint8_t checksum = 0;
    uint32_t i;

    if (read_le32(slot) != LEGACY_MAGIC)
        return 0;
    length = read_le32(&slot[4]);
    if (length != LEGACY_LEN && length != LEGACY_LEN_BEFORE_CLOCK_INTERVAL && length != LEGACY_LEN_BEFORE_CLOCK_TRIM)
        return 0;
    for (i = 0; i < length - 1; i++)
        checksum ^= slot[i];
    if (checksum != slot[length - 1])
        return 0;

    memset(record, 0, sizeof(*record));
    record->panel_model = slot[LEGACY_OFFSET_PANEL_MODEL];
    record->fast_refresh_enabled = slot[LEGACY_OFFSET_FAST_REFRESH];
    record->led_flashing_enabled = slot[LEGACY_OFFSET_LED];
    record->clock_trim = CLOCK_TRIM_DEFAULT;
    record->clock_interval = CLOCK_SCHEDULE_DEFAULT_MINUTES;
    if (length >= LEGACY_LEN_BEFORE_CLOCK_INTERVAL)
        record->clock_trim = (int16_t)read_le16(&slot[LEGACY_OFFSET_CLOCK_TRIM]);
    if (length >= LEGACY_LEN)
    {
        record->clock_interval = slot[LEGACY_OFFSET_CLOCK_INTERVAL];
        record->clock_sync = slot[LEGACY_OFFSET_CLOCK_SYNC];
    }
    return 1;
}

static uint8_t slot_is_empty(const uint8_t *slot)
{
    unsigned int i;

    for (i = 0; i < SETTINGS_SLOT_SIZE; i++)
    {
        if (slot[i] != 0xFF)
            return 0;
    }
    return 1;
}

void settings_log_scan_begin(settings_log_scan_t *scan)
{
    memset(scan, 0, sizeof(*scan));
    scan->newest = SETTINGS_SLOT_NONE;
    scan->first_empty = SETTINGS_SLOT_NONE;
}

void settings_log_scan_slot(settings_log_scan_t *scan, uint16_t index, const uint8_t *slot)
{
    uint32_t sequence;
    settings_log_record_t record;

    if (slot_is_empty(slot))
    {
        if (scan->first_empty == SETTINGS_SLOT_NONE)
            scan->first_empty = index;
    }
    else if (settings_log_decode(slot, &sequence, &record))
    {
        if (scan->newest == SETTINGS_SLOT_NONE || sequence > scan->newest_sequence)
        {
            scan->newest = index;
            scan->newest_sequence = sequence;
            scan->record = record;
        }
    }
    else if (index == 0 && settings_log_decode_legacy(slot, &record))
    {
        scan->has_legacy = 1;
        scan->legacy = record;
    }
    // Anything else is a slot a power cut interrupted: it stays as it is until the sector is erased.
}

uint16_t settings_log_next_slot(const settings_log_scan_t *scan, uint8_t *erase)
{
    *erase = scan->first_empty == SETTINGS_SLOT_NONE;
    return *erase ? 0 : scan->first_empty;
}

uint32_t settings_log_next_sequence(const settings_log_scan_t *scan)
{
    return scan->newest == SETTINGS_SLOT_NONE ? 1 : scan->newest_sequence + 1;
}
