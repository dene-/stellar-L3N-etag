# Bitmap fonts

`gen_gfx_fonts.py` converts glyphs of the [Spleen](https://github.com/fcambus/spleen) bitmap
font (BDF) into Adafruit-GFX style headers in `Firmware/src/fonts/`, consumed by
`epd_canvas_text` in `Firmware/src/epd_canvas.c`. Spleen is drawn for the pixel grid, and the
conversion is exact: glyphs are only cropped to their ink, so the panel shows the designed pixels.

| Header | Source | Characters | Used for |
| --- | --- | --- | --- |
| `font_clock_64.h` | spleen-32x64 | `-./0-9:` | time, large panels |
| `font_clock_48.h` | spleen-12x24, scaled x2 | `-./0-9:` | time, mid-size boxes |
| `font_clock_32.h` | spleen-16x32 | `-./0-9:` | time, smallest boxes |
| `font_text_24.h` | spleen-12x24 | ASCII + degree | side panel values |
| `font_text_16.h` | spleen-8x16 | ASCII + degree | date band, compact values |
| `font_text_12.h` | spleen-6x12 | ASCII + degree | status line, header, labels |

## Regenerate

```sh
python3 tools/fonts/gen_gfx_fonts.py            # downloads the pinned Spleen release (sha256-checked)
python3 tools/fonts/gen_gfx_fonts.py --bdf-dir DIR
python3 tools/fonts/gen_gfx_fonts.py --check    # also writes preview PNGs to <tmp>/gfx_font_check/
```

Sizes and character ranges are defined in `FONT_SPECS`. `--check` needs Pillow.

## Notes

- Slot 0x7F holds the degree sign (U+00B0); the firmware writes it as `EPD_DEGREE`.
- The colon of the clock fonts gets a narrow advance so `20:22` has no wide gaps.
- Scaled fonts duplicate pixels by an integer factor; nothing is resampled.

## License

Spleen is BSD-2-Clause, see `LICENSE_SPLEEN`.
