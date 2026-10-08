#include <stdint.h>
#include "tl_common.h"
#include "domain/bwry.h"
#include "infrastructure/epd/epd_spi.h"
#include "infrastructure/epd/epd_bwry_213.h"
#include "drivers.h"

// JD79661 four-colour controller, 2.13" black/white/red/yellow panel, 250x122 (Stellar 213Q-N).
// Experimental: the sequence is the one Waveshare and Adafruit use for their 2.13" BWRY JD79661
// panels with the waveforms from the panel's OTP; it has not run on a Hanshow tag yet.
//
// A gate line is one of our columns (rightmost first): 128 source pixels, 122 of them visible, as
// 32 bytes of 2-bit pixels (domain/bwry.h). BUSY is low while the controller works. There is no
// partial refresh.

#define GATES 250
#define COLUMN_BYTES 16 // our plane: 128 rows per column
#define LINE_BYTES 32   // 128 sources, 4 per byte

static const uint8_t init_sequence[] = {
    // command, data count, data...
    0x4D, 1, 0x78,
    0x00, 2, 0x0F, 0x29,                               // panel setting
    0x01, 2, 0x07, 0x00,                               // power setting
    0x03, 3, 0x10, 0x54, 0x44,                         // power off sequence
    0x06, 7, 0x05, 0x00, 0x3F, 0x0A, 0x25, 0x12, 0x1A, // booster soft start
    0x50, 1, 0x37,                                     // VCOM and data interval
    0x60, 2, 0x02, 0x02,                               // TCON
    0x61, 4, 0x00, 0x80, 0x00, GATES,                  // resolution: 128 sources, 250 gates
    0xE7, 1, 0x1C,
    0xE3, 1, 0x22,
    0xB4, 1, 0xD0,
    0xB5, 1, 0x03,
    0xE9, 1, 0x01,
    0x30, 1, 0x08, // PLL
};

// Leaves the controller powered on, ready for image data.
static _attribute_ram_code_ void init_and_power_on(void)
{
    unsigned int i = 0;

    EPD_CheckStatus(100); // the reset's busy phase
    while (i < sizeof(init_sequence))
    {
        uint8_t count = init_sequence[i + 1];
        uint8_t n;

        EPD_WriteCmd(init_sequence[i]);
        for (n = 0; n < count; n++)
            EPD_WriteData(init_sequence[i + 2 + n]);
        i += 2 + count;
    }
    EPD_WriteCmd(0x04); // power on
    EPD_CheckStatus(200);
}

// The JD79661 follows the UC8151 command set, so its sensor is read with 0x40 as well (unverified).
static _attribute_ram_code_ int16_t read_temp(void)
{
    return EPD_ReadTemperature(0x40, 0);
}

_attribute_ram_code_ int16_t EPD_BWRY_213_read_temp(void)
{
    int16_t temp;

    init_and_power_on();
    temp = read_temp();
    EPD_BWRY_213_set_sleep();
    return temp;
}

// Always a full refresh; red NULL draws black and white only.
_attribute_ram_code_ int16_t EPD_BWRY_213_Display(unsigned char *image, unsigned char *red, int size, uint8_t full_or_partial)
{
    uint8_t line[LINE_BYTES];
    int16_t temp;
    int column;

    init_and_power_on();
    temp = read_temp();

    EPD_WriteCmd(0x10);
    for (column = 0; column < GATES && (column + 1) * COLUMN_BYTES <= size; column++)
    {
        uint8_t i;

        bwry_pack_column(image + column * COLUMN_BYTES, red ? red + column * COLUMN_BYTES : 0, COLUMN_BYTES, line,
                         LINE_BYTES);
        for (i = 0; i < LINE_BYTES; i++)
            EPD_WriteData(line[i]); // chip select toggled per byte, as the reference drivers do
    }

    EPD_WriteCmd(0x12); // refresh
    EPD_WriteData(0x00);
    return temp;
}

_attribute_ram_code_ void EPD_BWRY_213_set_sleep(void)
{
    EPD_WriteCmd(0x02); // power off
    EPD_CheckStatus(500);
    WaitMs(100); // the reference drivers require at least 100 ms before deep sleep
    EPD_WriteCmd(0x07);
    EPD_WriteData(0xA5);
}
