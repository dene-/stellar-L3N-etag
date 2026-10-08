#pragma once
#include <stdint.h>

// Four-colour panels (PANEL_MODEL_BWRY213) take 2 bits per pixel, four pixels per byte, the first
// pixel in the top bits. The two planes hold the four colours like this:
//   black plane bit  red plane bit  colour
//         0               0          black
//         1               0          white
//         0               1          red
//         1               1          yellow
enum
{
    BWRY_BLACK = 0,
    BWRY_WHITE = 1,
    BWRY_YELLOW = 2,
    BWRY_RED = 3,
};

// Packs one column of the planes (column_bytes bytes each, top pixel in the MSB of the first byte;
// red NULL = no red or yellow) into out_bytes bytes of 2-bit pixels. Pixels past the column are
// white.
void bwry_pack_column(const uint8_t *black, const uint8_t *red, uint8_t column_bytes, uint8_t *out, uint8_t out_bytes);
