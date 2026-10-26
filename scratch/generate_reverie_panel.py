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

def render_delineator(x_start, x_end, center_y, hi_color, sh_color, comment=""):
    w = x_end - x_start
    top_y = center_y - 0.150
    bot_y = center_y
    return f'''  <!-- {comment} -->
  <g>
    <rect x="{x_start:.3f}" y="{top_y:.3f}" width="{w:.3f}" height="0.150" fill="{hi_color}" stroke="none"/>
    <rect x="{x_start:.3f}" y="{bot_y:.3f}" width="{w:.3f}" height="0.150" fill="{sh_color}" stroke="none"/>
  </g>'''

def build_svg_panel(theme_name, bg_color, title_color, label_color, line_color, hi_color, sh_color, ring_color, well_bg, well_border, fonts):
    d_din_bold = fonts['d_din_bold']
    d_din_cond_bold = fonts['d_din_cond_bold']
    d_din_cond = fonts['d_din_cond']
    qs_reg_font = fonts['qs_reg_font']

    # Panel dimensions: 20 HP (101.60 mm x 128.50 mm)
    # Left 8 HP Controls (0.00 - 40.64 mm) + Right 12 HP 4x4 Matrix & Gate Grid (40.64 - 101.60 mm)
    panel_w = 101.60
    panel_h = 128.50

    # Scales (AGENTS.md Section 7.5)
    scale_title = 0.005644       # 16 pt
    scale_version = 0.001600     # 4.5 pt
    scale_knob = 0.003528        # 10 pt
    scale_jack = 0.002822        # 8 pt

    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        f'  <!-- Panel Background: 20 HP ({theme_name} Theme) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="{bg_color}"/>',
        '',
        get_palette_badge_svg("Reverie"),
        '',
        '  <!-- ZONE 1: HEADER & TITLE -->',
        render_text(d_din_bold, "REVERIE", panel_w / 2.0, 8.000, scale_title, title_color, "Module Title"),
        render_text(qs_reg_font, "v1.1.0", panel_w - 2.15, 8.000, scale_version, "#aaaaaa" if theme_name != "Dark" else "#888888", "v1.1.0", align="right"),
        ''
    ]

    # ── LEFT 8 HP SECTION (0.00 to 40.64 mm) ──
    # Row 1: FREQ (10.82), RANGE (20.32), FINE (29.82) at Y = 21.00 mm (Label baseline Y = 15.25 mm)
    lines.append('  <!-- Left 8 HP: Primary Parameters -->')
    lines.append('  <!-- Range Button Indicator Ring Track (R = 4.12mm, Stroke = 1.40mm) -->')
    lines.append(f'  <circle cx="20.320" cy="21.000" r="4.120" fill="none" stroke="{ring_color}" stroke-width="1.400"/>')
    lines.append(render_text(d_din_cond_bold, "FREQ", 10.820, 15.250, scale_knob, label_color, "FREQ Knob Label"))
    lines.append(render_text(d_din_cond_bold, "RANGE", 20.320, 15.250, scale_knob, label_color, "RANGE Button Label"))
    lines.append(render_text(d_din_cond_bold, "FINE", 29.820, 15.250, scale_knob, label_color, "FINE Knob Label"))

    # Row 2: CYCLES (10.82), MORPH (29.82) at Y = 40.00 mm (Label baseline Y = 34.25 mm)
    lines.append(render_text(d_din_cond_bold, "CYCLES", 10.820, 34.250, scale_knob, label_color, "CYCLES Knob Label"))
    lines.append(render_text(d_din_cond_bold, "MORPH", 29.820, 34.250, scale_knob, label_color, "MORPH Knob Label"))

    # Delineator Line for Left 8 HP Section (10 mm above Row 1 Control Jacks at 65.00 mm -> Y = 55.00 mm)
    lines.append('')
    lines.append(render_delineator(40.64 * 0.20, 40.64, 55.000, hi_color, sh_color, "Left 8 HP Delineator at Y = 55.00mm"))

    # Left 8 HP Control Jacks (4 columns: 6.80, 16.00, 25.20, 34.40 mm; 5 rows: 65, 78, 91, 104, 117 mm)
    ctrl_x = [6.800, 16.000, 25.200, 34.400]
    ctrl_rows = [
        (65.000, ["SYNC", "X", "Y", "FREQ"]),
        (78.000, ["R", "G", "B", "MORPH"]),
        (91.000, ["Z/I", "X OUT", "Y OUT", "X SCAN"]),
        (104.000, ["R OUT", "G OUT", "B OUT", "Y SCAN"]),
    ]

    lines.append('')
    lines.append('  <!-- Left 8 HP: 5-Row Control & Signal Jacks -->')
    for ry, rlabels in ctrl_rows:
        lbl_y = ry - 5.000
        for cx, lbl in zip(ctrl_x, rlabels):
            lines.append(render_text(d_din_cond, lbl, cx, lbl_y, scale_jack, label_color, f"Jack {lbl} at ({cx:.1f}, {ry:.1f})"))

    # Row 5 (Y = 117.000 mm, Label baseline Y = 112.000 mm): Z Out (6.80), Sync Out (20.60), Record (34.40)
    lines.append(render_text(d_din_cond, "Z OUT", 6.800, 112.000, scale_jack, label_color, "Jack Z OUT"))
    lines.append(render_text(d_din_cond, "SYNC OUT", 20.600, 112.000, scale_jack, label_color, "Jack SYNC OUT"))
    lines.append(render_text(d_din_cond, "RECORD", 34.400, 112.000, scale_jack, label_color, "Jack RECORD"))

    # ── RIGHT 12 HP SECTION (40.64 to 101.60 mm, Center = 71.12 mm) ──
    # 4 Columns centered in 12 HP at pitch 12.50 mm: [52.37, 64.87, 77.37, 89.87]
    grid_x = [52.370, 64.870, 77.370, 89.870]
    grid_btn_y = [19.500, 32.000, 44.500, 57.000]
    btn_size = 10.000
    half_btn = btn_size / 2.0

    lines.append('')
    lines.append('  <!-- Right 12 HP: 4x4 Preset Matrix Wells -->')
    for r, by in enumerate(grid_btn_y):
        for c, bx in enumerate(grid_x):
            slot_num = r * 4 + c + 1
            rx = bx - half_btn
            ry = by - half_btn
            # Render preset-empty style rounded square placeholder on SVG faceplate for standalone/browser preview
            lines.append(f'  <rect x="{rx:.3f}" y="{ry:.3f}" width="{btn_size:.3f}" height="{btn_size:.3f}" rx="1.6" fill="#eaeaea" stroke="#a9a9a9" stroke-width="0.30"/>')
            inner_s = btn_size - 1.600
            lines.append(f'  <rect x="{rx + 0.800:.3f}" y="{ry + 0.800:.3f}" width="{inner_s:.3f}" height="{inner_s:.3f}" rx="1.1" fill="#ffffff" stroke="none"/>')
            lines.append(render_text(d_din_cond_bold, str(slot_num), bx, by + 1.250, scale_jack, "#444444", f"Preset Slot {slot_num}"))

    # Delineator Line for Right 12 HP Section (10 mm above Row 1 Gate Jacks at 78.00 mm -> Y = 68.00 mm)
    lines.append('')
    lines.append(render_delineator(44.000, panel_w, 68.000, hi_color, sh_color, "Right 12 HP Delineator at Y = 68.00mm"))

    # Right 12 HP: 4x4 Gate Input Grid of Jacks (Rows at 78, 91, 104, 117 mm)
    grid_jack_y = [78.000, 91.000, 104.000, 117.000]
    lines.append('')
    lines.append('  <!-- Right 12 HP: 4x4 Preset Gate Input Grid -->')
    for r, jy in enumerate(grid_jack_y):
        lbl_y = jy - 5.000
        for c, jx in enumerate(grid_x):
            slot_num = r * 4 + c + 1
            lines.append(render_text(d_din_cond, str(slot_num), jx, lbl_y, scale_jack, label_color, f"Preset Gate {slot_num}"))

    lines.append('</svg>')
    return '\n'.join(lines) + '\n'

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
            'ring_color': '#484848',
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
            'ring_color': '#9e9e9e',
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
            'ring_color': '#222222',
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
            skin['ring_color'],
            skin['well_bg'],
            skin['well_border'],
            fonts
        )
        with open(skin['filename'], 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Generated {skin['filename']} ({skin['name']} skin)")

    # Render verification bitmaps
    # 20 HP: 101.60 mm x 128.50 mm -> at 240px height = 190 px width
    os.makedirs('scratch', exist_ok=True)
    verify_png = 'scratch/reverie_verify.png'
    out_w = int(round(240 * (101.60 / 128.50)))
    cairosvg.svg2png(url='res/Reverie.svg', write_to=verify_png, output_width=out_w * 4, output_height=240 * 4)
    print(f"Rendered {verify_png} ({out_w * 4}x{240 * 4})")

    os.makedirs('metamodule/assets', exist_ok=True)
    meta_png = 'metamodule/assets/Reverie.png'
    cairosvg.svg2png(url='res/Reverie.svg', write_to=meta_png, output_width=out_w, output_height=240)
    print(f"Rendered {meta_png} ({out_w}x240)")

if __name__ == '__main__':
    main()
