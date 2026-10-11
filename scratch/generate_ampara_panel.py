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

    # Panel dimensions: 16 HP (81.28 mm x 128.5 mm)
    # MetaModule pixel width: 16 HP = 152 px (9.5 px/HP), Height = 240 px
    panel_w = 81.28
    panel_h = 128.50

    # Title "ampara" in Node.otf (scale 0.0048, baseline 7.620, centered in 81.28 mm)
    scale = 0.0048
    text = "ampara"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (panel_w - total_w) / 2.0

    ampara_title_block = ['  <!-- Label: "ampara" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            ampara_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.26.0"
    version_block = [
        '  <!-- Label: "v2.26.0" -->',
        render_qs_text(qs_reg_font, "v2.26.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.26.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 16 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (New module default placeholder: #5d5d5d neutral gray per AGENTS.md 6.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 56.50mm) -->',
        f'  <line x1="2.54" y1="56.50" x2="{panel_w - 2.54:.2f}" y2="56.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 84.50mm) -->',
        f'  <line x1="2.54" y1="84.50" x2="{panel_w - 2.54:.2f}" y2="84.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(ampara_title_block)
    svg_parts.extend(version_block)

    # Knob Columns (5 columns across 81.28 mm)
    # col_x = [12.14, 26.39, 40.64, 54.89, 69.14]
    knob_cols = [12.14, 26.39, 40.64, 54.89, 69.14]

    # Row 1 Knobs: FREQ, FINE, SHAPE, WARP, WINDOW (Center Y = 21.59, label Y = 14.500)
    svg_parts.append(render_qs_text(qs_font, "FREQ", knob_cols[0], 14.500, 0.002600, "#1c1c1c", "FREQ Label"))
    svg_parts.append(render_qs_text(qs_font, "FINE", knob_cols[1], 14.500, 0.002600, "#1c1c1c", "FINE Label"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", knob_cols[2], 14.500, 0.002600, "#1c1c1c", "SHAPE Label"))
    svg_parts.append(render_qs_text(qs_font, "WARP", knob_cols[3], 14.500, 0.002600, "#1c1c1c", "WARP Label"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", knob_cols[4], 14.500, 0.002200, "#1c1c1c", "WINDOW Label"))

    # Row 2 Knobs: DISP, RISE, FALL, MORPH, PHASE (Center Y = 43.00, label Y = 35.800)
    svg_parts.append(render_qs_text(qs_font, "DISP", knob_cols[0], 35.800, 0.002600, "#1c1c1c", "DISP Label"))
    svg_parts.append(render_qs_text(qs_font, "RISE", knob_cols[1], 35.800, 0.002600, "#1c1c1c", "RISE Label"))
    svg_parts.append(render_qs_text(qs_font, "FALL", knob_cols[2], 35.800, 0.002600, "#1c1c1c", "FALL Label"))
    svg_parts.append(render_qs_text(qs_font, "MORPH", knob_cols[3], 35.800, 0.002400, "#1c1c1c", "MORPH Label"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", knob_cols[4], 35.800, 0.002400, "#1c1c1c", "PHASE Label"))

    # Zone 3: CV Attenuverter Trimpots
    # Row 1 Attenuverters (5 Cols): FREQ, SHAPE, WARP, WINDOW, DISP (Center Y = 63.50, label Y = 59.500)
    trim_row1_cols = [12.14, 26.39, 40.64, 54.89, 69.14]
    svg_parts.append(render_qs_text(qs_font, "FREQ", trim_row1_cols[0], 59.500, 0.001800, "#2c2c2c", "FREQ Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", trim_row1_cols[1], 59.500, 0.001800, "#2c2c2c", "SHAPE Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "WARP", trim_row1_cols[2], 59.500, 0.001800, "#2c2c2c", "WARP Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", trim_row1_cols[3], 59.500, 0.001500, "#2c2c2c", "WINDOW Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "DISP", trim_row1_cols[4], 59.500, 0.001800, "#2c2c2c", "DISP Trim Label"))

    # Row 2 Attenuverters (4 Cols): RISE, FALL, MORPH, PHASE (Center Y = 75.50, label Y = 71.500)
    trim_row2_cols = [19.265, 33.515, 47.765, 62.015]
    svg_parts.append(render_qs_text(qs_font, "RISE", trim_row2_cols[0], 71.500, 0.001800, "#2c2c2c", "RISE Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "FALL", trim_row2_cols[1], 71.500, 0.001800, "#2c2c2c", "FALL Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "MORPH", trim_row2_cols[2], 71.500, 0.001700, "#2c2c2c", "MORPH Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", trim_row2_cols[3], 71.500, 0.001700, "#2c2c2c", "PHASE Trim Label"))

    # Zone 4: I/O Jacks (4 Columns: 13.09, 31.46, 49.82, 68.19 mm)
    jack_cols = [13.09, 31.46, 49.82, 68.19]

    # Jack Row 1 (Inputs): FREQ, SHAPE, WARP, WINDOW (Center Y = 89.50, label Y = 85.000)
    svg_parts.append(render_qs_text(qs_font, "FREQ", jack_cols[0], 85.000, 0.001800, "#2c2c2c", "FREQ Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", jack_cols[1], 85.000, 0.001800, "#2c2c2c", "SHAPE Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "WARP", jack_cols[2], 85.000, 0.001800, "#2c2c2c", "WARP Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", jack_cols[3], 85.000, 0.001500, "#2c2c2c", "WINDOW Jack Label"))

    # Jack Row 2 (Inputs): DISP, RISE, FALL, MORPH (Center Y = 99.00, label Y = 94.500)
    svg_parts.append(render_qs_text(qs_font, "DISP", jack_cols[0], 94.500, 0.001800, "#2c2c2c", "DISP Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "RISE", jack_cols[1], 94.500, 0.001800, "#2c2c2c", "RISE Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "FALL", jack_cols[2], 94.500, 0.001800, "#2c2c2c", "FALL Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "MORPH", jack_cols[3], 94.500, 0.001700, "#2c2c2c", "MORPH Jack Label"))

    # Jack Row 3: PHASE, SYNC IN, IN L, IN R (Center Y = 108.50, label Y = 104.000)
    svg_parts.append(render_qs_text(qs_font, "PHASE", jack_cols[0], 104.000, 0.001700, "#2c2c2c", "PHASE Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "SYNC", jack_cols[1], 104.000, 0.001800, "#2c2c2c", "SYNC IN Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "IN L", jack_cols[2], 104.000, 0.002000, "#1c1c1c", "IN L Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "IN R", jack_cols[3], 104.000, 0.002000, "#1c1c1c", "IN R Jack Label"))

    # Jack Row 4 (Fixed Y = 118.00): SYNC OUT, (Center Blank/Ground), OUT X, OUT Y (label Y = 113.500)
    svg_parts.append(render_qs_text(qs_font, "SYNC", jack_cols[0], 113.500, 0.001800, "#2c2c2c", "SYNC OUT Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "OUT X", jack_cols[2], 113.500, 0.002200, "#1c1c1c", "OUT X Label"))
    svg_parts.append(render_qs_text(qs_font, "OUT Y", jack_cols[3], 113.500, 0.002200, "#1c1c1c", "OUT Y Label"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Ampara.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap (152 x 240 at 4x resolution = 608 x 960)
    os.makedirs('scratch', exist_ok=True)
    verify_png = 'scratch/ampara_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=152 * 4, output_height=240 * 4)

    # Render MetaModule asset (152 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to='metamodule/assets/Ampara.png', output_width=152, output_height=240)
    print("Rendered metamodule/assets/Ampara.png successfully.")

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (152 * 4) / panel_w

    # Knobs at Y = 21.59, 43.00 mm (radius ~ 4.75 mm)
    kr = 4.75 * r_px
    for ky in [21.59, 43.00]:
        for kx in knob_cols:
            draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Range Button and LED at center between Col 1 and Col 2 (19.265 mm)
    btn_x = 19.265
    led_r = 1.5 * r_px
    draw.ellipse([btn_x * r_px - led_r, 15.50 * r_px - led_r, btn_x * r_px + led_r, 15.50 * r_px + led_r], outline='#d35fb7', width=2)
    btn_r = 2.0 * r_px
    draw.ellipse([btn_x * r_px - btn_r, 21.59 * r_px - btn_r, btn_x * r_px + btn_r, 21.59 * r_px + btn_r], outline='#f0e442', width=2)

    # Trimpots: Row 1 (63.50 mm) & Row 2 (75.50 mm)
    tr = 2.5 * r_px
    for tx in trim_row1_cols:
        draw.ellipse([tx * r_px - tr, 63.50 * r_px - tr, tx * r_px + tr, 63.50 * r_px + tr], outline='#00ff88', width=2)
    for tx in trim_row2_cols:
        draw.ellipse([tx * r_px - tr, 75.50 * r_px - tr, tx * r_px + tr, 75.50 * r_px + tr], outline='#00ff88', width=2)

    # Jacks: Row 1 (89.50), Row 2 (99.00), Row 3 (108.50), Row 4 (118.00)
    jr = 4.15 * r_px
    for jy in [89.50, 99.00, 108.50]:
        for jx in jack_cols:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    # Row 4 (118.00 mm): SYNC OUT (col 0), OUT X (col 2), OUT Y (col 3)
    for jx in [jack_cols[0], jack_cols[2], jack_cols[3]]:
        draw.ellipse([jx * r_px - jr, 118.00 * r_px - jr, jx * r_px + jr, 118.00 * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
