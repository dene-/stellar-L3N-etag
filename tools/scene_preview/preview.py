#!/usr/bin/env python3
"""Render the tag's clock scenes on the host and save them as PNGs.

Compiles Firmware/src/epd_canvas.c + epd_scenes.c with the host C compiler, renders every
scene for a few panel sizes and device states, and writes upscaled PNGs (default
/tmp/scene_preview). Needs a C compiler (cc) and Pillow.
"""
import argparse
import os
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Firmware", "src")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", default=os.path.join(tempfile.gettempdir(), "scene_preview"))
    parser.add_argument("--scale", type=int, default=3)
    args = parser.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    with tempfile.TemporaryDirectory() as build:
        exe = os.path.join(build, "scene_preview")
        subprocess.run(
            [
                os.environ.get("CC", "cc"),
                "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                "-funsigned-char", "-I", SRC,
                os.path.join(os.path.dirname(os.path.abspath(__file__)), "scene_preview.c"),
                os.path.join(SRC, "epd_canvas.c"),
                os.path.join(SRC, "epd_scenes.c"),
                "-o", exe,
            ],
            check=True,
        )
        ppms = subprocess.run([exe, build], check=True, capture_output=True, text=True).stdout.split()
        for ppm in ppms:
            image = Image.open(ppm)
            image = image.resize((image.width * args.scale, image.height * args.scale), Image.NEAREST)
            png = os.path.join(args.out_dir, os.path.splitext(os.path.basename(ppm))[0] + ".png")
            image.save(png)
            print(png)
    return 0


if __name__ == "__main__":
    sys.exit(main())
