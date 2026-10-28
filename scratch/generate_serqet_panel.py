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
    panel_hp = 28
    panel_w = panel_hp * 5.08  # 142.24 mm
    panel_h = 128.50           # Eurorack 3U
    
    mid_x = panel_w / 2.0      # 71.12 mm
    
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
    
    # ── ZONE 1: HEADER & TITLE ──
    # Module Title: Fixed Y = 8.000 mm (D-DIN-Bold 16 pt, centered)
    svg.append(render_text(ddin_bold, 'SERQET', mid_x, 8.000, 16.0, title_color))
    # Version Tag: Fixed Y = 8.000 mm (Quicksand-Regular 4.5 pt, X = W - 2.15 mm)
    svg.append(render_text(quicksand, 'v1.4.1', panel_w - 2.15, 8.000, 4.5, version_color))
    
    # ── ZONE 2: SIDE-BY-SIDE DUAL CRT SCOPES ──
    # Scope 1 (CUE MONITOR): 66 x 66 mm, 1.5 mm corner radius, Center (37.00, 43.50)
    cue_w, cue_h = 66.00, 66.00
    cue_x, cue_y = 37.00 - cue_w / 2.0, 43.50 - cue_h / 2.0  # (4.00, 10.50)
    svg.append('  <!-- Scope 1: Cue Monitor Bezel -->')
    svg.append(f'  <rect x="{cue_x:.2f}" y="{cue_y:.2f}" width="{cue_w:.2f}" height="{cue_h:.2f}" rx="1.5" ry="1.5" fill="#111111" stroke="{scope_bezel}" stroke-width="0.8" />')
    
    # Scope 2 (MAIN OUT): 66 x 66 mm, 1.5 mm corner radius, Center (105.24, 43.50)
    main_w, main_h = 66.00, 66.00
    main_x, main_y = 105.24 - main_w / 2.0, 43.50 - main_h / 2.0  # (72.24, 10.50)
    svg.append('  <!-- Scope 2: Main Out Bezel -->')
    svg.append(f'  <rect x="{main_x:.2f}" y="{main_y:.2f}" width="{main_w:.2f}" height="{main_h:.2f}" rx="1.5" ry="1.5" fill="#111111" stroke="{scope_bezel}" stroke-width="0.8" />')
    
    # ── CUE BUTTONS & SCOPE LABELS (Y = 80.00 mm, Y baseline = 81.75 mm) ──
    # CH 1 cue button: Center X: 7, Center Y: 80
    # CH 2 cue button: Center X: 67, Center Y: 80
    lbl_y = 81.75
    
    svg.append(render_text(ddin_cond_bold, 'CH 1', 13.00, lbl_y, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'CUE MONITOR', 37.00, lbl_y, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'CH 2', 61.00, lbl_y, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'MAIN OUT', 105.24, lbl_y, 8.0, label_color))
    
    # ── HORIZONTAL CROSSFADER SLIDER ──
    # Width: 128.24 mm, Center X: 71.12, Center Y: 88.50
    # Slot from X = 7.00 to 135.24 mm
    fader_w = 128.24
    fader_cx = 71.12
    fader_cy = 88.50
    slot_x0 = fader_cx - fader_w / 2.0  # 7.00 mm
    slot_x1 = fader_cx + fader_w / 2.0  # 135.24 mm
    
    svg.append('  <!-- Horizontal Fader Slot Graphic -->')
    svg.append(f'  <rect x="{slot_x0:.2f}" y="{fader_cy - 1.20:.2f}" width="{fader_w:.2f}" height="2.40" rx="1.2" ry="1.2" fill="#181818" stroke="{border_color}" stroke-width="0.3" />')
    
    # Crossfader scale hash marks: alternating full height (3.0mm) and 2/3 height (2.0mm)
    # Extremities thicker than the rest. White for Medium/Dark, border_color for Light.
    fader_hash_color = '#ffffff' if skin['name'] in ['Medium', 'Dark'] else skin['border_color']
    num_ticks = 21  # 20 intervals of 6.412 mm across 128.24 mm
    tick_step = fader_w / (num_ticks - 1)  # 6.412 mm
    for k in range(num_ticks):
        tx = slot_x0 + k * tick_step
        is_extremity = (k == 0 or k == num_ticks - 1)
        is_full_height = (k % 2 == 0)
        sw = 0.65 if is_extremity else 0.30
        h_tick = 3.00 if is_full_height else 2.00
        
        # Above slot
        svg.append(f'  <line x1="{tx:.3f}" y1="{fader_cy - 1.80 - h_tick:.2f}" x2="{tx:.3f}" y2="{fader_cy - 1.80:.2f}" stroke="{fader_hash_color}" stroke-width="{sw:.2f}" />')
        # Below slot
        svg.append(f'  <line x1="{tx:.3f}" y1="{fader_cy + 1.80:.2f}" x2="{tx:.3f}" y2="{fader_cy + 1.80 + h_tick:.2f}" stroke="{fader_hash_color}" stroke-width="{sw:.2f}" />')
    
    # Delineator Line separating Fader from Knobs/Jacks (Y = 94.00 mm, 10mm above row at 104mm)
    svg.append(f'  <line x1="4.00" y1="93.92" x2="{panel_w - 4.00:.2f}" y2="93.92" stroke="{hi_color}" stroke-width="0.15" />')
    svg.append(f'  <line x1="4.00" y1="94.08" x2="{panel_w - 4.00:.2f}" y2="94.08" stroke="{sh_color}" stroke-width="0.15" />')
    
    # ── LABELS ABOVE ROW AT Y = 104 mm (Y baseline: 99.00 mm) ──
    # CH 1 (label) Center X: 32.648
    svg.append(render_text(ddin_cond_bold, 'CH 1', 32.648, 99.00, 8.0, label_color))
    # GRATICULE (label) Center X: 77.532, Y baseline: 99.00 (no CUE or MAIN sub-labels)
    svg.append(render_text(ddin_cond_bold, 'GRATICULE', 77.532, 99.00, 8.0, label_color))
    
    # CRT Engine Control Labels
    svg.append(render_text(ddin_cond_bold, 'FOCUS', 96.768, 99.00, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'BEAM', 109.592, 99.00, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'DECAY', 122.416, 99.00, 8.0, label_color))
    svg.append(render_text(ddin_cond_bold, 'BLOOM', 135.240, 99.00, 8.0, label_color))
    
    # ── LABELS AT Y BASELINE: 112.00 mm ──
    # CH 1 & CH 2 Input Column Labels: X - 7, Y - 19.824, R - 32.648, G - 45.472, B - 58.296
    ch_labels = ['X', 'Y', 'R', 'G', 'B']
    ch1_x_coords = [7.000, 19.824, 32.648, 45.472, 58.296]
    for lbl, x in zip(ch_labels, ch1_x_coords):
        svg.append(render_text(ddin_cond_bold, lbl, x, 112.00, 8.0, label_color))
        
    # XFADE CV label at 71.12
    svg.append(render_text(ddin_cond_bold, 'XFADE', 71.120, 112.00, 8.0, label_color))
    
    # Output Column Labels: X - 83.944, Y - 96.768, R - 109.592, G - 122.416, B - 135.24
    out_x_coords = [83.944, 96.768, 109.592, 122.416, 135.240]
    for lbl, x in zip(ch_labels, out_x_coords):
        svg.append(render_text(ddin_cond_bold, lbl, x, 112.00, 8.0, label_color))
        
    # ── BOTTOM LABELS AT Y BASELINE: 125.50 mm ──
    # CH 2 (label) Center X: 32.648
    svg.append(render_text(ddin_cond_bold, 'CH 2', 32.648, 125.50, 8.0, label_color))
    # MAIN OUT (label) under the output columns (Center = 109.592)
    svg.append(render_text(ddin_cond_bold, 'MAIN OUT', 109.592, 125.50, 8.0, label_color))
    
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
        
    cairosvg.svg2png(url='res/Serqet.svg', write_to='metamodule/assets/Serqet.png', output_width=266, output_height=240)
    cairosvg.svg2png(url='res/Serqet.svg', write_to='scratch/serqet_verify.png', output_width=266 * 4, output_height=240 * 4)
    print('Rendered metamodule/assets/Serqet.png and scratch/serqet_verify.png successfully.')
