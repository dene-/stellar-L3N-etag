#include "domain/clock_calibration.h"

// The TC32 toolchain has no 64-bit arithmetic, so everything below fits 32 bits: the drift is
// bounded first (0.2 %), which bounds the product in the last step.
#define MAX_DRIFT_DIVISOR 500

int16_t clock_calibrated_trim(int16_t trim, uint32_t nominal_ticks, uint32_t device_elapsed_ms, uint32_t true_elapsed_ms)
{
    uint32_t ticks_per_second = nominal_ticks + trim;
    int32_t drift_ms = (int32_t)(device_elapsed_ms - true_elapsed_ms);
    int32_t max_drift_ms = (int32_t)(true_elapsed_ms / MAX_DRIFT_DIVISOR);
    uint32_t ticks_per_ms_q16; // ticks_per_second / true_elapsed_ms, 16 fraction bits
    int32_t calibrated;

    if (true_elapsed_ms < CLOCK_CALIBRATION_MIN_INTERVAL_MS || drift_ms > max_drift_ms || drift_ms < -max_drift_ms)
        return trim;
    // A clock that counted too much ran fast: its second needs proportionally more ticks,
    // ticks_per_second * device_elapsed / true_elapsed = ticks_per_second + ticks_per_second * drift / true_elapsed.
    ticks_per_ms_q16 = (ticks_per_second << 8) / (true_elapsed_ms >> 8);
    calibrated = (int32_t)ticks_per_second + drift_ms * (int32_t)ticks_per_ms_q16 / 65536 - (int32_t)nominal_ticks;
    if (calibrated > CLOCK_TRIM_LIMIT || calibrated < -CLOCK_TRIM_LIMIT)
        return trim;
    return (int16_t)calibrated;
}
