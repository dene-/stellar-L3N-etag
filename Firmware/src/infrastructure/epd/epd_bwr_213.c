#include <stdint.h>
#include "tl_common.h"
#include "infrastructure/epd/epd_spi.h"
#include "infrastructure/epd/epd_uc8151c.h"
#include "infrastructure/epd/epd_bwr_213.h"
#include "drivers.h"

// UC8151C EPD Controller — 2.13" B&W/Red (250x128)

enum PSR_FLAGS
{
    RES_96x230 = 0b00000000,
    RES_96x252 = 0b01000000,
    RES_128x296 = 0b10000000,
    RES_160x296 = 0b11000000,

    LUT_OTP = 0b00000000,
    LUT_REG = 0b00100000,

    FORMAT_BWR = 0b00000000,
    FORMAT_BW = 0b00010000,

    SCAN_DOWN = 0b00000000,
    SCAN_UP = 0b00001000,

    SHIFT_LEFT = 0b00000000,
    SHIFT_RIGHT = 0b00000100,

    BOOSTER_OFF = 0b00000000,
    BOOSTER_ON = 0b00000010,

    RESET_SOFT = 0b00000000,
    RESET_NONE = 0b00000001
};

#define scan_direction (SCAN_UP | RES_128x296 | FORMAT_BWR | BOOSTER_ON | RESET_NONE | LUT_OTP | SHIFT_RIGHT)

#define lut_bwr_213_refresh_time 6
static const uint8_t lut_bwr_213_20_part[] =
    {
        0x20, 0x00, lut_bwr_213_refresh_time, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t lut_bwr_213_22_part[] =
    {
        0x22, 0x80, lut_bwr_213_refresh_time, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t lut_bwr_213_23_part[] =
    {
        0x23, 0x40, lut_bwr_213_refresh_time, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const epd_uc8151c_config_t bwr_213_cfg = {
    .psr_full = scan_direction,
    .psr_partial = scan_direction | LUT_REG,
    .psr_byte2 = 0x0F,
    .resolution = {0x80, 0x01, 0x28},
    .lut_20 = lut_bwr_213_20_part,
    .lut_20_size = sizeof(lut_bwr_213_20_part),
    .lut_22 = lut_bwr_213_22_part,
    .lut_22_size = sizeof(lut_bwr_213_22_part),
    .lut_23 = lut_bwr_213_23_part,
    .lut_23_size = sizeof(lut_bwr_213_23_part),
};

_attribute_ram_code_ uint8_t EPD_BWR_213_read_temp(void)
{
    return epd_uc8151c_read_temp();
}

// Shared init for BWR213 display functions: power on, panel setting, temp read
static _attribute_ram_code_ uint8_t bwr_213_init_display(uint8_t full_or_partial)
{
    uint8_t temp;

    EPD_WriteCmd(0x04); // power on
    WaitMs(1);

    // Panel setting — note: BWR213 uses PSR_FLAGS directly, not the generic
    // uc8151c_panel_setting, because it skips resolution/vcom commands
    EPD_WriteCmd(0x00);
    EPD_WriteData(full_or_partial ? bwr_213_cfg.psr_full : bwr_213_cfg.psr_partial);
    EPD_WriteData(bwr_213_cfg.psr_byte2);

    EPD_WriteCmd(0x04); // power on analog
    EPD_CheckStatus(100);

    EPD_WriteCmd(0x40);
    temp = EPD_SPI_read();
    EPD_SPI_read();

    if (!full_or_partial)
        epd_uc8151c_load_partial_luts(&bwr_213_cfg);

    if (full_or_partial)
    {
        EPD_WriteCmd(0x10);
        EPD_WriteDataRepeat(0x00, 4000);
        EPD_WriteCmd(0x13);
        EPD_WriteDataRepeat(0x00, 4000);
        WaitMs(5);
    }

    return temp;
}

// red may be NULL: a full refresh clears the red RAM, a partial one keeps it.
_attribute_ram_code_ uint8_t EPD_BWR_213_Display(unsigned char *image, unsigned char *red, int size, uint8_t full_or_partial)
{
    uint8_t temp = bwr_213_init_display(full_or_partial);

    EPD_LoadImage(image, size, 0x10);
    if (red != NULL)
        EPD_LoadImage(red, size, 0x13);

    EPD_WriteCmd(0x12);

    return temp;
}

_attribute_ram_code_ void EPD_BWR_213_set_sleep(void)
{
    // UC8151C sleep (without vcom setting — BWR variant)
    EPD_WriteCmd(0x02);
    EPD_WriteCmd(0x07);
    EPD_WriteData(0xA5);
}