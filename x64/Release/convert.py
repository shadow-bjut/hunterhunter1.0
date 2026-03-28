from PIL import Image
import os
from collections import deque

THRESHOLD = 30  # 背景白色容差

def is_background_white(r, g, b):
    return (r >= 255 - THRESHOLD and
            g >= 255 - THRESHOLD and
            b >= 255 - THRESHOLD)

def remove_background(fname):
    img = Image.open(fname).convert("RGBA")
    w, h = img.size
    pixels = list(img.getdata())

    def idx(x, y):
        return y * w + x

    # BFS从四条边扩散，标记背景白色像素
    is_bg = [False] * (w * h)
    queue = deque()

    for x in range(w):
        for y in [0, h - 1]:
            r, g, b, a = pixels[idx(x, y)]
            if is_background_white(r, g, b):
                is_bg[idx(x, y)] = True
                queue.append((x, y))
    for y in range(h):
        for x in [0, w - 1]:
            r, g, b, a = pixels[idx(x, y)]
            if is_background_white(r, g, b) and not is_bg[idx(x, y)]:
                is_bg[idx(x, y)] = True
                queue.append((x, y))

    directions = [(1,0),(-1,0),(0,1),(0,-1)]
    while queue:
        cx, cy = queue.popleft()
        for dx, dy in directions:
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < w and 0 <= ny < h and not is_bg[idx(nx, ny)]:
                r, g, b, a = pixels[idx(nx, ny)]
                if is_background_white(r, g, b):
                    is_bg[idx(nx, ny)] = True
                    queue.append((nx, ny))

    # ★ 背景改为品红色(255,0,255)而不是透明，EasyX能正确读取
    new_pixels = []
    for i, (r, g, b, a) in enumerate(pixels):
        if is_bg[i]:
            new_pixels.append((255, 0, 255, 255))  # 品红 = 透明键
        else:
            new_pixels.append((r, g, b, 255))

    img.putdata(new_pixels)
    # ★ 保存为BMP，EasyX读取最稳定
    out_name = fname.replace(".jpg", ".bmp")
    img.convert("RGB").save(out_name, "BMP")
    print(f"[完成] {fname} → {out_name}")

files = [
    "qy_left_resized.jpg",
    "qy_right_resized.jpg",
    "qy_jump_left_resized.jpg",
    "qy_jump_right_resized.jpg",
    "qy_xd_left_resized.jpg",
    "qy_xd_right_resized.jpg",
    "xj_left_resized.jpg",
    "xj_right_resized.jpg",
    "xj_jump_left_resized.jpg",
    "xj_jump_right_resized.jpg",
    "xj_xd_left_resized.jpg",
    "xj_xd_right_resized.jpg",
]

for f in files:
    if not os.path.exists(f):
        print(f"[跳过] 找不到: {f}")
        continue
    remove_background(f)

print("\n全部处理完毕！")