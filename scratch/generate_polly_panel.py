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

def get_text_width(font, text, scale):
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    return sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)

def render_section_divider(center_y, panel_w, hi_color, sh_color):
    # Stretched to exactly 80% of module width:
    # Width = 0.8 * 71.12 = 56.896 mm, centered with 7.112 mm margins
    divider_w = panel_w * 0.80
    x_start = (panel_w - divider_w) / 2.0  # 7.112 mm
    fade_w = divider_w * 0.10             # 5.690 mm (10% fade at each end)
    mid_w = divider_w * 0.80              # 45.516 mm (80% solid in center)
    x_mid1 = x_start + fade_w             # 12.802 mm
    x_mid2 = x_mid1 + mid_w              # 58.318 mm

    # Total vertical height = 0.600 mm (0.300 mm highlight + 0.300 mm shadow)
    top_y = center_y - 0.300
    bot_y = center_y

    return f'''  <!-- Section Divider at Center Y = {center_y:.2f}mm (Width = {divider_w:.3f}mm, Total Height = 0.600mm) -->
  <g>
    <!-- Top Highlight Line ({hi_color}) -->
    <rect x="{x_start:.3f}" y="{top_y:.3f}" width="{fade_w:.3f}" height="0.300" fill="url(#div-fade-in-hi)"/>
    <rect x="{x_mid1:.3f}" y="{top_y:.3f}" width="{mid_w:.3f}" height="0.300" fill="{hi_color}"/>
    <rect x="{x_mid2:.3f}" y="{top_y:.3f}" width="{fade_w:.3f}" height="0.300" fill="url(#div-fade-out-hi)"/>
    <!-- Bottom Shadow Line ({sh_color}) -->
    <rect x="{x_start:.3f}" y="{bot_y:.3f}" width="{fade_w:.3f}" height="0.300" fill="url(#div-fade-in-sh)"/>
    <rect x="{x_mid1:.3f}" y="{bot_y:.3f}" width="{mid_w:.3f}" height="0.300" fill="{sh_color}"/>
    <rect x="{x_mid2:.3f}" y="{bot_y:.3f}" width="{fade_w:.3f}" height="0.300" fill="url(#div-fade-out-sh)"/>
  </g>'''

def build_svg_panel(theme_name, bg_color, title_color, label_color, line_color, hi_color, sh_color, fonts):
    d_din_bold = fonts['d_din_bold']
    d_din_cond_bold = fonts['d_din_cond_bold']
    d_din_cond = fonts['d_din_cond']
    qs_reg_font = fonts['qs_reg_font']

    # Panel dimensions: 14 HP (71.12 mm x 128.5 mm)
    panel_w = 71.12
    panel_h = 128.50

    # 5 Primary Columns layout (pitch = 13.50 mm)
    col_x = [8.56, 22.06, 35.56, 49.06, 62.56]

    # Row 3 Jacks (4 Columns symmetrically spaced across 14 HP)
    jack3_x = [11.06, 27.39, 43.73, 60.06]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        '  <defs>',
        '    <!-- Faked transparency gradients between background color and highlight/shadow -->',
        '    <linearGradient id="div-fade-in-hi" x1="0" y1="0" x2="1" y2="0">',
        f'      <stop offset="0" stop-color="{bg_color}"/>',
        f'      <stop offset="1" stop-color="{hi_color}"/>',
        '    </linearGradient>',
        '    <linearGradient id="div-fade-out-hi" x1="0" y1="0" x2="1" y2="0">',
        f'      <stop offset="0" stop-color="{hi_color}"/>',
        f'      <stop offset="1" stop-color="{bg_color}"/>',
        '    </linearGradient>',
        '    <linearGradient id="div-fade-in-sh" x1="0" y1="0" x2="1" y2="0">',
        f'      <stop offset="0" stop-color="{bg_color}"/>',
        f'      <stop offset="1" stop-color="{sh_color}"/>',
        '    </linearGradient>',
        '    <linearGradient id="div-fade-out-sh" x1="0" y1="0" x2="1" y2="0">',
        f'      <stop offset="0" stop-color="{sh_color}"/>',
        f'      <stop offset="1" stop-color="{bg_color}"/>',
        '    </linearGradient>',
        '  </defs>',
        f'  <!-- Panel Background: 14 HP ({theme_name} Theme) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="{bg_color}"/>',
        '  <!-- Left Edge Color Badge (Placeholder #5d5d5d per Rule 6.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        render_section_divider(58.00, panel_w, hi_color, sh_color),
        '',
        render_section_divider(83.00, panel_w, hi_color, sh_color),
        ''
    ]

    # Module title "POLLY" in D-DIN Bold, 16pt (scale 0.005644), Y baseline 8mm, centered horizontally
    title_scale = 16.0 * 25.4 / 72.0 / 1000.0  # 0.0056444
    svg_parts.append(render_text(d_din_bold, "POLLY", panel_w / 2.0, 8.000, title_scale, title_color, "POLLY", align="center"))

    # Version number "v2.0.0" in Quicksand-Regular, scale 0.001600, Y baseline 8mm (same as module name),
    # right-aligned at X = (panel_w - 2.15 mm)
    version_x = panel_w - 2.15
    svg_parts.append(render_text(qs_reg_font, "v2.0.0", version_x, 8.000, 0.001600, "#aaaaaa", "v2.0.0", align="right"))

    # ── Zone 1 & 2: Knobs (D-DIN Condensed Bold, 10pt, baseline - 5.75mm) ──
    knob_label_scale = 10.0 * 25.4 / 72.0 / 1000.0  # 0.0035278

    # Row 1 Knobs: Center Y = 18.00 mm -> Label baseline Y = 18.00 - 5.75 = 12.25 mm
    row1_labels = ["FREQ", "RANGE", "FINE", "SIDES", "ANGLE"]
    for i, lbl in enumerate(row1_labels):
        svg_parts.append(render_text(d_din_cond_bold, lbl, col_x[i], 12.250, knob_label_scale, label_color, f"Row 1: {lbl}"))

    # Row 2 Knobs: Center Y = 34.00 mm -> Label baseline Y = 34.00 - 5.75 = 28.25 mm
    row2_labels = ["SPLIT", "PAIR", "TRIO", "GROUP", "BUNCH"]
    for i, lbl in enumerate(row2_labels):
        svg_parts.append(render_text(d_din_cond_bold, lbl, col_x[i], 28.250, knob_label_scale, label_color, f"Row 2 (Dist): {lbl}"))

    # Row 3 Knobs: Center Y = 50.00 mm -> Label baseline Y = 50.00 - 5.75 = 44.25 mm
    row3_labels = ["PINCH", "TWIST", "BEVEL", "PHASE", "STRETCH"]
    for i, lbl in enumerate(row3_labels):
        svg_parts.append(render_text(d_din_cond_bold, lbl, col_x[i], 44.250, knob_label_scale, label_color, f"Row 3: {lbl}"))

    # Connecting concert line between PINCH and TWIST
    pinch_w = get_text_width(d_din_cond_bold, "PINCH", knob_label_scale)
    twist_w = get_text_width(d_din_cond_bold, "TWIST", knob_label_scale)
    pinch_right = col_x[0] + pinch_w / 2.0
    twist_left = col_x[1] - twist_w / 2.0
    line_x1 = pinch_right + 1.20
    line_x2 = twist_left - 1.20
    line_y = 44.250 - 1.20
    svg_parts.append('  <!-- Concert line between PINCH and TWIST -->')
    svg_parts.append(f'  <line x1="{line_x1:.3f}" y1="{line_y:.3f}" x2="{line_x2:.3f}" y2="{line_y:.3f}" stroke="{line_color}" stroke-width="0.300"/>')

    # ── Zone 3: CV Attenuverters (D-DIN Condensed, 8pt, baseline - 4.00mm) ──
    trim_label_scale = 8.0 * 25.4 / 72.0 / 1000.0  # 0.0028222

    # Row 1 Attenuverters: Center Y = 67.00 mm -> Label baseline Y = 67.00 - 4.00 = 63.00 mm
    trim1_labels = ["FREQ", "SIDES", "ANGLE", "PINCH", "TWIST"]
    for i, lbl in enumerate(trim1_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 63.000, trim_label_scale, label_color, f"Trim 1: {lbl}"))

    # Row 2 Attenuverters: Center Y = 78.00 mm -> Label baseline Y = 78.00 - 4.00 = 74.00 mm
    trim2_labels = ["FM", "DIST", "BEVEL", "PHASE", "STRETCH"]
    for i, lbl in enumerate(trim2_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 74.000, trim_label_scale, label_color, f"Trim 2: {lbl}"))

    # ── Zone 4: I/O Jacks (D-DIN Condensed, 8pt, baseline - 4.00mm) ──
    # Jack Row 1 (Inputs): Center Y = 94.50 mm -> Label baseline Y = 94.50 - 4.00 = 90.50 mm
    jack1_labels = ["FREQ", "SIDES", "ANGLE", "PINCH", "TWIST"]
    for i, lbl in enumerate(jack1_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 90.500, trim_label_scale, label_color, f"Jack 1: {lbl}"))

    # Jack Row 2 (Inputs): Center Y = 106.00 mm -> Label baseline Y = 106.00 - 4.00 = 102.00 mm
    jack2_labels = ["FM", "DIST", "BEVEL", "PHASE", "STRETCH"]
    for i, lbl in enumerate(jack2_labels):
        svg_parts.append(render_text(d_din_cond, lbl, col_x[i], 102.000, trim_label_scale, label_color, f"Jack 2: {lbl}"))

    # Jack Row 3 (Sync & Outputs): Center Y = 118.00 mm -> Label baseline Y = 118.00 - 4.00 = 114.00 mm
    jack3_labels = ["SYNC", "X", "Y", "SYNC"]
    for i, lbl in enumerate(jack3_labels):
        svg_parts.append(render_text(d_din_cond, lbl, jack3_x[i], 114.000, trim_label_scale, label_color, f"Jack 3: {lbl}"))

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
            'filename': 'res/Polly.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e'
        },
        {
            'name': 'Light',
            'filename': 'res/Polly_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8'
        },
        {
            'name': 'Dark',
            'filename': 'res/Polly_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'label_color': '#ffffff',
            'line_color': '#ffffff',
            'hi_color': '#585858',
            'sh_color': '#202020'
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

    # Render verification bitmap (133 x 240 at 4x resolution = 532 x 960) using default Medium skin
    panel_w = 71.12
    col_x = [8.56, 22.06, 35.56, 49.06, 62.56]
    jack3_x = [11.06, 27.39, 43.73, 60.06]

    verify_png = 'scratch/polly_verify.png'
    cairosvg.svg2png(url='res/Polly.svg', write_to=verify_png, output_width=133 * 4, output_height=240 * 4)

    # Render MetaModule asset (133 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url='res/Polly.svg', write_to='metamodule/assets/Polly.png', output_width=133, output_height=240)
    print("Rendered metamodule/assets/Polly.png successfully.")

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (133 * 4) / panel_w

    # Knobs at Y = 18.00, 34.00, 50.00 mm (radius ~ 4.75 mm)
    kr = 4.75 * r_px
    for ky in [18.00, 34.00, 50.00]:
        for i, kx in enumerate(col_x):
            if ky == 18.00 and i == 1:
                # Row 1 Col 1 is Range LED + Button
                continue
            draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Range Button and LED at col_x[1] (22.06 mm)
    led_r = 1.5 * r_px
    draw.ellipse([col_x[1] * r_px - led_r, 14.50 * r_px - led_r, col_x[1] * r_px + led_r, 14.50 * r_px + led_r], outline='#d35fb7', width=2)
    btn_r = 2.0 * r_px
    draw.ellipse([col_x[1] * r_px - btn_r, 18.00 * r_px - btn_r, col_x[1] * r_px + btn_r, 18.00 * r_px + btn_r], outline='#f0e442', width=2)

    # Trimpots: Y = 67.00 and 78.00 mm
    tr = 2.5 * r_px
    for ty in [67.00, 78.00]:
        for tx in col_x:
            draw.ellipse([tx * r_px - tr, ty * r_px - tr, tx * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks: Row 1 (94.50 mm), Row 2 (106.00 mm) at 5 columns
    jr = 4.15 * r_px
    for jy in [94.50, 106.00]:
        for jx in col_x:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    # Jacks: Row 3 (118.00 mm) at 4 columns
    for jx in jack3_x:
        draw.ellipse([jx * r_px - jr, 118.00 * r_px - jr, jx * r_px + jr, 118.00 * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
