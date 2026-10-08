#!/usr/bin/env python3
"""Generate the tag's bitmap font headers (Firmware/src/fonts/) from the Spleen bitmap fonts.

Spleen (https://github.com/fcambus/spleen) is a pixel-designed bitmap font; its BDF glyphs are
converted bit for bit (only cropped to their ink), so the panel shows exactly the designed pixels.
A spec may upscale a font by an integer factor; pixels are duplicated, never resampled.

Usage (from any working directory):

    python3 tools/fonts/gen_gfx_fonts.py [--bdf-dir DIR] [--out-dir DIR] [--check]

Without --bdf-dir the pinned Spleen release is downloaded (sha256-verified) and cached.
--check also writes 3x preview PNGs of every font to <tmp>/gfx_font_check/ (needs Pillow).
"""

import argparse
import hashlib
import io
import ssl
import sys
import tarfile
import tempfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT_DIR = REPO_ROOT / "Firmware" / "src" / "fonts"
CHECK_DIR = Path(tempfile.gettempdir()) / "gfx_font_check"

SPLEEN_VERSION = "2.1.0"
SPLEEN_URL = f"https://github.com/fcambus/spleen/releases/download/{SPLEEN_VERSION}/spleen-{SPLEEN_VERSION}.tar.gz"
SPLEEN_SHA256 = "8b47c56f1a6eb858fbcf9e34530557404b02fbb3455e38e64fb84473fd0c372f"
CACHE_DIR = Path(tempfile.gettempdir()) / f"spleen-{SPLEEN_VERSION}"

DEGREE_SLOT = 0x7F  # the firmware writes the degree sign as "\x7F" (EPD_DEGREE)
DEGREE_CODEPOINT = 0xB0
DIGITS = (0x2D, 0x3A)  # '-' '.' '/' '0'-'9' ':'
TEXT = (0x20, 0x7F)

U8 = (0, 0xFF)
I8 = (-0x80, 0x7F)
U16 = (0, 0xFFFF)


@dataclass(frozen=True)
class FontSpec:
    c_name: str
    bdf: str
    first: int
    last: int
    scale: int = 1
    narrow: str = ""  # characters whose advance shrinks to their ink plus a small gap


FONT_SPECS = [
    FontSpec("font_clock_64", "spleen-32x64.bdf", *DIGITS, narrow=":"),
    FontSpec("font_clock_48", "spleen-12x24.bdf", *DIGITS, scale=2, narrow=":"),
    FontSpec("font_clock_32", "spleen-16x32.bdf", *DIGITS, narrow=":"),
    FontSpec("font_text_24", "spleen-12x24.bdf", *TEXT),
    FontSpec("font_text_16", "spleen-8x16.bdf", *TEXT),
    FontSpec("font_text_12", "spleen-6x12.bdf", *TEXT),
]


@dataclass
class BdfGlyph:
    advance: int
    width: int
    height: int
    x_off: int
    y_off: int  # BDF: bottom edge relative to the baseline, up is positive
    rows: list  # list of lists of 0/1


@dataclass
class BdfFont:
    ascent: int
    descent: int
    glyphs: dict  # codepoint -> BdfGlyph


@dataclass
class Glyph:
    code: int
    bitmap_offset: int
    width: int
    height: int
    x_advance: int
    x_offset: int
    y_offset: int
    data: bytes


@dataclass
class BuiltFont:
    spec: FontSpec
    glyphs: list
    bitmap: bytes
    y_advance: int


def _fit(value, limits, what):
    lo, hi = limits
    if not lo <= value <= hi:
        raise OverflowError(f"{what} = {value} does not fit in its C type [{lo}, {hi}]")
    return value


def parse_bdf(path):
    ascent = descent = None
    glyphs = {}
    lines = Path(path).read_text(encoding="latin-1").splitlines()
    i = 0
    while i < len(lines):
        words = lines[i].split()
        i += 1
        if not words:
            continue
        if words[0] == "FONT_ASCENT":
            ascent = int(words[1])
        elif words[0] == "FONT_DESCENT":
            descent = int(words[1])
        elif words[0] == "STARTCHAR":
            code = advance = bbx = None
            while not lines[i].startswith("BITMAP"):
                w = lines[i].split()
                if w[0] == "ENCODING":
                    code = int(w[1])
                elif w[0] == "DWIDTH":
                    advance = int(w[1])
                elif w[0] == "BBX":
                    bbx = tuple(int(v) for v in w[1:5])
                i += 1
            i += 1
            width, height, x_off, y_off = bbx
            rows = []
            for _ in range(height):
                hex_row = lines[i].strip()
                i += 1
                bits = int(hex_row, 16)
                total = len(hex_row) * 4
                rows.append([(bits >> (total - 1 - col)) & 1 for col in range(width)])
            if lines[i].strip() != "ENDCHAR":
                raise ValueError(f"{path}: malformed glyph {code}")
            i += 1
            glyphs[code] = BdfGlyph(advance, width, height, x_off, y_off, rows)
    if ascent is None or descent is None:
        raise ValueError(f"{path}: missing FONT_ASCENT/FONT_DESCENT")
    return BdfFont(ascent, descent, glyphs)


def _convert_glyph(spec, code, src):
    """Crop a BDF glyph to its ink, scale it and pack it MSB-first."""
    s = spec.scale
    ink = [(r, c) for r, row in enumerate(src.rows) for c, bit in enumerate(row) if bit]
    advance = src.advance * s
    tag = f"{spec.c_name}[0x{code:02X}]"
    if not ink:
        return 1, 1, _fit(advance, U8, f"xAdvance {tag}"), 0, 0, b"\x00"

    top = min(r for r, _ in ink)
    bottom = max(r for r, _ in ink) + 1
    left = min(c for _, c in ink)
    right = max(c for _, c in ink) + 1
    width = (right - left) * s
    height = (bottom - top) * s
    x_offset = (src.x_off + left) * s
    y_offset = (top - (src.y_off + src.height)) * s

    if chr(code) in spec.narrow:
        gap = max(2, src.advance // 8) * s
        x_offset = gap
        advance = width + 2 * gap

    bits = bytearray((width * height + 7) // 8)
    idx = 0
    for row in range(height):
        src_row = src.rows[top + row // s]
        for col in range(width):
            if src_row[left + col // s]:
                bits[idx >> 3] |= 0x80 >> (idx & 7)
            idx += 1

    return (
        _fit(width, U8, f"width {tag}"),
        _fit(height, U8, f"height {tag}"),
        _fit(advance, U8, f"xAdvance {tag}"),
        _fit(x_offset, I8, f"xOffset {tag}"),
        _fit(y_offset, I8, f"yOffset {tag}"),
        bytes(bits),
    )


def build_font(spec, bdf_dir):
    bdf = parse_bdf(Path(bdf_dir) / spec.bdf)
    glyphs = []
    blob = bytearray()
    for code in range(spec.first, spec.last + 1):
        source = DEGREE_CODEPOINT if code == DEGREE_SLOT else code
        if source not in bdf.glyphs:
            raise KeyError(f"{spec.bdf} has no glyph U+{source:04X}")
        width, height, x_adv, x_off, y_off, data = _convert_glyph(spec, code, bdf.glyphs[source])
        offset = _fit(len(blob), U16, f"bitmapOffset {spec.c_name}[0x{code:02X}]")
        glyphs.append(Glyph(code, offset, width, height, x_adv, x_off, y_off, data))
        blob += data
    y_advance = _fit((bdf.ascent + bdf.descent) * spec.scale, U8, f"yAdvance {spec.c_name}")
    return BuiltFont(spec, glyphs, bytes(blob), y_advance)


def _char_comment(code):
    if code == DEGREE_SLOT:
        return "degree"
    ch = chr(code)
    if ch == "'":
        return "quote"
    if ch == "\\":
        return "backslash"
    return f"'{ch}'"


def render_header(built):
    spec = built.spec
    name = spec.c_name
    scale = f", scaled x{spec.scale}" if spec.scale > 1 else ""
    out = [
        "// Generated by tools/fonts/gen_gfx_fonts.py -- do not edit.",
        f"// Source: Spleen {SPLEEN_VERSION} {spec.bdf}{scale}, chars 0x{spec.first:02X}-0x{spec.last:02X}."
        " BSD-2-Clause, see tools/fonts/LICENSE_SPLEEN.",
        "#pragma once",
        '#include "../epd_font.h"',
        "",
        f"static const uint8_t {name}_bitmaps[] = {{",
    ]
    for g in built.glyphs:
        for i in range(0, len(g.data), 16):
            line = "\t" + ", ".join(f"0x{b:02X}" for b in g.data[i:i + 16]) + ","
            if i == 0:
                line += f" // {_char_comment(g.code)}"
            out.append(line)
    out += [
        "};",
        "",
        f"static const GFXglyph {name}_glyphs[] = {{",
        "\t// bitmapOffset, width, height, xAdvance, xOffset, yOffset",
    ]
    for g in built.glyphs:
        out.append(
            f"\t{{ {g.bitmap_offset}, {g.width}, {g.height}, {g.x_advance}, "
            f"{g.x_offset}, {g.y_offset} }}, // {_char_comment(g.code)}"
        )
    out += [
        "};",
        "",
        f"static const GFXfont {name} = {{",
        f"\t(uint8_t *){name}_bitmaps, (GFXglyph *){name}_glyphs, "
        f"0x{spec.first:02X}, 0x{spec.last:02X}, {built.y_advance}}};",
        "",
    ]
    return "\n".join(out)


def render_check(built, out_path):
    """Draw a sample string from the generated tables the way epd_canvas_text does."""
    from PIL import Image

    spec = built.spec
    text = "-0123456789:" if spec.last == DIGITS[1] else "Thu 08 Oct 23\x7fC 2.98V 89% Ag"
    margin = built.y_advance // 2
    baseline = margin + built.y_advance
    width = sum(built.glyphs[ord(ch) - spec.first].x_advance for ch in text) + 2 * margin
    height = 2 * built.y_advance
    canvas = bytearray(b"\xff" * (width * height))
    pen = margin
    for ch in text:
        g = built.glyphs[ord(ch) - spec.first]
        for idx in range(g.width * g.height):
            if built.bitmap[g.bitmap_offset + (idx >> 3)] & (0x80 >> (idx & 7)):
                x = pen + g.x_offset + idx % g.width
                y = baseline + g.y_offset + idx // g.width
                canvas[y * width + x] = 0
        pen += g.x_advance
    img = Image.frombytes("L", (width, height), bytes(canvas))
    img.resize((width * 3, height * 3), Image.NEAREST).save(out_path)
    return out_path


def fetch_spleen():
    """Download the pinned Spleen release once and return the directory holding its BDF files."""
    needed = {spec.bdf for spec in FONT_SPECS}
    if all((CACHE_DIR / name).is_file() for name in needed):
        return CACHE_DIR
    print(f"Downloading {SPLEEN_URL}")
    context = None
    try:  # python.org builds on macOS ship without a CA bundle
        import certifi

        context = ssl.create_default_context(cafile=certifi.where())
    except ImportError:
        pass
    with urllib.request.urlopen(SPLEEN_URL, context=context) as response:
        archive = response.read()
    digest = hashlib.sha256(archive).hexdigest()
    if digest != SPLEEN_SHA256:
        raise RuntimeError(f"Spleen archive sha256 {digest} != pinned {SPLEEN_SHA256}")
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as tar:
        for member in tar.getmembers():
            name = Path(member.name).name
            if name in needed:
                (CACHE_DIR / name).write_bytes(tar.extractfile(member).read())
    return CACHE_DIR


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--bdf-dir", help="directory with the Spleen .bdf files (default: download the pinned release)")
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR), help="output directory (default: Firmware/src/fonts)")
    parser.add_argument("--check", action="store_true", help=f"also write preview PNGs to {CHECK_DIR}")
    args = parser.parse_args(argv)

    bdf_dir = Path(args.bdf_dir) if args.bdf_dir else fetch_spleen()
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    if args.check:
        CHECK_DIR.mkdir(parents=True, exist_ok=True)

    for spec in FONT_SPECS:
        built = build_font(spec, bdf_dir)
        header = out_dir / f"{spec.c_name}.h"
        header.write_text(render_header(built), encoding="utf-8")
        print(f"{header}: {len(built.bitmap)} bitmap bytes, yAdvance {built.y_advance}")
        if args.check:
            print(f"  check: {render_check(built, CHECK_DIR / f'{spec.c_name}.png')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
