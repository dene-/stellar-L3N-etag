#pragma once
#include <stdint.h>

// Configuration for UC8151C family EPD controllers.
typedef struct
{
  // Panel setting flags for full refresh (cmd 0x00, first byte)
  uint8_t psr_full;
  // Panel setting flags for partial refresh (cmd 0x00, first byte)
  uint8_t psr_partial;
  // Panel setting second byte
  uint8_t psr_byte2;

  // Resolution setting (cmd 0x61) — 3 bytes
  uint8_t resolution[3];

  // Partial refresh LUT tables (UC8151C uses multiple LUT commands)
  const uint8_t *lut_20;
  uint16_t lut_20_size;
  const uint8_t *lut_22;
  uint16_t lut_22_size;
  const uint8_t *lut_23;
  uint16_t lut_23_size;
} epd_uc8151c_config_t;

// Shared UC8151C functions
void epd_uc8151c_booster_and_power_on(void);
uint8_t epd_uc8151c_read_temp(void);
void epd_uc8151c_set_sleep(void);
void epd_uc8151c_panel_setting(const epd_uc8151c_config_t *cfg, uint8_t full_or_partial);
void epd_uc8151c_load_partial_luts(const epd_uc8151c_config_t *cfg);
