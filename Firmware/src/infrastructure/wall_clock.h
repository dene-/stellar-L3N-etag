#pragma once
#include <stdint.h>
#include "application/ports/wall_clock.h"

// Software wall clock counting seconds of the system timer.
void wall_clock_init(void);
// Advances the clock by the second elapsed since the last call; run on every main loop pass.
void wall_clock_tick(void);
