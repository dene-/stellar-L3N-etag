#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd_spi.h"
#include "epd_uc8151c.h"
#include "drivers.h"

_attribute_ram_code_ void epd_uc8151c_booster_and_power_on(void)
{
  // Booster soft start
  EPD_WriteCmd(0x06);
  EPD_WriteData(0x17);
  EPD_WriteData(0x17);
  EPD_WriteData(0x17);

  // Power on
  EPD_WriteCmd(0x04);
  EPD_CheckStatus(100);
}

_attribute_ram_code_ uint8_t epd_uc8151c_read_temp(void)
{
  uint8_t temp;

  EPD_WriteCmd(0x04); // Power on
  EPD_CheckStatus(100);

  EPD_WriteCmd(0x40);
  temp = EPD_SPI_read();
  EPD_SPI_read(); // discard second byte

  // Power off + deep sleep
  EPD_WriteCmd(0x02);
  EPD_WriteCmd(0x07);
  EPD_WriteData(0xA5);

  return temp;
}

_attribute_ram_code_ void epd_uc8151c_set_sleep(void)
{
  // Vcom and data interval setting
  EPD_WriteCmd(0x50);
  EPD_WriteData(0xF7);

  // Power off
  EPD_WriteCmd(0x02);

  // Deep sleep
  EPD_WriteCmd(0x07);
  EPD_WriteData(0xA5);
}

_attribute_ram_code_ void epd_uc8151c_panel_setting(const epd_uc8151c_config_t *cfg, uint8_t full_or_partial)
{
  EPD_WriteCmd(0x00);
  EPD_WriteData(full_or_partial ? cfg->psr_full : cfg->psr_partial);
  EPD_WriteData(cfg->psr_byte2);

  // Resolution setting
  EPD_WriteCmd(0x61);
  EPD_WriteData(cfg->resolution[0]);
  EPD_WriteData(cfg->resolution[1]);
  EPD_WriteData(cfg->resolution[2]);

  // Vcom and data interval setting
  EPD_WriteCmd(0x50);
  EPD_WriteData(0x97);
}

_attribute_ram_code_ void epd_uc8151c_load_partial_luts(const epd_uc8151c_config_t *cfg)
{
  EPD_send_lut((uint8_t *)cfg->lut_20, cfg->lut_20_size);
  EPD_send_empty_lut(0x21, 260);
  EPD_send_lut((uint8_t *)cfg->lut_22, cfg->lut_22_size);
  EPD_send_lut((uint8_t *)cfg->lut_23, cfg->lut_23_size);
  EPD_send_empty_lut(0x24, 260);
}
