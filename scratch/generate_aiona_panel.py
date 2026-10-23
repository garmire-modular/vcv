import os
import re
import sys
from pathlib import Path

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

def render_text_centered(font, text, center_x, baseline_y, scale, fill, comment=""):
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

def render_text_right(font, text, right_x, baseline_y, scale, fill, comment=""):
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text]
    total_w = sum(widths)
    start_x = right_x - total_w
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

def render_section_divider(center_y, panel_w, hi_color="#999999", sh_color="#5e5e5e"):
    divider_w = panel_w * 0.80
    x_start = panel_w - divider_w
    top_y = center_y - 0.150
    bot_y = center_y
    return f'''  <!-- Section Divider at Center Y = {center_y:.2f}mm (Width = {divider_w:.3f}mm, Right-Aligned, Total Height = 0.300mm) -->
  <g>
    <!-- Top Highlight Line ({hi_color}) -->
    <rect x="{x_start:.3f}" y="{top_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{hi_color}"/>
    <!-- Bottom Shadow Line ({sh_color}) -->
    <rect x="{x_start:.3f}" y="{bot_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{sh_color}"/>
  </g>'''

def main():
    din_bold = TTFont('res/D-DIN-Bold.otf')
    din_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
    din_cond = TTFont('res/D-DINCondensed.otf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    # Panel dimensions: 8 HP (40.64 mm x 128.50 mm)
    panel_w = 40.64
    panel_h = 128.50

    col0 = 7.000
    col1 = panel_w / 2.0  # 20.320 mm
    col2 = panel_w - 7.000  # 33.640 mm

    # Title "AIONA" in D-DIN-Bold (16 pt scale 0.005644, centered in 40.64 mm)
    title_block = [
        render_text_centered(din_bold, "AIONA", panel_w / 2.0, 8.000, 0.005644, "#ffffff", "AIONA")
    ]

    # Version tag "v1.0.0"
    version_block = [
        render_text_right(qs_reg_font, "v1.0.0", panel_w - 2.150, 8.000, 0.001600, "#aaaaaa", "v1.0.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 8 HP (40.64 mm x 128.50 mm) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Placeholder #5d5d5d per Section 7.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Time LCD Display Bezel & Frame (Center X = 20.32 mm, Center Y = 16.50 mm, W = 34.00 mm, H = 8.50 mm) -->',
        '  <g id="lcd-frame">',
        '    <rect x="3.320" y="12.250" width="34.000" height="8.500" rx="0.800" fill="#101412" stroke="#2c3530" stroke-width="0.300"/>',
        '  </g>',
        '',
        '  <!-- Zone 2: Row at Y = 31.00 mm: Start Button (X = 7.00), Countdown LCD (X = 20.32), PWM Knob (X = 33.64) -->',
        render_text_centered(din_cond_bold, "START", col0, 25.250, 0.003528, "#222222", "START Button Label"),
        render_text_centered(din_cond_bold, "REMAIN", col1, 25.250, 0.003528, "#222222", "REMAIN Display Label"),
        render_text_centered(din_cond_bold, "PWM", col2, 25.250, 0.003528, "#222222", "PWM Knob Label"),
        '  <!-- Countdown LCD Housing (Center X = 20.32 mm, Center Y = 31.00 mm, W = 14.00 mm, H = 7.50 mm) -->',
        '  <g id="countdown-frame">',
        '    <rect x="13.320" y="27.250" width="14.000" height="7.500" rx="0.800" fill="#101412" stroke="#2c3530" stroke-width="0.300"/>',
        '  </g>',
        '',
        render_section_divider(39.00, panel_w),
        '',
        '  <!-- Zone 3: CV Attenuverters (Center Y = 49.00 mm, Label Baseline = 45.00 mm) -->',
        render_text_centered(din_cond, "HRS", col0, 45.000, 0.002822, "#222222", "HRS CV Trim Label"),
        render_text_centered(din_cond, "MIN", col1, 45.000, 0.002822, "#222222", "MIN CV Trim Label"),
        render_text_centered(din_cond, "PWM", col2, 45.000, 0.002822, "#222222", "PWM CV Trim Label"),
        '',
        render_section_divider(55.00, panel_w),
        '',
        '  <!-- Zone 4: I/O Jacks (5 Rows, Bottom Anchor Y = 117.00 mm, Pitch = 13.00 mm) -->',
        '  <!-- Jack Row 1 (Center Y = 65.00 mm, Label Baseline = 60.00 mm): HRS, MIN, PWM -->',
        render_text_centered(din_cond, "HRS", col0, 60.000, 0.002822, "#222222", "HRS CV Jack Label"),
        render_text_centered(din_cond, "MIN", col1, 60.000, 0.002822, "#222222", "MIN CV Jack Label"),
        render_text_centered(din_cond, "PWM", col2, 60.000, 0.002822, "#222222", "PWM CV Jack Label"),
        '',
        '  <!-- Jack Row 2 (Center Y = 78.00 mm, Label Baseline = 73.00 mm): START, TRIG, PULSE -->',
        render_text_centered(din_cond, "START", col0, 73.000, 0.002822, "#222222", "START Gate Jack Label"),
        render_text_centered(din_cond, "TRIG", col1, 73.000, 0.002822, "#222222", "TRIG Pulse Jack Label"),
        render_text_centered(din_cond, "PULSE", col2, 73.000, 0.002822, "#222222", "PULSE Jack Label"),
        '',
        '  <!-- Jack Row 3 (Center Y = 91.00 mm, Label Baseline = 86.00 mm): HIGH, SIN+, TRI+ -->',
        render_text_centered(din_cond, "HIGH", col0, 86.000, 0.002822, "#222222", "HIGH Gate Jack Label"),
        render_text_centered(din_cond, "SIN+", col1, 86.000, 0.002822, "#222222", "SIN+ Jack Label"),
        render_text_centered(din_cond, "TRI+", col2, 86.000, 0.002822, "#222222", "TRI+ Jack Label"),
        '',
        '  <!-- Jack Row 4 (Center Y = 104.00 mm, Label Baseline = 99.00 mm): LOW, SIN-, TRI- -->',
        render_text_centered(din_cond, "LOW", col0, 99.000, 0.002822, "#222222", "LOW Gate Jack Label"),
        render_text_centered(din_cond, "SIN-", col1, 99.000, 0.002822, "#222222", "SIN- Jack Label"),
        render_text_centered(din_cond, "TRI-", col2, 99.000, 0.002822, "#222222", "TRI- Jack Label"),
        '',
        '  <!-- Jack Row 5 (Center Y = 117.00 mm, Label Baseline = 112.00 mm): RAMP+, RAMP-, EOC -->',
        render_text_centered(din_cond, "RAMP+", col0, 112.000, 0.002822, "#222222", "RAMP+ Jack Label"),
        render_text_centered(din_cond, "RAMP-", col1, 112.000, 0.002822, "#222222", "RAMP- Jack Label"),
        render_text_centered(din_cond, "EOC", col2, 112.000, 0.002822, "#222222", "EOC Gate Jack Label"),
        ''
    ]

    svg_parts.extend(title_block)
    svg_parts.extend(version_block)
    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Aiona.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to='metamodule/assets/Aiona.png', output_width=76, output_height=240)
    print("Rendered metamodule/assets/Aiona.png successfully.")

    verify_png = 'scratch/aiona_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=76 * 4, output_height=240 * 4)
    print(f"Rendered {verify_png} successfully.")

if __name__ == '__main__':
    main()
