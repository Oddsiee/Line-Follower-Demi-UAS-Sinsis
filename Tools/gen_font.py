"""Generate 6x8 monochrome font C array (v2: TTF + supersample + threshold)."""
from PIL import Image, ImageDraw, ImageFont
import os

W, H = 6, 8
CANDIDATES = [
    r"C:\Windows\Fonts\consola.ttf",
    r"C:\Windows\Fonts\cour.ttf",
    r"C:\Windows\Fonts\arial.ttf",
    r"C:\Windows\Fonts\segoeui.ttf",
]
FONT_PATH = next((p for p in CANDIDATES if os.path.exists(p)), None)
print("Using font:", FONT_PATH)

def render(ch):
    S = 64
    f = ImageFont.truetype(FONT_PATH, S)
    tmp = Image.new("L", (S * 3, S * 2), 0)
    d = ImageDraw.Draw(tmp)
    bbox = d.textbbox((0, 0), ch, font=f)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    if w <= 0 or h <= 0:
        return [0] * W
    img = Image.new("L", (w + 2, h + 2), 0)
    d = ImageDraw.Draw(img)
    d.text((1 - bbox[0], 1 - bbox[1]), ch, fill=255, font=f)
    img = img.crop(img.getbbox())
    scale = min(W / img.width, H / img.height)
    nw = max(1, int(img.width * scale))
    nh = max(1, int(img.height * scale))
    img = img.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("L", (W, H), 0)
    canvas.paste(img, ((W - nw) // 2, (H - nh) // 2))
    cols = []
    for x in range(W):
        b = 0
        for y in range(H):
            if canvas.getpixel((x, y)) > 127:
                b |= (1 << y)
        cols.append(b)
    return cols

def preview(ch):
    cols = render(ch)
    for y in range(H):
        print(''.join('#' if (cols[x] >> y) & 1 else '.' for x in range(W)))

if __name__ == "__main__":
    for ch in ['A', '0', '5', 'G', 'M', 'x', ':', '-', 'g', '3']:
        print(f'=== {ch!r}')
        preview(ch)

    lines = ["/* 6x8 font, ASCII 32..126. 6 bytes/glyph = 6 columns, bit 0 = top row. */",
             "static const uint8_t FONT6x8[95][6] = {"]
    for code in range(32, 127):
        ch = chr(code)
        cols = render(ch)
        row = "    {" + ", ".join(f"0x{b:02X}" for b in cols) + "},"
        annot = ch if code not in (34, 39, 92) else "\\" + ch
        lines.append(f"{row:38s} /* {code:3d} '{annot}' */")
    lines.append("};")
    with open("font6x8.c", "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("\nWrote font6x8.c")
