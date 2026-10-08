#pragma once
#include <stdint.h>
#include "domain/epd_font.h"

// Drawing surface that renders straight into the two panel planes the EPD drivers consume.
//
// Panel plane layout (same as the web uploader's canvas2bytes): columns from the rightmost
// (x = width-1) to the leftmost, each column height/8 bytes, top pixel in the MSB.
// Black plane: 1 = white, 0 = black. Red plane: 1 = red. Height must be a multiple of 8.

typedef enum
{
    EPD_INK_WHITE = 0,
    EPD_INK_BLACK,
    EPD_INK_RED, // drawn black on panels without a red plane
} epd_ink_t;

typedef enum
{
    EPD_ALIGN_LEFT = 0,
    EPD_ALIGN_CENTER,
    EPD_ALIGN_RIGHT,
} epd_align_t;

typedef struct
{
    uint8_t *black;
    uint8_t *red;
    int16_t width;
    int16_t height;
    uint8_t has_red;
#ifdef EPD_CANVAS_COUNT_CLIPPED
    uint32_t clipped; // pixels drawn off-canvas plus text overflowing its box; the host preview fails on any
#endif
} epd_canvas_t;

// Ink bounds of a string relative to the pen start on the baseline (right/bottom exclusive).
typedef struct
{
    int16_t left;
    int16_t right;
    int16_t top;
    int16_t bottom;
} epd_text_box_t;

// Points the canvas at the plane buffers (each width*height/8 bytes) and clears them to white.
void epd_canvas_init(epd_canvas_t *c, uint8_t *black, uint8_t *red, int16_t width, int16_t height, uint8_t has_red);

void epd_canvas_pixel(epd_canvas_t *c, int16_t x, int16_t y, epd_ink_t ink);
void epd_canvas_fill(epd_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h, epd_ink_t ink);
void epd_canvas_frame(epd_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h, epd_ink_t ink);
void epd_canvas_line(epd_canvas_t *c, int16_t x0, int16_t y0, int16_t x1, int16_t y1, epd_ink_t ink);

epd_text_box_t epd_text_box(const GFXfont *font, const char *text);
// Draws text with its pen starting at (x, baseline); returns the pen position after the last glyph.
int16_t epd_canvas_text(epd_canvas_t *c, const GFXfont *font, int16_t x, int16_t baseline, const char *text, epd_ink_t ink);
// Aligns the ink box of text inside [left, right) and returns the pen start used.
int16_t epd_canvas_text_aligned(epd_canvas_t *c, const GFXfont *font, int16_t left, int16_t right, int16_t baseline,
                                epd_align_t align, const char *text, epd_ink_t ink);
// Baseline that vertically centres the ink box of text inside [top, top + h).
int16_t epd_text_center_baseline(const GFXfont *font, const char *text, int16_t top, int16_t h);
// First font in fonts[] (largest first) whose ink box for text fits max_w x max_h; the last one otherwise.
const GFXfont *epd_text_fit_font(const GFXfont *const *fonts, uint8_t count, const char *text, int16_t max_w, int16_t max_h);

// 25x12 battery outline, filled proportionally to percent, at its top-left corner (x, y).
void epd_canvas_battery(epd_canvas_t *c, int16_t x, int16_t y, uint8_t percent, epd_ink_t ink);
// 7x12 Bluetooth rune at its top-left corner (x, y).
void epd_canvas_bluetooth(epd_canvas_t *c, int16_t x, int16_t y, epd_ink_t ink);

#define EPD_BATTERY_ICON_W 25
#define EPD_BATTERY_ICON_H 12
#define EPD_BLUETOOTH_ICON_W 7
#define EPD_BLUETOOTH_ICON_H 12
