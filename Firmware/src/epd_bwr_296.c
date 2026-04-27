#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_ssd16xx.h"
#include "epd_bwr_296.h"
#include "drivers.h"

// SSD1675/SSD1680 EPD Controller — 2.9" B&W/Red (296x128)

#define BWR_296_Len 30
static const uint8_t LUT_bwr_296_part[] = {
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
    0x00,
    0x00,
    0x00,
    0x00,

    BWR_296_Len,
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
    0x22,
    0x22,
    0x22,
    0x22,
    0x22,
    0x22,
    0x00,
    0x00,
    0x00,
};

static const epd_ssd16xx_config_t bwr_296_cfg = {
    .ram_x_start = 0x00,
    .ram_x_end = 0x0F,
    .ram_y = {0x27, 0x01, 0x00, 0x00},
    .cursor_y = {0x27, 0x01},
    .driver_output = {0x28, 0x01, 0x01},
    .border_waveform = 0x05,
    .has_analog_digital_block = 1,
    .has_acvcom = 0,
    .has_display_update_ctl = 1,
    .partial_lut = LUT_bwr_296_part,
    .partial_lut_size = sizeof(LUT_bwr_296_part),
    .detect_lut_test_size = 153,
    .detect_register = 0,
    .detect_expected = 0,
};

// Shared init + temp read sequence for Display functions
static _attribute_ram_code_ uint8_t bwr_296_init_and_read_temp(void)
{
    uint8_t temp;

    epd_ssd16xx_init(&bwr_296_cfg);

    // Display update control
    EPD_WriteCmd(0x22);
    EPD_WriteData(0xB1);
    EPD_WriteCmd(0x20);
    EPD_CheckStatus_inverted(100);

    // Read temperature
    EPD_WriteCmd(0x1B);
    temp = EPD_SPI_read();
    EPD_SPI_read();
    WaitMs(5);

    return temp;
}

_attribute_ram_code_ uint8_t EPD_BWR_296_detect(void)
{
    return epd_ssd16xx_detect(&bwr_296_cfg);
}

_attribute_ram_code_ uint8_t EPD_BWR_296_read_temp(void)
{
    return epd_ssd16xx_read_temp(&bwr_296_cfg);
}

_attribute_ram_code_ uint8_t EPD_BWR_296_Display(unsigned char *image, int size, uint8_t full_or_partial)
{
    uint8_t temp = bwr_296_init_and_read_temp();

    epd_ssd16xx_set_cursor(&bwr_296_cfg);
    EPD_LoadImage(image, size, 0x24);

    // Clear red plane
    epd_ssd16xx_set_cursor(&bwr_296_cfg);
    EPD_WriteCmd(0x26);
    EPD_WriteDataRepeat(0x00, size);

    if (!full_or_partial)
        epd_ssd16xx_load_partial_lut(&bwr_296_cfg);

    epd_ssd16xx_activate();

    return temp;
}

_attribute_ram_code_ uint8_t EPD_BWR_296_Display_BWR(unsigned char *image, unsigned char *red_image, int size, uint8_t full_or_partial)
{
    if (red_image == NULL)
        return EPD_BWR_296_Display(image, size, full_or_partial);

    uint8_t temp = bwr_296_init_and_read_temp();

    epd_ssd16xx_set_cursor(&bwr_296_cfg);
    EPD_LoadImage(image, size, 0x24);

    epd_ssd16xx_set_cursor(&bwr_296_cfg);
    EPD_LoadImage(red_image, size, 0x26);

    if (!full_or_partial)
        epd_ssd16xx_load_partial_lut(&bwr_296_cfg);

    epd_ssd16xx_activate();

    return temp;
}

_attribute_ram_code_ void EPD_BWR_296_set_sleep(void)
{
    epd_ssd16xx_set_sleep();
}