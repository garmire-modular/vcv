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

    # 12 HP Dimensions: 60.96 mm x 128.50 mm
    panel_w = 60.96
    panel_h = 128.50
    center_x = panel_w / 2.0  # 30.48 mm

    # Title "matrix 3x3" in Node.otf (scale 0.0048, baseline 7.620, centered at panel_w / 2)
    cmap_node = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    scale_title = 0.0044
    title_text = "matrix 3x3"
    
    widths = []
    for c in title_text:
        if c == ' ':
            widths.append(500 * scale_title)
        else:
            gname = cmap_node[ord(c)] if cmap_node and ord(c) in cmap_node else c
            widths.append(hmtx[gname][0] * scale_title)
    total_w = sum(widths)
    start_x = (panel_w - total_w) / 2.0

    title_block = ['  <!-- Label: "matrix 3x3" -->']
    curr_x = start_x
    for c, w in zip(title_text, widths):
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale_title:.6f}, {-scale_title:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.26.0" in Quicksand-Regular (scale 0.001600, baseline 10.414)
    version_block = [render_qs_text(qs_reg_font, "v2.26.0", center_x, 10.414, 0.001600, "#aaaaaa", "v2.26.0")]

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
        '  <!-- Delineator Line (Between Scanner Controls and 3x3 Arrays at Y = 62.00mm) -->',
        f'  <line x1="3.00" y1="62.00" x2="{panel_w - 3.00:.2f}" y2="62.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # ---------------- Zone 2: Rows 1-3 Upper Scanner Section ----------------
    # Left column (Knobs): X = 11.50 mm
    # Center column (LED Matrix): X = 30.48 mm
    # Right column (CV Jacks): X = 49.46 mm
    col_knobs = 11.50
    col_cv = 49.46
    row_y = [22.00, 36.50, 51.00]

    # Knob Labels (Above knobs at -8.50mm from center)
    svg_parts.append(render_qs_text(qs_font, "SCAN X", col_knobs, 13.50, 0.002200, "#1c1c1c", "SCAN X KNOB"))
    svg_parts.append(render_qs_text(qs_font, "SCAN Y", col_knobs, 28.00, 0.002200, "#1c1c1c", "SCAN Y KNOB"))
    svg_parts.append(render_qs_text(qs_font, "BLEED", col_knobs, 42.50, 0.002200, "#1c1c1c", "BLEED KNOB"))

    # CV Jack Labels (Above CV jacks at -6.00mm from center)
    svg_parts.append(render_qs_text(qs_font, "CV", col_cv, 15.00, 0.002000, "#2c2c2c", "SCAN X CV"))
    svg_parts.append(render_qs_text(qs_font, "CV", col_cv, 29.50, 0.002000, "#2c2c2c", "SCAN Y CV"))
    svg_parts.append(render_qs_text(qs_font, "CV", col_cv, 44.00, 0.002000, "#2c2c2c", "BLEED CV"))

    # Middle LED Matrix Bezel & Apertures spanning Rows 1-3
    # 3x3 LED matrix grid centered at X = 30.48 mm, Y = 36.50 mm
    # Matrix coordinates: grid_dx = [-8.50, 0.00, +8.50], grid_dy = [-14.50, 0.00, +14.50]
    # Corresponding to Y = 22.00, 36.50, 51.00 mm
    led_x = [21.98, 30.48, 38.98]
    led_y = [22.00, 36.50, 51.00]

    # Bezel: width 24 mm, height 38 mm centered at (30.48, 36.50)
    bezel_x = 30.48 - 12.00
    bezel_y = 36.50 - 19.00
    svg_parts.append('  <!-- Center LED Matrix Display Bezel & Apertures -->')
    svg_parts.append(f'  <rect x="{bezel_x:.3f}" y="{bezel_y:.3f}" width="24.000" height="38.000" rx="3.000" fill="#222222" stroke="#444444" stroke-width="0.300"/>')
    for y in led_y:
        for x in led_x:
            svg_parts.append(f'  <circle cx="{x:.3f}" cy="{y:.3f}" r="1.800" fill="#151515" stroke="#333333" stroke-width="0.250"/>')

    # Grid Header label above bezel (Y = 14.00)
    svg_parts.append(render_qs_text(qs_font, "GRID", center_x, 14.50, 0.002200, "#2c2c2c", "GRID HEADER"))

    # ---------------- Zone 3 & 4: Rows 4-6 Lower Section (Attenuverters & Outputs) ----------------
    # Left: 3x3 Attenuverters
    # Right: 3x3 Output Jacks
    # Width = 60.96 mm.
    # Attenuverter center X: [9.50, 18.50, 27.50] -> Span 18.0 mm, center = 18.50 mm
    # Output jack center X: [37.50, 46.50, 55.50] -> Span 18.0 mm, center = 46.50 mm
    # Rows 4-6 Center Y: [76.00, 95.00, 114.00] -> Pitch 19.0 mm
    trim_x = [9.50, 18.50, 27.50]
    out_x = [37.50, 46.50, 55.50]
    cell_y = [76.00, 95.00, 114.00]

    # Section Headers
    svg_parts.append(render_qs_text(qs_font, "ATTENUATE", 18.50, 65.50, 0.002200, "#1c1c1c", "ATTENUATE HEADER"))
    svg_parts.append(render_qs_text(qs_font, "OUT", 46.50, 65.50, 0.002200, "#1c1c1c", "OUTPUT HEADER"))

    # Cell coordinates / labels: Row 1 (1 2 3), Row 2 (4 5 6), Row 3 (7 8 9)
    # Output labels centered above jacks
    cell_names = [
        ["1", "2", "3"],
        ["4", "5", "6"],
        ["7", "8", "9"]
    ]

    for r in range(3):
        # Attenuverter row label
        for c in range(3):
            # Trim label above trimpot (-5.5 mm)
            svg_parts.append(render_qs_text(qs_font, cell_names[r][c], trim_x[c], cell_y[r] - 5.50, 0.001600, "#2c2c2c", f"TRIM {cell_names[r][c]}"))
            # Output label above jack (-6.0 mm)
            svg_parts.append(render_qs_text(qs_font, cell_names[r][c], out_x[c], cell_y[r] - 6.00, 0.001800, "#1c1c1c", f"OUT {cell_names[r][c]}"))

    svg_parts.append('</svg>\n')
    svg_content = "\n".join(svg_parts)

    out_file = 'res/Matrix3x3.svg'
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print(f"Generated {out_file} successfully.")

    # Render MetaModule PNG (12 HP = 114 x 240 px)
    try:
        import resvg_py
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
