# GFX bitmap font generator

`gen_gfx_fonts.py` renders DejaVu Sans Bold glyphs monochrome (no anti-aliasing)
with Pillow and writes Adafruit-GFX style font headers into `Firmware/src/fonts/`.
The headers are consumed by `epd_canvas_text` in `Firmware/src/epd_canvas.c`.

Each header includes `../epd_font.h` for the `GFXglyph` / `GFXfont` types.

## Regenerate

From the repo root (works from any directory):

```sh
python3 tools/fonts/gen_gfx_fonts.py                       # uses matplotlib's bundled DejaVu fonts
python3 tools/fonts/gen_gfx_fonts.py --font-dir DIR        # DIR contains DejaVuSans-Bold.ttf
python3 tools/fonts/gen_gfx_fonts.py --check               # also save /tmp/gfx_font_check/*.png
```

Requires Python 3 with Pillow. matplotlib is optional and only used to locate the fonts.

Font sizes and character ranges are defined in `FONT_SPECS` in the script.

## Notes

- Character slot 0x7F is rendered from the degree sign U+00B0 (the firmware uses 0x7F as the degree glyph), not DEL.
- Glyph bitmaps are row-major, MSB-first, packed continuously across rows; each glyph starts on a fresh byte.
- `yAdvance` is `ascent + descent` from the font metrics.

## License

DejaVu fonts are derived from Bitstream Vera. The license text is in
`LICENSE_DEJAVU` (copied from matplotlib's `mpl-data/fonts/ttf/LICENSE_DEJAVU`).
It permits embedding and derivative works, including generated bitmap fonts.
