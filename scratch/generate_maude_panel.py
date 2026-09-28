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

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    # Panel dimensions: 12 HP (60.96 mm x 128.5 mm)
    panel_w = 60.96
    panel_h = 128.50

    # 4 Standard Columns (pitch = 15.24 mm, margin = 7.62 mm)
    col_x = [7.62, 22.86, 38.10, 53.34]

    # Title "maude" in Node.otf (scale 0.0048, baseline 7.620, centered in 60.96 mm)
    scale = 0.0048
    text = "maude"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (panel_w - total_w) / 2.0

    maude_title_block = ['  <!-- Label: "maude" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            maude_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.25.0"
    version_block = [
        '  <!-- Label: "v2.25.0" -->',
        render_qs_text(qs_reg_font, "v2.25.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.25.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 12 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Placeholder #5d5d5d per Rule 6.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 51.50mm) -->',
        f'  <line x1="2.54" y1="51.50" x2="{panel_w - 2.54:.2f}" y2="51.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / Jacks at Y = 77.50mm) -->',
        f'  <line x1="2.54" y1="77.50" x2="{panel_w - 2.54:.2f}" y2="77.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(maude_title_block)
    svg_parts.extend(version_block)

    # ── Zone 1 & 2: Main Parameter Knobs ──
    # Row 1 Labels (Knob center Y = 18.50 mm, Label Y = 12.20 mm, Scale 0.0024, Color #1c1c1c)
    row1_labels = ["FREQ", "RANGE", "FINE", "PHASE"]
    for i, lbl in enumerate(row1_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 12.200, 0.002400, "#1c1c1c", f"Row 1: {lbl}"))

    # Row 2 Labels: Subharmonic Engine (Knob center Y = 32.00 mm, Label Y = 25.70 mm)
    row2_labels = ["DIV", "RATIO", "SHAPE", "DEPTH"]
    for i, lbl in enumerate(row2_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 25.700, 0.002400, "#1c1c1c", f"Row 2: {lbl}"))

    # Row 3 Labels: Rosette Transformations (Knob center Y = 45.50 mm, Label Y = 39.20 mm)
    row3_labels = ["SPLIT", "FOLD", "TWIST", "STRETCH"]
    for i, lbl in enumerate(row3_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 39.200, 0.002200, "#1c1c1c", f"Row 3: {lbl}"))

    # ── Zone 3: CV Attenuverter Trimpots (3 Rows of 4 Trimpots) ──
    # Trim Row 1: FREQ, FM, FINE, PHASE (Center Y = 56.50 mm, Label Y = 53.20 mm, Scale 0.0014)
    trim1_labels = ["FREQ", "FM", "FINE", "PHASE"]
    for i, lbl in enumerate(trim1_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 53.200, 0.001400, "#2c2c2c", f"Trim 1: {lbl}"))

    # Trim Row 2: DIV, RATIO, SHAPE, DEPTH (Center Y = 64.50 mm, Label Y = 61.20 mm, Scale 0.0014)
    trim2_labels = ["DIV", "RATIO", "SHAPE", "DEPTH"]
    for i, lbl in enumerate(trim2_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 61.200, 0.001400, "#2c2c2c", f"Trim 2: {lbl}"))

    # Trim Row 3: SPLIT, FOLD, TWIST, STRETCH (Center Y = 72.50 mm, Label Y = 69.20 mm, Scale 0.0013)
    trim3_labels = ["SPLIT", "FOLD", "TWIST", "STRETCH"]
    for i, lbl in enumerate(trim3_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 69.200, 0.001300, "#2c2c2c", f"Trim 3: {lbl}"))

    # ── Zone 4: I/O Jacks (4 Rows of 4 Jacks) ──
    # Jack Row 1 (CV Inputs): FREQ, FM, FINE, PHASE (Center Y = 85.00 mm, Label Y = 80.20 mm, Scale 0.0014)
    jack1_labels = ["FREQ", "FM", "FINE", "PHASE"]
    for i, lbl in enumerate(jack1_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 80.200, 0.001400, "#2c2c2c", f"Jack 1: {lbl}"))

    # Jack Row 2 (CV Inputs): DIV, RATIO, SHAPE, DEPTH (Center Y = 96.00 mm, Label Y = 91.20 mm, Scale 0.0014)
    jack2_labels = ["DIV", "RATIO", "SHAPE", "DEPTH"]
    for i, lbl in enumerate(jack2_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 91.200, 0.001400, "#2c2c2c", f"Jack 2: {lbl}"))

    # Jack Row 3 (CV Inputs): SPLIT, FOLD, TWIST, STRETCH (Center Y = 107.00 mm, Label Y = 102.20 mm, Scale 0.0013)
    jack3_labels = ["SPLIT", "FOLD", "TWIST", "STRETCH"]
    for i, lbl in enumerate(jack3_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 102.200, 0.001300, "#2c2c2c", f"Jack 3: {lbl}"))

    # Jack Row 4 (Fixed Bottom Signal/Sync Row: SYNC IN, X OUT, Y OUT, SYNC OUT, Center Y = 118.00 mm)
    svg_parts.append(render_qs_text(qs_font, "SYNC", col_x[0], 113.200, 0.001400, "#2c2c2c", "Jack 4: SYNC IN"))
    svg_parts.append(render_qs_text(qs_font, "X", col_x[1], 113.200, 0.001800, "#3c3c3c", "Jack 4: X OUT"))
    svg_parts.append(render_qs_text(qs_font, "Y", col_x[2], 113.200, 0.001800, "#3c3c3c", "Jack 4: Y OUT"))
    svg_parts.append(render_qs_text(qs_font, "SYNC", col_x[3], 113.200, 0.001400, "#2c2c2c", "Jack 4: SYNC OUT"))

    # Centered IN / OUT port subheaders (below jacks at Y = 124.50 mm)
    svg_parts.append(render_qs_text(qs_font, "IN", col_x[0], 124.500, 0.001500, "#2c2c2c", "SYNC IN Sub"))
    svg_parts.append(render_qs_text(qs_font, "OUT", (col_x[1] + col_x[2]) / 2.0, 124.500, 0.001800, "#2c2c2c", "OUT Sub"))
    svg_parts.append(render_qs_text(qs_font, "OUT", col_x[3], 124.500, 0.001500, "#2c2c2c", "SYNC OUT Sub"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Maude.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (12 HP).")

    # Render verification bitmap (114 x 240 at 4x resolution = 456 x 960)
    verify_png = 'scratch/maude_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=114 * 4, output_height=240 * 4)

    # Render MetaModule asset (114 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to='metamodule/assets/Maude.png', output_width=114, output_height=240)
    print("Rendered metamodule/assets/Maude.png successfully.")

    # Draw hardware component overlay for clearance verification
    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (114 * 4) / panel_w

    # Knobs at Y = [18.50, 32.00, 45.50]
    kr = 4.75 * r_px
    for i, ky in enumerate([18.50, 32.00, 45.50]):
        for j, kx in enumerate(col_x):
            if i == 0 and j == 1:
                # Range Button + LED
                btn_r = 3.0 * r_px
                led_r = 1.2 * r_px
                draw.ellipse([kx * r_px - led_r, 13.50 * r_px - led_r, kx * r_px + led_r, 13.50 * r_px + led_r], outline='#d35fb7', width=2)
                draw.ellipse([kx * r_px - btn_r, 18.50 * r_px - btn_r, kx * r_px + btn_r, 18.50 * r_px + btn_r], outline='#f0e442', width=2)
            else:
                draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Trimpots at Y = [56.50, 64.50, 72.50]
    tr = 2.4 * r_px
    for ty in [56.50, 64.50, 72.50]:
        for tx in col_x:
            draw.ellipse([tx * r_px - tr, ty * r_px - tr, tx * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks at Y = [85.00, 96.00, 107.00, 118.00]
    jr = 4.0 * r_px
    for jy in [85.00, 96.00, 107.00, 118.00]:
        for jx in col_x:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00aaff', width=2)

    im.save(verify_png)
    print("Saved verification image to scratch/maude_verify.png.")

if __name__ == '__main__':
    main()
