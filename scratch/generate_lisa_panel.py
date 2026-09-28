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

    # Title "lisa" in Node.otf (scale 0.0048, baseline 7.620, centered in 30.48 mm)
    scale = 0.0048
    text = "lisa"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

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
        render_qs_text(qs_reg_font, "v2.25.0", 15.24, 10.414, 0.001600, "#aaaaaa", "v2.25.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Generate class: charcoal top anchor, lavender and deep teal bands, flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="28.448" fill="#1A1A1A" stroke="none"/>',
        '    <rect x="0.000" y="48.248" width="1.350" height="19.558" fill="#B8A0E8" stroke="none"/>',
        '    <rect x="0.000" y="67.806" width="1.350" height="40.894" fill="#005566" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 58.00mm) -->',
        '  <line x1="2.54" y1="58.00" x2="27.94" y2="58.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 90.00mm) -->',
        '  <line x1="2.54" y1="90.00" x2="27.94" y2="90.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(lisa_title_block)
    svg_parts.extend(version_block)

    # Row 1 Knobs: X & Y FREQ labels (Knob center Y = 21.59, label Y = 13.070, scale = 0.003000)
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 13.070, 0.003000, "#1c1c1c", "X Knob Header"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 13.070, 0.003000, "#1c1c1c", "Y Knob Header"))
    svg_parts.append(render_qs_text(qs_font, "FREQ", 15.24, 15.000, 0.002200, "#2c2c2c", "Row 1 Function Header"))

    # Row 2 Knobs: PHASE & DAMP labels (Knob center Y = 42.50, label Y = 34.000, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "PHASE", 7.62, 34.000, 0.002400, "#1c1c1c", "PHASE Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "DAMP", 22.86, 34.000, 0.002400, "#1c1c1c", "DAMP Knob Label"))

    # Trimpot Row 1: Single centered "FREQ" (Trimpot center Y = 68.00, label Y = 63.500)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 15.24, 63.500, 0.002000, "#2c2c2c", "FREQ CV Attenuverter Label"))

    # Trimpot Row 2: PHASE & DAMP (Trimpot center Y = 80.00, label Y = 75.500)
    svg_parts.append(render_qs_text(qs_font, "PHASE", 7.62, 75.500, 0.002000, "#2c2c2c", "PHASE CV Attenuverter Label"))
    svg_parts.append(render_qs_text(qs_font, "DAMP", 22.86, 75.500, 0.002000, "#2c2c2c", "DAMP CV Attenuverter Label"))

    # Jack Row 1 (Freq CV): Single centered "FREQ" (Jack center Y = 99.00, label Y = 97.500)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 15.24, 97.500, 0.002000, "#2c2c2c", "FREQ CV Jack Row Label"))

    # Jack Row 2 (Phase & Damp CV): Centered "CV" (Jack center Y = 108.50, label Y = 107.000)
    svg_parts.append(render_qs_text(qs_font, "CV", 15.24, 107.000, 0.002000, "#2c2c2c", "CV Jack Row Label"))

    # Jack Row 3 (Signal Outputs): Centered "OUT" (Jack center Y = 118.00, label Y = 116.500)
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 116.500, 0.002400, "#2c2c2c", "OUT Jack Row Label"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Lisa.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap
    verify_png = 'scratch/lisa_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=57 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (57 * 4) / 30.48

    # Knobs at Y = 21.59 mm and Y = 42.50 mm (radius ~ 4.5 mm)
    kr = 4.5 * r_px
    for ky in [21.59, 42.50]:
        draw.ellipse([7.62 * r_px - kr, ky * r_px - kr, 7.62 * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)
        draw.ellipse([22.86 * r_px - kr, ky * r_px - kr, 22.86 * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Trimpots at Y = 68.00 mm and Y = 80.00 mm (radius ~ 2.5 mm)
    tr = 2.5 * r_px
    for ty in [68.00, 80.00]:
        draw.ellipse([7.62 * r_px - tr, ty * r_px - tr, 7.62 * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)
        draw.ellipse([22.86 * r_px - tr, ty * r_px - tr, 22.86 * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks (Row 1: 99.00, Row 2: 108.50, Row 3: 118.00)
    jr = 4.15 * r_px
    for jy in [99.00, 108.50, 118.00]:
        draw.ellipse([7.62 * r_px - jr, jy * r_px - jr, 7.62 * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)
        draw.ellipse([22.86 * r_px - jr, jy * r_px - jr, 22.86 * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
