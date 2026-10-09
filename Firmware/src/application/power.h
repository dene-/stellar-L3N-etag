#pragma once
#include <stdint.h>

// What the supply voltage allows: flash writes (domain/battery.h has the thresholds) and panel
// refreshes.

// Returns 1 if flash may be erased or written, 0 if the battery is below BATTERY_FLASH_MIN_MV; an
// unknown reading allows it. Takes a fresh reading while the panel is idle and uses the last sample
// while it refreshes. Meant for the commands that start an upload or a firmware install, before
// anything is erased; does not change the stored sample.
uint8_t power_flash_write_allowed(void);

// Samples the battery. A reading of 0 or outside BATTERY_PLAUSIBLE_MIN_MV..MAX_MV is ignored and
// the previous value stays. The caller must not sample while the panel is refreshing: the refresh
// current drags the supply down and would pause refreshes for nothing.
void power_sample_battery(void);
// The last accepted sample in mV, 0 before the first one.
uint16_t power_battery_mv(void);

// Whether a panel refresh may start, from the last sample: refreshes pause below
// BATTERY_REFRESH_MIN_MV and resume from BATTERY_REFRESH_RESUME_MV.
uint8_t power_refresh_allowed(void);
