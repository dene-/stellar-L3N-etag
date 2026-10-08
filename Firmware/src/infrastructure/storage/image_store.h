#pragma once
#include "application/ports/image_storage.h"

// Drops a store left by older firmware that overlapped the MAC/calibration sectors; call at boot
// before the SDK reads its customised parameters.
void image_store_repair_legacy_overlap(void);
// Reads the store header at boot; valid images become pending display.
void image_store_init(void);
