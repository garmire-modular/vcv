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

    # Title "switch" in Node.otf (scale 0.0036, baseline 7.620, centered in 15.24 mm)
    scale = 0.0036
    text = "switch"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale for c in text)
    start_x = (15.24 - total_w) / 2.0

    title_block = ['  <!-- Label: "switch" -->']
    curr_x = start_x
    for c in text:
        w = hmtx[cmap[ord(c)]][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.19.0" centered at 7.62, baseline 10.414
    v219_version_block = [
        render_qs_text(qs_reg_font, "v2.19.0", 7.62, 10.414, 0.001400, "#aaaaaa", "v2.19.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="15.24mm" height="128.5mm" viewBox="0 0 15.24 128.5">',
        '  <!-- Panel Background: 3 HP (15.24 mm) -->',
        '  <rect width="15.24" height="128.5" fill="#7c7c7c"/>',
        '',
        '  <!-- Delineator Line 1 (Between IN 1 & IN 2 at Y = 41.50mm) -->',
        '  <line x1="1.50" y1="41.50" x2="13.74" y2="41.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Between IN 2 & SWITCH at Y = 69.00mm) -->',
        '  <line x1="1.50" y1="69.00" x2="13.74" y2="69.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 3 (Between SWITCH & OUT at Y = 96.50mm) -->',
        '  <line x1="1.50" y1="96.50" x2="13.74" y2="96.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(v219_version_block)

    # 8 Jack positions (all centered at x = 7.62 mm)
    # Pair 1:
    # Jack 1: Y = 23.00
    # Jack 2: Y = 35.50
    # Pair 2:
    # Jack 3: Y = 50.50
    # Jack 4: Y = 63.00
    # Pair 3:
    # Jack 5: Y = 78.00
    # Jack 6: Y = 90.50
    # Pair 4:
    # Jack 7: Y = 105.50
    # Jack 8: Y = 118.00 (Fixed bottom anchor)
    jacks = [
        ("X1", 23.00),
        ("Y1", 35.50),
        ("X2", 50.50),
        ("Y2", 63.00),
        ("SW X", 78.00),
        ("SW Y", 90.50),
        ("OUT X", 105.50),
        ("OUT Y", 118.00),
    ]

    # Labels sit -5.70 mm above jack center
    for label, y in jacks:
        svg_parts.append(render_qs_text(qs_font, label, 7.62, y - 5.70, 0.001900, "#1c1c1c", label))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Switch.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (3 HP).")

    # Render verification bitmap with overlaid ports
    verify_png = 'scratch/switch_verify.png'
    # 3 HP: width 28 px at 1x, so 28*4 = 112 px wide, 240*4 = 960 px high
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=28 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (28 * 4) / 15.24
    jr = 4.15 * r_px
    cx = 7.62 * r_px
    for _, y in jacks:
        cy = y * r_px
        draw.ellipse([cx - jr, cy - jr, cx + jr, cy + jr], outline='#00e5ff', width=2)
    im.save(verify_png)
    print(f"Rendered verification bitmap with ports: {verify_png}")

if __name__ == '__main__':
    main()
