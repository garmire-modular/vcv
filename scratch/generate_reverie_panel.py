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
from PIL import Image

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

def get_palette_badge_svg(module_name="Reverie"):
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

def build_svg_panel(theme_name, bg_color, title_color, label_color, line_color, hi_color, sh_color, well_bg, well_border, fonts):
    d_din_bold = fonts['d_din_bold']
    d_din_cond_bold = fonts['d_din_cond_bold']
    d_din_cond = fonts['d_din_cond']
    qs_reg_font = fonts['qs_reg_font']

    # Panel dimensions: 28 HP (142.24 mm x 128.50 mm)
    panel_w = 142.24
    panel_h = 128.50

    # Scales
    scale_title = 0.005644       # 16 pt
    scale_version = 0.001600     # 4.5 pt
    scale_knob = 0.003528        # 10 pt
    scale_trim = 0.002822        # 8 pt
    scale_jack = 0.002822        # 8 pt
    scale_sub = 0.002400         # ~6.8 pt

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        f'  <!-- Panel Background: 28 HP ({theme_name} Theme) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="{bg_color}"/>',
        '',
        get_palette_badge_svg("Reverie"),
        '',
        '  <!-- ZONE 1: HEADER & TITLE -->',
        render_text(d_din_bold, "REVERIE", panel_w / 2.0, 8.000, scale_title, title_color, "Module Title"),
        render_text(qs_reg_font, "v1.0.0", panel_w - 2.15, 8.000, scale_version, "#aaaaaa" if theme_name != "Dark" else "#888888", "Version Tag", align="right"),
        '',
        '  <!-- ZONE 2: PRIMARY CONTROLS (Left Side) -->'
    ]

    # Row 1 Knobs: FREQ, FINE, RANGE
    # Y = 20.00 mm (Label baseline Y = 14.250 mm)
    y_knob_row1 = 20.000
    y_knob_lbl1 = y_knob_row1 - 5.750
    lines.append(render_text(d_din_cond_bold, "FREQ", 16.000, y_knob_lbl1, scale_knob, label_color, "Freq Knob Label"))
    lines.append(render_text(d_din_cond_bold, "FINE", 39.000, y_knob_lbl1, scale_knob, label_color, "Fine Knob Label"))
    lines.append(render_text(d_din_cond_bold, "RANGE", 62.000, y_knob_lbl1, scale_knob, label_color, "Range Button Label"))

    # Range LED dots indication: Yellow, Orange, Purple
    lines.append('  <!-- Range Mode Indicator Sockets -->')
    lines.append(f'  <circle cx="58.500" cy="27.500" r="1.300" fill="{well_bg}" stroke="{well_border}" stroke-width="0.3"/>')
    lines.append(f'  <circle cx="62.000" cy="27.500" r="1.300" fill="{well_bg}" stroke="{well_border}" stroke-width="0.3"/>')
    lines.append(f'  <circle cx="65.500" cy="27.500" r="1.300" fill="{well_bg}" stroke="{well_border}" stroke-width="0.3"/>')

    # Row 2 Knobs: MORPH, CYCLES
    # Y = 42.000 mm (Label baseline Y = 36.250 mm)
    y_knob_row2 = 42.000
    y_knob_lbl2 = y_knob_row2 - 5.750
    lines.append(render_text(d_din_cond_bold, "MORPH", 22.000, y_knob_lbl2, scale_knob, label_color, "Morph Knob Label"))
    lines.append(render_text(d_din_cond_bold, "CYCLES", 52.000, y_knob_lbl2, scale_knob, label_color, "Cycles Knob Label"))

    # ZONE 3: CV ATTENUVERTERS (Trimpots)
    # Trimpot Row Center Y = 66.000 mm (Label baseline Y = 62.000 mm, offset -4.0 mm)
    y_trim = 66.000
    y_trim_lbl = y_trim - 4.000
    lines.append('')
    lines.append('  <!-- ZONE 3: CV ATTENUVERTERS -->')
    lines.append(render_text(d_din_cond, "FREQ", 12.000, y_trim_lbl, scale_trim, label_color, "Freq Atten Label"))
    lines.append(render_text(d_din_cond, "MORPH", 28.373, y_trim_lbl, scale_trim, label_color, "Morph Atten Label"))
    lines.append(render_text(d_din_cond, "SCAN X", 49.747, y_trim_lbl, scale_trim, label_color, "Scan X Atten Label"))
    lines.append(render_text(d_din_cond, "SCAN Y", 71.120, y_trim_lbl, scale_trim, label_color, "Scan Y Atten Label"))

    # ZONE 4: 4x4 PRESET GRID (Right Side)
    # Center X = 108.000 mm, Center Y = 46.000 mm
    # Spacing delta_x = 12.500 mm, delta_y = 12.500 mm
    grid_cx = 108.000
    grid_cy = 46.000
    pitch = 12.500
    btn_size = 10.000
    half_btn = btn_size / 2.0

    lines.append('')
    lines.append('  <!-- ZONE 4: PRESET MATRIX WELLS -->')
    lines.append(render_text(d_din_cond_bold, "PRESET MATRIX", grid_cx, 15.500, scale_knob, label_color, "Matrix Title"))

    # Outline frame enclosing the 4x4 matrix
    frame_w = 4.0 * pitch + 3.000
    frame_h = 4.0 * pitch + 3.000
    frame_x = grid_cx - frame_w / 2.0
    frame_y = grid_cy - frame_h / 2.0
    lines.append(f'  <rect x="{frame_x:.3f}" y="{frame_y:.3f}" width="{frame_w:.3f}" height="{frame_h:.3f}" rx="3.0" fill="none" stroke="{well_border}" stroke-width="0.4"/>')

    for r in range(4):
        by = grid_cy + (r - 1.5) * pitch
        for c in range(4):
            bx = grid_cx + (c - 1.5) * pitch
            rx = bx - half_btn
            ry = by - half_btn
            lines.append(f'  <rect x="{rx:.3f}" y="{ry:.3f}" width="{btn_size:.3f}" height="{btn_size:.3f}" rx="2.0" fill="{well_bg}" stroke="{well_border}" stroke-width="0.35"/>')

    # DELINEATOR LINE 2
    # Separating Zone 3 / Grid and Jack Section
    # Center Y = 81.000 mm (10 mm above Jack Row 3 at 91.000 mm)
    lines.append('')
    lines.append('  <!-- DELINEATOR LINE 2 (Section 7.3: 10 mm above CV jack row) -->')
    x_del_start = 0.20 * panel_w
    del_w = panel_w - x_del_start
    lines.append(f'  <rect x="{x_del_start:.3f}" y="80.850" width="{del_w:.3f}" height="0.150" fill="{hi_color}" stroke="none"/>')
    lines.append(f'  <rect x="{x_del_start:.3f}" y="81.000" width="{del_w:.3f}" height="0.150" fill="{sh_color}" stroke="none"/>')

    # ZONE 5: I/O PORTS & JACKS
    # Row 3 (CV Inputs): Center Y = 91.000 mm (Label baseline Y = 86.000 mm)
    lines.append('')
    lines.append('  <!-- Row 3: CV Inputs (Center Y = 91.000 mm) -->')
    lines.append(render_text(d_din_cond, "CV INPUTS", 41.560, 86.000, scale_jack, label_color, "CV Section Centered Label"))

    # Row 2 (Signal Inputs): Center Y = 104.000 mm (Label baseline Y = 99.000 mm)
    # 7 Columns: X, Y, R, G, B, Z/I, SYNC
    cols = [7.000, 28.373, 49.747, 71.120, 92.493, 113.867, 135.240]
    headers = ["X", "Y", "R", "G", "B", "Z/I", "SYNC"]

    lines.append('')
    lines.append('  <!-- Row 2: Signal Inputs (Center Y = 104.000 mm) -->')
    for cx, h in zip(cols, headers):
        lines.append(render_text(d_din_cond, h, cx, 99.000, scale_jack, label_color, f"{h} Input Header"))

    # Small row indicator on far left
    lines.append(render_text(d_din_cond, "IN", 1.800, 105.000, scale_sub, label_color, "In Row Indicator", align="left"))

    # Row 1 (Signal Outputs): Center Y = 117.000 mm (Label baseline Y = 112.000 mm)
    lines.append('')
    lines.append('  <!-- Row 1: Signal Outputs (Center Y = 117.000 mm) -->')
    lines.append(render_text(d_din_cond, "OUT", 1.800, 118.000, scale_sub, label_color, "Out Row Indicator", align="left"))
    # Centered row label across the 7 output jacks (satisfies Section 7.5.3 single centered label directive)
    lines.append(render_text(d_din_cond, "SIGNAL OUTPUTS", panel_w / 2.0, 112.000, scale_jack, label_color, "Outputs Single Centered Label"))

    lines.append('</svg>')
    return '\n'.join(lines)

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
            'filename': 'res/Reverie.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e',
            'well_bg': '#5a5a5a',
            'well_border': '#484848'
        },
        {
            'name': 'Light',
            'filename': 'res/Reverie_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8',
            'well_bg': '#b4b4b4',
            'well_border': '#9e9e9e'
        },
        {
            'name': 'Dark',
            'filename': 'res/Reverie_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'label_color': '#ffffff',
            'line_color': '#ffffff',
            'hi_color': '#585858',
            'sh_color': '#202020',
            'well_bg': '#282828',
            'well_border': '#1f1f1f'
        }
    ]

    for skin in skins:
        svg_content = build_svg_panel(
            skin['name'],
            skin['bg_color'],
            skin['title_color'],
            skin['label_color'],
            skin['line_color'],
            skin['hi_color'],
            skin['sh_color'],
            skin['well_bg'],
            skin['well_border'],
            fonts
        )
        with open(skin['filename'], 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Generated {skin['filename']} ({skin['name']} skin)")

    # Render verification bitmaps
    os.makedirs('scratch', exist_ok=True)
    verify_png = 'scratch/reverie_verify.png'
    # 28 HP: 142.24 mm x 128.50 mm -> at 240px height = 265.6 px width
    out_w = int(240 * (142.24 / 128.50))
    cairosvg.svg2png(url='res/Reverie.svg', write_to=verify_png, output_width=out_w * 4, output_height=240 * 4)
    print(f"Rendered {verify_png}")

    os.makedirs('metamodule/assets', exist_ok=True)
    meta_png = 'metamodule/assets/Reverie.png'
    cairosvg.svg2png(url='res/Reverie.svg', write_to=meta_png, output_width=out_w, output_height=240)
    print(f"Rendered {meta_png}")

if __name__ == '__main__':
    main()
