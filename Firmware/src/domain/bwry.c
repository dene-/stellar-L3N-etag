#include "domain/bwry.h"

void bwry_pack_column(const uint8_t *black, const uint8_t *red, uint8_t column_bytes, uint8_t *out, uint8_t out_bytes)
{
    uint16_t pixels = (uint16_t)column_bytes * 8;
    uint16_t pixel = 0;
    uint8_t i;

    for (i = 0; i < out_bytes; i++)
    {
        uint8_t byte = 0;
        uint8_t n;

        for (n = 0; n < 4; n++, pixel++)
        {
            uint8_t code = BWRY_WHITE;

            if (pixel < pixels)
            {
                uint8_t mask = 0x80 >> (pixel & 7);
                uint8_t white = (black[pixel >> 3] & mask) != 0;
                uint8_t colour = red && (red[pixel >> 3] & mask);

                code = colour ? (white ? BWRY_YELLOW : BWRY_RED) : (white ? BWRY_WHITE : BWRY_BLACK);
            }
            byte = (uint8_t)((byte << 2) | code);
        }
        out[i] = byte;
    }
}
