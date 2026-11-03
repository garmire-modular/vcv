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

ddin_bold = TTFont('res/D-DIN-Bold.otf')
ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
ddin_cond = TTFont('res/D-DINCondensed.otf')
qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

def render_qs_text(font, text, center_x, baseline_y, scale, fill, comment=""):
    text_str = str(text)
    if ('v' in text_str and '.' in text_str) or 'version' in comment.lower():
        use_font = qs_reg_font
        use_scale = 0.001600
    elif font == ddin_bold:
        use_font = ddin_bold
        use_scale = scale
    elif ('knob' in comment.lower() or 'header' in comment.lower() or text_str in ['COPIES', 'SCALE', 'SHIFT', 'ROTATE', 'FEEDBACK']) and baseline_y < 50.0:
        use_font = ddin_cond_bold
        use_scale = 10.0 * 25.4 / (72.0 * 1000.0)  # 10pt = 0.003528
    else:
        use_font = ddin_cond
        use_scale = 8.0 * 25.4 / (72.0 * 1000.0)   # 8pt = 0.002822
    
    cmap = use_font.getBestCmap()
    hmtx = use_font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * use_scale if ord(c) in cmap else 500 * use_scale for c in text_str]
    total_w = sum(widths)
    start_x = center_x - total_w / 2.0
    res = []
    if comment:
        res.append(f'  <!-- Label: "{comment}" -->')
    curr = start_x
    for c, w in zip(text_str, widths):
        if c != ' ':
            pd = get_simplified_glyph_path(use_font, c)
            res.append(f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({use_scale:.6f}, {-use_scale:.6f})"><path d="{pd}" fill="{fill}"/></g>')
        curr += w
    return "\n".join(res)

def main():
    node_font = ddin_bold
    qs_font = ddin_cond
    panel_w = 40.64
    panel_h = 128.50
    col_headers = ["IN", "SW", "OUT 1", "OUT 2"]
    version_str = "v2.0.0"

    # Title "ROUTE XL" in D-DIN-Bold (16pt, scale 0.005644, baseline 7.620)
    base_title_scale = 16.0 * 25.4 / (72.0 * 1000.0)
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in "ROUTE XL")
    title_scale = base_title_scale
    if raw_w > (panel_w - 3.0):
        title_scale = base_title_scale * ((panel_w - 3.0) / raw_w)

    title_block = [
        '  <!-- Label: "ROUTE XL" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "ROUTE XL", panel_w / 2.0, 7.620, title_scale, "#ffffff", "ROUTE XL")
    ]

    version_block = [
        '  <!-- Label: "v2.0.0" -->',
        render_qs_text(qs_reg_font, "v2.0.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.0.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        f'  <!-- Panel Background: 8 HP ({panel_w:.2f} mm) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Centered 88.9mm x 1.35mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="14.224" fill="#FFFFFF" stroke="none"/>',
        '    <rect x="0.000" y="34.024" width="1.350" height="24.892" fill="#0072B2" stroke="none"/>',
        '    <rect x="0.000" y="58.916" width="1.350" height="17.780" fill="#B8A0E8" stroke="none"/>',
        '    <rect x="0.000" y="76.696" width="1.350" height="32.004" fill="#005566" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Between Trajectory & Color at Y = 54.00mm) -->',
        f'  <line x1="2.50" y1="54.00" x2="{panel_w - 2.50:.2f}" y2="54.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Between Color & Intensity at Y = 108.00mm) -->',
        f'  <line x1="2.50" y1="108.00" x2="{panel_w - 2.50:.2f}" y2="108.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    col_x = [6.07, 15.57, 25.07, 34.57]

    # Column Headers at Y = 15.50
    for header, cx in zip(col_headers, col_x):
        svg_parts.append(render_qs_text(qs_font, header, cx, 15.50, 0.002200, "#1c1c1c", f"Header: {header}"))

    # 6 Rows: X, Y, R, G, B, I
    row_data = [
        ("X", 27.00),
        ("Y", 45.00),
        ("R", 63.00),
        ("G", 81.00),
        ("B", 99.00),
        ("I", 117.00),
    ]

    all_jacks = []
    for r_label, ry in row_data:
        # Single horizontally centered label across the row (no duplicated labels)
        svg_parts.append(render_qs_text(qs_font, r_label, panel_w / 2.0, ry - 5.60, 0.002600, "#1c1c1c", f"Row: {r_label}"))
        for cx in col_x:
            all_jacks.append((cx, ry))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/RouteXL.svg'
    import sys
    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('RouteXL', "\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (8 HP).")

    verify_png = 'scratch/routexl_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=76 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (76 * 4) / panel_w
    jr = 4.15 * r_px
    for cx, cy in all_jacks:
        px = cx * r_px
        py = cy * r_px
        draw.ellipse([px - jr, py - jr, px + jr, py + jr], outline='#00e5ff', width=2)
    im.save(verify_png)
    print(f"Rendered verification bitmap with ports: {verify_png}")

if __name__ == '__main__':
    main()
