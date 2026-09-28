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

    # Panel dimensions: 6 HP (30.48 mm x 128.5 mm)
    panel_w = 30.48
    panel_h = 128.5

    # Title "maude" in Node.otf (scale 0.0048, baseline 7.620, centered in 30.48 mm)
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
        f'  <!-- Panel Background: 6 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (New module default placeholder: #5d5d5d neutral gray per AGENTS.md 6.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 52.50mm) -->',
        f'  <line x1="2.54" y1="52.50" x2="{panel_w - 2.54:.2f}" y2="52.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 80.50mm) -->',
        f'  <line x1="2.54" y1="80.50" x2="{panel_w - 2.54:.2f}" y2="80.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(maude_title_block)
    svg_parts.extend(version_block)

    # Columns:
    # 2-Column: 7.62 mm, 22.86 mm (Center = 15.24 mm)

    # Row 1 Knobs: DIVISIONS & RATIO (Knob center Y = 21.59, label Y = 13.070)
    svg_parts.append(render_qs_text(qs_font, "DIV", 7.62, 13.070, 0.003000, "#1c1c1c", "DIVISIONS Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "RATIO", 22.86, 13.070, 0.002800, "#1c1c1c", "RATIO Knob Label"))

    # Row 2 Knobs: DEPTH & WARP (Knob center Y = 43.00, label Y = 34.500)
    svg_parts.append(render_qs_text(qs_font, "DEPTH", 7.62, 34.500, 0.002800, "#1c1c1c", "DEPTH Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "WARP", 22.86, 34.500, 0.002800, "#1c1c1c", "WARP Knob Label"))

    # Zone 3: CV Attenuverter Trimpots (2 Rows of 2 Trimpots at X = 7.62, 22.86 mm)
    # Row 1 Trimpots: DIV & RATIO (Center Y = 61.00, label Y = 56.500)
    svg_parts.append(render_qs_text(qs_font, "DIV", 7.62, 56.500, 0.002000, "#2c2c2c", "DIV CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "RATIO", 22.86, 56.500, 0.001800, "#2c2c2c", "RATIO CV Trim Label"))

    # Row 2 Trimpots: DEPTH & WARP (Center Y = 73.00, label Y = 68.500)
    svg_parts.append(render_qs_text(qs_font, "DEPTH", 7.62, 68.500, 0.001800, "#2c2c2c", "DEPTH CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "WARP", 22.86, 68.500, 0.001800, "#2c2c2c", "WARP CV Trim Label"))

    # Zone 4: I/O Jacks (4 Rows of 2 Jacks at X = 7.62, 22.86 mm, matching Copycat)
    # Header above Row 1: X (7.62) & Y (22.86) at Y = 84.000
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 84.000, 0.003000, "#1c1c1c", "X Column Header"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 84.000, 0.003000, "#1c1c1c", "Y Column Header"))

    # Jack Row 1 (Signal Inputs): Centered label IN between X and Y at Y = 88.000
    svg_parts.append(render_qs_text(qs_font, "IN", 15.24, 88.000, 0.002400, "#2c2c2c", "Signal IN Label"))

    # Jack Row 2 (CV Inputs: DIV & RATIO): Centered label CV at Y = 97.500
    svg_parts.append(render_qs_text(qs_font, "CV", 15.24, 97.500, 0.002400, "#2c2c2c", "Row 2 CV Label"))

    # Jack Row 3 (CV Inputs: DEPTH & WARP): Centered label CV at Y = 107.000
    svg_parts.append(render_qs_text(qs_font, "CV", 15.24, 107.000, 0.002400, "#2c2c2c", "Row 3 CV Label"))

    # Jack Row 4 (Signal Outputs): Centered label OUT between X and Y at Y = 116.500
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 116.500, 0.002400, "#2c2c2c", "Signal OUT Label"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Maude.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap (57 x 240 at 4x resolution = 228 x 960)
    verify_png = 'scratch/maude_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=57 * 4, output_height=240 * 4)

    # Render MetaModule asset (57 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to='metamodule/assets/Maude.png', output_width=57, output_height=240)
    print("Rendered metamodule/assets/Maude.png successfully.")

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (57 * 4) / panel_w

    # Knobs at Y = 21.59, 43.00 mm (radius ~ 4.5 mm, centered at 7.62 and 22.86 mm)
    kr = 4.5 * r_px
    for ky in [21.59, 43.00]:
        for kx in [7.62, 22.86]:
            draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Trimpots: 2 Rows (Y = 61.00, 73.00 mm at X = 7.62, 22.86 mm)
    tr = 2.5 * r_px
    for ty in [61.00, 73.00]:
        for tx in [7.62, 22.86]:
            draw.ellipse([tx * r_px - tr, ty * r_px - tr, tx * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks: 4 Rows (Y = 89.50, 99.00, 108.50, 118.00 mm at X = 7.62, 22.86 mm)
    jr = 4.15 * r_px
    for jy in [89.50, 99.00, 108.50, 118.00]:
        for jx in [7.62, 22.86]:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
