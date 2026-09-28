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

    # Panel dimensions: 14 HP (71.12 mm x 128.5 mm)
    panel_w = 71.12
    panel_h = 128.50

    # 5 Columns layout (centered with pitch = 13.50 mm)
    col_x = [8.56, 22.06, 35.56, 49.06, 62.56]

    # Title "polly" in Node.otf (scale 0.0048, baseline 7.620, centered in 71.12 mm)
    scale = 0.0048
    text = "polly"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (panel_w - total_w) / 2.0

    polly_title_block = ['  <!-- Label: "polly" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            polly_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.25.0"
    version_block = [
        '  <!-- Label: "v2.25.0" -->',
        render_qs_text(qs_reg_font, "v2.25.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.25.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 14 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Placeholder #5d5d5d per Rule 6.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 60.50mm) -->',
        f'  <line x1="2.54" y1="60.50" x2="{panel_w - 2.54:.2f}" y2="60.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / Jacks at Y = 85.50mm) -->',
        f'  <line x1="2.54" y1="85.50" x2="{panel_w - 2.54:.2f}" y2="85.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]

    svg_parts.extend(polly_title_block)
    svg_parts.extend(version_block)

    # ── Zone 1 & 2: Primary Parameter Rows ─────────────────────────
    # Row 1 Labels (Baseline Y = 13.07 mm, Scale 0.0022, Color #1c1c1c)
    row1_labels = ["FREQ", "RANGE", "FINE", "SIDES", "ANGLE"]
    for i, lbl in enumerate(row1_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 13.070, 0.002200, "#1c1c1c", f"Row 1: {lbl}"))

    # Row 2 Labels (Baseline Y = 28.50 mm, Scale 0.0022, Color #1c1c1c)
    row2_labels = ["PINCH", "TWIST", "FILLET", "PHASE", "BULGE"]
    for i, lbl in enumerate(row2_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 28.500, 0.002200, "#1c1c1c", f"Row 2: {lbl}"))

    # Row 3 Labels (Baseline Y = 44.00 mm, Scale 0.0022, Color #1c1c1c)
    row3_labels = ["CLUST", "HARM", "TRIAD", "SWING", "SPLIT"]
    for i, lbl in enumerate(row3_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 44.000, 0.002200, "#1c1c1c", f"Row 3: {lbl}"))

    # ── Zone 3: CV Attenuverter Trimpots ───────────────────────────
    # Row 1 Attenuverters (Baseline Y = 64.80 mm, Scale 0.0018, Color #2c2c2c)
    trim1_labels = ["FREQ", "SIDES", "ANGLE", "PINCH", "TWIST"]
    for i, lbl in enumerate(trim1_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 64.800, 0.001800, "#2c2c2c", f"Trim 1: {lbl}"))

    # Row 2 Attenuverters (Baseline Y = 74.80 mm, Scale 0.0018, Color #2c2c2c)
    trim2_labels = ["FM", "FILLET", "CLUST", "PHASE", "BULGE"]
    for i, lbl in enumerate(trim2_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 74.800, 0.001800, "#2c2c2c", f"Trim 2: {lbl}"))

    # ── Zone 4: I/O Jacks ──────────────────────────────────────────
    # Jack Row 1 (Inputs) (Baseline Y = 89.20 mm, Scale 0.0019, Color #2c2c2c)
    jack1_labels = ["FREQ", "SIDES", "ANGLE", "PINCH", "TWIST"]
    for i, lbl in enumerate(jack1_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 89.200, 0.001900, "#2c2c2c", f"Jack 1: {lbl}"))

    # Jack Row 2 (Inputs) (Baseline Y = 100.20 mm, Scale 0.0019, Color #2c2c2c)
    jack2_labels = ["FM", "FILLET", "CLUST", "PHASE", "BULGE"]
    for i, lbl in enumerate(jack2_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 100.200, 0.001900, "#2c2c2c", f"Jack 2: {lbl}"))

    # Jack Row 3 (Sync & Outputs) (Baseline Y = 112.50 mm, Scale 0.0019, Color #2c2c2c)
    jack3_labels = ["SYNC", "SPLIT", "SYNC", "X", "Y"]
    for i, lbl in enumerate(jack3_labels):
        svg_parts.append(render_qs_text(qs_font, lbl, col_x[i], 112.500, 0.001900, "#2c2c2c", f"Jack 3: {lbl}"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Polly.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap (133 x 240 at 4x resolution = 532 x 960)
    verify_png = 'scratch/polly_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=133 * 4, output_height=240 * 4)

    # Render MetaModule asset (133 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to='metamodule/assets/Polly.png', output_width=133, output_height=240)
    print("Rendered metamodule/assets/Polly.png successfully.")

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (133 * 4) / panel_w

    # Knobs at Y = 21.59, 37.00, 52.50 mm (radius ~ 4.75 mm)
    kr = 4.75 * r_px
    for ky in [21.59, 37.00, 52.50]:
        for i, kx in enumerate(col_x):
            if ky == 21.59 and i == 1:
                # Row 1 Col 1 is Range LED + Button
                continue
            draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Range Button and LED at col_x[1] (22.06 mm)
    led_r = 1.5 * r_px
    draw.ellipse([col_x[1] * r_px - led_r, 15.50 * r_px - led_r, col_x[1] * r_px + led_r, 15.50 * r_px + led_r], outline='#d35fb7', width=2)
    btn_r = 2.0 * r_px
    draw.ellipse([col_x[1] * r_px - btn_r, 21.59 * r_px - btn_r, col_x[1] * r_px + btn_r, 21.59 * r_px + btn_r], outline='#f0e442', width=2)

    # Trimpots: Y = 69.50 and 79.50 mm
    tr = 2.5 * r_px
    for ty in [69.50, 79.50]:
        for tx in col_x:
            draw.ellipse([tx * r_px - tr, ty * r_px - tr, tx * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks: Row 1 (93.50 mm), Row 2 (104.50 mm), Row 3 (117.50 mm)
    jr = 4.15 * r_px
    for jy in [93.50, 104.50, 117.50]:
        for jx in col_x:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
