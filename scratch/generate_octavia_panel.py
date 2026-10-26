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
    
    # Palette badge (Generate class placeholder: #5d5d5d per AGENTS.md 7.6.2)
    svg.append('  <g id="palette-badge">')
    svg.append('    <rect x="0" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />')
    svg.append('  </g>')
    
    # Header: Title "OCTAVIA" & Version tag "v1.0.0" at Y = 8.000 mm
    svg.append(render_text(ddin_bold, 'OCTAVIA', panel_w / 2.0, 8.000, 14.0, title_color))
    svg.append(render_text(quicksand, 'v1.0.0', panel_w - 2.15, 8.000, 4.5, version_color))
    
    # Row 1 Controls: FREQ, RANGE, FINE at Y = 17.50 mm (labels at Y = 12.25 mm)
    ctrl_y = 17.50
    label_y = 12.25
    svg.append(render_text(ddin_cond_bold, 'FREQ', col0, label_y, 8.5, label_color))
    svg.append(render_text(ddin_cond_bold, 'RANGE', col1, label_y, 8.5, label_color))
    svg.append(render_text(ddin_cond_bold, 'FINE', col2, label_y, 8.5, label_color))
    
    # Range button illuminated indicator ring track (X = col1, Y = ctrl_y, R = 4.12 mm, stroke = 1.40 mm)
    ring_track_color = '#3a3a3a' if bg_color != '#333333' else '#222222'
    svg.append(f'  <circle cx="{col1:.3f}" cy="{ctrl_y:.3f}" r="4.120" fill="none" stroke="{ring_track_color}" stroke-width="1.400" />')
    
    # Section Delineator line 10.00 mm above top jack row (Y = 26.00 - 10.00 = 16.00? No, jacks are at Y = 26.00 mm)
    # The controls are at Y = 17.50, Jack Row 1 at Y = 26.00 mm. Delineator line between them at Y = 21.75 mm:
    delin_y = 21.75
    delin_start_x = 0.20 * panel_w
    delin_end_x = panel_w
    svg.append(f'  <line x1="{delin_start_x:.3f}" y1="{delin_y - 0.075:.3f}" x2="{delin_end_x:.3f}" y2="{delin_y - 0.075:.3f}" stroke="{hi_color}" stroke-width="0.150" />')
    svg.append(f'  <line x1="{delin_start_x:.3f}" y1="{delin_y + 0.075:.3f}" x2="{delin_end_x:.3f}" y2="{delin_y + 0.075:.3f}" stroke="{sh_color}" stroke-width="0.150" />')
    
    # 8 Jack Rows (Y = 26.00 mm down to 117.00 mm, pitch 13.00 mm)
    pitch = 13.00
    bottom_y = 117.00
    jack_rows = [bottom_y - (7 - i) * pitch for i in range(8)]
    
    # Phase increments: 0°, 45°, 90°, 135°, 180°, 225°, 270°, 315°
    phases = ['0°', '45°', '90°', '135°', '180°', '225°', '270°', '315°']
    
    for r in range(8):
        jy = jack_rows[r]
        phase_str = phases[r]
        j_label_y = jy - 4.80
        
        # Col 0: SINE + Phase (or SIN)
        svg.append(render_text(ddin_cond_bold, f'SIN {phase_str}', col0, j_label_y, 7.0, label_color))
        # Col 1: RAMP + Phase (or RMP)
        svg.append(render_text(ddin_cond_bold, f'RMP {phase_str}', col1, j_label_y, 7.0, label_color))
        # Col 2: SAW + Phase (or SAW)
        svg.append(render_text(ddin_cond_bold, f'SAW {phase_str}', col2, j_label_y, 7.0, label_color))
        
    svg.append('</svg>')
    
    svg_str = '\n'.join(svg)
    with open(svg_filename, 'w', encoding='utf-8') as f:
        f.write(svg_str)
    print(f'Generated {svg_filename}')

skins = [
    {
        'name': 'Medium',
        'filename': 'res/Octavia.svg',
        'bg_color': '#6e6e6e',
        'title_color': '#ffffff',
        'label_color': '#222222',
        'version_color': '#aaaaaa',
        'hi_color': '#999999',
        'sh_color': '#5e5e5e'
    },
    {
        'name': 'Light',
        'filename': 'res/Octavia_Light.svg',
        'bg_color': '#c6c6c6',
        'title_color': '#333333',
        'label_color': '#222222',
        'version_color': '#666666',
        'hi_color': '#e4e4e4',
        'sh_color': '#a8a8a8'
    },
    {
        'name': 'Dark',
        'filename': 'res/Octavia_Dark.svg',
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
cairosvg.svg2png(url='res/Octavia.svg', write_to='metamodule/assets/Octavia.png', output_width=76, output_height=240)
cairosvg.svg2png(url='res/Octavia.svg', write_to='scratch/octavia_verify.png', output_width=76 * 4, output_height=240 * 4)
print('Rendered metamodule/assets/Octavia.png and scratch/octavia_verify.png')
