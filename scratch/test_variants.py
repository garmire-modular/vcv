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

def generate_variant_a():
    # Variant A: Left-aligned labels (or left of jack at x = 6.2 mm)
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    scale = 0.0048
    text = "switch"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    title_block = ['  <!-- Label: "switch" -->']
    curr_x = start_x
    for c in text:
        w = hmtx[cmap[ord(c)]][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    v219_version_block = [
        render_qs_text(qs_reg_font, "v2.19.0", 15.24, 10.414, 0.001600, "#aaaaaa", "v2.19.0")
    ]

    # Let's space the 4 pairs:
    # Pair 1: Jack 1 (22.50), Jack 2 (34.50)  -- Delineator at 42.00
    # Pair 2: Jack 3 (49.50), Jack 4 (61.50)  -- Delineator at 69.00
    # Pair 3: Jack 5 (76.50), Jack 6 (88.50)  -- Delineator at 96.00
    # Pair 4: Jack 7 (103.50), Jack 8 (118.00) wait: Jack 7 (106.00), Jack 8 (118.00)
    # Let's calculate:
    # Pair 4: 106.00, 118.00 (pitch 12)
    # Del 3 at 98.50
    # Pair 3: 81.00, 93.00 (pitch 12)
    # Del 2 at 73.50
    # Pair 2: 56.00, 68.00 (pitch 12)
    # Del 1 at 48.50
    # Pair 1: 31.00, 43.00 (pitch 12)
    # Gap between pairs: 56 - 43 = 13.0 mm. Delineator at 49.5 mm (6.5mm from each center).
    # Wait, 31 mm is a bit low. What if:
    # Pitch within pair = 11.5 mm, gap between pairs = 15.5 mm:
    # Pair 4: 106.50, 118.00. Del 3: 99.00
    # Pair 3: 79.50, 91.00.   Del 2: 72.00
    # Pair 2: 52.50, 64.00.   Del 1: 45.00
    # Pair 1: 25.50, 37.00.
    # Look at that:
    # Jack 1: 25.50 mm (leaves 15mm from version tag!)
    # Jack 2: 37.00 mm (pitch 11.5mm)
    # Del 1: 44.75 mm
    # Jack 3: 52.50 mm (pitch 15.5mm from Jack 2)
    # Jack 4: 64.00 mm (pitch 11.5mm)
    # Del 2: 71.75 mm
    # Jack 5: 79.50 mm (pitch 15.5mm from Jack 4)
    # Jack 6: 91.00 mm (pitch 11.5mm)
    # Del 3: 98.75 mm
    # Jack 7: 106.50 mm (pitch 15.5mm from Jack 6)
    # Jack 8: 118.00 mm (pitch 11.5mm)
    # This geometry is symmetric and elegant!

    # Now let's test where labels go:
    # If labels are centered above each jack:
    # Jack 1 (25.50): label baseline at 19.50 mm
    # Jack 2 (37.00): label baseline at 31.00 mm  (Jack 1 bottom is 29.65, so 31.0 is below Jack 1, but still close)
    # What if label is to the LEFT of the jack?
    # Center X = 6.2 mm. Center Y = Jack Y + 0.6 mm (to align baseline visually with jack center).
    # Let's test left-side labels in Variant A!

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <rect width="30.48" height="128.5" fill="#7c7c7c"/>',
        '  <!-- Delineators -->',
        '  <line x1="2.54" y1="44.75" x2="27.94" y2="44.75" stroke="#999999" stroke-width="0.176"/>',
        '  <line x1="2.54" y1="71.75" x2="27.94" y2="71.75" stroke="#999999" stroke-width="0.176"/>',
        '  <line x1="2.54" y1="98.75" x2="27.94" y2="98.75" stroke="#999999" stroke-width="0.176"/>',
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(v219_version_block)

    jacks = [
        ("X1", 25.50),
        ("Y1", 37.00),
        ("X2", 52.50),
        ("Y2", 64.00),
        ("SW X", 79.50),
        ("SW Y", 91.00),
        ("OUT X", 106.50),
        ("OUT Y", 118.00),
    ]

    for label, y in jacks:
        # Left-side label: centered horizontally in the left margin (x = 6.5 mm)
        # Font baseline at y + 0.7 mm
        svg_parts.append(render_qs_text(qs_font, label, 6.5, y + 0.7, 0.002200, "#1c1c1c", label))

    svg_parts.append('</svg>')
    with open('scratch/variant_a.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")

    cairosvg.svg2png(url='scratch/variant_a.svg', write_to='scratch/variant_a.png', output_width=57*4, output_height=240*4)

    # Overlay ports
    im = Image.open('scratch/variant_a.png')
    draw = ImageDraw.Draw(im)
    r_px = (57 * 4) / 30.48
    jr = 4.15 * r_px
    cx = 15.24 * r_px
    for _, y in jacks:
        cy = y * r_px
        draw.ellipse([cx - jr, cy - jr, cx + jr, cy + jr], outline='cyan', width=2)
    im.save('scratch/variant_a_ports.png')
    print("Variant A generated.")

def generate_variant_b():
    # Variant B: Group headers above each pair, X and Y indicated on left and right, or centered group header
    # Group 1 (IN 1): Header at 18.00 mm. Jack 1 (25.50) has 'X' on left. Jack 2 (37.00) has 'Y' on left.
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    scale = 0.0048
    text = "switch"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    title_block = ['  <!-- Label: "switch" -->']
    curr_x = start_x
    for c in text:
        w = hmtx[cmap[ord(c)]][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    v219_version_block = [
        render_qs_text(qs_reg_font, "v2.19.0", 15.24, 10.414, 0.001600, "#aaaaaa", "v2.19.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <rect width="30.48" height="128.5" fill="#7c7c7c"/>',
        '  <!-- Delineators -->',
        '  <line x1="2.54" y1="45.00" x2="27.94" y2="45.00" stroke="#999999" stroke-width="0.176"/>',
        '  <line x1="2.54" y1="72.00" x2="27.94" y2="72.00" stroke="#999999" stroke-width="0.176"/>',
        '  <line x1="2.54" y1="99.00" x2="27.94" y2="99.00" stroke="#999999" stroke-width="0.176"/>',
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(v219_version_block)

    # Group 1: INPUT 1
    svg_parts.append(render_qs_text(qs_font, "INPUT 1", 15.24, 17.50, 0.002200, "#1c1c1c", "INPUT 1"))
    svg_parts.append(render_qs_text(qs_font, "X", 6.5, 25.50 + 0.7, 0.002600, "#1c1c1c", "X1"))
    svg_parts.append(render_qs_text(qs_font, "Y", 6.5, 37.00 + 0.7, 0.002600, "#1c1c1c", "Y1"))

    # Group 2: INPUT 2
    svg_parts.append(render_qs_text(qs_font, "INPUT 2", 15.24, 48.00, 0.002200, "#1c1c1c", "INPUT 2"))
    svg_parts.append(render_qs_text(qs_font, "X", 6.5, 54.50 + 0.7, 0.002600, "#1c1c1c", "X2"))
    svg_parts.append(render_qs_text(qs_font, "Y", 6.5, 66.00 + 0.7, 0.002600, "#1c1c1c", "Y2"))

    # Group 3: SWITCH (TRACK)
    svg_parts.append(render_qs_text(qs_font, "SWITCH", 15.24, 75.00, 0.002200, "#1c1c1c", "SWITCH"))
    svg_parts.append(render_qs_text(qs_font, "X", 6.5, 81.50 + 0.7, 0.002600, "#1c1c1c", "SW X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 6.5, 93.00 + 0.7, 0.002600, "#1c1c1c", "SW Y"))

    # Group 4: OUTPUT
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 102.00, 0.002200, "#1c1c1c", "OUT"))
    svg_parts.append(render_qs_text(qs_font, "X", 6.5, 107.50 + 0.7, 0.002600, "#1c1c1c", "OUT X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 6.5, 118.00 + 0.7, 0.002600, "#1c1c1c", "OUT Y"))

    svg_parts.append('</svg>')
    with open('scratch/variant_b.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")

    cairosvg.svg2png(url='scratch/variant_b.svg', write_to='scratch/variant_b.png', output_width=57*4, output_height=240*4)

    im = Image.open('scratch/variant_b.png')
    draw = ImageDraw.Draw(im)
    r_px = (57 * 4) / 30.48
    jr = 4.15 * r_px
    cx = 15.24 * r_px
    jacks = [25.50, 37.00, 54.50, 66.00, 81.50, 93.00, 107.50, 118.00]
    for y in jacks:
        cy = y * r_px
        draw.ellipse([cx - jr, cy - jr, cx + jr, cy + jr], outline='cyan', width=2)
    im.save('scratch/variant_b_ports.png')
    print("Variant B generated.")

if __name__ == '__main__':
    generate_variant_a()
    generate_variant_b()
