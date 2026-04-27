#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_ssd16xx.h"
#include "epd_bw_213_ice.h"
#include "drivers.h"

// SSD1675/SSD1680 EPD Controller — 2.13" B&W (ICE variant, 212x104)

#define BW_213_ice_Len 30
static const uint8_t LUT_BW_213_ice_part[] = {
    0x40,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,

    BW_213_ice_Len,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

static const epd_ssd16xx_config_t bw_213_ice_cfg = {
    .ram_x_start = 0x00,
    .ram_x_end = 0x0C,
    .ram_y = {0x28, 0x01, 0x54, 0x00},
    .cursor_y = {0x28, 0x01},
    .driver_output = {0x28, 0x01, 0x01},
    .border_waveform = 0x01,
    .has_analog_digital_block = 1,
    .has_acvcom = 1,
    .has_display_update_ctl = 0,
    .partial_lut = LUT_BW_213_ice_part,
    .partial_lut_size = sizeof(LUT_BW_213_ice_part),
    .detect_lut_test_size = 0,
    .detect_register = 0x2F,
    .detect_expected = 0x01,
};

_attribute_ram_code_ uint8_t EPD_BW_213_ice_detect(void)
{
    return epd_ssd16xx_detect(&bw_213_ice_cfg);
}

_attribute_ram_code_ uint8_t EPD_BW_213_ice_read_temp(void)
{
    return epd_ssd16xx_read_temp(&bw_213_ice_cfg);
}

_attribute_ram_code_ uint8_t EPD_BW_213_ice_Display(unsigned char *image, int size, uint8_t full_or_partial)
{
    uint8_t temp;

    epd_ssd16xx_init(&bw_213_ice_cfg);

    // Display update control — temperature activation
    EPD_WriteCmd(0x22);
    EPD_WriteData(0xA1);
    EPD_WriteCmd(0x20);
    EPD_CheckStatus_inverted(100);

    // Read temperature
    EPD_WriteCmd(0x1B);
    temp = EPD_SPI_read();
    EPD_SPI_read();
    WaitMs(5);

    // Display update control
    EPD_WriteCmd(0x22);
    EPD_WriteData(0xB1);
    EPD_WriteCmd(0x20);
    EPD_CheckStatus_inverted(100);

    // Display update control
    EPD_WriteCmd(0x21);
    EPD_WriteData(0x03);

    epd_ssd16xx_set_cursor(&bw_213_ice_cfg);
    EPD_LoadImage(image, size, 0x24);

    EPD_WriteCmd(0x22);
    EPD_WriteData(0x40);

    if (!full_or_partial)
        epd_ssd16xx_load_partial_lut(&bw_213_ice_cfg);

    epd_ssd16xx_activate();

    return temp;
}

_attribute_ram_code_ void EPD_BW_213_ice_set_sleep(void)
{
    epd_ssd16xx_set_sleep();
}
