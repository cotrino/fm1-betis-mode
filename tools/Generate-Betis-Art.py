"""Generate original limited-palette pixel illustrations, without source photos."""
from pathlib import Path

WIDTH, HEIGHT = 32, 40
PALETTE = [0x0000, 0x1083, 0x0208, 0x056d, 0x07e0, 0xf7be, 0xd5aa, 0x9b86,
           0x51e5, 0x2104, 0xe673, 0xffbd, 0x7c92, 0x001f, 0xffe0, 0xffff]


class Canvas:
    def __init__(self):
        self.pixels = [0] * (WIDTH * HEIGHT)

    def box(self, x, y, w, h, color):
        for yy in range(max(0, y), min(HEIGHT, y + h)):
            for xx in range(max(0, x), min(WIDTH, x + w)):
                self.pixels[yy * WIDTH + xx] = color

    def ellipse(self, x, y, rx, ry, color):
        for yy in range(HEIGHT):
            for xx in range(WIDTH):
                if ((xx - x) / rx) ** 2 + ((yy - y) / ry) ** 2 <= 1:
                    self.pixels[yy * WIDTH + xx] = color

    def packed(self):
        runs = []
        for color in self.pixels:
            if runs and runs[-1][1] == color and runs[-1][0] < 16:
                runs[-1][0] += 1
            else:
                runs.append([1, color])
        return [(length - 1) * 16 + color for length, color in runs]


def portrait(skin, hair, style, beard):
    c = Canvas()
    c.ellipse(16, 36, 15, 10, 2)
    c.ellipse(16, 37, 14, 10, 5)
    for x in range(4, 30, 6):
        c.box(x, 29, 3, 11, 3)
    c.box(12, 23, 8, 8, skin)
    c.box(13, 25, 7, 2, 7)
    c.ellipse(16, 15, 10, 13, 9)
    c.ellipse(16, 16, 9, 12, skin)
    c.box(7, 14, 2, 5, skin)
    c.box(24, 14, 2, 5, skin)
    c.box(9, 15, 2, 7, 7)
    c.box(21, 16, 3, 7, 7)
    c.ellipse(15, 12, 6, 7, 10 if skin == 6 else skin)
    c.box(11, 14, 4, 1, 9)
    c.box(18, 14, 4, 1, 9)
    c.box(12, 16, 2, 1, 15)
    c.box(19, 16, 2, 1, 15)
    c.box(13, 16, 1, 2, 9)
    c.box(19, 16, 1, 2, 9)
    c.box(16, 17, 1, 4, 7)
    c.box(14, 23, 6, 1, 8)
    if beard:
        c.box(11, 21, 2, 4, hair)
        c.box(21, 21, 2, 4, hair)
        c.box(12, 25, 10, 2, hair)
        c.box(14, 22, 6, 1, hair)
    c.ellipse(16, 7, 9, 5, hair)
    c.box(7, 9, 3, 6, hair)
    c.box(23, 9, 2, 5, hair)
    if style == "bun":
        c.ellipse(17, 2, 4, 2, hair)
        c.box(8, 8, 17, 2, 8)
    elif style == "bald":
        c.ellipse(16, 8, 8, 6, skin)
    elif style == "curl":
        for x in range(8, 26, 3):
            c.ellipse(x, 6 + x % 2, 2, 3, hair)
    elif style == "fade":
        c.box(8, 9, 2, 4, 7)
        c.box(23, 9, 2, 4, 7)
    elif style == "part":
        c.box(18, 3, 1, 6, 10)
        c.box(9, 8, 7, 2, hair)
    elif style == "sweep":
        c.box(9, 5, 10, 2, 10)
        c.box(10, 7, 8, 1, 8)
    elif style == "long":
        c.box(6, 9, 3, 17, hair)
        c.box(24, 9, 3, 17, hair)
        c.box(14, 3, 1, 5, 10)
    c.box(11, 30, 3, 2, 9)
    c.box(18, 30, 3, 2, 9)
    c.box(13, 32, 5, 1, 5)
    return c


def shield():
    c = Canvas()
    for y in range(13, 37):
        half = (38 - y) * 12 // 25
        c.box(16 - half - 1, y, half * 2 + 3, 1, 9)
        c.box(16 - half, y, half * 2 + 1, 1, 5)
        for x in range(16 - half, 17 + half):
            if (x - 3) % 4 < 2:
                c.box(x, y, 1, 1, 3)
    c.ellipse(16, 16, 8, 7, 9)
    c.ellipse(16, 16, 7, 6, 3)
    c.box(11, 12, 3, 9, 5)
    c.box(14, 12, 5, 2, 5)
    c.box(14, 16, 4, 2, 5)
    c.box(14, 20, 5, 2, 5)
    c.box(18, 13, 2, 3, 5)
    c.box(18, 18, 2, 3, 5)
    c.box(9, 8, 15, 3, 14)
    for x, y in [(9, 5), (15, 3), (22, 5)]:
        c.box(x, y, 2, 5, 14)
        c.box(x, y, 1, 1, 15)
    return c


def mascot():
    c = Canvas()
    for x, y, rx, ry in [(5, 9, 5, 3), (10, 5, 5, 3), (16, 4, 4, 4),
                         (22, 6, 5, 3), (26, 10, 5, 3)]:
        c.ellipse(x, y, rx, ry, 2)
        c.ellipse(x, y - 1, rx - 1, ry - 1, 4)
    c.box(14, 7, 5, 7, 7)
    c.ellipse(16, 18, 9, 9, 9)
    c.ellipse(16, 18, 8, 8, 10)
    c.ellipse(12, 16, 2, 3, 15)
    c.ellipse(20, 16, 2, 3, 15)
    c.box(12, 16, 1, 2, 9)
    c.box(19, 16, 1, 2, 9)
    c.box(12, 22, 8, 2, 9)
    c.box(13, 22, 6, 1, 15)
    c.box(10, 27, 13, 9, 5)
    for x in [10, 15, 20]:
        c.box(x, 27, 2, 9, 3)
    c.box(7, 28, 3, 6, 10)
    c.box(23, 28, 3, 6, 10)
    c.box(10, 36, 6, 3, 9)
    c.box(18, 36, 6, 3, 9)
    return c


def main():
    # Original stylized fan portraits; likeness is interpretive, not photo-derived.
    profiles = [(6, 9, "fade", True), (6, 8, "long", True), (7, 9, "fade", False),
                (6, 8, "sweep", True), (6, 9, "part", True), (7, 9, "bald", True),
                (6, 8, "short", True), (6, 9, "bun", True), (6, 11, "fade", False),
                (7, 9, "curl", False), (7, 9, "fade", True)]
    art = [shield(), mascot()] + [portrait(*profile) for profile in profiles]
    output = ["/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */",
              "/* Original pixel illustrations; regenerate with tools/Generate-Betis-Art.py. */",
              "#ifndef FM1_BETIS_PIXELS_H", "#define FM1_BETIS_PIXELS_H",
              "static const uint16_t betis_palette[16] = {" +
              ",".join(f"0x{p:04x}" for p in PALETTE) + "};",
              "/* One byte: upper nibble run length minus one, lower nibble color. */"]
    for index, canvas in enumerate(art):
        output.append(f"static const uint8_t betis_pixels_{index}[] = {{")
        data = canvas.packed()
        for at in range(0, len(data), 32):
            output.append("        " + ",".join(f"0x{b:02x}" for b in data[at:at + 32]) + ",")
        output.append("};")
    output.append("static const uint8_t *const betis_pixels[13] = {" +
                  ",".join(f"betis_pixels_{index}" for index in range(13)) + "};")
    output.extend(["#endif", ""])
    root = Path(__file__).resolve().parent.parent
    (root / "vendor/fm1_open/app/betis_pixels.h").write_text(
        "\n".join(output), encoding="ascii", newline="\n")
    # SVG contact sheet is a reusable visual asset, generated from the same pixels.
    names = ["Shield", "Mascot", "Valles", "Bellerin", "Natan", "Bartra",
             "R. Rodriguez", "Amrabat", "Fornals", "Isco", "Antony", "Abde", "Cucho"]
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1050" height="440" viewBox="0 0 525 220">',
           '<rect width="525" height="220" fill="#101c1a"/>']
    for i, canvas in enumerate(art):
        x, y = (i % 7) * 75, (i // 7) * 110
        for yy in range(HEIGHT):
            for xx in range(WIDTH):
                color = canvas.pixels[yy * WIDTH + xx]
                if not color:
                    continue
                p = PALETTE[color]
                rgb = f"#{((p >> 11) * 255 // 31):02x}{(((p >> 5) & 63) * 255 // 63):02x}{((p & 31) * 255 // 31):02x}"
                svg.append(f'<rect x="{x + xx * 2}" y="{y + yy * 2}" width="2" height="2" fill="{rgb}"/>')
        svg.append(f'<text x="{x}" y="{y + 95}" fill="white" font-size="6">{names[i]}</text>')
    svg.append("</svg>")
    (root / "web/betis-sprites.svg").write_text("\n".join(svg), encoding="ascii", newline="\n")


if __name__ == "__main__":
    main()
