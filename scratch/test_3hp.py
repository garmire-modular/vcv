import os
import re
import sys

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

def generate_3hp_design1():
    # Design 1: Each jack labeled individually centered above it: X1, Y1, X2, Y2, SW X, SW Y, OUT X, OUT Y
    # with thin delineator lines between the 4 groups.
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    # Title "switch" in Node.otf (scale 0.0036 -> width ~9.86mm, centered in 15.24mm)
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

    # Version tag "v2.19.0" centered at 7.62
    v219_version_block = [
        render_qs_text(qs_reg_font, "v2.19.0", 7.62, 10.414, 0.001400, "#aaaaaa", "v2.19.0")
    ]

    # Jacks and Delineator positions in 3HP (15.24mm):
    # Jack 8 (OUT Y) at Y = 118.00 mm (Fixed bottom anchor)
    # Pitch within pair = 11.50 mm
    # Gap between pairs = 14.50 mm (Delineator in the middle at +7.25 mm)
    #
    # Pair 4 (OUT):
    # Jack 7 (OUT X): 106.50
    # Jack 8 (OUT Y): 118.00
    # Delineator 3 at 99.25
    #
    # Pair 3 (SWITCH):
    # Jack 5 (SW X):  80.50
    # Jack 6 (SW Y):  92.00
    # Delineator 2 at 73.25
    #
    # Pair 2 (IN 2):
    # Jack 3 (X2):    54.50
    # Jack 4 (Y2):    66.00
    # Delineator 1 at 47.25
    #
    # Pair 1 (IN 1):
    # Jack 1 (X1):    28.50
    # Jack 2 (Y1):    40.00

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="15.24mm" height="128.5mm" viewBox="0 0 15.24 128.5">',
        '  <!-- Panel Background: 3 HP (15.24 mm) -->',
        '  <rect width="15.24" height="128.5" fill="#7c7c7c"/>',
        '',
        '  <!-- Delineators (width from 1.5mm to 13.74mm) -->',
        '  <line x1="1.50" y1="47.25" x2="13.74" y2="47.25" stroke="#999999" stroke-width="0.176"/>',
        '  <line x1="1.50" y1="73.25" x2="13.74" y2="73.25" stroke="#999999" stroke-width="0.176"/>',
        '  <line x1="1.50" y1="99.25" x2="13.74" y2="99.25" stroke="#999999" stroke-width="0.176"/>',
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(v219_version_block)

    # Jack labels centered at x = 7.62 mm
    # Scale = 0.0020 (height ~1.4mm)
    # Baseline at Y - 4.90 mm (jack collar top is at Y - 4.15 mm, so text sits nicely above collar)
    jacks = [
        ("X1", 28.50),
        ("Y1", 40.00),
        ("X2", 54.50),
        ("Y2", 66.00),
        ("SW X", 80.50),
        ("SW Y", 92.00),
        ("OUT X", 106.50),
        ("OUT Y", 118.00),
    ]

    for label, y in jacks:
        svg_parts.append(render_qs_text(qs_font, label, 7.62, y - 4.90, 0.002000, "#1c1c1c", label))

    svg_parts.append('</svg>')
    with open('scratch/design_3hp_1.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")

    # Render with ports overlay
    cairosvg.svg2png(url='scratch/design_3hp_1.svg', write_to='scratch/design_3hp_1.png', output_width=28 * 4, output_height=240 * 4)

    im = Image.open('scratch/design_3hp_1.png')
    draw = ImageDraw.Draw(im)
    r_px = (28 * 4) / 15.24
    jr = 4.15 * r_px
    cx = 7.62 * r_px
    for _, y in jacks:
        cy = y * r_px
        draw.ellipse([cx - jr, cy - jr, cx + jr, cy + jr], outline='#00e5ff', width=2)
    im.save('scratch/design_3hp_1_ports.png')
    print("Design 1 (3HP) generated.")

def generate_3hp_design2():
    # Design 2: Group headers IN 1, IN 2, SW, OUT with X and Y above each jack
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

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

    v219_version_block = [
        render_qs_text(qs_reg_font, "v2.19.0", 7.62, 10.414, 0.001400, "#aaaaaa", "v2.19.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="15.24mm" height="128.5mm" viewBox="0 0 15.24 128.5">',
        '  <rect width="15.24" height="128.5" fill="#7c7c7c"/>',
        '  <!-- Delineators -->',
        '  <line x1="1.50" y1="43.50" x2="13.74" y2="43.50" stroke="#999999" stroke-width="0.176"/>',
        '  <line x1="1.50" y1="70.50" x2="13.74" y2="70.50" stroke="#999999" stroke-width="0.176"/>',
        '  <line x1="1.50" y1="97.50" x2="13.74" y2="97.50" stroke="#999999" stroke-width="0.176"/>',
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(v219_version_block)

    # Group 1: IN 1
    svg_parts.append(render_qs_text(qs_font, "IN 1", 7.62, 19.50, 0.002000, "#1c1c1c", "IN 1"))
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 24.80, 0.001800, "#1c1c1c", "X1"))
    svg_parts.append(render_qs_text(qs_font, "Y", 7.62, 36.30, 0.001800, "#1c1c1c", "Y1"))

    # Group 2: IN 2
    svg_parts.append(render_qs_text(qs_font, "IN 2", 7.62, 46.50, 0.002000, "#1c1c1c", "IN 2"))
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 51.80, 0.001800, "#1c1c1c", "X2"))
    svg_parts.append(render_qs_text(qs_font, "Y", 7.62, 63.30, 0.001800, "#1c1c1c", "Y2"))

    # Group 3: SWITCH
    svg_parts.append(render_qs_text(qs_font, "SWITCH", 7.62, 73.50, 0.001900, "#1c1c1c", "SWITCH"))
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 78.80, 0.001800, "#1c1c1c", "SW X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 7.62, 90.30, 0.001800, "#1c1c1c", "SW Y"))

    # Group 4: OUT
    svg_parts.append(render_qs_text(qs_font, "OUT", 7.62, 100.50, 0.002000, "#1c1c1c", "OUT"))
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 105.80, 0.001800, "#1c1c1c", "OUT X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 7.62, 117.30, 0.001800, "#1c1c1c", "OUT Y"))

    svg_parts.append('</svg>')
    with open('scratch/design_3hp_2.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")

    cairosvg.svg2png(url='scratch/design_3hp_2.svg', write_to='scratch/design_3hp_2.png', output_width=28 * 4, output_height=240 * 4)

    im = Image.open('scratch/design_3hp_2.png')
    draw = ImageDraw.Draw(im)
    r_px = (28 * 4) / 15.24
    jr = 4.15 * r_px
    cx = 7.62 * r_px
    jacks = [25.50, 37.00, 52.50, 64.00, 79.50, 91.00, 106.50, 118.00]
    for y in jacks:
        cy = y * r_px
        draw.ellipse([cx - jr, cy - jr, cx + jr, cy + jr], outline='#00e5ff', width=2)
    im.save('scratch/design_3hp_2_ports.png')
    print("Design 2 (3HP) generated.")

if __name__ == '__main__':
    generate_3hp_design1()
    generate_3hp_design2()
