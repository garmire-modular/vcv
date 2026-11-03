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
import resvg_py

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
    if ('v' in str(text) and '.' in str(text)) or 'version' in comment.lower():
        use_font = qs_reg_font
        use_scale = 0.001600
    elif font == ddin_bold:
        use_font = ddin_bold
        use_scale = scale
    elif baseline_y < 52.0:
        use_font = ddin_cond_bold
        use_scale = 10.0 * 25.4 / (72.0 * 1000.0)  # 10pt = 0.003528
    else:
        use_font = ddin_cond
        use_scale = 8.0 * 25.4 / (72.0 * 1000.0)   # 8pt = 0.002822
    
    cmap = use_font.getBestCmap()
    hmtx = use_font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * use_scale if ord(c) in cmap else 500 * use_scale for c in str(text)]
    total_w = sum(widths)
    start_x = center_x - total_w / 2.0
    res = []
    if comment:
        res.append(f'  <!-- Label: "{comment}" -->')
    curr = start_x
    for c, w in zip(str(text), widths):
        if c != ' ':
            pd = get_simplified_glyph_path(use_font, c)
            res.append(f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({use_scale:.6f}, {-use_scale:.6f})"><path d="{pd}" fill="{fill}"/></g>')
        curr += w
    return "\n".join(res)

def main():
    node_font = ddin_bold
    qs_font = ddin_cond
    panel_w = 60.96
    panel_h = 128.50
    panel_w = 60.96
    panel_h = 128.50
    center_x = panel_w / 2.0  # 30.48 mm

    # Title "MATRIX 3x3" in D-DIN-Bold (16pt, scale 0.005644, baseline 7.620)
    base_title_scale = 16.0 * 25.4 / (72.0 * 1000.0)
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in "MATRIX 3x3")
    title_scale = base_title_scale
    if raw_w > (panel_w - 3.0):
        title_scale = base_title_scale * ((panel_w - 3.0) / raw_w)

    title_block = [
        '  <!-- Label: "MATRIX 3x3" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "MATRIX 3x3", panel_w / 2.0, 7.620, title_scale, "#ffffff", "MATRIX 3x3")
    ]

    version_block = [
        '  <!-- Label: "v2.0.0" -->',
        render_qs_text(qs_reg_font, "v2.0.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.0.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        f'  <!-- Panel Background: 12 HP ({panel_w:.2f} mm) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge: Default Placeholder #5d5d5d (Centered 88.9mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line (Between Scanner Controls and 3x3 Arrays at Y = 78.00mm) -->',
        f'  <line x1="3.00" y1="78.00" x2="{panel_w - 3.00:.2f}" y2="78.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # ---------------- Zone 2: Upper Section ----------------
    # 3 Horizontal Columns across 12 HP (60.96 mm):
    col_x = [12.00, 30.48, 48.96]

    # Row 1: Three Knobs (Scan X, Scan Y, Bleed) at Center Y = 21.00 mm (Fixed anchor)
    knob_y = 21.00
    svg_parts.append(render_qs_text(qs_font, "SCAN X", col_x[0], knob_y - 8.20, 0.002200, "#1c1c1c", "SCAN X KNOB"))
    svg_parts.append(render_qs_text(qs_font, "SCAN Y", col_x[1], knob_y - 8.20, 0.002200, "#1c1c1c", "SCAN Y KNOB"))
    svg_parts.append(render_qs_text(qs_font, "BLEED", col_x[2], knob_y - 8.20, 0.002200, "#1c1c1c", "BLEED KNOB"))

    # Row 2: Square Shaped Matrix with 5mm LEDs (Center Y = 45.50 mm)
    # Square bezel: 28 mm x 28 mm centered at (30.48, 45.50) -> Y: [31.50, 59.50]
    # 3x3 LED centers: X = [22.48, 30.48, 38.48] (pitch 8.0 mm), Y = [37.50, 45.50, 53.50] (pitch 8.0 mm)
    # 5mm LEDs (aperture r = 2.65 mm)
    led_x = [22.48, 30.48, 38.48]
    led_y = [37.50, 45.50, 53.50]
    bezel_size = 28.00
    bezel_x = center_x - bezel_size / 2.0
    bezel_y = 45.50 - bezel_size / 2.0

    svg_parts.append('  <!-- Square 3x3 LED Matrix Bezel & 5mm Apertures -->')
    svg_parts.append(f'  <rect x="{bezel_x:.3f}" y="{bezel_y:.3f}" width="{bezel_size:.3f}" height="{bezel_size:.3f}" rx="2.500" fill="#222222" stroke="#444444" stroke-width="0.300"/>')
    for y in led_y:
        for x in led_x:
            svg_parts.append(f'  <circle cx="{x:.3f}" cy="{y:.3f}" r="2.650" fill="#151515" stroke="#333333" stroke-width="0.250"/>')

    # Row 3: Three CV Input Jacks at Center Y = 70.00 mm
    cv_y = 70.00
    svg_parts.append(render_qs_text(qs_font, "X CV", col_x[0], cv_y - 6.00, 0.001900, "#2c2c2c", "X CV"))
    svg_parts.append(render_qs_text(qs_font, "Y CV", col_x[1], cv_y - 6.00, 0.001900, "#2c2c2c", "Y CV"))
    svg_parts.append(render_qs_text(qs_font, "BLEED CV", col_x[2], cv_y - 6.00, 0.001900, "#2c2c2c", "BLEED CV"))

    # ---------------- Zone 3 & 4: Rows 4-6 Lower Section (Attenuverters & Outputs) ----------------
    # Anchored from fixed bottom output jack row at Y = 118.00 mm!
    # Pitch between rows: ΔY = 13.5 mm:
    # Row 6 (Bottom): Y = 118.00 mm (Standard Garmire bottom anchor)
    # Row 5 (Middle): Y = 104.50 mm
    # Row 4 (Top):    Y = 91.00 mm
    trim_x = [9.50, 18.50, 27.50]
    out_x = [37.50, 46.50, 55.50]
    cell_y = [91.00, 104.50, 118.00]

    # Section Headers (Y = 82.50 mm)
    svg_parts.append(render_qs_text(qs_font, "ATTENUATE", 18.50, 82.50, 0.002000, "#1c1c1c", "ATTENUATE HEADER"))
    svg_parts.append(render_qs_text(qs_font, "OUT", 46.50, 82.50, 0.002000, "#1c1c1c", "OUTPUT HEADER"))

    cell_names = [
        ["1", "2", "3"],
        ["4", "5", "6"],
        ["7", "8", "9"]
    ]

    for r in range(3):
        for c in range(3):
            # Trim label above trimpot (-5.5 mm)
            svg_parts.append(render_qs_text(qs_font, cell_names[r][c], trim_x[c], cell_y[r] - 5.50, 0.001600, "#2c2c2c", f"TRIM {cell_names[r][c]}"))
            # Output label above jack (-5.8 mm)
            svg_parts.append(render_qs_text(qs_font, cell_names[r][c], out_x[c], cell_y[r] - 5.80, 0.001800, "#1c1c1c", f"OUT {cell_names[r][c]}"))

    svg_parts.append('</svg>\n')
    svg_content = "\n".join(svg_parts)

    out_file = 'res/Matrix3x3.svg'
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(svg_content)
    import sys
    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('Matrix3x3', svg_content)
    print(f"Generated {out_file} successfully.")

    # Render MetaModule PNG (12 HP = 114 x 240 px)
    try:
        png_path = 'metamodule/assets/Matrix3x3.png'
        svg_mod = re.sub(r'width="[0-9.]+mm"', 'width="114"', svg_content)
        svg_mod = re.sub(r'height="[0-9.]+mm"', 'height="240"', svg_mod)
        png_bytes = resvg_py.svg_to_bytes(svg_string=svg_mod, width=114, height=240)
        with open(png_path, 'wb') as pf:
            pf.write(png_bytes)
        print(f"Rendered {png_path} successfully (114x240).")
    except Exception as e:
        print(f"Note: MetaModule asset rendering skipped or failed: {e}")

if __name__ == '__main__':
    main()
