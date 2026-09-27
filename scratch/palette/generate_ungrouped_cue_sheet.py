import os
import math
from PIL import Image, ImageDraw, ImageFont

def hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip('#')
    return tuple(int(hex_str[i:i+2], 16) for i in (0, 2, 4))

def rgb_to_lab(r, g, b):
    def to_lin(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    rl, gl, bl = to_lin(r), to_lin(g), to_lin(b)
    x = rl * 0.4124564 + gl * 0.3575761 + bl * 0.1804375
    y = rl * 0.2126729 + gl * 0.7151522 + bl * 0.0721750
    z = rl * 0.0193339 + gl * 0.1191920 + bl * 0.9503041
    x, y, z = x * 100, y * 100, z * 100
    xn, yn, zn = 95.047, 100.0, 108.883
    def f(t):
        return t ** (1/3) if t > 0.008856 else (7.787 * t) + (16 / 116)
    fx, fy, fz = f(x / xn), f(y / yn), f(z / zn)
    return (116 * fy) - 16, 500 * (fx - fy), 200 * (fy - fz)

def delta_e(c1, c2):
    return math.sqrt((c1[0]-c2[0])**2 + (c1[1]-c2[1])**2 + (c1[2]-c2[2])**2)

def simulate_deutan(r, g, b):
    return (min(255, max(0, int(0.625 * r + 0.375 * g))),
            min(255, max(0, int(0.700 * r + 0.300 * g))),
            min(255, max(0, int(0.300 * g + 0.700 * b))))

def simulate_protan(r, g, b):
    return (min(255, max(0, int(0.567 * r + 0.433 * g))),
            min(255, max(0, int(0.558 * r + 0.442 * g))),
            min(255, max(0, int(0.242 * g + 0.758 * b))))

def simulate_tritan(r, g, b):
    return (min(255, max(0, int(0.950 * r + 0.050 * g))),
            min(255, max(0, int(0.000 * r + 0.433 * g + 0.567 * b))),
            min(255, max(0, int(0.000 * r + 0.475 * g + 0.525 * b))))

palette = [
    ('White', '#FFFFFF'),
    ('Safety Orange', '#E69F00'),
    ('Coral Peach', '#FF8866'),
    ('Pale Lavender', '#B8A0E8'),
    ('Sky Blue', '#56B4E9'),
    ('Dark Petrol', '#005566'),
    ('Cobalt Blue', '#0072B2'),
    ('Imperial Violet', '#442288'),
    ('Deep Wine', '#882255'),
    ('Vermilion', '#D55E00'),
    ('Reddish Purple', '#CC79A7'),
    ('Charcoal', '#1A1A1A'),
]

def get_similarity_order(sim_fn):
    n = len(palette)
    sim_rgbs = [sim_fn(*hex_to_rgb(c[1])) for c in palette]
    labs = [rgb_to_lab(*rgb) for rgb in sim_rgbs]
    dist = [[delta_e(labs[i], labs[j]) for j in range(n)] for i in range(n)]

    best_path = None
    best_cost = 1e9

    for start in range(n):
        for second in range(n):
            if start == second:
                continue
            unvis = set(range(n)) - {start, second}
            path = [start, second]
            curr = second
            while unvis:
                nxt = min(unvis, key=lambda x: dist[curr][x])
                path.append(nxt)
                unvis.remove(nxt)
                curr = nxt

            # 2-opt local search
            for _ in range(50):
                improved = False
                for i in range(n - 1):
                    for j in range(i + 2, n + 1):
                        sub = path[i:j]
                        new_sub = list(reversed(sub))
                        d_old = (dist[path[i-1]][path[i]] if i > 0 else 0) + (dist[path[j-1]][path[j]] if j < n else 0)
                        d_new = (dist[path[i-1]][new_sub[0]] if i > 0 else 0) + (dist[new_sub[-1]][path[j]] if j < n else 0)
                        if d_new < d_old - 1e-4:
                            path[i:j] = new_sub
                            improved = True
                if not improved:
                    break

            cost = sum(dist[path[k]][path[k+1]] for k in range(n-1))
            if cost < best_cost:
                best_cost = cost
                best_path = list(path)

    # Orient so lighter color (higher L*) is near the top
    l_start = labs[best_path[0]][0]
    l_end = labs[best_path[-1]][0]
    if l_start < l_end:
        best_path.reverse()

    return [(palette[idx][0], palette[idx][1], sim_rgbs[idx]) for idx in best_path]

def main():
    W, H = 1820, 880
    img = Image.new("RGBA", (W, H), (18, 20, 24, 255))
    draw = ImageDraw.Draw(img)

    # Fonts
    font_dir = "res"
    font_title = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 30)
    font_sub = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Medium.ttf"), 15)
    font_sec_hdr = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 17)
    font_col_th = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 12)
    font_name = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 14)
    font_mono = ImageFont.truetype("consola.ttf", 12)

    # Top Header
    draw.text((60, 26), "FULL PALETTE PERCEPTUAL SIMILARITY CUE SHEET", fill=(255, 255, 255), font=font_title)
    draw.text((60, 66), "Independent 1D Perceptual Ordering (Shortest Color Space Traversal) Under Normal & Color-Blind Vision", fill=(140, 146, 160), font=font_sub)
    draw.line([(60, 96), (W - 60, 96)], fill=(40, 44, 54), width=2)

    sections = [
        ("NORMAL VISION", lambda r, g, b: (r, g, b)),
        ("DEUTERANOPIA (DEUTAN)", simulate_deutan),
        ("PROTANOPIA (PROTAN)", simulate_protan),
        ("TRITANOPIA (TRITAN)", simulate_tritan),
    ]

    sec_w = 385
    sec_gap = 50
    start_x = 60
    header_y = 115

    for sec_idx, (sec_title, sim_fn) in enumerate(sections):
        sx = start_x + sec_idx * (sec_w + sec_gap)
        ordered_items = get_similarity_order(sim_fn)

        # Section Header Banner
        draw.rounded_rectangle([sx, header_y, sx + sec_w, header_y + 32], radius=4, fill=(30, 34, 44))
        draw.text((sx + 14, header_y + 7), sec_title, fill=(235, 240, 250), font=font_sec_hdr)

        # Column Subheaders
        sub_y = header_y + 40
        draw.text((sx + 12, sub_y), "COLOR & HEX", fill=(115, 122, 138), font=font_col_th)
        draw.text((sx + 230, sub_y), "SWATCH", fill=(115, 122, 138), font=font_col_th)

        # Rows
        row_y = sub_y + 22
        row_h = 50
        swatch_w = 140
        swatch_h = 38

        for idx, (name, hex_code, sim_rgb) in enumerate(ordered_items):
            ry = row_y + idx * row_h
            bg_color = (25, 28, 36) if idx % 2 == 0 else (20, 22, 28)
            draw.rounded_rectangle([sx, ry, sx + sec_w, ry + row_h - 6], radius=4, fill=bg_color)

            # Left column: Name & Hex
            draw.text((sx + 12, ry + 6), name, fill=(245, 248, 255), font=font_name)
            draw.text((sx + 12, ry + 24), hex_code, fill=(160, 166, 180), font=font_mono)

            # Right column: Swatch
            swatch_x = sx + 230
            draw.rounded_rectangle(
                [swatch_x, ry + 3, swatch_x + swatch_w, ry + 3 + swatch_h],
                radius=4,
                fill=sim_rgb,
                outline=(55, 60, 75),
                width=1
            )

    # Save outputs
    out_dir = r"C:\Users\Pat\.gemini\antigravity\brain\1810fd88-4c00-4461-8eb4-ad0fdb55eb44"
    out_png = os.path.join(out_dir, "cue_sheet_ungrouped.png")
    img.save(out_png)

    scratch_png = os.path.join("scratch", "palette", "cue_sheet_ungrouped.png")
    img.save(scratch_png)
    print(f"Ungrouped cue sheet saved to {out_png} and {scratch_png}")

if __name__ == "__main__":
    main()
