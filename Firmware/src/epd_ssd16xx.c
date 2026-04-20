#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd_spi.h"
#include "epd_ssd16xx.h"
#include "drivers.h"

// Shared initialization sequence for SSD1675/SSD1680 controllers.
// Called at the start of read_temp and Display functions.
_attribute_ram_code_ void epd_ssd16xx_init(const epd_ssd16xx_config_t *cfg)
{
  // SW Reset
  EPD_WriteCmd(0x12);
  EPD_CheckStatus_inverted(100);

  if (cfg->has_analog_digital_block)
  {
    // Set Analog Block control
    EPD_WriteCmd(0x74);
    EPD_WriteData(0x54);
    // Set Digital Block control
    EPD_WriteCmd(0x7E);
    EPD_WriteData(0x3B);
  }

  if (cfg->has_acvcom)
  {
    // ACVCOM Setting
    EPD_WriteCmd(0x2B);
    EPD_WriteData(0x04);
    EPD_WriteData(0x63);
  }

  if (cfg->has_analog_digital_block)
  {
    // Booster soft start
    EPD_WriteCmd(0x0C);
    EPD_WriteData(0x8B);
    EPD_WriteData(0x9C);
    EPD_WriteData(0x96);
    EPD_WriteData(0x0F);
  }

  // Driver output control
  EPD_WriteCmd(0x01);
  EPD_WriteData(cfg->driver_output[0]);
  EPD_WriteData(cfg->driver_output[1]);
  EPD_WriteData(cfg->driver_output[2]);

  // Data entry mode setting
  EPD_WriteCmd(0x11);
  EPD_WriteData(0x01);

  // Temperature sensor control
  EPD_WriteCmd(0x18);
  EPD_WriteData(0x80);

  // Set RAM X address range
  EPD_WriteCmd(0x44);
  EPD_WriteData(cfg->ram_x_start);
  EPD_WriteData(cfg->ram_x_end);

  // Set RAM Y address range
  EPD_WriteCmd(0x45);
  EPD_WriteData(cfg->ram_y[0]);
  EPD_WriteData(cfg->ram_y[1]);
  EPD_WriteData(cfg->ram_y[2]);
  EPD_WriteData(cfg->ram_y[3]);

  // Border waveform control
  EPD_WriteCmd(0x3C);
  EPD_WriteData(cfg->border_waveform);

  if (cfg->has_display_update_ctl)
  {
    // Display update control
    EPD_WriteCmd(0x21);
    EPD_WriteData(0x00);
    EPD_WriteData(0x80);
  }
}

_attribute_ram_code_ uint8_t epd_ssd16xx_read_temp(const epd_ssd16xx_config_t *cfg)
{
  uint8_t temp;

  epd_ssd16xx_init(cfg);

  // Display update control
  EPD_WriteCmd(0x22);
  EPD_WriteData(0xB1);

  // Master Activation
  EPD_WriteCmd(0x20);
  EPD_CheckStatus_inverted(100);

  // Temperature sensor read from register
  EPD_WriteCmd(0x1B);
  temp = EPD_SPI_read();
  EPD_SPI_read(); // discard second byte

  WaitMs(5);

  epd_ssd16xx_set_sleep();

  return temp;
}

_attribute_ram_code_ uint8_t epd_ssd16xx_detect(const epd_ssd16xx_config_t *cfg)
{
  // SW Reset
  EPD_WriteCmd(0x12);
  WaitMs(10);

  if (cfg->detect_lut_test_size > 0)
  {
    // LUT memory test: write a pattern and read it back
    int i;
    EPD_WriteCmd(0x32);
    for (i = 0; i < cfg->detect_lut_test_size; i++)
      EPD_WriteData(0xA5);

    EPD_WriteCmd(0x33);
    for (i = 0; i < cfg->detect_lut_test_size; i++)
    {
      if (EPD_SPI_read() != 0xA5)
        return 0;
    }
    return 1;
  }
  else
  {
    // Register-based detection
    EPD_WriteCmd(cfg->detect_register);
    return (EPD_SPI_read() == cfg->detect_expected) ? 1 : 0;
  }
}

_attribute_ram_code_ void epd_ssd16xx_set_sleep(void)
{
  EPD_WriteCmd(0x10);
  EPD_WriteData(0x01);
}

_attribute_ram_code_ void epd_ssd16xx_set_cursor(const epd_ssd16xx_config_t *cfg)
{
  // Set RAM X address
  EPD_WriteCmd(0x4E);
  EPD_WriteData(cfg->ram_x_start);

  // Set RAM Y address
  EPD_WriteCmd(0x4F);
  EPD_WriteData(cfg->cursor_y[0]);
  EPD_WriteData(cfg->cursor_y[1]);
}

_attribute_ram_code_ void epd_ssd16xx_load_partial_lut(const epd_ssd16xx_config_t *cfg)
{
  if (cfg->partial_lut != NULL && cfg->partial_lut_size > 0)
  {
    EPD_WriteCmd(0x32);
    EPD_WriteDataBulk(cfg->partial_lut, cfg->partial_lut_size);
  }
}

_attribute_ram_code_ void epd_ssd16xx_activate(void)
{
  // Display update control
  EPD_WriteCmd(0x22);
  EPD_WriteData(0xC7);

  // Master Activation
  EPD_WriteCmd(0x20);
}
