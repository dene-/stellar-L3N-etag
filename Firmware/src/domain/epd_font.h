#pragma once
#include <stdint.h>

// Adafruit GFX bitmap font format. Glyph pixels are packed row-major, MSB first,
// continuously across rows; each glyph starts on a byte boundary.
// Fonts are generated into src/fonts/ by tools/fonts/gen_gfx_fonts.py.

typedef struct
{
    uint16_t bitmapOffset; // index of the glyph's first byte in GFXfont.bitmap
    uint8_t width;         // bitmap size in pixels
    uint8_t height;
    uint8_t xAdvance; // pen advance
    int8_t xOffset;   // pen position on the baseline -> bitmap top-left corner
    int8_t yOffset;
} GFXglyph;

typedef struct
{
    uint8_t *bitmap;
    GFXglyph *glyph;
    uint8_t first; // first and last character code covered by glyph[]
    uint8_t last;
    uint8_t yAdvance; // line height
} GFXfont;

// The generated text fonts carry the degree sign in the otherwise unused DEL slot.
#define EPD_DEGREE "\x7F"
