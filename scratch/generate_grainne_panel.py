import os
import re
import colorsys
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
from PIL import Image, ImageDraw

def clean_svg_path_numbers(d_str):
    d_str = re.sub(r'([MLHVCSQTAZmlhvcsqtaz])', r' \1 ', d_str)
    tokens = d_str.split()
    out = []
    for t in tokens:
        if re.match(r'^-?\d+(\.\d+)?$', t):
            out.append(f'{float(t):.1f}')
        else:
            out.append(t)
    return ' '.join(out)

ddin_bold = TTFont('res/D-DIN-Bold.otf')
ddin_cond = TTFont('res/D-DINCondensed.otf')
ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
quicksand = TTFont('res/Quicksand-Regular.ttf')

def render_text_centered(font, text, x_center, y_baseline, scale, color='#222222', comment=''):
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    gset = font.getGlyphSet()

    widths = [hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text]
    total_w = sum(widths)
    cur_x = x_center - total_w / 2.0
    paths = []
    if comment:
        paths.append(f'  <!-- Label: "{comment}" -->')
    for c, w in zip(text, widths):
        if c != ' ':
            cp = ord(c)
            gname = cmap[cp] if cp in cmap else c
            path = pathops.Path()
            pen = pathops.PathPen(path)
            gset[gname].draw(pen)
            simplified = pathops.simplify(path)
            svg_pen = SVGPathPen(None)
            simplified.draw(svg_pen)
            d = svg_pen.getCommands()
            d_clean = clean_svg_path_numbers(d)
            paths.append(f'  <path d="{d_clean}" fill="{color}" transform="translate({cur_x:.3f},{y_baseline:.3f}) scale({scale:.6f},{-scale:.6f})" />')
        cur_x += w
    return '\n'.join(paths)

def render_text_right(font, text, x_right, y_baseline, scale, color='#aaaaaa', comment=''):
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    gset = font.getGlyphSet()

    widths = [hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text]
    total_w = sum(widths)
    cur_x = x_right - total_w
    paths = []
    if comment:
        paths.append(f'  <!-- Label: "{comment}" -->')
    for c, w in zip(text, widths):
        if c != ' ':
            cp = ord(c)
            gname = cmap[cp] if cp in cmap else c
            path = pathops.Path()
            pen = pathops.PathPen(path)
            gset[gname].draw(pen)
            simplified = pathops.simplify(path)
            svg_pen = SVGPathPen(None)
            simplified.draw(svg_pen)
            d = svg_pen.getCommands()
            d_clean = clean_svg_path_numbers(d)
            paths.append(f'  <path d="{d_clean}" fill="{color}" transform="translate({cur_x:.3f},{y_baseline:.3f}) scale({scale:.6f},{-scale:.6f})" />')
        cur_x += w
    return '\n'.join(paths)

def render_section_divider(center_y, panel_w, hi_color='#999999', sh_color='#5e5e5e'):
    divider_w = panel_w * 0.80
    x_start = panel_w - divider_w
    top_y = center_y - 0.150
    bot_y = center_y
    return f'''  <!-- Section Divider at Center Y = {center_y:.2f}mm -->
  <g>
    <rect x="{x_start:.3f}" y="{top_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{hi_color}" />
    <rect x="{x_start:.3f}" y="{bot_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{sh_color}" />
  </g>'''

def render_gradient_preview_bar(center_y, panel_w, height_mm=1.500):
    bar_w = panel_w * 0.80
    x_start = panel_w - bar_w
    y_top = center_y - height_mm / 2.0
    stops = [(1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)]
    num_slices = 48
    slice_w = bar_w / num_slices
    rects = [f'  <!-- Static Gradient Preview Bar at Center Y = {center_y:.2f}mm (X = {x_start:.3f}mm, W = {bar_w:.3f}mm, H = {height_mm:.3f}mm) -->', '  <g id="gradient-preview-bar">']
    for i in range(num_slices):
        p = (i + 0.5) / num_slices
        scaled = p * 3.0
        seg = min(int(scaled), 2)
        u = scaled - seg
        cA = stops[seg]
        cB = stops[(seg + 1) % 3]
        r = int(round(((1.0 - u) * cA[0] + u * cB[0]) * 255))
        g = int(round(((1.0 - u) * cA[1] + u * cB[1]) * 255))
        b = int(round(((1.0 - u) * cA[2] + u * cB[2]) * 255))
        sx = x_start + i * slice_w
        sw = slice_w + (0.05 if i < num_slices - 1 else 0.0)
        rects.append(f'    <rect x="{sx:.3f}" y="{y_top:.3f}" width="{sw:.3f}" height="{height_mm:.3f}" fill="#{r:02x}{g:02x}{b:02x}" />')
    rects.append('  </g>')
    return '\n'.join(rects)

def generate_panel(skin):
    panel_w = 40.64
    panel_h = 128.50

    # 3-Column Grid (Outer margins 7.00 mm, pitch 13.32 mm)
    col0 = 7.00
    col1 = 20.32
    col2 = 33.64

    # 4-Column Bottom Output Jack Grid (6.07, 15.57, 25.07, 34.57 mm, pitch 9.50 mm)
    out0 = 6.07
    out1 = 15.57
    out2 = 25.07
    out3 = 34.57

    bg_color = skin['bg_color']
    title_color = skin['title_color']
    label_color = skin['label_color']
    version_color = skin['version_color']
    hi_color = skin['hi_color']
    sh_color = skin['sh_color']
    ring_color = skin['ring_color']
    svg_filename = skin['filename']

    svg = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        f'  <!-- Panel Background: 8 HP (40.64 mm x 128.50 mm) -->',
        f'  <rect x="0" y="0" width="{panel_w:.2f}" height="{panel_h:.2f}" fill="{bg_color}" />',
        '  <!-- Left Edge Color Badge (Default placeholder: #5d5d5d per AGENTS.md Section 7.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />',
        '  </g>',
        '',
        '  <!-- Zone 1: Header & Title -->',
        render_text_centered(ddin_bold, 'GRÁINNE', panel_w / 2.0, 8.000, 0.005644, title_color, 'GRÁINNE'),
        render_text_right(quicksand, 'v1.0.1', panel_w - 2.150, 8.000, 0.001600, version_color, 'v1.0.1'),
        '',
        '  <!-- Zone 2: Primary Controls -->',
        '  <!-- Row 1 (Center Y = 18.00 mm, Label Baseline = 12.25 mm): FREQ, SCALE, FINE -->',
        render_text_centered(ddin_cond_bold, 'FREQ', col0, 12.250, 0.003528, label_color, 'FREQ'),
        render_text_centered(ddin_cond_bold, 'SCALE', col1, 12.250, 0.003528, label_color, 'SCALE'),
        render_text_centered(ddin_cond_bold, 'FINE', col2, 12.250, 0.003528, label_color, 'FINE'),
        f'  <!-- SCALE Range Button Halo Ring Track -->',
        f'  <circle cx="{col1:.3f}" cy="18.000" r="4.120" fill="none" stroke="{ring_color}" stroke-width="1.400" />',
        '',
        '  <!-- Row 2 (Center Y = 33.50 mm, Label Baseline = 27.75 mm): HUE 1, HUE 2, HUE 3 with Halo Rings -->',
        render_text_centered(ddin_cond_bold, 'HUE 1', col0, 27.750, 0.003100, label_color, 'HUE 1'),
        render_text_centered(ddin_cond_bold, 'HUE 2', col1, 27.750, 0.003100, label_color, 'HUE 2'),
        render_text_centered(ddin_cond_bold, 'HUE 3', col2, 27.750, 0.003100, label_color, 'HUE 3'),
        f'  <!-- Hue Trimpot Halo Ring Tracks (R = 4.12 mm, Stroke = 1.40 mm) -->',
        f'  <circle cx="{col0:.3f}" cy="33.500" r="4.120" fill="none" stroke="{ring_color}" stroke-width="1.400" />',
        f'  <circle cx="{col1:.3f}" cy="33.500" r="4.120" fill="none" stroke="{ring_color}" stroke-width="1.400" />',
        f'  <circle cx="{col2:.3f}" cy="33.500" r="4.120" fill="none" stroke="{ring_color}" stroke-width="1.400" />',
        '',
        '  <!-- Row 3 (Center Y = 46.50 mm, Label Baseline = 42.50 mm): 1-2, 2-3, 3-1 Midpoint Trimpots -->',
        render_text_centered(ddin_cond_bold, '1-2', col0, 42.500, 0.003100, label_color, '1-2 Midpoint'),
        render_text_centered(ddin_cond_bold, '2-3', col1, 42.500, 0.003100, label_color, '2-3 Midpoint'),
        render_text_centered(ddin_cond_bold, '3-1', col2, 42.500, 0.003100, label_color, '3-1 Midpoint'),
        '',
        render_gradient_preview_bar(52.50, panel_w, 1.500),
        '',
        '  <!-- Zone 3: CV Attenuverters (Center Y = 62.50 mm, Label Baseline = 58.50 mm): FREQ, FM, HUE -->',
        render_text_centered(ddin_cond, 'FREQ', col0, 58.500, 0.002822, label_color, 'FREQ CV Depth'),
        render_text_centered(ddin_cond, 'FM', col1, 58.500, 0.002822, label_color, 'FM CV Depth'),
        render_text_centered(ddin_cond, 'HUE', col2, 58.500, 0.002822, label_color, 'HUE CV Depth'),
        '',
        render_section_divider(68.00, panel_w, hi_color, sh_color),
        '',
        '  <!-- Zone 4: I/O Jacks (4 Rows, Bottom Anchor Y = 117.00 mm, Pitch = 13.00 mm) -->',
        '  <!-- Jack Row 1 (Center Y = 78.00 mm, Label Baseline = 73.00 mm): SYNC, FREQ, FM -->',
        render_text_centered(ddin_cond, 'SYNC', col0, 73.000, 0.002822, label_color, 'SYNC Input'),
        render_text_centered(ddin_cond, 'FREQ', col1, 73.000, 0.002822, label_color, 'FREQ Input'),
        render_text_centered(ddin_cond, 'FM', col2, 73.000, 0.002822, label_color, 'FM Input'),
        '',
        '  <!-- Jack Row 2 (Center Y = 91.00 mm, Label Baseline = 86.00 mm): H1, H2, H3 -->',
        render_text_centered(ddin_cond, 'H1', col0, 86.000, 0.002822, label_color, 'H1 Input'),
        render_text_centered(ddin_cond, 'H2', col1, 86.000, 0.002822, label_color, 'H2 Input'),
        render_text_centered(ddin_cond, 'H3', col2, 86.000, 0.002822, label_color, 'H3 Input'),
        '',
        '  <!-- Jack Row 3 (Center Y = 104.00 mm, Label Baseline = 99.00 mm): 1-2, 2-3, 3-1 -->',
        render_text_centered(ddin_cond, '1-2', col0, 99.000, 0.002822, label_color, '1-2 Midpoint Input'),
        render_text_centered(ddin_cond, '2-3', col1, 99.000, 0.002822, label_color, '2-3 Midpoint Input'),
        render_text_centered(ddin_cond, '3-1', col2, 99.000, 0.002822, label_color, '3-1 Midpoint Input'),
        '',
        '  <!-- Jack Row 4 (Center Y = 117.00 mm, Label Baseline = 112.00 mm): R, G, B, SYNC -->',
        render_text_centered(ddin_cond, 'R', out0, 112.000, 0.002822, label_color, 'R Output'),
        render_text_centered(ddin_cond, 'G', out1, 112.000, 0.002822, label_color, 'G Output'),
        render_text_centered(ddin_cond, 'B', out2, 112.000, 0.002822, label_color, 'B Output'),
        render_text_centered(ddin_cond, 'SYNC', out3, 112.000, 0.002822, label_color, 'SYNC Output'),
        '</svg>'
    ]

    os.makedirs('res', exist_ok=True)
    with open(svg_filename, 'w', encoding='utf-8') as f:
        f.write('\n'.join(svg) + '\n')
    print(f'Generated {svg_filename}')

def main():
    skins = [
        {
            'name': 'Medium',
            'filename': 'res/Grainne.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'label_color': '#222222',
            'version_color': '#aaaaaa',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e',
            'ring_color': '#484848'
        },
        {
            'name': 'Light',
            'filename': 'res/Grainne_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'label_color': '#222222',
            'version_color': '#666666',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8',
            'ring_color': '#9e9e9e'
        },
        {
            'name': 'Dark',
            'filename': 'res/Grainne_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'label_color': '#ffffff',
            'version_color': '#888888',
            'hi_color': '#585858',
            'sh_color': '#202020',
            'ring_color': '#222222'
        }
    ]

    for s in skins:
        generate_panel(s)

    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url='res/Grainne.svg', write_to='metamodule/assets/Grainne.png', output_width=76, output_height=240)
    verify_png = 'scratch/grainne_verify.png'
    cairosvg.svg2png(url='res/Grainne.svg', write_to=verify_png, output_width=76 * 4, output_height=240 * 4)

    # Overlay verification footprints
    panel_w = 40.64
    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (76 * 4) / panel_w

    # Row 1: FREQ & FINE main knobs (R = 4.75 mm), SCALE button (R = 2.60 mm) + ring (R = 4.12 mm)
    kr = 4.75 * r_px
    for kx in [7.00, 33.64]:
        draw.ellipse([kx * r_px - kr, 18.00 * r_px - kr, kx * r_px + kr, 18.00 * r_px + kr], outline='#ffaa00', width=2)
    btn_r = 2.60 * r_px
    ring_r = 4.12 * r_px
    draw.ellipse([20.32 * r_px - btn_r, 18.00 * r_px - btn_r, 20.32 * r_px + btn_r, 18.00 * r_px + btn_r], outline='#f0e442', width=2)
    draw.ellipse([20.32 * r_px - ring_r, 18.00 * r_px - ring_r, 20.32 * r_px + ring_r, 18.00 * r_px + ring_r], outline='#40b0a6', width=2)

    # Row 2: HUE 1, HUE 2, HUE 3 trimpots (R = 3.04 mm) + halo rings (R = 4.12 mm)
    tr = 3.04 * r_px
    hue_colors = ['#ff3333', '#33ff33', '#3388ff']
    for kx, hc in zip([7.00, 20.32, 33.64], hue_colors):
        draw.ellipse([kx * r_px - tr, 33.50 * r_px - tr, kx * r_px + tr, 33.50 * r_px + tr], outline='#00ff88', width=2)
        draw.ellipse([kx * r_px - ring_r, 33.50 * r_px - ring_r, kx * r_px + ring_r, 33.50 * r_px + ring_r], outline=hc, width=3)

    # Row 3 (Y = 46.50 mm) & Zone 3 Attenuverters (Y = 62.50 mm)
    for ty in [46.50, 62.50]:
        for tx in [7.00, 20.32, 33.64]:
            draw.ellipse([tx * r_px - tr, ty * r_px - tr, tx * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks Rows 1-3 (Y = 78.00, 91.00, 104.00 mm)
    jr = 4.15 * r_px
    for jy in [78.00, 91.00, 104.00]:
        for jx in [7.00, 20.32, 33.64]:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    # Jacks Row 4 (Y = 117.00 mm at 6.07, 15.57, 25.07, 34.57 mm)
    for jx in [6.07, 15.57, 25.07, 34.57]:
        draw.ellipse([jx * r_px - jr, 117.00 * r_px - jr, jx * r_px + jr, 117.00 * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f'Rendered metamodule/assets/Grainne.png and {verify_png}')

if __name__ == '__main__':
    main()
