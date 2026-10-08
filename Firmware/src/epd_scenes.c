#include "epd_scenes.h"

// Spleen bitmap fonts (tools/fonts/gen_gfx_fonts.py), drawn 1:1 so every glyph keeps its designed pixels.
#include "fonts/font_clock_32.h"
#include "fonts/font_clock_48.h"
#include "fonts/font_clock_64.h"
#include "fonts/font_text_12.h"
#include "fonts/font_text_16.h"
#include "fonts/font_text_24.h"

#define MARGIN 4
#define LOW_BATTERY_PERCENT 15

#define FONT_SMALL (&font_text_12) // status line, header, labels
#define FONT_BODY (&font_text_16)  // date band, compact values

static const GFXfont *const time_fonts[] = {&font_clock_64, &font_clock_48, &font_clock_32};
static const GFXfont *const value_fonts[] = {&font_text_24, &font_text_16};

static const char *const weekday_names[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *const month_names[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Baseline that centres capital letters (not descenders) of font inside [top, top + h).
static int16_t cap_baseline(const GFXfont *font, int16_t top, int16_t h)
{
    return epd_text_center_baseline(font, "H", top, h);
}

static int16_t text_width(const GFXfont *font, const char *text)
{
    epd_text_box_t box = epd_text_box(font, text);
    return box.right - box.left;
}

// Minimal formatting: the SDK's sprintf is not available to host builds of this file.
// Each helper writes a NUL-terminated string at out and returns a pointer to that NUL.
static char *put_str(char *out, const char *s)
{
    while (*s)
        *out++ = *s++;
    *out = '\0';
    return out;
}

static char *put_uint(char *out, uint32_t value, uint8_t min_digits)
{
    char digits[10];
    uint8_t n = 0;

    do
    {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value || n < min_digits);
    while (n)
        *out++ = digits[--n];
    *out = '\0';
    return out;
}

static char *put_int(char *out, int32_t value)
{
    if (value < 0)
    {
        *out++ = '-';
        value = -value;
    }
    return put_uint(out, (uint32_t)value, 1);
}

static void format_time(const epd_scene_data_t *d, char *out)
{
    if (d->time_valid)
        put_uint(put_str(put_uint(out, d->time.tm_hour, 2), ":"), d->time.tm_min, 2);
    else
        put_str(out, "--:--");
}

static void format_temperature(const epd_scene_data_t *d, char *out)
{
    put_str(put_int(out, d->temperature_c), EPD_DEGREE "C");
}

// Time as large as fits into the box, centred.
static void draw_time(epd_canvas_t *c, const epd_scene_data_t *d, int16_t left, int16_t top, int16_t w, int16_t h)
{
    char text[8];
    const GFXfont *font;

    format_time(d, text);
    font = epd_text_fit_font(time_fonts, sizeof(time_fonts) / sizeof(time_fonts[0]), text, w - 2 * MARGIN, h - 2 * MARGIN);
    epd_canvas_text_aligned(c, font, left, left + w, epd_text_center_baseline(font, text, top, h), EPD_ALIGN_CENTER, text, EPD_INK_BLACK);
}

// Right-aligned "[bluetooth] [battery] 89%" ending at right, centred in [top, top + h). Returns its left edge.
static int16_t draw_status(epd_canvas_t *c, const epd_scene_data_t *d, int16_t right, int16_t top, int16_t h)
{
    char text[8];
    epd_ink_t battery_ink = (d->battery_percent <= LOW_BATTERY_PERCENT) ? EPD_INK_RED : EPD_INK_BLACK;
    int16_t x;

    put_str(put_uint(text, d->battery_percent, 1), "%");
    x = epd_canvas_text_aligned(c, FONT_SMALL, right - 40, right, cap_baseline(FONT_SMALL, top, h), EPD_ALIGN_RIGHT, text, battery_ink);
    x += epd_text_box(FONT_SMALL, text).left;

    x -= 4 + EPD_BATTERY_ICON_W;
    epd_canvas_battery(c, x, top + (h - EPD_BATTERY_ICON_H) / 2, d->battery_percent, battery_ink);

    if (d->ble_connected)
    {
        x -= 7 + EPD_BLUETOOTH_ICON_W;
        epd_canvas_bluetooth(c, x, top + (h - EPD_BLUETOOTH_ICON_H) / 2, EPD_INK_BLACK);
    }
    return x;
}

// Red band along the bottom: weekday on the left, date on the right.
static void draw_date_band(epd_canvas_t *c, const epd_scene_data_t *d, int16_t top, int16_t h)
{
    const GFXfont *font = FONT_BODY;
    int16_t baseline = cap_baseline(font, top, h);
    char weekday[12];
    char date[16];
    char *p;
    uint8_t week = (d->time.tm_week >= 0 && d->time.tm_week < 7) ? d->time.tm_week : 0;
    uint8_t month = (d->time.tm_month >= 1 && d->time.tm_month <= 12) ? d->time.tm_month : 1;

    epd_canvas_fill(c, 0, top, c->width, h, EPD_INK_RED);

    if (!d->time_valid)
    {
        epd_canvas_text_aligned(c, font, 0, c->width, baseline, EPD_ALIGN_CENTER, "Set time via Bluetooth", EPD_INK_WHITE);
        return;
    }

    put_str(weekday, weekday_names[week]);
    p = put_uint(date, d->time.tm_day, 1);
    p = put_str(p, " ");
    p = put_str(p, month_names[month - 1]);
    p = put_str(p, " ");
    put_uint(p, d->time.tm_year, 1);
    if (text_width(font, weekday) + text_width(font, date) + 4 * MARGIN > c->width)
        weekday[3] = '\0';

    epd_canvas_text_aligned(c, font, 2 * MARGIN, c->width, baseline, EPD_ALIGN_LEFT, weekday, EPD_INK_WHITE);
    epd_canvas_text_aligned(c, font, 0, c->width - 2 * MARGIN, baseline, EPD_ALIGN_RIGHT, date, EPD_INK_WHITE);
}

static int16_t date_band_height(const epd_canvas_t *c)
{
    return (c->height >= 120) ? 24 : 20;
}

void epd_scene_draw_clock(epd_canvas_t *c, const epd_scene_data_t *d)
{
    const int16_t status_h = 18;
    const int16_t band_h = date_band_height(c);
    const int16_t band_top = c->height - band_h;
    char text[8];

    format_temperature(d, text);
    epd_canvas_text_aligned(c, FONT_SMALL, MARGIN, c->width, cap_baseline(FONT_SMALL, 0, status_h), EPD_ALIGN_LEFT, text, EPD_INK_BLACK);
    draw_status(c, d, c->width - MARGIN, 0, status_h);

    draw_time(c, d, 0, status_h, c->width, band_top - status_h);
    draw_date_band(c, d, band_top, band_h);
}

// One cell of the side panel: small label on top, value below; value only when the cell is short.
static void draw_info_cell(epd_canvas_t *c, int16_t left, int16_t top, int16_t w, int16_t h, const char *label, const char *value)
{
    if (h >= 38)
    {
        const GFXfont *font = epd_text_fit_font(value_fonts, sizeof(value_fonts) / sizeof(value_fonts[0]), value, w, h);

        epd_canvas_text_aligned(c, FONT_SMALL, left, left + w, top + 4 - epd_text_box(FONT_SMALL, "H").top, EPD_ALIGN_LEFT, label, EPD_INK_BLACK);
        epd_canvas_text_aligned(c, font, left, left + w, top + h - 5, EPD_ALIGN_LEFT, value, EPD_INK_BLACK);
    }
    else
    {
        epd_canvas_text_aligned(c, FONT_BODY, left, left + w, cap_baseline(FONT_BODY, top, h), EPD_ALIGN_LEFT, value, EPD_INK_BLACK);
    }
}

void epd_scene_draw_dashboard(epd_canvas_t *c, const epd_scene_data_t *d)
{
    const int16_t header_h = 18;
    const int16_t rule_h = 2;
    const int16_t band_h = date_band_height(c);
    const int16_t band_top = c->height - band_h;
    const int16_t body_top = header_h + rule_h;
    const int16_t body_h = band_top - body_top;
    const int16_t side_w = (c->width >= 280) ? 84 : ((c->width >= 240) ? 76 : 66);
    const int16_t divider_x = c->width - side_w - rule_h;
    const int16_t cell_left = divider_x + rule_h + 2 * MARGIN;
    const int16_t cell_w = c->width - cell_left - MARGIN;
    const int16_t cell_h = body_h / 2;
    char text[12];
    int16_t status_left;
    char *p;

    // Header: tag name and connection/battery status, ruled off from the body.
    status_left = draw_status(c, d, c->width - MARGIN, 0, header_h);
    epd_canvas_text_aligned(c, FONT_SMALL, MARGIN, status_left - MARGIN, cap_baseline(FONT_SMALL, 0, header_h), EPD_ALIGN_LEFT,
                            d->device_name, EPD_INK_BLACK);
    epd_canvas_fill(c, 0, header_h, c->width, rule_h, EPD_INK_BLACK);

    draw_time(c, d, 0, body_top, divider_x, body_h);

    // Side panel: temperature over battery voltage.
    epd_canvas_fill(c, divider_x, body_top, rule_h, body_h, EPD_INK_BLACK);
    epd_canvas_fill(c, divider_x + rule_h, body_top + cell_h, c->width - divider_x - rule_h, 1, EPD_INK_BLACK);

    format_temperature(d, text);
    draw_info_cell(c, cell_left, body_top, cell_w, cell_h, "TEMP", text);
    p = put_uint(text, d->battery_mv / 1000, 1);
    p = put_str(p, ".");
    p = put_uint(p, (d->battery_mv % 1000) / 10, 2);
    put_str(p, "V");
    draw_info_cell(c, cell_left, body_top + cell_h + 1, cell_w, body_h - cell_h - 1, "BATTERY", text);

    draw_date_band(c, d, band_top, band_h);
}
