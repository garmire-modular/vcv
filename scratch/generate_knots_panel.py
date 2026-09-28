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
import os
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

try:
    import cairocffi
    cairo_dll = os.path.join(msys_bin, 'libcairo-2.dll')
    if os.path.exists(cairo_dll):
        cairocffi.cairo = cairocffi.ffi.dlopen(cairo_dll)
    import cairosvg
except Exception:
    cairosvg = None

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

    # Title "knots" in Node.otf (scale 0.0048, baseline 7.620, centered in 30.48 mm)
    scale = 0.0048
    text = "knots"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    knots_title_block = ['  <!-- Label: "knots" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            knots_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
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

        '  <!-- Left Edge Color Badge (Centered 88.9mm x 2.54mm, Flush X=0) -->',

        '  <g id="palette-badge">',

        '    <rect x="0.000" y="19.800" width="2.540" height="44.450" fill="#FFFFFF" stroke="none"/>',

        '    <rect x="0.000" y="64.250" width="2.540" height="28.448" fill="#D55E00" stroke="none"/>',

        '    <rect x="0.000" y="92.698" width="2.540" height="16.002" fill="#FF8866" stroke="none"/>',

        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 52.50mm) -->',
        '  <line x1="2.54" y1="52.50" x2="27.94" y2="52.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 80.50mm) -->',
        '  <line x1="2.54" y1="80.50" x2="27.94" y2="80.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(knots_title_block)
    svg_parts.extend(version_block)

    # Top Knobs X & Y labels (Y = 13.070, scale = 0.003000)
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 13.070, 0.003000, "#1c1c1c", "X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 13.070, 0.003000, "#1c1c1c", "Y"))

    # Trimpot Row Label: single centered "KNOTS" (Trimpot center Y = 73.00, label Y = 68.50)
    svg_parts.append(render_qs_text(qs_font, "KNOTS", 15.24, 68.500, 0.002000, "#2c2c2c", "KNOTS CV Attenuverter Label"))

    # Jack Row 1: X, Y (Y = 84.000, scale = 0.003000), IN (Y = 88.000, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 84.000, 0.003000, "#1c1c1c", "X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 84.000, 0.003000, "#1c1c1c", "Y"))
    svg_parts.append(render_qs_text(qs_font, "IN", 15.24, 88.000, 0.002400, "#2c2c2c", "IN"))

    # Jack Row 2: CV (Y = 97.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "CV", 15.24, 97.500, 0.002400, "#2c2c2c", "CV"))

    # Jack Row 3: OUT (Y = 116.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 116.500, 0.002400, "#2c2c2c", "OUT"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Knots.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap
    verify_png = 'scratch/knots_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=57 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (57 * 4) / 30.48

    # Knobs at Y = 21.59 mm (radius ~ 4.5 mm)
    kr = 4.5 * r_px
    draw.ellipse([7.62 * r_px - kr, 21.59 * r_px - kr, 7.62 * r_px + kr, 21.59 * r_px + kr], outline='#ffaa00', width=2)
    draw.ellipse([22.86 * r_px - kr, 21.59 * r_px - kr, 22.86 * r_px + kr, 21.59 * r_px + kr], outline='#ffaa00', width=2)

    # Trimpots at Y = 73.00 mm (radius ~ 2.5 mm)
    tr = 2.5 * r_px
    draw.ellipse([7.62 * r_px - tr, 73.00 * r_px - tr, 7.62 * r_px + tr, 73.00 * r_px + tr], outline='#00ff88', width=2)
    draw.ellipse([22.86 * r_px - tr, 73.00 * r_px - tr, 22.86 * r_px + tr, 73.00 * r_px + tr], outline='#00ff88', width=2)

    # Jacks (Row 1: 89.50, Row 2: 99.00, Row 3: 118.00)
    jr = 4.15 * r_px
    for jy in [89.50, 99.00, 118.00]:
        draw.ellipse([7.62 * r_px - jr, jy * r_px - jr, 7.62 * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)
        draw.ellipse([22.86 * r_px - jr, jy * r_px - jr, 22.86 * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
