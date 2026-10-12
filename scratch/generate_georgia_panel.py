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

    # Panel dimensions: 18 HP (91.44 mm x 128.5 mm)
    # MetaModule pixel width: 18 HP = 171 px (9.5 px/HP), Height = 240 px
    panel_w = 91.44
    panel_h = 128.50

    # Title "georgia" in Node.otf (scale 0.0048, baseline 7.620, centered in 91.44 mm)
    scale = 0.0048
    text = "georgia"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (panel_w - total_w) / 2.0

    georgia_title_block = ['  <!-- Label: "georgia" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            georgia_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v1.3.0"
    version_block = [
        '  <!-- Label: "v1.3.0" -->',
        render_qs_text(qs_reg_font, "v1.3.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v1.3.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 18 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (New module default placeholder: #5d5d5d neutral gray per AGENTS.md 6.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 53.50mm) -->',
        f'  <line x1="2.54" y1="53.50" x2="{panel_w - 2.54:.2f}" y2="53.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 83.50mm) -->',
        f'  <line x1="2.54" y1="83.50" x2="{panel_w - 2.54:.2f}" y2="83.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(georgia_title_block)
    svg_parts.extend(version_block)

    # 5 Columns across 91.44 mm
    # cols = [13.52, 29.62, 45.72, 61.82, 77.92] mm
    cols = [13.52 + i * 16.10 for i in range(5)]

    # Uniform typography scale for all labels
    L_SCALE = 0.002200

    # Row 1 Knobs (Center Y = 21.59 mm, label Y = 14.500 mm):
    # Col 0: FREQ
    # Col 1: RANGE (Button + LED centered directly above DEPTH)
    # Col 2: FINE
    # Col 3: SHAPE
    # Col 4: MORPH
    svg_parts.append(render_qs_text(qs_font, "FREQ", cols[0], 14.500, L_SCALE, "#1c1c1c", "FREQ Label"))
    svg_parts.append(render_qs_text(qs_font, "RANGE", cols[1], 14.500, L_SCALE, "#1c1c1c", "RANGE Label"))
    svg_parts.append(render_qs_text(qs_font, "FINE", cols[2], 14.500, L_SCALE, "#1c1c1c", "FINE Label"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", cols[3], 14.500, L_SCALE, "#1c1c1c", "SHAPE Label"))
    svg_parts.append(render_qs_text(qs_font, "MORPH", cols[4], 14.500, L_SCALE, "#1c1c1c", "MORPH Label"))

    # Row 2 Knobs (Center Y = 34.50 mm, label Y = 28.500 mm):
    # Col 0: DENSITY
    # Col 1: DEPTH
    # Col 2: WINDOW
    # Col 3: SPREAD
    # Col 4: AMP
    svg_parts.append(render_qs_text(qs_font, "DENSITY", cols[0], 28.500, L_SCALE, "#1c1c1c", "DENSITY Label"))
    svg_parts.append(render_qs_text(qs_font, "DEPTH", cols[1], 28.500, L_SCALE, "#1c1c1c", "DEPTH Label"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", cols[2], 28.500, L_SCALE, "#1c1c1c", "WINDOW Label"))
    svg_parts.append(render_qs_text(qs_font, "SPREAD", cols[3], 28.500, L_SCALE, "#1c1c1c", "SPREAD Label"))
    svg_parts.append(render_qs_text(qs_font, "AMP", cols[4], 28.500, L_SCALE, "#1c1c1c", "AMP Label"))

    # Row 3 Knobs (Center Y = 47.50 mm, label Y = 41.500 mm):
    # Col 0: TWIST
    # Col 1: ROT X
    # Col 2: ROT Y
    # Col 3: ROT Z
    # Col 4: STRETCH
    svg_parts.append(render_qs_text(qs_font, "TWIST", cols[0], 41.500, L_SCALE, "#1c1c1c", "TWIST Label"))
    svg_parts.append(render_qs_text(qs_font, "ROT X", cols[1], 41.500, L_SCALE, "#1c1c1c", "ROT X Label"))
    svg_parts.append(render_qs_text(qs_font, "ROT Y", cols[2], 41.500, L_SCALE, "#1c1c1c", "ROT Y Label"))
    svg_parts.append(render_qs_text(qs_font, "ROT Z", cols[3], 41.500, L_SCALE, "#1c1c1c", "ROT Z Label"))
    svg_parts.append(render_qs_text(qs_font, "STRETCH", cols[4], 41.500, L_SCALE, "#1c1c1c", "STRETCH Label"))

    # Zone 3: CV Attenuverter Trimpots (3 rows of 5 columns)
    # Row 1 (Center Y = 59.50 mm, label Y = 56.000 mm):
    # AUDIO IN (Col 0), FREQ (Col 1), FINE (Col 2), SHAPE (Col 3), MORPH (Col 4)
    svg_parts.append(render_qs_text(qs_font, "IN", cols[0], 56.000, L_SCALE, "#2c2c2c", "IN Trim"))
    svg_parts.append(render_qs_text(qs_font, "FREQ", cols[1], 56.000, L_SCALE, "#2c2c2c", "FREQ Trim"))
    svg_parts.append(render_qs_text(qs_font, "FINE", cols[2], 56.000, L_SCALE, "#2c2c2c", "FINE Trim"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", cols[3], 56.000, L_SCALE, "#2c2c2c", "SHAPE Trim"))
    svg_parts.append(render_qs_text(qs_font, "MORPH", cols[4], 56.000, L_SCALE, "#2c2c2c", "MORPH Trim"))

    # Row 2 (Center Y = 68.50 mm, label Y = 65.000 mm):
    # DENSITY (Col 0), DEPTH (Col 1), WINDOW (Col 2), SPREAD (Col 3), AMP (Col 4)
    svg_parts.append(render_qs_text(qs_font, "DENSITY", cols[0], 65.000, L_SCALE, "#2c2c2c", "DENSITY Trim"))
    svg_parts.append(render_qs_text(qs_font, "DEPTH", cols[1], 65.000, L_SCALE, "#2c2c2c", "DEPTH Trim"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", cols[2], 65.000, L_SCALE, "#2c2c2c", "WINDOW Trim"))
    svg_parts.append(render_qs_text(qs_font, "SPREAD", cols[3], 65.000, L_SCALE, "#2c2c2c", "SPREAD Trim"))
    svg_parts.append(render_qs_text(qs_font, "AMP", cols[4], 65.000, L_SCALE, "#2c2c2c", "AMP Trim"))

    # Row 3 (Center Y = 77.50 mm, label Y = 74.000 mm):
    # TWIST (Col 0), ROT X (Col 1), ROT Y (Col 2), ROT Z (Col 3), STRETCH (Col 4)
    svg_parts.append(render_qs_text(qs_font, "TWIST", cols[0], 74.000, L_SCALE, "#2c2c2c", "TWIST Trim"))
    svg_parts.append(render_qs_text(qs_font, "ROT X", cols[1], 74.000, L_SCALE, "#2c2c2c", "ROT X Trim"))
    svg_parts.append(render_qs_text(qs_font, "ROT Y", cols[2], 74.000, L_SCALE, "#2c2c2c", "ROT Y Trim"))
    svg_parts.append(render_qs_text(qs_font, "ROT Z", cols[3], 74.000, L_SCALE, "#2c2c2c", "ROT Z Trim"))
    svg_parts.append(render_qs_text(qs_font, "STRETCH", cols[4], 74.000, L_SCALE, "#2c2c2c", "STRETCH Trim"))

    # Zone 4: Jacks
    # Row 1 Jacks (Center Y = 89.50 mm, label Y = 85.000 mm):
    # IN, FREQ, FINE, SHAPE, MORPH
    svg_parts.append(render_qs_text(qs_font, "IN", cols[0], 85.000, L_SCALE, "#1c1c1c", "IN Jack"))
    svg_parts.append(render_qs_text(qs_font, "FREQ", cols[1], 85.000, L_SCALE, "#2c2c2c", "FREQ Jack"))
    svg_parts.append(render_qs_text(qs_font, "FINE", cols[2], 85.000, L_SCALE, "#2c2c2c", "FINE Jack"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", cols[3], 85.000, L_SCALE, "#2c2c2c", "SHAPE Jack"))
    svg_parts.append(render_qs_text(qs_font, "MORPH", cols[4], 85.000, L_SCALE, "#2c2c2c", "MORPH Jack"))

    # Row 2 Jacks (Center Y = 99.00 mm, label Y = 94.500 mm):
    # DENSITY, DEPTH, WINDOW, SPREAD, AMP
    svg_parts.append(render_qs_text(qs_font, "DENSITY", cols[0], 94.500, L_SCALE, "#2c2c2c", "DENSITY Jack"))
    svg_parts.append(render_qs_text(qs_font, "DEPTH", cols[1], 94.500, L_SCALE, "#2c2c2c", "DEPTH Jack"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", cols[2], 94.500, L_SCALE, "#2c2c2c", "WINDOW Jack"))
    svg_parts.append(render_qs_text(qs_font, "SPREAD", cols[3], 94.500, L_SCALE, "#2c2c2c", "SPREAD Jack"))
    svg_parts.append(render_qs_text(qs_font, "AMP", cols[4], 94.500, L_SCALE, "#2c2c2c", "AMP Jack"))

    # Row 3 Jacks (Center Y = 108.50 mm, label Y = 104.000 mm):
    # TWIST, ROT X, ROT Y, ROT Z, STRETCH
    svg_parts.append(render_qs_text(qs_font, "TWIST", cols[0], 104.000, L_SCALE, "#2c2c2c", "TWIST Jack"))
    svg_parts.append(render_qs_text(qs_font, "ROT X", cols[1], 104.000, L_SCALE, "#2c2c2c", "ROT X Jack"))
    svg_parts.append(render_qs_text(qs_font, "ROT Y", cols[2], 104.000, L_SCALE, "#2c2c2c", "ROT Y Jack"))
    svg_parts.append(render_qs_text(qs_font, "ROT Z", cols[3], 104.000, L_SCALE, "#2c2c2c", "ROT Z Jack"))
    svg_parts.append(render_qs_text(qs_font, "STRETCH", cols[4], 104.000, L_SCALE, "#2c2c2c", "STRETCH Jack"))

    # Row 4 Jacks (Fixed Y = 118.00 mm, label Y = 113.500 mm):
    # SYNC IN, X, Y, SYNC OUT (Symmetrically spaced in 4 positions)
    bottom_jacks = [17.50, 36.31, 55.13, 73.94]
    svg_parts.append(render_qs_text(qs_font, "SYNC IN", bottom_jacks[0], 113.500, L_SCALE, "#2c2c2c", "SYNC IN Jack"))
    svg_parts.append(render_qs_text(qs_font, "X", bottom_jacks[1], 113.500, L_SCALE, "#1c1c1c", "X Jack"))
    svg_parts.append(render_qs_text(qs_font, "Y", bottom_jacks[2], 113.500, L_SCALE, "#1c1c1c", "Y Jack"))
    svg_parts.append(render_qs_text(qs_font, "SYNC OUT", bottom_jacks[3], 113.500, L_SCALE, "#2c2c2c", "SYNC OUT Jack"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Georgia.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap (171 x 240 at 4x resolution = 684 x 960)
    os.makedirs('scratch', exist_ok=True)
    verify_png = 'scratch/georgia_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=171 * 4, output_height=240 * 4)

    # Render MetaModule asset (171 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to='metamodule/assets/Georgia.png', output_width=171, output_height=240)
    print("Rendered metamodule/assets/Georgia.png successfully.")

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (171 * 4) / panel_w

    # Knobs:
    kr = 4.75 * r_px
    # Row 1: FREQ (col 0), FINE (col 2), SHAPE (col 3), MORPH (col 4)
    draw.ellipse([cols[0] * r_px - kr, 21.59 * r_px - kr, cols[0] * r_px + kr, 21.59 * r_px + kr], outline='#ffaa00', width=2)
    for kx in cols[2:]:
        draw.ellipse([kx * r_px - kr, 21.59 * r_px - kr, kx * r_px + kr, 21.59 * r_px + kr], outline='#ffaa00', width=2)

    # Range button and LED at cols[1]
    led_r = 1.5 * r_px
    draw.ellipse([cols[1] * r_px - led_r, 15.50 * r_px - led_r, cols[1] * r_px + led_r, 15.50 * r_px + led_r], outline='#d35fb7', width=2)
    btn_r = 2.0 * r_px
    draw.ellipse([cols[1] * r_px - btn_r, 21.59 * r_px - btn_r, cols[1] * r_px + btn_r, 21.59 * r_px + btn_r], outline='#f0e442', width=2)

    # Row 2 & Row 3 Knobs (Y = 34.50, 47.50 mm)
    for ky in [34.50, 47.50]:
        for kx in cols:
            draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Trimpots: 5 cols at Y = 59.50, 68.50, 77.50 mm
    tr = 2.5 * r_px
    for ty in [59.50, 68.50, 77.50]:
        for tx in cols:
            draw.ellipse([tx * r_px - tr, ty * r_px - tr, tx * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks: Row 1 (89.50), Row 2 (99.00), Row 3 (108.50)
    jr = 4.15 * r_px
    for jy in [89.50, 99.00, 108.50]:
        for jx in cols:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    # Row 4 (118.00 mm): 4 bottom jacks
    for jx in bottom_jacks:
        draw.ellipse([jx * r_px - jr, 118.00 * r_px - jr, jx * r_px + jr, 118.00 * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
