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

def build_svg_panel(theme_name, bg_color, title_color, label_color, line_color, hi_color, sh_color, screen_bg, fonts):
    d_din_bold = fonts['d_din_bold']
    d_din_cond = fonts['d_din_cond']
    qs_reg_font = fonts['qs_reg_font']

    # Panel dimensions: 26 HP (132.08 mm x 128.50 mm)
    panel_w = 132.08
    panel_h = 128.50

    badge_svg = get_palette_badge_svg("Pansyview")

    # Display Canvas Box (Left section): 94 mm x 94 mm at (10.00, 15.00) mm
    # Clean, crisp sharp corners (rx=0, square bezel)
    disp_x = 10.00
    disp_y = 15.00
    disp_size = 94.00

    # Controls Column: Single vertical column on the right at X = 119.00 mm
    ctrl_x = 119.00

    # 9 Knobs uniformly spaced along the vertical column from Y = 19.00 mm to Y = 117.00 mm
    # pitch = (117.00 - 19.00) / 8 = 12.25 mm
    knob_y = [19.00 + i * 12.25 for i in range(9)]

    # Jacks Row: 6 inputs below display canvas at Y = 118.00 mm
    # Centered under canvas: [15.00, 30.00, 45.00, 60.00, 75.00, 90.00] mm
    jack_x = [15.00, 30.00, 45.00, 60.00, 75.00, 90.00]
    jack_y = 118.00

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 26 HP ({theme_name} Theme) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="{bg_color}"/>',
        badge_svg,
        '',
        '  <!-- Field Canvas Frame Bezel (94mm x 94mm, Sharp Square Corners) -->',
        f'  <rect x="{disp_x:.3f}" y="{disp_y:.3f}" width="{disp_size:.3f}" height="{disp_size:.3f}" fill="{screen_bg}" stroke="#181818" stroke-width="0.600"/>',
        ''
    ]

    # Section divider above the jack row (from x = 8.00 to x = 97.00 at Y = 111.00 mm):
    div_w = 89.00
    svg_parts.append(f'''  <!-- Section Divider above Input Jack Row -->
  <g>
    <rect x="8.000" y="110.850" width="{div_w:.3f}" height="0.150" fill="{hi_color}"/>
    <rect x="8.000" y="111.000" width="{div_w:.3f}" height="0.150" fill="{sh_color}"/>
  </g>''')

    # Vertical divider separating display section from controls (at X = 106.00 mm from Y = 15.00 to 122.00):
    svg_parts.append(f'''  <!-- Vertical Section Divider between Display and Controls -->
  <g>
    <rect x="105.850" y="15.000" width="0.150" height="106.000" fill="{hi_color}"/>
    <rect x="106.000" y="15.000" width="0.150" height="106.000" fill="{sh_color}"/>
  </g>''')

    # Module title "PANSYVIEW" in D-DIN Bold, 16pt (scale 0.005644), baseline 8mm
    title_scale = 16.0 * 25.4 / 72.0 / 1000.0
    svg_parts.append(render_text(d_din_bold, "PANSYVIEW", panel_w / 2.0, 8.000, title_scale, title_color, "PANSYVIEW", align="center"))

    # Version tag "v1.2.0" in Quicksand-Regular, scale 0.001600, right-aligned at X = panel_w - 2.15 mm
    version_x = panel_w - 2.15
    svg_parts.append(render_text(qs_reg_font, "v1.2.0", version_x, 8.000, 0.001600, "#aaaaaa", "v1.2.0", align="right"))

    # ── Control Knobs Labels (Single Vertical Row, All Consistent D-DIN Condensed Font) ──
    trim_label_scale = 7.5 * 25.4 / 72.0 / 1000.0   # 0.0026458
    jack_label_scale = 8.0 * 25.4 / 72.0 / 1000.0   # 0.0028222

    knob_labels = [
        "ARRANGEMENT",
        "LOCATION",
        "LOC. HUE",
        "TRAIL",
        "TRAIL HUE",
        "GLOW",
        "GLOW HUE",
        "SPEAKERS",
        "FIELD"
    ]

    for i, lbl in enumerate(knob_labels):
        label_y = knob_y[i] - 4.20
        svg_parts.append(render_text(d_din_cond, lbl, ctrl_x, label_y, trim_label_scale, label_color, f"Knob {i+1}: {lbl}"))

    # ── Input Jacks Row (below field visualizer, Y = 118.00 mm -> Label Y = 113.20 mm) ──
    jack_labels = ["X", "Y", "ROTATION", "FOCUS", "CENTER", "SMOOTH"]
    for i, lbl in enumerate(jack_labels):
        svg_parts.append(render_text(d_din_cond, lbl, jack_x[i], 113.200, jack_label_scale, label_color, f"Input: {lbl}"))

    svg_parts.append('</svg>')
    return "\n".join(svg_parts) + "\n"

def main():
    fonts = {
        'd_din_bold': TTFont('res/D-DIN-Bold.otf'),
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

    os.makedirs('scratch', exist_ok=True)
    verify_png = 'scratch/pansyview_verify.png'
    cairosvg.svg2png(url='res/Pansyview.svg', write_to=verify_png, output_width=247 * 4, output_height=240 * 4)

    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url='res/Pansyview.svg', write_to='metamodule/assets/Pansyview.png', output_width=247, output_height=240)
    print("Rendered metamodule/assets/Pansyview.png successfully.")

if __name__ == '__main__':
    main()
