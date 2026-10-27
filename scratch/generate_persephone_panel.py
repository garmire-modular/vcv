import os
import re
from pathlib import Path

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

def render_text(font, text, x_center, y_baseline, pt_size, color='#222222'):
    scale = (pt_size * 25.4) / (72.0 * font['head'].unitsPerEm)
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    gset = font.getGlyphSet()
    
    widths = [hmtx[cmap[ord(c)]][0] * scale for c in text]
    total_w = sum(widths)
    cur_x = x_center - total_w / 2.0
    paths = []
    for c, w in zip(text, widths):
        cp = ord(c)
        gname = cmap[cp]
        path = pathops.Path()
        pen = pathops.PathPen(path)
        gset[gname].draw(pen)
        simplified = pathops.simplify(path)
        svg_pen = SVGPathPen(None)
        simplified.draw(svg_pen)
        d = svg_pen.getCommands()
        d_clean = clean_svg_path_numbers(d)
        paths.append(f'<path d="{d_clean}" fill="{color}" transform="translate({cur_x:.3f},{y_baseline:.3f}) scale({scale:.6f},{-scale:.6f})" />')
        cur_x += w
    return ''.join(paths)

def generate_panel(skin):
    panel_w = 40.64
    panel_h = 128.50
    
    col0 = 7.00
    col1 = 20.32
    col2 = 33.64
    
    bg_color = skin['bg_color']
    title_color = skin['title_color']
    label_color = skin['label_color']
    version_color = skin['version_color']
    hi_color = skin['hi_color']
    sh_color = skin['sh_color']
    svg_filename = skin['filename']
    
    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">')
    svg.append(f'  <rect x="0" y="0" width="{panel_w:.2f}" height="{panel_h:.2f}" fill="{bg_color}" />')
    
    # Palette badge (Utility class placeholder: #5d5d5d per AGENTS.md 7.6.2)
    svg.append('  <g id="palette-badge">')
    svg.append('    <rect x="0" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />')
    svg.append('  </g>')
    
    # Header: Title "PERSEPHONE" & Version tag "v1.0.0" at Y = 8.000 mm
    svg.append(render_text(ddin_bold, 'PERSEPHONE', panel_w / 2.0, 8.000, 13.5, title_color))
    svg.append(render_text(quicksand, 'v1.0.0', panel_w - 2.15, 8.000, 4.5, version_color))
    
    # Zone 2: ARM button at X = col1, Y = 17.50 mm (label at Y = 12.25 mm)
    btn_y = 17.50
    svg.append(render_text(ddin_cond_bold, 'ARM', col1, 12.25, 8.5, label_color))
    
    # ARM button ring track (X = col1, Y = btn_y, R = 4.12 mm, stroke = 1.40 mm)
    ring_track_color = '#3a3a3a' if bg_color != '#333333' else '#222222'
    svg.append(f'  <circle cx="{col1:.3f}" cy="{btn_y:.3f}" r="4.120" fill="none" stroke="{ring_track_color}" stroke-width="1.400" />')
    
    # Status LED cluster labels at Y = 24.80 mm
    led_label_y = 24.80
    led_x = [10.50, 17.00, 23.64, 30.14]
    svg.append(render_text(ddin_cond_bold, 'SAFE', led_x[0], led_label_y, 6.2, label_color))
    svg.append(render_text(ddin_cond_bold, 'FAIL', led_x[1], led_label_y, 6.2, label_color))
    svg.append(render_text(ddin_cond_bold, 'SLEW', led_x[2], led_label_y, 6.2, label_color))
    svg.append(render_text(ddin_cond_bold, 'TEMP', led_x[3], led_label_y, 6.2, label_color))
    
    # Interlock Arm Gate Jack at X = col1, Y = 39.50 mm (label at Y = 34.50 mm)
    gate_y = 39.50
    svg.append(render_text(ddin_cond_bold, 'GATE', col1, 34.50, 7.5, label_color))
    
    # Section Delineator line 10.00 mm above top jack row (Y = 65.00 - 10.00 = 55.00 mm)
    delin_y = 55.00
    delin_start_x = 0.20 * panel_w
    delin_end_x = panel_w
    svg.append(f'  <line x1="{delin_start_x:.3f}" y1="{delin_y - 0.075:.3f}" x2="{delin_end_x:.3f}" y2="{delin_y - 0.075:.3f}" stroke="{hi_color}" stroke-width="0.150" />')
    svg.append(f'  <line x1="{delin_start_x:.3f}" y1="{delin_y + 0.075:.3f}" x2="{delin_end_x:.3f}" y2="{delin_y + 0.075:.3f}" stroke="{sh_color}" stroke-width="0.150" />')
    
    # Zone 4 Column Headers: IN over Col 0, OUT over Col 2 at Y = 59.50 mm
    hdr_y = 59.50
    svg.append(render_text(ddin_cond_bold, 'IN', col0, hdr_y, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'OUT', col2, hdr_y, 8.0, label_color))
    
    # 5 Jack Rows (Bottom-up: Row 4 at 117.00, Row 3 at 104.00, Row 2 at 91.00, Row 1 at 78.00, Row 0 at 65.00)
    jack_rows = [65.00, 78.00, 91.00, 104.00, 117.00]
    row_labels = ['X', 'Y', 'R', 'G', 'B']
    
    for r in range(5):
        jy = jack_rows[r]
        j_label_y = jy - 5.00
        # Single centered label across both columns
        svg.append(render_text(ddin_cond_bold, row_labels[r], col1, j_label_y, 8.0, label_color))
        
    svg.append('</svg>')
    
    svg_str = '\n'.join(svg)
    with open(svg_filename, 'w', encoding='utf-8') as f:
        f.write(svg_str)
    print(f'Generated {svg_filename}')

skins = [
    {
        'name': 'Medium',
        'filename': 'res/Persephone.svg',
        'bg_color': '#6e6e6e',
        'title_color': '#ffffff',
        'label_color': '#222222',
        'version_color': '#aaaaaa',
        'hi_color': '#999999',
        'sh_color': '#5e5e5e'
    },
    {
        'name': 'Light',
        'filename': 'res/Persephone_Light.svg',
        'bg_color': '#c6c6c6',
        'title_color': '#333333',
        'label_color': '#222222',
        'version_color': '#666666',
        'hi_color': '#e4e4e4',
        'sh_color': '#a8a8a8'
    },
    {
        'name': 'Dark',
        'filename': 'res/Persephone_Dark.svg',
        'bg_color': '#333333',
        'title_color': '#c6c6c6',
        'label_color': '#ffffff',
        'version_color': '#888888',
        'hi_color': '#585858',
        'sh_color': '#202020'
    }
]

for s in skins:
    generate_panel(s)

# Render MetaModule bitmap (76 x 240) and verification image
cairosvg.svg2png(url='res/Persephone.svg', write_to='metamodule/assets/Persephone.png', output_width=76, output_height=240)
cairosvg.svg2png(url='res/Persephone.svg', write_to='scratch/persephone_verify.png', output_width=76 * 4, output_height=240 * 4)
print('Rendered metamodule/assets/Persephone.png and scratch/persephone_verify.png')
