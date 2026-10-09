#pragma once
#include <stdint.h>

// Settings kept as a log in one flash sector. A save programs the next empty slot (no erase, so a
// power cut can only damage the slot being written); a load takes the newest slot that is intact.
// The sector is erased only when the log is full.
//
// Slot (SETTINGS_SLOT_SIZE bytes, little endian):
//   0  magic        SETTINGS_SLOT_MAGIC
//   4  sequence     32 bit, grows by one per save
//   8  length       payload bytes that follow, at least SETTINGS_PAYLOAD_LEN
//   9  payload      panel model, fast refresh, LED flashing, clock trim (2), clock interval,
//                   clock sync, scene, slideshow interval (2); further bytes are ignored
//   .. zero padding
//   28 CRC-32 of bytes 0 to 27
// A slot that is all 0xFF is empty.
#define SETTINGS_SLOT_SIZE 32
#define SETTINGS_SLOT_MAGIC 0x31544553UL // "SET1"
#define SETTINGS_PAYLOAD_LEN 10
#define SETTINGS_SLOT_NONE 0xFFFF

typedef struct
{
    uint8_t panel_model;
    uint8_t fast_refresh_enabled;
    uint8_t led_flashing_enabled;
    int16_t clock_trim;
    uint8_t clock_interval;
    uint8_t clock_sync;
    uint8_t scene;
    uint16_t slideshow_interval;
} settings_log_record_t;

// Writes the slot image of a record.
void settings_log_encode(uint8_t *slot, uint32_t sequence, const settings_log_record_t *record);
// Reads a slot; 0 if it is not an intact record.
uint8_t settings_log_decode(const uint8_t *slot, uint32_t *sequence, settings_log_record_t *record);

// Settings of firmware before the log: one record at the start of the sector (25 bytes, shorter in
// still earlier firmware) with a XOR checksum. Reads it into `record` (fields that record predates
// get their defaults, scene and slideshow interval are zero); 0 if it is not an intact record.
uint8_t settings_log_decode_legacy(const uint8_t *slot, settings_log_record_t *record);

// What a pass over all slots found. Feed every slot, in order, to settings_log_scan_slot().
typedef struct
{
    uint16_t newest;         // slot of the newest intact record, SETTINGS_SLOT_NONE if none
    uint32_t newest_sequence;
    uint16_t first_empty;    // SETTINGS_SLOT_NONE if the log is full
    uint8_t has_legacy;      // the first slot holds an intact legacy record
    settings_log_record_t record; // the newest intact record
    settings_log_record_t legacy; // the legacy record
} settings_log_scan_t;

void settings_log_scan_begin(settings_log_scan_t *scan);
void settings_log_scan_slot(settings_log_scan_t *scan, uint16_t index, const uint8_t *slot);
// Where the next save goes: the first empty slot, else slot 0 after erasing the sector (*erase set).
uint16_t settings_log_next_slot(const settings_log_scan_t *scan, uint8_t *erase);
uint32_t settings_log_next_sequence(const settings_log_scan_t *scan);
