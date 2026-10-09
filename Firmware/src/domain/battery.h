#pragma once
#include <stdint.h>

// Charge estimate from the voltage of a lithium coin cell (CR2032/CR2450 chemistry), following its
// discharge curve rather than a straight line.
uint8_t battery_percent(uint16_t battery_mv);

// Thresholds on the supply voltage, a margin above where the chip browns out under the load of the
// operation. A power loss in the middle of a flash erase or write corrupts the stored image or
// firmware, so those are refused below BATTERY_FLASH_MIN_MV. A panel refresh pulls the supply down
// further than anything else, so refreshes stop below BATTERY_REFRESH_MIN_MV (the panel keeps the
// last image without power) and start again from BATTERY_REFRESH_RESUME_MV; the gap keeps a cell
// that recovers a little after a rest from switching on every reading.
#define BATTERY_FLASH_MIN_MV 2400
#define BATTERY_REFRESH_MIN_MV 2200
#define BATTERY_REFRESH_RESUME_MV 2300

// The ADC reads 0 when the measurement failed and noise outside the range a running chip can have;
// such readings say nothing about the cell and are ignored.
#define BATTERY_PLAUSIBLE_MIN_MV 1500
#define BATTERY_PLAUSIBLE_MAX_MV 3600
uint8_t battery_reading_plausible(uint16_t battery_mv);

// Whether flash may be erased or written at this voltage; an implausible reading (0 = unknown)
// does not block.
uint8_t battery_flash_write_ok(uint16_t battery_mv);
// Whether panel refreshes may run at this voltage, given whether they are paused now (hysteresis
// between BATTERY_REFRESH_MIN_MV and BATTERY_REFRESH_RESUME_MV). An implausible reading keeps the
// current state.
uint8_t battery_refresh_ok(uint16_t battery_mv, uint8_t paused);
