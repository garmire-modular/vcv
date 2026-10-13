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

def get_palette_badge_svg(module_name="Pansyview"):
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

def build_svg_panel(theme_name, bg_color, title_color, label_color, line_color, hi_color, sh_color, screen_bg, fonts):
    d_din_bold = fonts['d_din_bold']
    d_din_cond_bold = fonts['d_din_cond_bold']
    d_din_cond = fonts['d_din_cond']
    qs_reg_font = fonts['qs_reg_font']

    # Panel dimensions: 26 HP (132.08 mm x 128.50 mm) - almost perfectly square!
    panel_w = 132.08
    panel_h = 128.50

    badge_svg = get_palette_badge_svg("Pansyview")

    # Display Canvas Box (Left section):
    # Centered vertically in main area, with margins
    # Let's give it an inset dark display frame
    disp_x = 10.00
    disp_y = 15.00
    disp_size = 98.00  # 98 mm x 98 mm square frame

    # Controls Section (Right strip):
    # col_x: x = 119.00 mm
    ctrl_x = 119.00

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 26 HP ({theme_name} Theme) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="{bg_color}"/>',
        badge_svg,
        '',
        '  <!-- Field Canvas Frame Bezel -->',
        f'  <rect x="{disp_x:.3f}" y="{disp_y:.3f}" width="{disp_size:.3f}" height="{disp_size:.3f}" rx="3.000" fill="{screen_bg}" stroke="#181818" stroke-width="0.600"/>',
        ''
    ]

    # Section divider in the control column between Selector knob and CV inputs:
    # Selector at Y=23.00, CV inputs at Y=50..118
    # Divider at Y=36.00 mm from x = 109.00 to panel_w - 2.0
    div_w = panel_w - 110.00
    svg_parts.append(f'''  <!-- Section Divider between Layout Selector and CV Inputs -->
  <g>
    <rect x="110.000" y="35.850" width="{div_w:.3f}" height="0.150" fill="{hi_color}"/>
    <rect x="110.000" y="36.000" width="{div_w:.3f}" height="0.150" fill="{sh_color}"/>
  </g>''')

    # Module title "PANSYVIEW" in D-DIN Bold, 16pt (scale 0.005644), baseline 8mm
    title_scale = 16.0 * 25.4 / 72.0 / 1000.0
    svg_parts.append(render_text(d_din_bold, "PANSYVIEW", panel_w / 2.0, 8.000, title_scale, title_color, "PANSYVIEW", align="center"))

    # Version tag "v1.0.1" in Quicksand-Regular, scale 0.001600, right-aligned at X = panel_w - 2.15 mm
    version_x = panel_w - 2.15
    svg_parts.append(render_text(qs_reg_font, "v1.0.1", version_x, 8.000, 0.001600, "#aaaaaa", "v1.0.1", align="right"))

    # ── Control Column Labels ──
    knob_label_scale = 10.0 * 25.4 / 72.0 / 1000.0  # 0.0035278
    jack_label_scale = 8.0 * 25.4 / 72.0 / 1000.0   # 0.0028222

    # Layout Selector Knob: Center Y = 23.00 mm -> Label baseline Y = 23.00 - 6.20 = 16.80 mm
    svg_parts.append(render_text(d_din_cond_bold, "LAYOUT", ctrl_x, 16.800, knob_label_scale, label_color, "LAYOUT Knob Label"))

    # CV Inputs (stacked vertically):
    # Jack 1: X (Center Y = 46.00 mm -> Label Y = 41.50 mm)
    svg_parts.append(render_text(d_din_cond, "X", ctrl_x, 41.500, jack_label_scale, label_color, "X CV Label"))

    # Jack 2: Y (Center Y = 60.50 mm -> Label Y = 56.00 mm)
    svg_parts.append(render_text(d_din_cond, "Y", ctrl_x, 56.000, jack_label_scale, label_color, "Y CV Label"))

    # Jack 3: ROTATION (Center Y = 75.00 mm -> Label Y = 70.50 mm)
    svg_parts.append(render_text(d_din_cond, "ROT", ctrl_x, 70.500, jack_label_scale, label_color, "ROT CV Label"))

    # Jack 4: FOCUS (Center Y = 89.50 mm -> Label Y = 85.00 mm)
    svg_parts.append(render_text(d_din_cond, "FOCUS", ctrl_x, 85.000, jack_label_scale, label_color, "FOCUS CV Label"))

    # Jack 5: CENTER (Center Y = 104.00 mm -> Label Y = 99.50 mm)
    svg_parts.append(render_text(d_din_cond, "CENTER", ctrl_x, 99.500, jack_label_scale, label_color, "CENTER CV Label"))

    # Jack 6: SMOOTH (Center Y = 118.00 mm -> Label Y = 113.50 mm)
    svg_parts.append(render_text(d_din_cond, "SMOOTH", ctrl_x, 113.500, jack_label_scale, label_color, "SMOOTH CV Label"))

    # Bottom Display Status Subtitle (below display frame): Center Y = 120.00 mm
    svg_parts.append(render_text(d_din_cond, "FIELD VISUALIZER", disp_x + disp_size / 2.0, 119.500, 0.003000, line_color, "FIELD VISUALIZER Label"))

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
            'filename': 'res/Pansyview.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e',
            'screen_bg': '#1e1e1e'
        },
        {
            'name': 'Light',
            'filename': 'res/Pansyview_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8',
            'screen_bg': '#222222'
        },
        {
            'name': 'Dark',
            'filename': 'res/Pansyview_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'label_color': '#ffffff',
            'line_color': '#ffffff',
            'hi_color': '#585858',
            'sh_color': '#202020',
            'screen_bg': '#141414'
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
            skin['screen_bg'],
            fonts
        )
        with open(skin['filename'], 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Generated {skin['filename']} ({skin['name']} skin) successfully.")

    # 26 HP pixel width for MetaModule: 26 * 9.5 = 247 px, Height = 240 px
    # High-res verification PNG: 247 * 4 = 988 px, 240 * 4 = 960 px
    os.makedirs('scratch', exist_ok=True)
    verify_png = 'scratch/pansyview_verify.png'
    cairosvg.svg2png(url='res/Pansyview.svg', write_to=verify_png, output_width=247 * 4, output_height=240 * 4)

    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url='res/Pansyview.svg', write_to='metamodule/assets/Pansyview.png', output_width=247, output_height=240)
    print("Rendered metamodule/assets/Pansyview.png successfully.")

if __name__ == '__main__':
    main()
