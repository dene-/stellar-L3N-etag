// Host build of the tag's scene renderer: draws every clock scene for a few panel sizes and
// device states into PPM images. Run tools/scene_preview/preview.py instead of this directly.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "domain/epd_scenes.h"

#define MAX_PLANE (200 * 200 / 8)

typedef struct
{
    const char *name;
    int16_t width;
    int16_t height;
    uint8_t has_red;
} panel_t;

typedef struct
{
    const char *name;
    epd_scene_data_t data;
} state_t;

typedef struct
{
    const char *name;
    epd_scene_draw_fn draw;
} scene_t;

static const panel_t panels[] = {
    {"bwr296x128", 296, 128, 1},
    {"bw296x128", 296, 128, 0},
    {"bwr250x128", 250, 128, 1},
    {"bw250x128", 250, 128, 0},
    {"bw212x104", 212, 104, 0},
    {"bwr200x200", 200, 200, 1},
};

// Covers the widest strings each layout must hold: 3-digit negative temperature, "Wednesday",
// two-digit day, full battery with Bluetooth, and the unset-time placeholder.
static const state_t states[] = {
    {"normal", {{0, 22, 20, 8, 10, 2026, 4}, 1, 23, 2980, 89, 1, "THX_3AF8AC"}},
    {"unset_lowbat", {{0, 7, 0, 0, 0, 0, 0}, 0, -5, 2410, 12, 0, "THX_3AF8AC"}},
    {"widest", {{0, 59, 23, 30, 9, 2026, 3}, 1, -15, 3010, 100, 1, "THX_3AF8AC"}},
};

static const scene_t scenes[] = {
    {"clock", epd_scene_draw_clock},
    {"dashboard", epd_scene_draw_dashboard},
};

// Decodes the panel planes exactly like the EPD would show them.
static int write_ppm(const char *path, const epd_canvas_t *c)
{
    FILE *f = fopen(path, "wb");
    int16_t x, y;

    if (!f)
        return -1;
    fprintf(f, "P6\n%d %d\n255\n", c->width, c->height);
    for (y = 0; y < c->height; y++)
    {
        for (x = 0; x < c->width; x++)
        {
            int index = (c->width - 1 - x) * (c->height / 8) + y / 8;
            uint8_t mask = 0x80 >> (y & 7);
            uint8_t rgb[3];

            if (c->red[index] & mask)
                rgb[0] = 200, rgb[1] = 30, rgb[2] = 30;
            else if (c->black[index] & mask)
                rgb[0] = 235, rgb[1] = 235, rgb[2] = 225;
            else
                rgb[0] = 20, rgb[1] = 20, rgb[2] = 20;
            fwrite(rgb, 1, 3, f);
        }
    }
    return fclose(f);
}

int main(int argc, char **argv)
{
    static uint8_t black[MAX_PLANE];
    static uint8_t red[MAX_PLANE];
    const char *out_dir = (argc > 1) ? argv[1] : ".";
    size_t p, s, sc;

    int status = 0;

    for (p = 0; p < sizeof(panels) / sizeof(panels[0]); p++)
        for (s = 0; s < sizeof(states) / sizeof(states[0]); s++)
            for (sc = 0; sc < sizeof(scenes) / sizeof(scenes[0]); sc++)
            {
                epd_canvas_t c;
                char path[512];

                epd_canvas_init(&c, black, red, panels[p].width, panels[p].height, panels[p].has_red);
                scenes[sc].draw(&c, &states[s].data);
                snprintf(path, sizeof(path), "%s/%s_%s_%s.ppm", out_dir, panels[p].name, scenes[sc].name, states[s].name);
                if (write_ppm(path, &c))
                {
                    perror(path);
                    return 1;
                }
                if (c.clipped)
                {
                    fprintf(stderr, "%s: %u pixels clipped or overflowing their box\n", path, (unsigned)c.clipped);
                    status = 2;
                }
                printf("%s\n", path);
            }
    return status;
}
