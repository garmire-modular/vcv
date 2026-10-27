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
    panel_w = 20.32
    panel_h = 128.50
    
    col_in = 5.20
    col_mid = 10.16
    col_out = 15.12
    
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
    
    # Palette badge (Utility class placeholder: #5d5d5d per AGENTS.md 7.6.2, 1.35mm width for 4HP)
    svg.append('  <g id="palette-badge">')
    svg.append('    <rect x="0" y="19.800" width="1.350" height="88.900" fill="#5d5d5d" stroke="none" />')
    svg.append('  </g>')
    
    # Header: Full-sized Title "SALUS" (16pt D-DIN-Bold) at Y = 8.000 mm, centered at col_mid
    svg.append(render_text(ddin_bold, 'SALUS', col_mid, 8.000, 16.0, title_color))
    # Version tag "v1.0.4" at Y = 8.000 mm (right margin X = panel_w - 2.15 mm) per AGENTS.md 7.5
    svg.append(render_text(quicksand, 'v1.0.4', panel_w - 2.15, 8.000, 4.5, version_color))
    
    # Two-Column LED Architecture:
    # Col 1 (col_in = 5.20 mm): Status LED (Green -> Yellow -> Amber, turns off/0 while red is active)
    # Col 2 (col_out = 15.12 mm): Trip LED (Red latch while active, 2.0s slow fade release to red_0)
    # Centered Row Labels: 8 pt D-DINCondensed-Bold at Y = 13.00, 20.00, 27.00 mm
    # LED Center Positions: Y = 16.50, 23.50, 30.50 mm
    led_rows = [16.50, 23.50, 30.50]
    layer_labels = ['SCAN', 'GALVO', 'TEMP']
    
    bezel_color = '#383838' if bg_color != '#333333' else '#202020'
    for i in range(3):
        ly = led_rows[i]
        label_base = ly - 3.50
        # Single centered label across both LED columns (col_mid) strictly 8 pt per AGENTS.md 7.5
        svg.append(render_text(ddin_cond_bold, layer_labels[i], col_mid, label_base, 8.0, label_color))
        # Subtle guide rings for 4mm LEDs in both columns
        svg.append(f'  <circle cx="{col_in:.3f}" cy="{ly:.3f}" r="2.000" fill="none" stroke="{bezel_color}" stroke-width="0.300" />')
        svg.append(f'  <circle cx="{col_out:.3f}" cy="{ly:.3f}" r="2.000" fill="none" stroke="{bezel_color}" stroke-width="0.300" />')
    
    # E-STOP Section: Button directly above GATE CV Jack (both centered at midline col_mid = 10.16 mm)
    # Single centered label "E-STOP" strictly 8 pt at Y = 35.50 mm
    # Button center: Y = 40.50 mm (diameter 8.38 mm)
    # Jack center:   Y = 50.00 mm
    svg.append(render_text(ddin_cond_bold, 'E-STOP', col_mid, 35.50, 8.0, label_color))
    
    # Section Delineator line between E-STOP section and I/O Jacks at Y = 56.00 mm
    delin_y = 56.00
    delin_start_x = 0.20 * panel_w
    delin_end_x = panel_w
    svg.append(f'  <line x1="{delin_start_x:.3f}" y1="{delin_y - 0.075:.3f}" x2="{delin_end_x:.3f}" y2="{delin_y - 0.075:.3f}" stroke="{hi_color}" stroke-width="0.150" />')
    svg.append(f'  <line x1="{delin_start_x:.3f}" y1="{delin_y + 0.075:.3f}" x2="{delin_end_x:.3f}" y2="{delin_y + 0.075:.3f}" stroke="{sh_color}" stroke-width="0.150" />')
    
    # ROW 3-7 Column Headers: IN over col_in, OUT over col_out at Y = 59.50 mm (strictly 8 pt)
    hdr_y = 59.50
    svg.append(render_text(ddin_cond_bold, 'IN', col_in, hdr_y, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'OUT', col_out, hdr_y, 8.0, label_color))
    
    # 5 Jack Rows: Bottom-up: Row 7 at 117.00, Row 6 at 104.00, Row 5 at 91.00, Row 4 at 78.00, Row 3 at 65.00
    jack_rows = [65.00, 78.00, 91.00, 104.00, 117.00]
    row_labels = ['X', 'Y', 'R', 'G', 'B']
    
    for r in range(5):
        jy = jack_rows[r]
        j_label_y = jy - 5.00
        # Single centered label across both columns at midline strictly 8 pt per AGENTS.md 7.5
        svg.append(render_text(ddin_cond_bold, row_labels[r], col_mid, j_label_y, 8.0, label_color))
        
    svg.append('</svg>')
    
    svg_str = '\n'.join(svg)
    with open(svg_filename, 'w', encoding='utf-8') as f:
        f.write(svg_str)
    print(f'Generated {svg_filename}')

skins = [
    {
        'name': 'Medium',
        'filename': 'res/Salus.svg',
        'bg_color': '#6e6e6e',
        'title_color': '#ffffff',
        'label_color': '#222222',
        'version_color': '#aaaaaa',
        'hi_color': '#999999',
        'sh_color': '#5e5e5e'
    },
    {
        'name': 'Light',
        'filename': 'res/Salus_Light.svg',
        'bg_color': '#c6c6c6',
        'title_color': '#333333',
        'label_color': '#222222',
        'version_color': '#666666',
        'hi_color': '#e4e4e4',
        'sh_color': '#a8a8a8'
    },
    {
        'name': 'Dark',
        'filename': 'res/Salus_Dark.svg',
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

# Render MetaModule bitmap (38 x 240) and verification image
cairosvg.svg2png(url='res/Salus.svg', write_to='metamodule/assets/Salus.png', output_width=38, output_height=240)
cairosvg.svg2png(url='res/Salus.svg', write_to='scratch/salus_verify.png', output_width=38 * 4, output_height=240 * 4)
print('Rendered metamodule/assets/Salus.png and scratch/salus_verify.png')
