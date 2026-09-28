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

    # Panel dimensions: 8 HP (40.64 mm x 128.5 mm)
    panel_w = 40.64
    panel_h = 128.5

    # Title "lisa" in Node.otf (scale 0.0048, baseline 7.620, centered in 40.64 mm)
    scale = 0.0048
    text = "lisa"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (panel_w - total_w) / 2.0

    lisa_title_block = ['  <!-- Label: "lisa" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            lisa_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.25.0"
    version_block = [
        '  <!-- Label: "v2.25.0" -->',
        render_qs_text(qs_reg_font, "v2.25.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.25.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 8 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Generate class: charcoal top anchor, lavender and deep petrol bands, flush X=0, width 2.540mm) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="28.448" fill="#1A1A1A" stroke="none"/>',
        '    <rect x="0.000" y="48.248" width="2.540" height="19.558" fill="#B8A0E8" stroke="none"/>',
        '    <rect x="0.000" y="67.806" width="2.540" height="40.894" fill="#005566" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 67.50mm) -->',
        f'  <line x1="2.54" y1="67.50" x2="{panel_w - 2.54:.2f}" y2="67.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 93.00mm) -->',
        f'  <line x1="2.54" y1="93.00" x2="{panel_w - 2.54:.2f}" y2="93.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(lisa_title_block)
    svg_parts.extend(version_block)

    # Knob Columns: X_left = 10.82 mm, X_right = 29.82 mm, Center = 20.32 mm
    # 4-Column Ports/Trimpots: 6.07, 15.57, 25.07, 34.57 mm

    # Row 1 Knobs: FREQ & FINE (Knob center Y = 23.00, label Y = 14.500, scale = 0.002800)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 10.82, 14.500, 0.002800, "#1c1c1c", "FREQ Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "FINE", 29.82, 14.500, 0.002800, "#1c1c1c", "FINE Knob Label"))

    # Row 2 Knobs: RATIO X & Y (Knob center Y = 42.00, label Y = 34.500)
    svg_parts.append(render_qs_text(qs_font, "RATIO", 20.32, 32.200, 0.002000, "#2c2c2c", "RATIO Group Header"))
    svg_parts.append(render_qs_text(qs_font, "X", 10.82, 34.500, 0.002800, "#1c1c1c", "X Ratio Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "Y", 29.82, 34.500, 0.002800, "#1c1c1c", "Y Ratio Knob Label"))

    # Row 3 Knobs: PHASE & DAMP (Knob center Y = 60.50, label Y = 53.000)
    svg_parts.append(render_qs_text(qs_font, "PHASE", 10.82, 53.000, 0.002400, "#1c1c1c", "PHASE Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "DAMP", 29.82, 53.000, 0.002400, "#1c1c1c", "DAMP Knob Label"))

    # Zone 3: CV Attenuverter Trimpots
    # Attenuverter Row 1: FREQ, FM, X, Y (Center Y = 75.50, label Y = 71.000)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 6.07, 71.000, 0.001900, "#2c2c2c", "FREQ CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "FM", 15.57, 71.000, 0.001900, "#2c2c2c", "FM CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "X", 25.07, 71.000, 0.001900, "#2c2c2c", "X Ratio CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "Y", 34.57, 71.000, 0.001900, "#2c2c2c", "Y Ratio CV Trim Label"))

    # Attenuverter Row 2: PHASE, DAMP (Center Y = 86.50, label Y = 82.000)
    svg_parts.append(render_qs_text(qs_font, "PHASE", 10.82, 82.000, 0.002000, "#2c2c2c", "PHASE CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "DAMP", 29.82, 82.000, 0.002000, "#2c2c2c", "DAMP CV Trim Label"))

    # Zone 4: I/O Jacks
    # Jack Row 1 (Inputs): FREQ, FM, X, Y (Center Y = 104.00, label Y = 98.000)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 6.07, 98.000, 0.001900, "#2c2c2c", "FREQ Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "FM", 15.57, 98.000, 0.001900, "#2c2c2c", "FM Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "X", 25.07, 98.000, 0.001900, "#2c2c2c", "X Ratio Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "Y", 34.57, 98.000, 0.001900, "#2c2c2c", "Y Ratio Jack Label"))

    # Jack Row 2 (Sync & Outputs): SYNC IN, X OUT, Y OUT, SYNC OUT (Center Y = 118.00)
    svg_parts.append(render_qs_text(qs_font, "SYNC IN", 6.07, 112.500, 0.001700, "#2c2c2c", "SYNC IN Label"))
    svg_parts.append(render_qs_text(qs_font, "OUT", 20.32, 110.200, 0.001700, "#2c2c2c", "OUT Shared Header"))
    svg_parts.append(render_qs_text(qs_font, "X", 15.57, 112.500, 0.002200, "#3c3c3c", "X OUT Label"))
    svg_parts.append(render_qs_text(qs_font, "Y", 25.07, 112.500, 0.002200, "#3c3c3c", "Y OUT Label"))
    svg_parts.append(render_qs_text(qs_font, "SYNC OUT", 34.57, 112.500, 0.001700, "#2c2c2c", "SYNC OUT Label"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Lisa.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap (76 x 240 at 4x resolution = 304 x 960)
    verify_png = 'scratch/lisa_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=76 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (76 * 4) / panel_w

    # Knobs at Y = 23.00, 42.00, 60.50 mm (radius ~ 4.75 mm, centered at 10.82 and 29.82 mm)
    kr = 4.75 * r_px
    for ky in [23.00, 42.00, 60.50]:
        for kx in [10.82, 29.82]:
            draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Range Button and LED at center X = 20.32 mm
    led_r = 1.5 * r_px
    draw.ellipse([20.32 * r_px - led_r, 17.50 * r_px - led_r, 20.32 * r_px + led_r, 17.50 * r_px + led_r], outline='#cc79a7', width=2)
    btn_r = 2.0 * r_px
    draw.ellipse([20.32 * r_px - btn_r, 25.50 * r_px - btn_r, 20.32 * r_px + btn_r, 25.50 * r_px + btn_r], outline='#f0e442', width=2)

    # Trimpots (Row 1 at Y = 75.50 mm at 6.07, 15.57, 25.07, 34.57 mm)
    tr = 2.5 * r_px
    for tx in [6.07, 15.57, 25.07, 34.57]:
        draw.ellipse([tx * r_px - tr, 75.50 * r_px - tr, tx * r_px + tr, 75.50 * r_px + tr], outline='#00ff88', width=2)
    # Trimpots (Row 2 at Y = 86.50 mm at 10.82, 29.82 mm)
    for tx in [10.82, 29.82]:
        draw.ellipse([tx * r_px - tr, 86.50 * r_px - tr, tx * r_px + tr, 86.50 * r_px + tr], outline='#00ff88', width=2)

    # Jacks (Row 1 at Y = 104.00 mm, Row 2 at Y = 118.00 mm at 6.07, 15.57, 25.07, 34.57 mm)
    jr = 4.15 * r_px
    for jy in [104.00, 118.00]:
        for jx in [6.07, 15.57, 25.07, 34.57]:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
