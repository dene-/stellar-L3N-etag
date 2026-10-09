// domain/settings_log: which slot is newest, where the next record goes, and what a power cut in the
// middle of a save leaves behind. The sector is a byte array; programming only clears bits, as flash does.
#include <string.h>
#include "check.h"
#include "domain/clock_calibration.h"
#include "domain/clock_schedule.h"
#include "domain/settings_log.h"

#define SLOTS 8 // the real log has more; the logic does not depend on the count

static uint8_t sector[SLOTS * SETTINGS_SLOT_SIZE];
static int erases;

static void erase(void)
{
    memset(sector, 0xFF, sizeof(sector));
    erases++;
}

static void scan(settings_log_scan_t *result)
{
    uint16_t i;

    settings_log_scan_begin(result);
    for (i = 0; i < SLOTS; i++)
        settings_log_scan_slot(result, i, &sector[i * SETTINGS_SLOT_SIZE]);
}

// What the storage adapter does on a save; `programmed` bytes of the slot reach the flash before power
// is lost (SETTINGS_SLOT_SIZE = the save completes).
static void save_cut(const settings_log_record_t *record, unsigned int programmed)
{
    settings_log_scan_t result;
    uint8_t erase_first;
    uint8_t slot[SETTINGS_SLOT_SIZE];
    uint16_t index;
    unsigned int i;

    scan(&result);
    index = settings_log_next_slot(&result, &erase_first);
    if (erase_first)
        erase();
    settings_log_encode(slot, settings_log_next_sequence(&result), record);
    for (i = 0; i < programmed; i++)
        sector[index * SETTINGS_SLOT_SIZE + i] &= slot[i];
}

static void save(const settings_log_record_t *record)
{
    save_cut(record, SETTINGS_SLOT_SIZE);
}

static settings_log_record_t record_with_trim(int16_t trim)
{
    settings_log_record_t record = {
        .panel_model = 3,
        .fast_refresh_enabled = 1,
        .led_flashing_enabled = 0,
        .clock_trim = trim,
        .clock_interval = 5,
        .clock_sync = 1,
        .scene = 2,
        .slideshow_interval = 600,
    };

    return record;
}

static void test_empty_sector(void)
{
    settings_log_scan_t result;
    uint8_t erase_first;

    erase();
    scan(&result);
    CHECK_EQ(result.newest, SETTINGS_SLOT_NONE);
    CHECK_EQ(result.has_legacy, 0);
    CHECK_EQ(settings_log_next_slot(&result, &erase_first), 0);
    CHECK_EQ(erase_first, 0);
    CHECK_EQ(settings_log_next_sequence(&result), 1);
}

static void test_record_round_trip(void)
{
    settings_log_scan_t result;
    settings_log_record_t record = record_with_trim(-1234);

    erase();
    save(&record);
    scan(&result);
    CHECK_EQ(result.newest, 0);
    CHECK_EQ(result.newest_sequence, 1);
    CHECK_EQ(result.record.panel_model, 3);
    CHECK_EQ(result.record.fast_refresh_enabled, 1);
    CHECK_EQ(result.record.led_flashing_enabled, 0);
    CHECK_EQ(result.record.clock_trim, -1234);
    CHECK_EQ(result.record.clock_interval, 5);
    CHECK_EQ(result.record.clock_sync, 1);
    CHECK_EQ(result.record.scene, 2);
    CHECK_EQ(result.record.slideshow_interval, 600);
}

static void test_newest_record_wins(void)
{
    settings_log_scan_t result;
    uint8_t erase_first;
    int16_t trim;
    int erases_at_start;

    erase();
    erases_at_start = erases;
    for (trim = 1; trim <= 4; trim++)
    {
        settings_log_record_t record = record_with_trim(trim);
        save(&record);
    }
    scan(&result);
    CHECK_EQ(result.newest, 3);
    CHECK_EQ(result.newest_sequence, 4);
    CHECK_EQ(result.record.clock_trim, 4);
    CHECK_EQ(settings_log_next_slot(&result, &erase_first), 4);
    CHECK_EQ(erase_first, 0);
    CHECK_EQ(settings_log_next_sequence(&result), 5);
    CHECK_EQ(erases, erases_at_start); // no erase while there is room
}

// A save cut short at any byte leaves the previous record in force.
static void test_torn_write_is_ignored(void)
{
    unsigned int programmed;

    for (programmed = 0; programmed < SETTINGS_SLOT_SIZE; programmed++)
    {
        settings_log_scan_t result;
        uint8_t erase_first;
        settings_log_record_t old_record = record_with_trim(100);
        settings_log_record_t new_record = record_with_trim(200);

        erase();
        save(&old_record);
        save_cut(&new_record, programmed);
        scan(&result);
        CHECK_EQ(result.newest, 0);
        CHECK_EQ(result.record.clock_trim, 100);

        // The next save goes after the damaged slot and becomes the newest again.
        settings_log_next_slot(&result, &erase_first);
        CHECK_EQ(erase_first, 0);
        save(&new_record);
        scan(&result);
        CHECK_EQ(result.record.clock_trim, 200);
        CHECK(result.newest >= 1);
    }
}

static void test_full_log_erases_and_restarts_at_slot_zero(void)
{
    settings_log_scan_t result;
    uint8_t erase_first;
    int16_t trim;

    erase();
    for (trim = 1; trim <= SLOTS; trim++)
    {
        settings_log_record_t record = record_with_trim(trim);
        save(&record);
    }
    scan(&result);
    CHECK_EQ(result.newest, SLOTS - 1);
    CHECK_EQ(result.record.clock_trim, SLOTS);
    CHECK_EQ(settings_log_next_slot(&result, &erase_first), 0);
    CHECK_EQ(erase_first, 1);

    {
        settings_log_record_t record = record_with_trim(99);
        int erases_before = erases;

        save(&record);
        CHECK_EQ(erases, erases_before + 1);
    }
    scan(&result);
    CHECK_EQ(result.newest, 0);
    CHECK_EQ(result.record.clock_trim, 99);
    // The sequence keeps counting across the erase.
    CHECK_EQ(result.newest_sequence, SLOTS + 1);
}

// Slots that hold something else than a record never pass as one.
static void test_damaged_slots_are_not_records(void)
{
    settings_log_scan_t result;
    settings_log_record_t record = record_with_trim(7);

    erase();
    save(&record);
    sector[SETTINGS_SLOT_SIZE + 3] = 0x00; // garbage in slot 1: not empty, no magic
    scan(&result);
    CHECK_EQ(result.newest, 0);
    CHECK_EQ(result.first_empty, 2);

    // A flipped payload bit fails the CRC.
    erase();
    save(&record);
    sector[10] ^= 0x01;
    scan(&result);
    CHECK_EQ(result.newest, SETTINGS_SLOT_NONE);
    CHECK_EQ(result.has_legacy, 0);
}

// The record of firmware before the log: 25 bytes at the start of the sector, XOR checksum last.
static void put_legacy(uint8_t length)
{
    uint8_t xor = 0;
    uint8_t i;

    erase();
    sector[0] = 0x24;
    sector[1] = 0xF1;
    sector[2] = 0xCF;
    sector[3] = 0xAB;
    sector[4] = length;
    sector[5] = sector[6] = sector[7] = 0;
    memset(&sector[8], 0, length - 8);
    sector[13] = 1; // LED flashing
    sector[14] = 1; // fast refresh
    sector[19] = 2; // panel model
    if (length >= 23)
    {
        sector[20] = 0x34; // trim 0x1234
        sector[21] = 0x12;
    }
    if (length >= 25)
    {
        sector[22] = 15; // clock interval
        sector[23] = 1;  // clock sync
    }
    for (i = 0; i < length - 1; i++)
        xor ^= sector[i];
    sector[length - 1] = xor;
}

static void test_legacy_record(void)
{
    settings_log_scan_t result;
    uint8_t erase_first;

    put_legacy(25);
    scan(&result);
    CHECK_EQ(result.newest, SETTINGS_SLOT_NONE);
    CHECK_EQ(result.has_legacy, 1);
    CHECK_EQ(result.legacy.led_flashing_enabled, 1);
    CHECK_EQ(result.legacy.fast_refresh_enabled, 1);
    CHECK_EQ(result.legacy.panel_model, 2);
    CHECK_EQ(result.legacy.clock_trim, 0x1234);
    CHECK_EQ(result.legacy.clock_interval, 15);
    CHECK_EQ(result.legacy.clock_sync, 1);

    // The legacy record stays where it is until the log wraps; the first new record goes to the next slot.
    CHECK_EQ(settings_log_next_slot(&result, &erase_first), 1);
    CHECK_EQ(erase_first, 0);
    {
        settings_log_record_t record = result.legacy;
        save(&record);
    }
    scan(&result);
    CHECK_EQ(result.newest, 1);
    CHECK_EQ(result.record.panel_model, 2);
}

static void test_older_legacy_lengths_get_defaults(void)
{
    settings_log_scan_t result;

    put_legacy(23);
    scan(&result);
    CHECK_EQ(result.has_legacy, 1);
    CHECK_EQ(result.legacy.clock_trim, 0x1234);
    CHECK_EQ(result.legacy.clock_interval, CLOCK_SCHEDULE_DEFAULT_MINUTES);
    CHECK_EQ(result.legacy.clock_sync, 0);

    put_legacy(21);
    scan(&result);
    CHECK_EQ(result.has_legacy, 1);
    CHECK_EQ(result.legacy.panel_model, 2);
    CHECK_EQ(result.legacy.clock_trim, CLOCK_TRIM_DEFAULT);
    CHECK_EQ(result.legacy.clock_interval, CLOCK_SCHEDULE_DEFAULT_MINUTES);

    // A bad checksum is not a record.
    put_legacy(25);
    sector[24] ^= 0x10;
    scan(&result);
    CHECK_EQ(result.has_legacy, 0);
}

int main(void)
{
    test_empty_sector();
    test_record_round_trip();
    test_newest_record_wins();
    test_torn_write_is_ignored();
    test_full_log_erases_and_restarts_at_slot_zero();
    test_damaged_slots_are_not_records();
    test_legacy_record();
    test_older_legacy_lengths_get_defaults();
    return check_report();
}
