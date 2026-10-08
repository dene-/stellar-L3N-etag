#include <string.h>
#include "domain/epd_canvas.h"

void epd_canvas_init(epd_canvas_t *c, uint8_t *black, uint8_t *red, int16_t width, int16_t height, uint8_t has_red)
{
    uint16_t size = (uint16_t)(width * (height >> 3));

    c->black = black;
    c->red = red;
    c->width = width;
    c->height = height;
    c->has_red = has_red;
#ifdef EPD_CANVAS_COUNT_CLIPPED
    c->clipped = 0;
#endif
    memset(black, 0xFF, size);
    memset(red, 0x00, size);
}

void epd_canvas_pixel(epd_canvas_t *c, int16_t x, int16_t y, epd_ink_t ink)
{
    uint16_t index;
    uint8_t mask;

    if (x < 0 || y < 0 || x >= c->width || y >= c->height)
    {
#ifdef EPD_CANVAS_COUNT_CLIPPED
        c->clipped++;
#endif
        return;
    }

    index = (uint16_t)((c->width - 1 - x) * (c->height >> 3) + (y >> 3));
    mask = 0x80 >> (y & 7);

    switch (ink)
    {
    case EPD_INK_WHITE:
        c->black[index] |= mask;
        c->red[index] &= ~mask;
        break;
    case EPD_INK_BLACK:
        c->black[index] &= ~mask;
        c->red[index] &= ~mask;
        break;
    case EPD_INK_RED:
        // Red pixels are black in the black plane, so they degrade to black on BW panels.
        c->black[index] &= ~mask;
        if (c->has_red)
            c->red[index] |= mask;
        else
            c->red[index] &= ~mask;
        break;
    }
}

void epd_canvas_fill(epd_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h, epd_ink_t ink)
{
    int16_t px, py;

    for (px = x; px < x + w; px++)
        for (py = y; py < y + h; py++)
            epd_canvas_pixel(c, px, py, ink);
}

void epd_canvas_frame(epd_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h, epd_ink_t ink)
{
    epd_canvas_fill(c, x, y, w, 1, ink);
    epd_canvas_fill(c, x, y + h - 1, w, 1, ink);
    epd_canvas_fill(c, x, y, 1, h, ink);
    epd_canvas_fill(c, x + w - 1, y, 1, h, ink);
}

void epd_canvas_line(epd_canvas_t *c, int16_t x0, int16_t y0, int16_t x1, int16_t y1, epd_ink_t ink)
{
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    for (;;)
    {
        epd_canvas_pixel(c, x0, y0, ink);
        if (x0 == x1 && y0 == y1)
            return;
        int16_t e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static const GFXglyph *epd_glyph(const GFXfont *font, char ch)
{
    uint8_t code = (uint8_t)ch;

    if (code < font->first || code > font->last)
        return NULL;
    return &font->glyph[code - font->first];
}

static uint8_t epd_glyph_has_ink(const GFXfont *font, const GFXglyph *g)
{
    uint16_t bytes = (uint16_t)((g->width * g->height + 7) >> 3);
    const uint8_t *bits = font->bitmap + g->bitmapOffset;
    uint16_t i;

    for (i = 0; i < bytes; i++)
        if (bits[i])
            return 1;
    return 0;
}

epd_text_box_t epd_text_box(const GFXfont *font, const char *text)
{
    epd_text_box_t box = {0, 0, 0, 0};
    uint8_t has_ink = 0;
    int16_t pen = 0;

    for (; *text; text++)
    {
        const GFXglyph *g = epd_glyph(font, *text);
        if (!g)
            continue;
        if (epd_glyph_has_ink(font, g))
        {
            int16_t left = pen + g->xOffset;
            int16_t right = left + g->width;
            int16_t top = g->yOffset;
            int16_t bottom = top + g->height;

            if (!has_ink)
            {
                box.left = left;
                box.right = right;
                box.top = top;
                box.bottom = bottom;
                has_ink = 1;
            }
            else
            {
                if (left < box.left)
                    box.left = left;
                if (right > box.right)
                    box.right = right;
                if (top < box.top)
                    box.top = top;
                if (bottom > box.bottom)
                    box.bottom = bottom;
            }
        }
        pen += g->xAdvance;
    }
    return box;
}

int16_t epd_canvas_text(epd_canvas_t *c, const GFXfont *font, int16_t x, int16_t baseline, const char *text, epd_ink_t ink)
{
    for (; *text; text++)
    {
        const GFXglyph *g = epd_glyph(font, *text);
        const uint8_t *bits;
        uint16_t bit = 0;
        int16_t gx, gy;

        if (!g)
            continue;

        bits = font->bitmap + g->bitmapOffset;
        for (gy = 0; gy < g->height; gy++)
        {
            for (gx = 0; gx < g->width; gx++, bit++)
            {
                if (bits[bit >> 3] & (0x80 >> (bit & 7)))
                    epd_canvas_pixel(c, x + g->xOffset + gx, baseline + g->yOffset + gy, ink);
            }
        }
        x += g->xAdvance;
    }
    return x;
}

int16_t epd_canvas_text_aligned(epd_canvas_t *c, const GFXfont *font, int16_t left, int16_t right, int16_t baseline,
                                epd_align_t align, const char *text, epd_ink_t ink)
{
    epd_text_box_t box = epd_text_box(font, text);
    int16_t x;

    switch (align)
    {
    case EPD_ALIGN_CENTER:
        x = left + ((right - left) - (box.right - box.left)) / 2 - box.left;
        break;
    case EPD_ALIGN_RIGHT:
        x = right - box.right;
        break;
    default:
        x = left - box.left;
        break;
    }
#ifdef EPD_CANVAS_COUNT_CLIPPED
    if (box.right - box.left > right - left)
        c->clipped += (uint32_t)((box.right - box.left) - (right - left));
#endif

    epd_canvas_text(c, font, x, baseline, text, ink);
    return x;
}

int16_t epd_text_center_baseline(const GFXfont *font, const char *text, int16_t top, int16_t h)
{
    epd_text_box_t box = epd_text_box(font, text);

    return top + (h - (box.bottom - box.top)) / 2 - box.top;
}

const GFXfont *epd_text_fit_font(const GFXfont *const *fonts, uint8_t count, const char *text, int16_t max_w, int16_t max_h)
{
    uint8_t i;

    for (i = 0; i + 1 < count; i++)
    {
        epd_text_box_t box = epd_text_box(fonts[i], text);
        if (box.right - box.left <= max_w && box.bottom - box.top <= max_h)
            return fonts[i];
    }
    return fonts[count - 1];
}

void epd_canvas_battery(epd_canvas_t *c, int16_t x, int16_t y, uint8_t percent, epd_ink_t ink)
{
    const int16_t body_w = EPD_BATTERY_ICON_W - 2;
    const int16_t inner_w = body_w - 4;
    int16_t level_w;

    if (percent > 100)
        percent = 100;
    level_w = (int16_t)((inner_w * percent + 50) / 100);
    if (percent > 0 && level_w == 0)
        level_w = 1;

    epd_canvas_frame(c, x, y, body_w, EPD_BATTERY_ICON_H, ink);
    epd_canvas_fill(c, x + body_w, y + 3, 2, EPD_BATTERY_ICON_H - 6, ink);
    epd_canvas_fill(c, x + 2, y + 2, level_w, EPD_BATTERY_ICON_H - 4, ink);
}

void epd_canvas_bluetooth(epd_canvas_t *c, int16_t x, int16_t y, epd_ink_t ink)
{
    const int16_t r = EPD_BLUETOOTH_ICON_W - 1;
    const int16_t b = EPD_BLUETOOTH_ICON_H - 1;
    const int16_t m = r / 2;

    epd_canvas_line(c, x + m, y, x + m, y + b, ink);
    epd_canvas_line(c, x + m, y, x + r, y + 3, ink);
    epd_canvas_line(c, x + r, y + 3, x, y + b - 3, ink);
    epd_canvas_line(c, x + m, y + b, x + r, y + b - 3, ink);
    epd_canvas_line(c, x + r, y + b - 3, x, y + 3, ink);
}
