from PIL import Image
import os
from collections import deque

def is_bg_xj(r, g, b):
    # 小杰：淡蓝色背景 (145,215,249) 附近
    return (130 <= r <= 170 and
            200 <= g <= 235 and
            235 <= b <= 255)

def is_bg_qy(r, g, b):
    # 奇犽：米白色背景 (253,252,250) 附近
    return (r >= 235 and
            g >= 235 and
            b >= 230)

def remove_background(fname, bg_func):
    img = Image.open(fname).convert("RGBA")
    w, h = img.size
    pixels = list(img.getdata())

    def idx(x, y):
        return y * w + x

    is_bg = [False] * (w * h)
    queue = deque()

    # 四条边入队
    for x in range(w):
        for y in [0, h - 1]:
            r, g, b, a = pixels[idx(x, y)]
            if bg_func(r, g, b):
                is_bg[idx(x, y)] = True
                queue.append((x, y))
    for y in range(h):
        for x in [0, w - 1]:
            r, g, b, a = pixels[idx(x, y)]
            if bg_func(r, g, b) and not is_bg[idx(x, y)]:
                is_bg[idx(x, y)] = True
                queue.append((x, y))

    directions = [(1,0),(-1,0),(0,1),(0,-1)]
    while queue:
        cx, cy = queue.popleft()
        for dx, dy in directions:
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < w and 0 <= ny < h and not is_bg[idx(nx, ny)]:
                r, g, b, a = pixels[idx(nx, ny)]
                if bg_func(r, g, b):
                    is_bg[idx(nx, ny)] = True
                    queue.append((nx, ny))

    new_pixels = []
    for i, (r, g, b, a) in enumerate(pixels):
        if is_bg[i]:
            new_pixels.append((255, 0, 255, 255))  # 品红 = 透明键
        else:
            new_pixels.append((r, g, b, 255))

    img.putdata(new_pixels)
    out_name = fname.replace(".jpg", ".bmp")
    img.convert("RGB").save(out_name, "BMP")
    print(f"[完成] {fname} → {out_name}")

# 小杰用蓝色检测
xj_files = [
    "xj_left_resized.jpg",
    "xj_right_resized.jpg",
    "xj_jump_left_resized.jpg",
    "xj_jump_right_resized.jpg",
    "xj_xd_left_resized.jpg",
    "xj_xd_right_resized.jpg",
]

# 奇犽用米白色检测
qy_files = [
    "qy_left_resized.jpg",
    "qy_right_resized.jpg",
    "qy_jump_left_resized.jpg",
    "qy_jump_right_resized.jpg",
    "qy_xd_left_resized.jpg",
    "qy_xd_right_resized.jpg",
]

for f in xj_files:
    if not os.path.exists(f):
        print(f"[跳过] 找不到: {f}")
        continue
    remove_background(f, is_bg_xj)

for f in qy_files:
    if not os.path.exists(f):
        print(f"[跳过] 找不到: {f}")
        continue
    remove_background(f, is_bg_qy)

print("\n全部处理完毕！")