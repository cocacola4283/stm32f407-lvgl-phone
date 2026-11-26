from PIL import Image, ImageDraw, ImageFont, ImageFilter
import struct, os

W = H = 84
R = 19
OUT = r"D:\TFT-lvgl\.build-check\icons"
os.makedirs(OUT, exist_ok=True)
MDL2  = r"C:\Windows\Fonts\segmdl2.ttf"
ARIAL = r"C:\Windows\Fonts\arialbd.ttf"

# 桌面壁纸渐变（和 app_shell.c 里一致）
BG_TOP = (0x1B,0x2A,0x38)
BG_BOT = (0x0E,0x16,0x20)
SCR_H = 240

def bg_at(gy):
    t = gy / (SCR_H - 1)
    return tuple(int(BG_TOP[i] + (BG_BOT[i]-BG_TOP[i])*t) for i in range(3))

# (文件名, 类型, 内容, 顶色, 底色, 图标左上角x, y)
ICONS = [
    ("I2048",  "text", "2048",   (0xFF,0xB7,0x4D),(0xF5,0x7C,0x00),  12,  34),
    ("IFILES", "glyph","\ue8b7", (0x4F,0xC3,0xF7),(0x02,0x88,0xD1), 118,  34),
    ("ITODO",  "glyph","\ue8fd", (0x81,0xC7,0x84),(0x38,0x8E,0x3C), 224,  34),
    ("ISETUP", "glyph","\ue713", (0xB0,0xBE,0xC5),(0x60,0x7D,0x8B),  12, 138),
    ("IPHOTO", "glyph","\ue91b", (0xF0,0x62,0x92),(0xC2,0x18,0x5B), 118, 138),
    ("IABOUT", "glyph","\ue946", (0x95,0x75,0xCD),(0x5E,0x35,0xB1), 224, 138),
]

def rounded_mask(w,h,r):
    m = Image.new("L",(w,h),0)
    ImageDraw.Draw(m).rounded_rectangle([0,0,w-1,h-1], radius=r, fill=255)
    return m

def gradient(w,h,c1,c2):
    im = Image.new("RGB",(1,h))
    px = im.load()
    for y in range(h):
        t = y/(h-1)
        px[0,y] = tuple(int(c1[i]+(c2[i]-c1[i])*t) for i in range(3))
    return im.resize((w,h))

def to_rgb565(img):   # 不透明 RGB565, cf=0x12
    hdr = struct.pack("<BBHHHHH", 0x19, 0x12, 0, W, H, W*2, 0)
    rgb = img.convert("RGB")
    px = rgb.load()
    data = bytearray()
    for y in range(H):
        for x in range(W):
            r,g,b = px[x,y]
            data += struct.pack("<H", ((r>>3)<<11)|((g>>2)<<5)|(b>>3))
    return hdr + bytes(data)

for name, kind, content, c1, c2, ix, iy in ICONS:
    # 1) 图标本体（RGBA）
    icon = gradient(W,H,c1,c2).convert("RGBA")
    icon.putalpha(rounded_mask(W,H,R))
    layer = Image.new("RGBA",(W,H),(0,0,0,0))
    d = ImageDraw.Draw(layer)
    if kind == "text":
        for size in range(34,10,-1):
            f = ImageFont.truetype(ARIAL,size)
            bb = d.textbbox((0,0),content,font=f)
            if (bb[2]-bb[0])<=W-16 and (bb[3]-bb[1])<=H-20: break
    else:
        for size in range(64,18,-1):
            f = ImageFont.truetype(MDL2,size)
            bb = d.textbbox((0,0),content,font=f)
            if (bb[2]-bb[0])<=W-24 and (bb[3]-bb[1])<=H-24: break
    tw,th = bb[2]-bb[0], bb[3]-bb[1]
    d.text(((W-tw)/2-bb[0], (H-th)/2-bb[1]), content, font=f, fill=(255,255,255,255))
    icon = Image.alpha_composite(icon, layer)

    # 2) 背景（按图标在屏幕上的真实 y 位置取渐变）
    bgimg = Image.new("RGB",(W,H))
    p = bgimg.load()
    for y in range(H):
        c = bg_at(iy+y)
        for x in range(W):
            p[x,y] = c
    bgimg = bgimg.convert("RGBA")

    # 3) 阴影（烘焙进背景）：把图标轮廓向下偏移 2px、模糊后压暗
    shadow = Image.new("RGBA",(W,H),(0,0,0,0))
    mask = rounded_mask(W,H,R)
    sh = Image.new("RGBA",(W,H),(0,0,0,0))
    sh.paste(Image.new("RGBA",(W,H),(0,0,0,150)), (0,0), mask)
    sh = sh.filter(ImageFilter.GaussianBlur(3))
    shadow.alpha_composite(sh, (0,2))
    bgimg = Image.alpha_composite(bgimg, shadow)

    # 4) 把图标叠到背景上 → 完全不透明
    flat = Image.alpha_composite(bgimg, icon).convert("RGB")

    flat.save(os.path.join(OUT, name + ".png"))
    data = to_rgb565(flat)
    with open(os.path.join(OUT, name + ".BIN"), "wb") as fp:
        fp.write(data)
    print(name, len(data), "bytes  (RGB565 opaque)")
