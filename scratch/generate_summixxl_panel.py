import os
import re
import sys

# Ensure MSYS2 DLLs are found for cairosvg / libcairo
msys_bin = r'C:\msys64\mingw64\bin'
if os.path.exists(msys_bin):
    os.environ['PATH'] = msys_bin + os.path.pathsep + os.environ.get('PATH', '')
    if hasattr(os, 'add_dll_directory'):
        try:
            os.add_dll_directory(msys_bin)
        except Exception:
            pass

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops
import cairosvg
from PIL import Image, ImageDraw

def get_simplified_glyph_path(font, char):
    cmap = font.getBestCmap()
    gname = cmap[ord(char)] if cmap and ord(char) in cmap else char
    gset = font.getGlyphSet()
    
    path = pathops.Path()
    pen = pathops.PathPen(path)
    gset[gname].draw(pen)
    
    simplified = pathops.simplify(path)
    svg_pen = SVGPathPen(None)
    simplified.draw(svg_pen)
    
    path_d = svg_pen.getCommands()
    path_d = re.sub(r'(\d+\.\d+)', lambda m: f'{float(m.group(1)):.1f}', path_d)
    path_d = re.sub(r'([MLCQZHVmlcqzhv])', r' \1 ', path_d)
    path_d = re.sub(r'\s+', ' ', path_d).strip()
    return path_d

def render_qs_text(font, text, center_x, baseline_y, scale, fill, comment=""):
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text]
    total_w = sum(widths)
    start_x = center_x - total_w / 2.0
    res = []
    if comment:
        res.append(f'  <!-- Label: "{comment}" -->')
    curr = start_x
    for c, w in zip(text, widths):
        if c != ' ':
            pd = get_simplified_glyph_path(font, c)
            res.append(f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({scale:.6f}, {-scale:.6f})"><path d="{pd}" fill="{fill}"/></g>')
        curr += w
    return "\n".join(res)

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    panel_w = 40.64
    panel_h = 128.50
    title_text = "sum/mix xl"
    col_headers = ["IN 1", "IN 2", "SUM", "MULT"]
    version_str = "v2.22.0"

    scale_title = 0.0035
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale_title for c in title_text)
    start_x = (panel_w - total_w) / 2.0

    title_block = [f'  <!-- Label: "{title_text}" -->']
    curr_x = start_x
    for c in title_text:
        w = hmtx[cmap[ord(c)]][0] * scale_title
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale_title:.6f}, {-scale_title:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    version_block = [
        render_qs_text(qs_reg_font, version_str, panel_w / 2.0, 10.414, 0.001400, "#aaaaaa", version_str)
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        f'  <!-- Panel Background: 8 HP ({panel_w:.2f} mm) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#7c7c7c"/>',
        '',
        '  <!-- Delineator Line 1 (Between Trajectory & Color at Y = 54.00mm) -->',
        f'  <line x1="2.50" y1="54.00" x2="{panel_w - 2.50:.2f}" y2="54.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Between Color & Intensity at Y = 108.00mm) -->',
        f'  <line x1="2.50" y1="108.00" x2="{panel_w - 2.50:.2f}" y2="108.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    col_x = [6.07, 15.57, 25.07, 34.57]

    # Column Headers at Y = 15.50
    for header, cx in zip(col_headers, col_x):
        svg_parts.append(render_qs_text(qs_font, header, cx, 15.50, 0.002200, "#1c1c1c", f"Header: {header}"))

    # 6 Rows: X, Y, R, G, B, I
    row_data = [
        ("X", 27.00),
        ("Y", 45.00),
        ("R", 63.00),
        ("G", 81.00),
        ("B", 99.00),
        ("I", 117.00),
    ]

    all_jacks = []
    for r_label, ry in row_data:
        # Single horizontally centered label across the row (no duplicated labels)
        svg_parts.append(render_qs_text(qs_font, r_label, panel_w / 2.0, ry - 5.60, 0.002600, "#1c1c1c", f"Row: {r_label}"))
        for cx in col_x:
            all_jacks.append((cx, ry))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/SumMixXL.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (8 HP).")

    verify_png = 'scratch/summixxl_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=76 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (76 * 4) / panel_w
    jr = 4.15 * r_px
    for cx, cy in all_jacks:
        px = cx * r_px
        py = cy * r_px
        draw.ellipse([px - jr, py - jr, px + jr, py + jr], outline='#00e5ff', width=2)
    im.save(verify_png)
    print(f"Rendered verification bitmap with ports: {verify_png}")

if __name__ == '__main__':
    main()
