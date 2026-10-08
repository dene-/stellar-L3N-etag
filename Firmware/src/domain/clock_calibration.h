#pragma once
#include <stdint.h>

// The wall clock counts one second per (nominal + trim) system timer ticks. The timer runs on the
// internal 32 kHz RC oscillator while the chip sleeps, which is off by some hundred ppm per device.
// The trim is measured between two clock syncs from the phone and stored.
#define CLOCK_TRIM_DEFAULT 5000 // ticks; what an earlier fixed correction used
#define CLOCK_TRIM_LIMIT 30000  // about 1900 ppm; larger results mean a bad measurement
// Shorter sync intervals are skipped: BLE latency would dominate the measured drift.
#define CLOCK_CALIBRATION_MIN_INTERVAL_MS (6UL * 3600 * 1000)

// The trim that would have made the clock count true_elapsed_ms where, running with trim, it
// counted device_elapsed_ms. Returns trim unchanged if the interval is too short or the result
// out of range. nominal_ticks + CLOCK_TRIM_LIMIT must stay below 2^24.
int16_t clock_calibrated_trim(int16_t trim, uint32_t nominal_ticks, uint32_t device_elapsed_ms, uint32_t true_elapsed_ms);
