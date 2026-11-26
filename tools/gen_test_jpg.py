from PIL import Image, ImageDraw, ImageFont
import os

OUT = "F:" + os.sep
if not os.path.exists(OUT):
    os.makedirs(OUT, exist_ok=True)

W, H = 320, 240
font_big = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 42)
font_med = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 20)

specs = [
    ("PIC1", (255,120,40), (40,20,120), "PHOTO 1", "320x240 JPEG test"),
    ("PIC2", (40,180,255), (0,40,90),  "PHOTO 2", "RGB565 color test"),
    ("PIC3", (60,220,120), (10,70,40), "PHOTO 3", "TJpgDec streaming"),
    ("PIC4", (230,40,120), (60,10,50), "PHOTO 4", "no 150KB framebuffer"),
]

for name, c1, c2, title, sub in specs:
    img = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(img)
    for y in range(H):
        for x in range(0, W, 4):
            t = (x / W * 0.5) + (y / H * 0.5)
            col = tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3))
            d.rectangle([x, y, x + 3, y], fill=col)
    bars = [(255,0,0),(0,255,0),(0,0,255),(255,255,0),(0,255,255),(255,0,255)]
    bw = W // len(bars)
    for i, c in enumerate(bars):
        d.rectangle([i*bw, H-24, (i+1)*bw-1, H-1], fill=c)
    d.text((16, 30), title, font=font_big, fill=(255,255,255))
    d.text((16, 84), sub, font=font_med, fill=(255,255,255))
    path = os.path.join(OUT, name + ".JPG")
    img.save(path, "JPEG", quality=85)
    print(path, os.path.getsize(path), "bytes")
