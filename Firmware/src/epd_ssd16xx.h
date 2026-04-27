#pragma once
#include <stdint.h>

// Configuration for SSD1675/SSD1680 family EPD controllers.
// Each display model provides a const instance of this struct.
typedef struct
{
  // RAM X address range (cmd 0x44)
  uint8_t ram_x_start;
  uint8_t ram_x_end;

  // RAM Y address range (cmd 0x45) — 4 bytes: y_start_hi, y_start_lo, y_end_hi, y_end_lo
  uint8_t ram_y[4];

  // RAM Y cursor position for image load (cmd 0x4F) — 2 bytes: y_hi, y_lo
  uint8_t cursor_y[2];

  // Driver output control (cmd 0x01) — 3 bytes
  uint8_t driver_output[3];

  // Border waveform control (cmd 0x3C)
  uint8_t border_waveform;

  // Set to 1 if this controller needs analog/digital block setup (0x74, 0x7E)
  uint8_t has_analog_digital_block;

  // Set to 1 if this controller needs ACVCOM setting (0x2B)
  uint8_t has_acvcom;

  // Set to 1 if this controller needs display update control (0x21) with 0x00, 0x80
  uint8_t has_display_update_ctl;

  // Partial refresh LUT data (without the 0x32 command byte)
  const uint8_t *partial_lut;
  uint16_t partial_lut_size;

  // Detect: number of LUT bytes to test (0 = use register-based detection)
  uint8_t detect_lut_test_size;

  // Detect: if detect_lut_test_size == 0, read this register and expect this value
  uint8_t detect_register;
  uint8_t detect_expected;
} epd_ssd16xx_config_t;

// Shared SSD16xx functions
void epd_ssd16xx_init(const epd_ssd16xx_config_t *cfg);
uint8_t epd_ssd16xx_read_temp(const epd_ssd16xx_config_t *cfg);
uint8_t epd_ssd16xx_detect(const epd_ssd16xx_config_t *cfg);
void epd_ssd16xx_set_sleep(void);

// Display helpers
void epd_ssd16xx_set_cursor(const epd_ssd16xx_config_t *cfg);
void epd_ssd16xx_load_partial_lut(const epd_ssd16xx_config_t *cfg);
void epd_ssd16xx_activate(void);
