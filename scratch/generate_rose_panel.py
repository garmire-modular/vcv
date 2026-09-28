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

    # Title "rose" in Node.otf (scale 0.0048, baseline 7.620, centered in 30.48 mm)
    scale = 0.0048
    text = "rose"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    rose_title_block = ['  <!-- Label: "rose" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            rose_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
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
        '  <!-- Left Edge Color Badge (Placeholder #5d5d5d per Rule 6.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 56.50mm) -->',
        '  <line x1="2.54" y1="56.50" x2="27.94" y2="56.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 90.00mm) -->',
        '  <line x1="2.54" y1="90.00" x2="27.94" y2="90.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(rose_title_block)
    svg_parts.extend(version_block)

    # Row 1 Knob: PETALS (Center Y = 21.59, label Y = 13.070, scale = 0.002800)
    svg_parts.append(render_qs_text(qs_font, "PETALS", 15.24, 13.070, 0.002800, "#1c1c1c", "PETALS Knob Label"))

    # Row 2 Knob: OFFSET (Center Y = 43.00, label Y = 34.500, scale = 0.002800)
    svg_parts.append(render_qs_text(qs_font, "OFFSET", 15.24, 34.500, 0.002800, "#1c1c1c", "OFFSET Knob Label"))

    # Trimpot Row: PETALS (X = 7.62) & OFFSET (X = 22.86) (Center Y = 73.00, label Y = 67.500, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "PETALS", 7.62, 67.500, 0.002000, "#2c2c2c", "PETALS CV Attenuverter Label"))
    svg_parts.append(render_qs_text(qs_font, "OFFSET", 22.86, 67.500, 0.002000, "#2c2c2c", "OFFSET CV Attenuverter Label"))

    # Jack Row 1 (CV Inputs): PETALS (X = 7.62) & OFFSET (X = 22.86) (Center Y = 99.00, label Y = 94.000, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "PETALS", 7.62, 94.000, 0.002000, "#2c2c2c", "PETALS CV Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "OFFSET", 22.86, 94.000, 0.002000, "#2c2c2c", "OFFSET CV Jack Label"))

    # Jack Row 2 (Sync In & Sync Out): "SYNC" with "IN / OUT" (Center Y = 108.50)
    svg_parts.append(render_qs_text(qs_font, "SYNC", 15.24, 105.800, 0.001900, "#2c2c2c", "SYNC Header"))
    svg_parts.append(render_qs_text(qs_font, "IN   OUT", 15.24, 108.600, 0.001500, "#3c3c3c", "SYNC Direction Label"))

    # Jack Row 3 (Signal Outputs): Centered "OUT" (Center Y = 118.00, label Y = 116.500)
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 116.500, 0.002400, "#2c2c2c", "OUT Jack Row Label"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Rose.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap
    verify_png = 'scratch/rose_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=57 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (57 * 4) / 30.48

    # Knobs at Y = 21.59 mm and Y = 43.00 mm (radius ~ 4.5 mm, centered at X = 15.24 mm)
    kr = 4.5 * r_px
    for ky in [21.59, 43.00]:
        draw.ellipse([15.24 * r_px - kr, ky * r_px - kr, 15.24 * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Trimpots at Y = 73.00 mm (radius ~ 2.5 mm, at X = 7.62 mm and X = 22.86 mm)
    tr = 2.5 * r_px
    draw.ellipse([7.62 * r_px - tr, 73.00 * r_px - tr, 7.62 * r_px + tr, 73.00 * r_px + tr], outline='#00ff88', width=2)
    draw.ellipse([22.86 * r_px - tr, 73.00 * r_px - tr, 22.86 * r_px + tr, 73.00 * r_px + tr], outline='#00ff88', width=2)

    # Jacks (Row 1: 99.00, Row 2: 108.50, Row 3: 118.00)
    jr = 4.15 * r_px
    for jy in [99.00, 108.50, 118.00]:
        draw.ellipse([7.62 * r_px - jr, jy * r_px - jr, 7.62 * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)
        draw.ellipse([22.86 * r_px - jr, jy * r_px - jr, 22.86 * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
