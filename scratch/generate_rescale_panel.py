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

    # Title "rescale" in Node.otf (scale 0.0048, baseline 7.620, centered in 30.48 mm)
    scale = 0.0048
    text = "rescale"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    title_block = ['  <!-- Title: "rescale" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.25.0" centered at 15.24 mm, baseline 10.414
    version_block = [
        render_qs_text(qs_reg_font, "v2.25.0", 15.24, 10.414, 0.001600, "#aaaaaa", "v2.25.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP (30.48 mm) -->',
        '  <rect width="30.48" height="128.5" fill="#6e6e6e"/>',

        '  <!-- Left Edge Color Badge (Centered 88.9mm x 2.54mm, Flush X=0) -->',

        '  <g id="palette-badge">',

        '    <rect x="0.000" y="19.800" width="2.540" height="31.115" fill="#FFFFFF" stroke="none"/>',

        '    <rect x="0.000" y="50.915" width="2.540" height="40.005" fill="#005566" stroke="none"/>',

        '    <rect x="0.000" y="90.920" width="2.540" height="17.780" fill="#56B4E9" stroke="none"/>',

        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Below Title/Version at Y = 15.00mm) -->',
        '  <line x1="2.54" y1="15.00" x2="27.94" y2="15.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Between Inputs & Outputs at Y = 72.50mm) -->',
        '  <line x1="2.54" y1="72.50" x2="27.94" y2="72.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # Section Headers
    # IN Header (Centered at 15.24, Y = 19.50)
    svg_parts.append(render_qs_text(qs_font, "IN", 15.24, 19.50, 0.002800, "#1c1c1c", "IN"))
    # OUT Header (Centered at 15.24, Y = 77.00)
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 77.00, 0.002800, "#1c1c1c", "OUT"))

    # Jacks and Labels
    # Row 1 (Inputs 1V): Y = 32.00
    # Row 2 (Inputs 5V): Y = 47.00
    # Row 3 (Inputs 10V): Y = 62.00
    # Row 4 (Outputs 1V): Y = 88.00
    # Row 5 (Outputs 5V): Y = 103.00
    # Row 6 (Outputs 10V): Y = 118.00
    jack_rows = [
        # (row_index, y_center, left_label, right_label)
        (1, 32.00, "0-1V", "±1V"),
        (2, 47.00, "0-5V", "±5V"),
        (3, 62.00, "0-10V", "±10V"),
        (4, 88.00, "0-1V", "±1V"),
        (5, 103.00, "0-5V", "±5V"),
        (6, 118.00, "0-10V", "±10V"),
    ]

    for row_idx, y, left_lbl, right_lbl in jack_rows:
        label_y = y - 5.50
        # Left column (x = 7.62)
        svg_parts.append(render_qs_text(qs_font, left_lbl, 7.62, label_y, 0.002100, "#1c1c1c", f"Row {row_idx} Left {left_lbl}"))
        # Right column (x = 22.86)
        svg_parts.append(render_qs_text(qs_font, right_lbl, 22.86, label_y, 0.002100, "#1c1c1c", f"Row {row_idx} Right {right_lbl}"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Rescale.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (6 HP).")

    # Render verification bitmap with overlaid ports
    verify_png = 'scratch/rescale_verify.png'
    # 6 HP: width 57 px at 1x, so 57*4 = 228 px wide, 240*4 = 960 px high
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=57 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (57 * 4) / 30.48
    jr = 4.15 * r_px
    cx_left = 7.62 * r_px
    cx_right = 22.86 * r_px

    for _, y, _, _ in jack_rows:
        cy = y * r_px
        draw.ellipse([cx_left - jr, cy - jr, cx_left + jr, cy + jr], outline='#00e5ff', width=2)
        draw.ellipse([cx_right - jr, cy - jr, cx_right + jr, cy + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap with ports: {verify_png}")

if __name__ == '__main__':
    main()
