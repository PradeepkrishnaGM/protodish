"""Draws the app icon, godot/icon.png (M8): a dish holding a few cells in lineage colors.

Run with the project venv (needs Pillow): .venv/bin/python packaging/make_icon.py
"""
import colorsys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 256
UNIT = 16  # one site, in pixels
BACKGROUND = (10, 13, 15)  # the app's background, Color(0.04, 0.05, 0.06)
RIM = (150, 162, 175)

# Lineage view colors: hue = tag, saturation 0.85, value 0.95 (DECISIONS M7b).
def lineage(tag: float) -> tuple[int, int, int]:
    return tuple(round(c * 255) for c in colorsys.hsv_to_rgb(tag, 0.85, 0.95))

# (column, row, tag) on a 16 × 16 grid of sites. Two bodies and some free cells.
CELLS = [
    (5, 5, 0.50), (6, 5, 0.50), (6, 6, 0.50), (5, 6, 0.52),  # a body of 4
    (9, 4, 0.48), (11, 6, 0.55),
    (9, 9, 0.30), (10, 9, 0.30), (10, 10, 0.32),  # a body of 3
    (4, 9, 0.85), (6, 11, 0.88), (12, 10, 0.30),
    (8, 12, 0.10), (3, 7, 0.50), (8, 7, 0.86),
]

def main() -> None:
    scale = 4  # draw large, then shrink, for smooth edges
    s = SIZE * scale
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle((0, 0, s - 1, s - 1), radius=48 * scale, fill=BACKGROUND)
    margin = 18 * scale
    draw.ellipse((margin, margin, s - margin, s - margin), outline=RIM, width=8 * scale)
    u = UNIT * scale
    gap = 2 * scale
    for col, row, tag in CELLS:
        x, y = col * u, row * u
        draw.rectangle((x + gap, y + gap, x + u - gap, y + u - gap), fill=lineage(tag))
    out = Path(__file__).resolve().parent.parent / "godot" / "icon.png"
    img.resize((SIZE, SIZE), Image.LANCZOS).save(out)
    print(f"wrote {out}")

if __name__ == "__main__":
    main()
