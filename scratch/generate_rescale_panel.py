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
    panel_w = 30.48
    panel_h = 128.50

    # Title "RESCALE" in D-DIN-Bold (16pt, scale 0.005644, baseline 7.620)
    base_title_scale = 16.0 * 25.4 / (72.0 * 1000.0)
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in "RESCALE")
    title_scale = base_title_scale
    if raw_w > (panel_w - 3.0):
        title_scale = base_title_scale * ((panel_w - 3.0) / raw_w)

    title_block = [
        '  <!-- Label: "RESCALE" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "RESCALE", panel_w / 2.0, 7.620, title_scale, "#ffffff", "RESCALE")
    ]

    version_block = [
        '  <!-- Label: "v2.0.0" -->',
        render_qs_text(qs_reg_font, "v2.0.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.0.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP (30.48 mm) -->',
        '  <rect width="30.48" height="128.5" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Centered 88.9mm x 1.35mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="31.115" fill="#FFFFFF" stroke="none"/>',
        '    <rect x="0.000" y="50.915" width="1.350" height="40.005" fill="#005566" stroke="none"/>',
        '    <rect x="0.000" y="90.920" width="1.350" height="17.780" fill="#56B4E9" stroke="none"/>',
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
    import sys
    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('Rescale', "\n".join(svg_parts) + "\n")
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
