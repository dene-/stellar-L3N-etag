#pragma once
#include "application/ports/image_storage.h"

// Reads the store header at boot; valid images become pending display.
void image_store_init(void);
