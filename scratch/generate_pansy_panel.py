import json
import os
from pathlib import Path
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

def render_text(font, text, x_pos, baseline_y, scale, fill, comment="", align="center"):
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text]
    total_w = sum(widths)
    if align == "right":
        start_x = x_pos - total_w
    elif align == "left":
        start_x = x_pos
    else:
        start_x = x_pos - total_w / 2.0
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

def get_palette_badge_svg(module_name="Pansy"):
    palette_path = Path("scratch/palette/palette.json")
    if palette_path.exists():
        try:
            with open(palette_path, "r", encoding="utf-8") as f:
                pal_data = json.load(f)
            if module_name in pal_data:
                entry = pal_data[module_name]
                geom = entry.get("badgeGeometry", {})
                bw = geom.get("widthMm", 2.54)
                start_y = geom.get("y", 19.8)
                start_x = geom.get("x", 0.0)
                bands = entry.get("bands", [])
                if bands:
                    lines = ['  <!-- Left Edge Color Badge (from scratch/palette/palette.json) -->',
                             '  <g id="palette-badge">']
                    curr_y = start_y
                    for b in bands:
                        bh = b.get("heightMm", 0.0)
                        hex_col = b.get("hex", "#5d5d5d")
                        lines.append(f'    <rect x="{start_x:.3f}" y="{curr_y:.3f}" width="{bw:.3f}" height="{bh:.3f}" fill="{hex_col}" stroke="none"/>')
                        curr_y += bh
                    lines.append('  </g>')
                    return "\n".join(lines)
        except Exception as e:
            print(f"Warning: could not load palette.json: {e}")
    return '''  <!-- Left Edge Color Badge (Placeholder #5d5d5d) -->
  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>
  </g>'''

def render_section_divider(center_y, panel_w, hi_color, sh_color):
    # Stretched to exactly 80% of module width, aligned to the right edge:
    divider_w = panel_w * 0.80
    x_start = panel_w - divider_w

    # Total vertical height = 0.300 mm (0.150 mm highlight + 0.150 mm shadow)
    top_y = center_y - 0.150
    bot_y = center_y

    return f'''  <!-- Section Divider at Center Y = {center_y:.2f}mm (Width = {divider_w:.3f}mm, Right-Aligned, Total Height = 0.300mm) -->
  <g>
    <!-- Top Highlight Line ({hi_color}) -->
    <rect x="{x_start:.3f}" y="{top_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{hi_color}"/>
    <!-- Bottom Shadow Line ({sh_color}) -->
    <rect x="{x_start:.3f}" y="{bot_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{sh_color}"/>
  </g>'''

def build_svg_panel(theme_name, bg_color, title_color, label_color, line_color, hi_color, sh_color, fonts):
    d_din_bold = fonts['d_din_bold']
    d_din_cond_bold = fonts['d_din_cond_bold']
    d_din_cond = fonts['d_din_cond']
    qs_reg_font = fonts['qs_reg_font']

    # Panel dimensions: 12 HP (60.96 mm x 128.50 mm)
    panel_w = 60.96
    panel_h = 128.50

    # 3 Primary Columns across 60.96 mm: pitch = 18.48 mm, margin = 12.00 mm
    col_x = [12.00, 30.48, 48.96]

    badge_svg = get_palette_badge_svg("Pansy")

    # Section Dividers
    # Divider 1 between Knobs (Row 2 Y=41.50) and Attenuverters (Row 1 Y=63.00): Center Y = 53.00 mm
    # Divider 2 between Attenuverters (Row 2 Y=75.00) and Jacks (Row 1 Y=89.50): Center Y = 82.00 mm
    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 12 HP ({theme_name} Theme) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="{bg_color}"/>',
        badge_svg,
        '',
        render_section_divider(53.00, panel_w, hi_color, sh_color),
        '',
        render_section_divider(82.00, panel_w, hi_color, sh_color),
        ''
    ]

    # Module title "PANSY" in D-DIN Bold, 16pt (scale 0.005644), Y baseline 8mm, centered horizontally
    title_scale = 16.0 * 25.4 / 72.0 / 1000.0  # 0.0056444
    svg_parts.append(render_text(d_din_bold, "PANSY", panel_w / 2.0, 8.000, title_scale, title_color, "PANSY", align="center"))

    # Version number "v1.0.0" in Quicksand-Regular, scale 0.001600, Y baseline 8mm, right-aligned
    version_x = panel_w - 2.15
    svg_parts.append(render_text(qs_reg_font, "v1.0.0", version_x, 8.000, 0.001600, "#aaaaaa", "v1.0.0", align="right"))

    # ── Zone 1 & 2: Main Parameter Knobs (D-DIN Condensed Bold, 10pt, baseline - 5.75mm) ──
    knob_label_scale = 10.0 * 25.4 / 72.0 / 1000.0  # 0.0035278

    # Row 1 Knobs: Center Y = 21.50 mm -> Label baseline Y = 21.50 - 5.75 = 15.75 mm
    row1_labels = ["X-AXIS", "Y-AXIS", "ROTATION"]
    for i, lbl in enumerate(row1_labels):
        svg_parts.append(render_text(d_din_cond_bold, lbl, col_x[i], 15.750, knob_label_scale, label_color, f"Row 1: {lbl}"))

    # Row 2 Knobs: Center Y = 41.50 mm -> Label baseline Y = 41.50 - 5.75 = 35.75 mm
    row2_labels = ["FOCUS", "CENTER", "SMOOTH"]
    for i, lbl in enumerate(row2_labels):
        svg_parts.append(render_text(d_din_cond_bold, lbl, col_x[i], 35.750, knob_label_scale, label_color, f"Row 2: {lbl}"))

    # ── Zone 3: CV Attenuverters (D-DIN Condensed, 8pt, baseline - 4.00mm) ──
    trim_label_scale = 8.0 * 25.4 / 72.0 / 1000.0  # 0.0028222

    # Row 1 Attenuverters: Center Y = 63.00 mm -> Label baseline Y = 63.00 - 4.00 = 59.00 mm
    trim1_labels = ["X", "Y", "ROT"]
    for i, lbl in enumerate(trim1_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 59.000, trim_label_scale, label_color, f"Trim 1: {lbl}"))

    # Row 2 Attenuverters: Center Y = 74.00 mm -> Label baseline Y = 74.00 - 4.00 = 70.00 mm
    trim2_labels = ["FOCUS", "CENTER", "SMOOTH"]
    for i, lbl in enumerate(trim2_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 70.000, trim_label_scale, label_color, f"Trim 2: {lbl}"))

    # ── Zone 4: I/O Jacks (D-DIN Condensed, 8pt, baseline - 5.00mm) ──
    # Jack Row 1 (Inputs 1-3): Center Y = 89.50 mm -> Label baseline Y = 89.50 - 5.00 = 84.50 mm
    jack1_labels = ["X", "Y", "ROT"]
    for i, lbl in enumerate(jack1_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 84.500, trim_label_scale, label_color, f"Jack 1: {lbl}"))

    # Jack Row 2 (Inputs 4-6): Center Y = 99.00 mm -> Label baseline Y = 99.00 - 5.00 = 94.00 mm
    jack2_labels = ["FOCUS", "CENTER", "SMOOTH"]
    for i, lbl in enumerate(jack2_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 94.000, trim_label_scale, label_color, f"Jack 2: {lbl}"))

    # Jack Row 3 (Outputs 1-3): Center Y = 108.50 mm -> Label baseline Y = 108.50 - 5.00 = 103.50 mm
    jack3_labels = ["X OUT", "Y OUT", "ROT OUT"]
    for i, lbl in enumerate(jack3_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 103.500, trim_label_scale, label_color, f"Jack 3: {lbl}"))

    # Jack Row 4 (Outputs 4-6): Center Y = 118.00 mm -> Label baseline Y = 118.00 - 5.00 = 113.00 mm
    jack4_labels = ["FOC OUT", "CTR OUT", "SMH OUT"]
    for i, lbl in enumerate(jack4_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 113.000, trim_label_scale, label_color, f"Jack 4: {lbl}"))

    svg_parts.append('</svg>')
    return "\n".join(svg_parts) + "\n"

def main():
    fonts = {
        'd_din_bold': TTFont('res/D-DIN-Bold.otf'),
        'd_din_cond_bold': TTFont('res/D-DINCondensed-Bold.otf'),
        'd_din_cond': TTFont('res/D-DINCondensed.otf'),
        'qs_reg_font': TTFont('res/Quicksand-Regular.ttf')
    }

    skins = [
        {
            'name': 'Medium',
            'filename': 'res/Pansy.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e',
        },
        {
            'name': 'Light',
            'filename': 'res/Pansy_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8',
        },
        {
            'name': 'Dark',
            'filename': 'res/Pansy_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'label_color': '#ffffff',
            'line_color': '#ffffff',
            'hi_color': '#585858',
            'sh_color': '#202020',
        }
    ]

    os.makedirs('res', exist_ok=True)
    for skin in skins:
        svg_content = build_svg_panel(
            skin['name'],
            skin['bg_color'],
            skin['title_color'],
            skin['label_color'],
            skin['line_color'],
            skin['hi_color'],
            skin['sh_color'],
            fonts
        )
        with open(skin['filename'], 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Generated {skin['filename']} ({skin['name']} skin) successfully.")

    # 12 HP pixel width for MetaModule: 12 * 9.5 = 114 px, Height = 240 px
    # High-res verification PNG: 114 * 4 = 456 px, 240 * 4 = 960 px
    os.makedirs('scratch', exist_ok=True)
    verify_png = 'scratch/pansy_verify.png'
    cairosvg.svg2png(url='res/Pansy.svg', write_to=verify_png, output_width=114 * 4, output_height=240 * 4)

    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url='res/Pansy.svg', write_to='metamodule/assets/Pansy.png', output_width=114, output_height=240)
    print("Rendered metamodule/assets/Pansy.png successfully.")

if __name__ == '__main__':
    main()
