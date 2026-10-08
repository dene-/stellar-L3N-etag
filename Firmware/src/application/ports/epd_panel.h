#pragma once
#include <stdint.h>

// The e-paper panel hardware (infrastructure/epd). model is a resolved PANEL_MODEL_* id, never AUTO.
// Each call powers the controller up as needed; refresh leaves it powered until epd_panel_sleep().

// Reads the controller family and returns the model this firmware assumes for it
// (PANEL_MODEL_BWR296 for SSD16xx, PANEL_MODEL_BWR213 for UC8151).
uint8_t epd_panel_detect(void);
int8_t epd_panel_read_temperature(uint8_t model);
// Starts a refresh with the planes (size bytes each; red NULL = no red) and returns the temperature
// the controller measured for it.
int8_t epd_panel_refresh(uint8_t model, uint8_t *black, uint8_t *red, uint16_t size, uint8_t full);
uint8_t epd_panel_is_idle(uint8_t model);
// Puts the controller to deep sleep and cuts the panel power.
void epd_panel_sleep(uint8_t model);
