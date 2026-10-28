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

def generate_serqet_panel(skin):
    panel_hp = 20
    panel_w = panel_hp * 5.08  # 101.60 mm
    panel_h = 128.50
    
    # 5 Main Columns:
    # Col 1: Ch 1 In (7.00 mm)
    # Col 2: Ch 2 In (20.32 mm)
    # Col 3: Dual Scope (Center = 50.80 mm)
    # Col 4: Fader & CV (81.28 mm)
    # Col 5: Out (94.60 mm)
    col1_x = 7.00
    col2_x = 20.32
    col3_x = 50.80
    col4_x = 81.28
    col5_x = 94.60
    
    bg_color = skin['bg_color']
    title_color = skin['title_color']
    label_color = skin['label_color']
    version_color = skin['version_color']
    hi_color = skin['hi_color']
    sh_color = skin['sh_color']
    border_color = skin['border_color']
    scope_bezel = skin['scope_bezel']
    
    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">')
    svg.append(f'  <rect x="0" y="0" width="{panel_w:.2f}" height="{panel_h:.2f}" fill="{bg_color}" />')
    
    # Palette badge (Placeholder: #5d5d5d, 2.54mm width, 88.90mm height, centered vertically)
    svg.append('  <g id="palette-badge">')
    svg.append('    <rect x="0" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />')
    svg.append('  </g>')
    
    # Module Title & Version
    svg.append(render_text(ddin_bold, 'SERQET', col3_x, 8.000, 16.0, title_color))
    svg.append(render_text(quicksand, 'v1.0.0', panel_w - 2.15, 8.000, 4.5, version_color))
    
    # Column Headers (strictly 8 pt D-DINCondensed-Bold at Y = 13.00 mm)
    svg.append(render_text(ddin_cond_bold, 'CH 1', col1_x, 13.00, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'CH 2', col2_x, 13.00, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'MAIN OUT', col3_x, 13.00, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'XFADE', col4_x, 13.00, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'OUT', col5_x, 13.00, 8.0, label_color))
    
    # Dual Scope Bezels in Column 3
    # Scope 1 (Main Out): Center (50.80, 39.00), Width 44.00, Height 44.00
    s1_w, s1_h = 44.00, 44.00
    s1_x, s1_y = col3_x - s1_w / 2.0, 17.00
    svg.append(f'  <!-- Scope 1: Main Out Bezel -->')
    svg.append(f'  <rect x="{s1_x:.2f}" y="{s1_y:.2f}" width="{s1_w:.2f}" height="{s1_h:.2f}" rx="2.5" ry="2.5" fill="#111111" stroke="{scope_bezel}" stroke-width="0.8" />')
    
    # Scope 2 (Cue Monitor): Center (50.80, 89.00), Width 44.00, Height 44.00
    s2_w, s2_h = 44.00, 44.00
    s2_x, s2_y = col3_x - s2_w / 2.0, 67.00
    svg.append(f'  <!-- Scope 2: Cue Bezel -->')
    svg.append(f'  <rect x="{s2_x:.2f}" y="{s2_y:.2f}" width="{s2_w:.2f}" height="{s2_h:.2f}" rx="2.5" ry="2.5" fill="#111111" stroke="{scope_bezel}" stroke-width="0.8" />')
    svg.append(render_text(ddin_cond_bold, 'CUE MONITOR', col3_x, 65.50, 7.0, label_color))
    
    # Cue Buttons beneath Scope 2 (Y = 117.00 mm, col3_x +/- 7.50 mm -> 43.30 and 58.30 mm)
    btn_y = 117.00
    btn1_x = col3_x - 7.50
    btn2_x = col3_x + 7.50
    svg.append(render_text(ddin_cond_bold, '1', btn1_x, btn_y - 5.00, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, '2', btn2_x, btn_y - 5.00, 8.0, label_color))
    
    # 5 Jack Rows (Bottom-up: Row 7 at 117.00, Row 6 at 104.00, Row 5 at 91.00, Row 4 at 78.00, Row 3 at 65.00)
    jack_rows = [65.00, 78.00, 91.00, 104.00, 117.00]
    jack_labels = ['X', 'Y', 'R', 'G', 'B']
    
    # Centered labels between Col 1 & 2 (at X = 13.66 mm)
    mid_in = (col1_x + col2_x) / 2.0
    for r in range(5):
        jy = jack_rows[r]
        svg.append(render_text(ddin_cond_bold, jack_labels[r], mid_in, jy - 5.00, 8.0, label_color))
        # Col 5 Output row labels
        svg.append(render_text(ddin_cond_bold, jack_labels[r], col5_x, jy - 5.00, 8.0, label_color))
        
    # Column 4: Vertical Crossfader & CV Jack
    # Fader Slot: Center X = 81.28, Y from 22.00 to 92.00 (Length 70mm, Width 2.4mm)
    slot_x = col4_x - 1.20
    svg.append(f'  <!-- Fader Slot Graphic -->')
    svg.append(f'  <rect x="{slot_x:.2f}" y="22.00" width="2.40" height="70.00" rx="1.2" ry="1.2" fill="#181818" stroke="{border_color}" stroke-width="0.3" />')
    # Fader Scale Ticks along Col 4
    for t in range(11):
        ty = 22.00 + t * 7.00
        svg.append(f'  <line x1="{col4_x - 4.50:.2f}" y1="{ty:.2f}" x2="{col4_x - 2.20:.2f}" y2="{ty:.2f}" stroke="{border_color}" stroke-width="0.3" />')
        svg.append(f'  <line x1="{col4_x + 2.20:.2f}" y1="{ty:.2f}" x2="{col4_x + 4.50:.2f}" y2="{ty:.2f}" stroke="{border_color}" stroke-width="0.3" />')
    
    # Fader End Labels
    svg.append(render_text(ddin_cond_bold, '1', col4_x - 6.50, 24.00, 7.5, label_color))
    svg.append(render_text(ddin_cond_bold, '2', col4_x - 6.50, 93.00, 7.5, label_color))
    
    # Fader CV Jack at Y = 117.00 mm
    svg.append(render_text(ddin_cond_bold, 'CV', col4_x, 117.00 - 5.00, 8.0, label_color))
    
    svg.append('</svg>')
    
    svg_str = '\n'.join(svg)
    out_file = skin['filename']
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(svg_str)
    print(f'Generated {out_file}')

if __name__ == '__main__':
    os.makedirs('res', exist_ok=True)
    os.makedirs('metamodule/assets', exist_ok=True)
    os.makedirs('scratch', exist_ok=True)
    
    skins = [
        {
            'name': 'Medium',
            'filename': 'res/Serqet.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'label_color': '#222222',
            'version_color': '#aaaaaa',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e',
            'border_color': '#444444',
            'scope_bezel': '#2b2b2b'
        },
        {
            'name': 'Light',
            'filename': 'res/Serqet_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'label_color': '#222222',
            'version_color': '#666666',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8',
            'border_color': '#999999',
            'scope_bezel': '#555555'
        },
        {
            'name': 'Dark',
            'filename': 'res/Serqet_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'label_color': '#ffffff',
            'version_color': '#888888',
            'hi_color': '#585858',
            'sh_color': '#202020',
            'border_color': '#222222',
            'scope_bezel': '#111111'
        }
    ]
    
    for s in skins:
        generate_serqet_panel(s)
        
    cairosvg.svg2png(url='res/Serqet.svg', write_to='metamodule/assets/Serqet.png', output_width=190, output_height=240)
    cairosvg.svg2png(url='res/Serqet.svg', write_to='scratch/serqet_verify.png', output_width=190 * 4, output_height=240 * 4)
    print('Rendered metamodule/assets/Serqet.png and scratch/serqet_verify.png successfully.')
