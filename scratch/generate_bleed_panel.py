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

    # 8 HP Dimensions: 40.64 mm x 128.50 mm
    panel_w = 40.64
    panel_h = 128.50

    # Title "bleed" in Node.otf (scale 0.0048, baseline 7.620, centered at x = 20.32 mm)
    scale = 0.0048
    text = "bleed"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (panel_w - total_w) / 2.0

    bleed_title_block = ['  <!-- Label: "bleed" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            bleed_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.25.0"
    version_block = [
        '  <!-- Label: "v2.25.0" -->',
        render_qs_text(qs_reg_font, "v2.25.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.25.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        f'  <!-- Panel Background: 8 HP ({panel_w:.2f} mm) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Centered 88.9mm x 1.35mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="26.670" fill="#FFFFFF" stroke="none"/>',
        '    <rect x="0.000" y="46.470" width="1.350" height="35.560" fill="#882255" stroke="none"/>',
        '    <rect x="0.000" y="82.030" width="1.350" height="26.670" fill="#D55E00" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 54.50mm) -->',
        f'  <line x1="2.54" y1="54.50" x2="{panel_w - 2.54:.2f}" y2="54.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 81.50mm) -->',
        f'  <line x1="2.54" y1="81.50" x2="{panel_w - 2.54:.2f}" y2="81.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(bleed_title_block)
    svg_parts.extend(version_block)

    # Knob Columns: X at 10.82 mm, Y at 29.82 mm
    kx_left = 10.82
    kx_right = 29.82
    cx_mid = 20.32

    # Row 1 Knobs: X & Y BLEED labels (Knob center Y = 20.00, label Y = 12.800, scale = 0.003000)
    svg_parts.append(render_qs_text(qs_font, "X", kx_left, 12.800, 0.003000, "#1c1c1c", "X Knob Header"))
    svg_parts.append(render_qs_text(qs_font, "Y", kx_right, 12.800, 0.003000, "#1c1c1c", "Y Knob Header"))
    svg_parts.append(render_qs_text(qs_font, "BLEED", cx_mid, 14.000, 0.002200, "#2c2c2c", "Row 1 Function Header"))

    # Row 2 Knobs: FEEDBACK (Knob center Y = 34.50, label Y = 27.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "FEEDBACK", cx_mid, 27.500, 0.002400, "#1c1c1c", "Row 2 Feedback Header"))

    # Row 3 Knobs: PHASE (Knob center Y = 49.00, label Y = 42.000, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "PHASE", cx_mid, 42.000, 0.002400, "#1c1c1c", "Row 3 Phase Header"))

    # Zone 3: Trimpots (3 rows at Y = 61.50, 69.50, 77.50)
    # Trimpot Row 1: BLEED (Y = 61.50, label Y = 57.50)
    svg_parts.append(render_qs_text(qs_font, "BLEED", cx_mid, 57.500, 0.002000, "#2c2c2c", "Bleed CV Depth Label"))
    # Trimpot Row 2: FEEDBACK (Y = 69.50, label Y = 65.50)
    svg_parts.append(render_qs_text(qs_font, "FDBK", cx_mid, 65.500, 0.002000, "#2c2c2c", "Feedback CV Depth Label"))
    # Trimpot Row 3: PHASE (Y = 77.50, label Y = 73.50)
    svg_parts.append(render_qs_text(qs_font, "PHASE", cx_mid, 73.500, 0.002000, "#2c2c2c", "Phase CV Depth Label"))

    # Zone 4: I/O Jacks
    # 4 standard columns: col_x = [6.07, 15.57, 25.07, 34.57]
    col_x = [6.07, 15.57, 25.07, 34.57]

    # Jack Row 1 (Signal Inputs at Y = 89.50 mm)
    # Column headers above jacks at Y = 84.50 mm
    svg_parts.append(render_qs_text(qs_font, "X1", col_x[0], 84.500, 0.002200, "#1c1c1c", "X1 Input Header"))
    svg_parts.append(render_qs_text(qs_font, "Y1", col_x[1], 84.500, 0.002200, "#1c1c1c", "Y1 Input Header"))
    svg_parts.append(render_qs_text(qs_font, "X2", col_x[2], 84.500, 0.002200, "#1c1c1c", "X2 Input Header"))
    svg_parts.append(render_qs_text(qs_font, "Y2", col_x[3], 84.500, 0.002200, "#1c1c1c", "Y2 Input Header"))

    # Jack Row 3 (Phase CV at Y = 108.50 mm, at kx_left and kx_right)
    svg_parts.append(render_qs_text(qs_font, "PHASE", cx_mid, 108.500, 0.002000, "#2c2c2c", "Phase CV Row Label"))

    # Jack Row 4 (Signal Outputs at Y = 118.00 mm)
    # OUT label centered below jacks at Y = 125.00 mm
    svg_parts.append(render_qs_text(qs_font, "OUT", cx_mid, 125.000, 0.002200, "#2c2c2c", "OUT Row Label"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Bleed.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (8 HP).")

    # Render verification bitmap
    verify_png = 'scratch/bleed_verify.png'
    # 8 HP: 40.64 mm -> width 76 px at 1x, 76*4 = 304 px wide, 240*4 = 960 px high
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=76 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (76 * 4) / panel_w

    # Knobs at Y = [20.00, 34.50, 49.00]
    kr = 4.2 * r_px
    for ky in [20.00, 34.50, 49.00]:
        draw.ellipse([kx_left * r_px - kr, ky * r_px - kr, kx_left * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)
        draw.ellipse([kx_right * r_px - kr, ky * r_px - kr, kx_right * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Trimpots at Y = [61.50, 69.50, 77.50]
    tr = 2.4 * r_px
    for ty in [61.50, 69.50, 77.50]:
        draw.ellipse([kx_left * r_px - tr, ty * r_px - tr, kx_left * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)
        draw.ellipse([kx_right * r_px - tr, ty * r_px - tr, kx_right * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks
    jr = 4.15 * r_px
    # Row 1 (Inputs)
    for cx in col_x:
        draw.ellipse([cx * r_px - jr, 89.50 * r_px - jr, cx * r_px + jr, 89.50 * r_px + jr], outline='#00e5ff', width=2)
    # Row 2 (Bleed & Fdbk CV)
    for cx in col_x:
        draw.ellipse([cx * r_px - jr, 99.00 * r_px - jr, cx * r_px + jr, 99.00 * r_px + jr], outline='#00e5ff', width=2)
    # Row 3 (Phase CV)
    for cx in [kx_left, kx_right]:
        draw.ellipse([cx * r_px - jr, 108.50 * r_px - jr, cx * r_px + jr, 108.50 * r_px + jr], outline='#00e5ff', width=2)
    # Row 4 (Outputs)
    for cx in col_x:
        draw.ellipse([cx * r_px - jr, 118.00 * r_px - jr, cx * r_px + jr, 118.00 * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
