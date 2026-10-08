#pragma once
#include <stdint.h>
#include "application/ports/epd_panel.h"

// Power management during a refresh: lets a suspend end when BUSY reaches the panel's idle level.
void epd_panel_wake_on_idle(uint8_t model);
void epd_panel_wake_on_idle_off(void);
