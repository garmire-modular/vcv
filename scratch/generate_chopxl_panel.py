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

    # 12 HP Dimensions: 60.96 mm x 128.50 mm
    panel_w = 60.96
    panel_h = 128.50
    title_text = "chop xl"
    version_str = "v2.24.0"

    scale_title = 0.0042
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale_title for c in title_text)
    start_x = (panel_w - total_w) / 2.0

    title_block = [f'  <!-- Label: "{title_text}" -->']
    curr_x = start_x
    for c in title_text:
        w = hmtx[cmap[ord(c)]][0] * scale_title
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale_title:.6f}, {-scale_title:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    version_block = [
        render_qs_text(qs_reg_font, version_str, panel_w / 2.0, 10.414, 0.001400, "#aaaaaa", version_str)
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        f'  <!-- Panel Background: 12 HP ({panel_w:.2f} mm) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#7c7c7c"/>',
        '',
        '  <!-- Horizontal Line 1 (Between Knobs & Trim Labels at Y = 33.50mm) -->',
        f'  <line x1="2.54" y1="33.50" x2="{panel_w - 2.54:.2f}" y2="33.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Horizontal Line 2 (Between Trims & CV Inputs at Y = 53.50mm) -->',
        f'  <line x1="2.54" y1="53.50" x2="{panel_w - 2.54:.2f}" y2="53.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # 4 Control Columns (x = 7.62, 22.86, 38.10, 53.34 mm)
    ctrl_x = [7.62, 22.86, 38.10, 53.34]
    ctrl_names = ["COUNT", "LENGTH", "POSITION", "VARIETY"]

    # Row 1: Knobs (center Y = 23.50 mm, label baseline Y = 14.50 mm)
    for name, cx in zip(ctrl_names, ctrl_x):
        svg_parts.append(render_qs_text(qs_font, name, cx, 14.50, 0.002200, "#1c1c1c", f"Knob: {name}"))

    # Row 2: Attenuverter Trimpots (center Y = 45.50 mm, label baseline Y = 38.50 mm)
    trim_labels = ["COUNT", "LENGTH", "POS", "VAR"]
    for t_label, cx in zip(trim_labels, ctrl_x):
        svg_parts.append(render_qs_text(qs_font, t_label, cx, 38.50, 0.001800, "#2c2c2c", f"Trim: {t_label}"))

    # Row 3: CV Inputs (center Y = 63.00 mm, aligns with XORXY Row 4)

    # Row 4: Channel Headers for the 6 channels (baseline Y = 78.00 mm, aligns with XORXY Row 5)
    col_x = [6.73 + i * 9.50 for i in range(6)]
    chan_names = ["X", "Y", "R", "G", "B", "I"]
    for c_name, cx in zip(chan_names, col_x):
        svg_parts.append(render_qs_text(qs_font, c_name, cx, 78.00, 0.002400, "#1c1c1c", f"Channel: {c_name}"))

    # Rows 5, 6, 7: Signal Jack Rows (aligned with XORXY spacing):
    # Row 5: Input 1 (center Y = 90.50 mm, single centered label "IN 1" at baseline Y = 83.50 mm)
    # Row 6: Input 2 (center Y = 104.50 mm, single centered label "IN 2" at baseline Y = 97.50 mm)
    # Row 7: Output  (center Y = 118.00 mm, single centered label "OUT" at baseline Y = 111.50 mm)
    sig_rows = [
        ("IN 1",  90.50,  83.50),
        ("IN 2", 104.50,  97.50),
        ("OUT",  118.00, 111.50)
    ]

    all_jacks = []
    # Add CV jacks (Y = 63.00 mm)
    for cx in ctrl_x:
        all_jacks.append((cx, 63.00, 4.15))

    # Add signal jacks and single centered row labels
    for r_label, r_y, lbl_y in sig_rows:
        svg_parts.append(render_qs_text(qs_font, r_label, panel_w / 2.0, lbl_y, 0.002000, "#1c1c1c", f"Row: {r_label}"))
        for cx in col_x:
            all_jacks.append((cx, r_y, 4.15))

    # Trimpot centers (Y = 45.50 mm)
    all_trims = [(cx, 45.50, 3.25) for cx in ctrl_x]
    # Knob centers (Y = 23.50 mm)
    all_knobs = [(cx, 23.50, 5.00) for cx in ctrl_x]

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/ChopXL.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (12 HP).")

    # Render verification bitmap (12HP = 114x240 px in MetaModule, render at 4x)
    verify_png = 'scratch/chopxl_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=114 * 4, output_height=240 * 4)

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (114 * 4) / panel_w

    # Overlay knobs in orange
    for cx, cy, rad in all_knobs:
        px, py, r = cx * r_px, cy * r_px, rad * r_px
        draw.ellipse([px - r, py - r, px + r, py + r], outline='#ff9100', width=2)

    # Overlay trimpots in green
    for cx, cy, rad in all_trims:
        px, py, r = cx * r_px, cy * r_px, rad * r_px
        draw.ellipse([px - r, py - r, px + r, py + r], outline='#00e676', width=2)

    # Overlay jacks in cyan
    for cx, cy, rad in all_jacks:
        px, py, r = cx * r_px, cy * r_px, rad * r_px
        draw.ellipse([px - r, py - r, px + r, py + r], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap with ports: {verify_png}")

if __name__ == '__main__':
    main()
