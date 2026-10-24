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

music_font = TTFont('res/NotoMusic-Regular.ttf')
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

def render_note_label(cp, x_center, y_baseline, scale=0.0028, dotted=False, triplet=False, color='#222222'):
    cmap = music_font.getBestCmap()
    gset = music_font.getGlyphSet()
    gname = cmap[cp]
    path = pathops.Path()
    pen = pathops.PathPen(path)
    gset[gname].draw(pen)
    simplified = pathops.simplify(path)
    svg_pen = SVGPathPen(None)
    simplified.draw(svg_pen)
    d = svg_pen.getCommands()
    d_clean = clean_svg_path_numbers(d)
    
    bounds = path.bounds
    glyph_w = (bounds[2] - bounds[0]) * scale
    glyph_h = (bounds[3] - bounds[1]) * scale
    
    elements = []
    
    if triplet:
        # Prepend a '3' in front of the note glyph
        t_scale = (7.5 * 25.4) / (72.0 * ddin_cond_bold['head'].unitsPerEm)
        t_cmap = ddin_cond_bold.getBestCmap()
        t_w = ddin_cond_bold['hmtx'][t_cmap[ord('3')]][0] * t_scale
        gap = 0.5
        total_w = t_w + gap + glyph_w
        
        start_x = x_center - total_w / 2.0
        # Direct render '3'
        t_svg = render_text(ddin_cond_bold, '3', start_x + t_w / 2.0, y_baseline, 7.5, color)
        elements.append(t_svg)
        
        note_x = start_x + t_w + gap
        elements.append(f'<path d="{d_clean}" fill="{color}" transform="translate({note_x:.3f},{y_baseline:.3f}) scale({scale:.6f},{-scale:.6f})" />')
    elif dotted:
        dot_r = 0.35
        gap = 0.6
        total_w = glyph_w + gap + dot_r * 2.0
        start_x = x_center - total_w / 2.0
        elements.append(f'<path d="{d_clean}" fill="{color}" transform="translate({start_x:.3f},{y_baseline:.3f}) scale({scale:.6f},{-scale:.6f})" />')
        dot_x = start_x + glyph_w + gap + dot_r
        dot_y = y_baseline - 0.5
        elements.append(f'<circle cx="{dot_x:.3f}" cy="{dot_y:.3f}" r="{dot_r:.3f}" fill="{color}" />')
    else:
        start_x = x_center - glyph_w / 2.0
        elements.append(f'<path d="{d_clean}" fill="{color}" transform="translate({start_x:.3f},{y_baseline:.3f}) scale({scale:.6f},{-scale:.6f})" />')
        
    return ''.join(elements)

def generate_panel(skin_dict):
    W = 40.64
    H = 128.50
    col0 = 11.50
    col1 = 20.32
    col2 = 29.14
    
    # 7 rows of jacks with 13.0mm universal Garmire pitch, bottom anchor Y = 117.00mm
    bottom_y = 117.00
    pitch = 13.00
    jack_rows = [bottom_y - (6 - i) * pitch for i in range(7)]
    
    bg_color = skin_dict['bg_color']
    title_color = skin_dict['title_color']
    label_color = skin_dict['label_color']
    hi_color = skin_dict['hi_color']
    sh_color = skin_dict['sh_color']
    svg_filename = skin_dict['filename']
    
    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H:.1f}mm" viewBox="0 0 {W:.2f} {H:.1f}">')
    svg.append(f'  <rect width="{W:.2f}" height="{H:.1f}" fill="{bg_color}" />')
    
    # Palette badge (flush left, 2.54mm wide, 88.9mm tall, centered at Y = 64.25mm)
    svg.append('  <g id="palette-badge">')
    svg.append('    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />')
    svg.append('  </g>')
    
    # Title & Version (v1.0.1)
    svg.append(render_text(ddin_bold, 'BEATRICE', W / 2.0, 6.8, 14.0, title_color))
    svg.append(render_text(quicksand, 'v1.0.1', W - 2.15, 6.8, 4.5, '#aaaaaa'))
    
    # LCD Display Frame (34.00 x 8.50 mm, centered vertically at Y = 15.00mm, spanning 10.75 - 19.25mm)
    lcd_y = 10.75
    lcd_w = 34.00
    lcd_h = 8.50
    lcd_x = (W - lcd_w) / 2.0
    svg.append(f'  <rect x="{lcd_x:.3f}" y="{lcd_y:.3f}" width="{lcd_w:.3f}" height="{lcd_h:.3f}" rx="1.2" fill="#101412" stroke="#2c3530" stroke-width="0.3" />')
    
    # Up/down arrows in LCD
    # left arrows (integer BPM)
    svg.append(f'  <path d="M 5.5 {lcd_y+3.0} L 6.5 {lcd_y+1.4} L 7.5 {lcd_y+3.0} Z" fill="#38f870" />')
    svg.append(f'  <path d="M 5.5 {lcd_y+5.5} L 6.5 {lcd_y+7.1} L 7.5 {lcd_y+5.5} Z" fill="#38f870" />')
    # right arrows (decimal BPM)
    svg.append(f'  <path d="M {W-7.5} {lcd_y+3.0} L {W-6.5} {lcd_y+1.4} L {W-5.5} {lcd_y+3.0} Z" fill="#38f870" />')
    svg.append(f'  <path d="M {W-7.5} {lcd_y+5.5} L {W-6.5} {lcd_y+7.1} L {W-5.5} {lcd_y+5.5} Z" fill="#38f870" />')
    
    # Controls row: BPM Knob, Start button, Reset button at Y = 26.50mm
    ctrl_y = 26.50
    
    # BPM Knob label
    svg.append(render_text(ddin_cond_bold, 'BPM', col0, ctrl_y - 5.75, 8.0, label_color))
    # Start button label
    svg.append(render_text(ddin_cond_bold, 'START', col1, ctrl_y - 4.50, 7.5, label_color))
    # Reset button label
    svg.append(render_text(ddin_cond_bold, 'RESET', col2, ctrl_y - 4.50, 7.5, label_color))
    
    # Delineator line above Row 1 jacks (10.0mm above Row 1 center: 39.0 - 10.0 = 29.0mm)
    # Span 80% of width: x_start = 0.20 * W = 8.128mm, x_end = W
    delineator_y = jack_rows[0] - 7.0 # 32.00mm, cleanly between controls and jacks
    delin_x_start = 0.20 * W
    delin_w = W - delin_x_start
    svg.append(f'  <rect x="{delin_x_start:.3f}" y="{delineator_y:.3f}" width="{delin_w:.3f}" height="0.150" fill="{hi_color}" />')
    svg.append(f'  <rect x="{delin_x_start:.3f}" y="{delineator_y + 0.150:.3f}" width="{delin_w:.3f}" height="0.150" fill="{sh_color}" />')
    
    # Row 1 Jack labels (BPM CV, START, RESET) at Y = jack_rows[0] - 5.0mm = 34.00mm
    r1_y = jack_rows[0]
    svg.append(render_text(ddin_cond_bold, 'BPM', col0, r1_y - 5.0, 7.5, label_color))
    svg.append(render_text(ddin_cond_bold, 'START', col1, r1_y - 5.0, 7.5, label_color))
    svg.append(render_text(ddin_cond_bold, 'RESET', col2, r1_y - 5.0, 7.5, label_color))
    
    # Division Rows (Rows 2 to 7)
    notes_cp = [0x1D15D, 0x1D15E, 0x1D15F, 0x1D160, 0x1D161, 0x1D162]
    
    for row_idx in range(6):
        jy = jack_rows[row_idx + 1]
        cp = notes_cp[row_idx]
        
        # Col 0: Straight note
        svg.append(render_note_label(cp, col0, jy - 5.0, scale=0.0028, dotted=False, triplet=False, color=label_color))
        # Col 1: Dotted note
        svg.append(render_note_label(cp, col1, jy - 5.0, scale=0.0028, dotted=True, triplet=False, color=label_color))
        # Col 2: Triplet note with '3' in front of symbol
        svg.append(render_note_label(cp, col2, jy - 5.0, scale=0.0028, dotted=False, triplet=True, color=label_color))
        
    svg.append('</svg>')
    
    svg_str = '\n'.join(svg)
    with open(svg_filename, 'w', encoding='utf-8') as f:
        f.write(svg_str)
    print(f'Generated {svg_filename}')

skins = [
    {
        'name': 'Medium',
        'filename': 'res/Beatrice.svg',
        'bg_color': '#6e6e6e',
        'title_color': '#ffffff',
        'label_color': '#222222',
        'hi_color': '#999999',
        'sh_color': '#5e5e5e'
    },
    {
        'name': 'Light',
        'filename': 'res/Beatrice_Light.svg',
        'bg_color': '#c6c6c6',
        'title_color': '#333333',
        'label_color': '#222222',
        'hi_color': '#e4e4e4',
        'sh_color': '#a8a8a8'
    },
    {
        'name': 'Dark',
        'filename': 'res/Beatrice_Dark.svg',
        'bg_color': '#333333',
        'title_color': '#c6c6c6',
        'label_color': '#ffffff',
        'hi_color': '#585858',
        'sh_color': '#202020'
    }
]

for s in skins:
    generate_panel(s)

# Render MetaModule bitmap (76 x 240) and verification image
cairosvg.svg2png(url='res/Beatrice.svg', write_to='metamodule/assets/Beatrice.png', output_width=76, output_height=240)
cairosvg.svg2png(url='res/Beatrice.svg', write_to='scratch/beatrice_verify.png', output_width=76 * 4, output_height=240 * 4)
print('Rendered metamodule/assets/Beatrice.png and scratch/beatrice_verify.png')
