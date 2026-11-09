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

ddin_bold = TTFont('res/D-DIN-Bold.otf')
ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
ddin_cond = TTFont('res/D-DINCondensed.otf')
qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

def render_text(font, text, center_x, baseline_y, scale, fill, comment="", align="center"):
    text_str = str(text)
    if ('v' in text_str and '.' in text_str) or 'version' in comment.lower():
        use_font = qs_reg_font
        use_scale = 0.001600
    elif font == ddin_bold:
        use_font = ddin_bold
        use_scale = scale
    elif ('knob' in comment.lower() or 'header' in comment.lower() or text_str in ['TIME', 'LOW/MID', 'MID/HIGH', 'AMP', 'SMOOTH']) and baseline_y < 70.0:
        use_font = ddin_cond_bold
        use_scale = scale
    else:
        use_font = ddin_cond
        use_scale = scale
    
    cmap = use_font.getBestCmap()
    hmtx = use_font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * use_scale if ord(c) in cmap else 500 * use_scale for c in text_str]
    total_w = sum(widths)
    if align == "right":
        start_x = center_x - total_w
    elif align == "left":
        start_x = center_x
    else:
        start_x = center_x - total_w / 2.0
    res = []
    if comment:
        res.append(f'  <!-- Label: "{comment}" -->')
    curr = start_x
    for c, w in zip(text_str, widths):
        if c != ' ':
            pd = get_simplified_glyph_path(use_font, c)
            res.append(f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({use_scale:.6f}, {-use_scale:.6f})"><path d="{pd}" fill="{fill}"/></g>')
        curr += w
    return "\n".join(res)

def get_palette_badge_svg(module_name="Hecate"):
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
                        h = b.get("heightMm", 0.0)
                        c = b.get("color", "#5d5d5d")
                        lines.append(f'    <rect x="{start_x:.3f}" y="{curr_y:.3f}" width="{bw:.3f}" height="{h:.3f}" fill="{c}" stroke="none" />')
                        curr_y += h
                    lines.append('  </g>')
                    return "\n".join(lines)
        except Exception:
            pass
    return ('  <!-- Left Edge Color Badge (neutral placeholder) -->\n'
            '  <g id="palette-badge">\n'
            '    <rect x="0" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />\n'
            '  </g>')

def generate_hecate_svg(skin):
    panel_w = 81.280   # 16 HP
    panel_h = 128.500  # 3U
    mid_x = panel_w / 2.0 # 40.640 mm
    
    # 4 Columns: margins 7.000 mm, pitch 22.427 mm
    x0 = 7.000
    x1 = 29.427
    x2 = 51.853
    x3 = 74.280
    
    # Colors for current skin
    bg_color = skin['bg_color']
    title_color = skin['title_color']
    label_color = skin['label_color']
    line_color = skin['line_color']
    hi_color = skin['hi_color']
    sh_color = skin['sh_color']
    disp_border = skin['disp_border']
    disp_bg = skin['disp_bg']
    
    # Text scales
    s_title = 0.005644      # 16 pt
    s_version = 0.001600    # 4.5 pt
    s_knob = 0.003528       # 10 pt
    s_trim = 0.002822       # 8 pt
    s_jack = 0.002822       # 8 pt
    
    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.3f}mm" height="{panel_h:.3f}mm" viewBox="0 0 {panel_w:.3f} {panel_h:.3f}">')
    svg.append(f'  <!-- Background -->')
    svg.append(f'  <rect x="0" y="0" width="{panel_w:.3f}" height="{panel_h:.3f}" fill="{bg_color}" stroke="none"/>')
    svg.append(f'')
    svg.append(get_palette_badge_svg("Hecate"))
    svg.append(f'')
    
    # ZONE 1: HEADER & TITLE
    svg.append(f'  <!-- ZONE 1: HEADER & TITLE -->')
    svg.append(render_text(ddin_bold, "HECATE", mid_x, 8.000, s_title, title_color, "HECATE Title"))
    svg.append(render_text(qs_reg_font, "v1.0.0", panel_w - 2.150, 8.000, s_version, title_color, "v1.0.0", align="right"))
    svg.append(f'')
    
    # ZONE 2: 3-BAND DISPLAY FRAME (spanning Columns 1, 2, 3)
    # Display box: X = 18.000 to 76.500 (w = 58.500), Y = 13.500 to 39.500 (h = 26.000)
    disp_x = 18.000
    disp_y = 13.500
    disp_w = 58.500
    disp_h = 26.000
    disp_rx = 1.500
    svg.append(f'  <!-- ZONE 2: 3-BAND DISPLAY BEZEL & SCREEN AREA -->')
    svg.append(f'  <rect x="{disp_x:.3f}" y="{disp_y:.3f}" width="{disp_w:.3f}" height="{disp_h:.3f}" rx="{disp_rx:.3f}" fill="{disp_bg}" stroke="{disp_border}" stroke-width="0.350"/>')
    svg.append(f'')
    
    # Column 0 Knobs beside display:
    # TIME knob at Y = 18.000
    svg.append(render_text(ddin_cond_bold, "TIME", x0, 12.250, s_knob, label_color, "TIME knob"))
    # LOW/MID knob at Y = 34.000
    svg.append(render_text(ddin_cond_bold, "LOW/MID", x0, 28.250, s_knob, label_color, "LOW/MID knob"))
    svg.append(f'')
    
    # ZONE 3: PRIMARY KNOB ROW 1 (Y = 50.000 mm)
    # Col 0: MID/HIGH knob
    svg.append(render_text(ddin_cond_bold, "MID/HIGH", x0, 44.250, s_knob, label_color, "MID/HIGH knob"))
    # Cols 1, 2, 3: AMP knobs
    svg.append(render_text(ddin_cond_bold, "LOW AMP", x1, 44.250, s_knob, label_color, "LOW AMP knob"))
    svg.append(render_text(ddin_cond_bold, "MID AMP", x2, 44.250, s_knob, label_color, "MID AMP knob"))
    svg.append(render_text(ddin_cond_bold, "HIGH AMP", x3, 44.250, s_knob, label_color, "HIGH AMP knob"))
    svg.append(f'')
    
    # PRIMARY KNOB ROW 2 (Y = 65.000 mm)
    # Col 0: AUDIO TRIM trimpot
    svg.append(render_text(ddin_cond, "TRIM", x0, 60.500, s_trim, label_color, "AUDIO TRIM trimpot"))
    # Cols 1, 2, 3: SMOOTH knobs
    svg.append(render_text(ddin_cond_bold, "SMOOTH", x1, 59.250, s_knob, label_color, "LOW SMOOTH knob"))
    svg.append(render_text(ddin_cond_bold, "SMOOTH", x2, 59.250, s_knob, label_color, "MID SMOOTH knob"))
    svg.append(render_text(ddin_cond_bold, "SMOOTH", x3, 59.250, s_knob, label_color, "HIGH SMOOTH knob"))
    svg.append(f'')
    
    # DELINEATOR LINE 1 (Y = 70.500 mm)
    delin1_y = 70.500
    delin_x_start = panel_w * 0.20 # 16.256 mm
    svg.append(f'  <!-- Delineator Line 1 -->')
    svg.append(f'  <line x1="{delin_x_start:.3f}" y1="{delin1_y:.3f}" x2="{panel_w:.3f}" y2="{delin1_y:.3f}" stroke="{hi_color}" stroke-width="0.150"/>')
    svg.append(f'  <line x1="{delin_x_start:.3f}" y1="{delin1_y + 0.150:.3f}" x2="{panel_w:.3f}" y2="{delin1_y + 0.150:.3f}" stroke="{sh_color}" stroke-width="0.150"/>')
    svg.append(f'')
    
    # ZONE 4: CV ATTENUVERTERS (TRIMPOTS)
    # Row 1 at Y = 76.000 mm
    svg.append(render_text(ddin_cond, "L/M CV", x0, 71.500, s_trim, label_color, "LOW/MID CV depth trimpot"))
    svg.append(render_text(ddin_cond, "AMP CV", x1, 71.500, s_trim, label_color, "LOW AMP CV depth trimpot"))
    svg.append(render_text(ddin_cond, "AMP CV", x2, 71.500, s_trim, label_color, "MID AMP CV depth trimpot"))
    svg.append(render_text(ddin_cond, "AMP CV", x3, 71.500, s_trim, label_color, "HIGH AMP CV depth trimpot"))
    svg.append(f'')
    
    # Row 2 at Y = 86.000 mm
    svg.append(render_text(ddin_cond, "M/H CV", x0, 81.500, s_trim, label_color, "MID/HIGH CV depth trimpot"))
    svg.append(render_text(ddin_cond, "SM CV", x1, 81.500, s_trim, label_color, "LOW SMOOTH CV depth trimpot"))
    svg.append(render_text(ddin_cond, "SM CV", x2, 81.500, s_trim, label_color, "MID SMOOTH CV depth trimpot"))
    svg.append(render_text(ddin_cond, "SM CV", x3, 81.500, s_trim, label_color, "HIGH SMOOTH CV depth trimpot"))
    svg.append(f'')
    
    # DELINEATOR LINE 2 (Y = 91.000 mm)
    delin2_y = 91.000
    svg.append(f'  <!-- Delineator Line 2 -->')
    svg.append(f'  <line x1="{delin_x_start:.3f}" y1="{delin2_y:.3f}" x2="{panel_w:.3f}" y2="{delin2_y:.3f}" stroke="{hi_color}" stroke-width="0.150"/>')
    svg.append(f'  <line x1="{delin_x_start:.3f}" y1="{delin2_y + 0.150:.3f}" x2="{panel_w:.3f}" y2="{delin2_y + 0.150:.3f}" stroke="{sh_color}" stroke-width="0.150"/>')
    svg.append(f'')
    
    # ZONE 5: JACKS (3 ROWS)
    # Jack Row 1 at Y = 96.000 mm
    svg.append(render_text(ddin_cond, "AUDIO", x0, 91.000, s_jack, label_color, "AUDIO in jack"))
    svg.append(render_text(ddin_cond, "AMP", x1, 91.000, s_jack, label_color, "LOW AMP CV jack"))
    svg.append(render_text(ddin_cond, "AMP", x2, 91.000, s_jack, label_color, "MID AMP CV jack"))
    svg.append(render_text(ddin_cond, "AMP", x3, 91.000, s_jack, label_color, "HIGH AMP CV jack"))
    svg.append(f'')
    
    # Jack Row 2 at Y = 106.500 mm
    svg.append(render_text(ddin_cond, "L/M", x0, 101.500, s_jack, label_color, "LOW/MID CV jack"))
    svg.append(render_text(ddin_cond, "SMOOTH", x1, 101.500, s_jack, label_color, "LOW SMOOTH CV jack"))
    svg.append(render_text(ddin_cond, "SMOOTH", x2, 101.500, s_jack, label_color, "MID SMOOTH CV jack"))
    svg.append(render_text(ddin_cond, "SMOOTH", x3, 101.500, s_jack, label_color, "HIGH SMOOTH CV jack"))
    svg.append(f'')
    
    # Jack Row 3 at Y = 117.000 mm (Bottom Outputs & M/H CV)
    svg.append(render_text(ddin_cond, "M/H", x0, 112.000, s_jack, label_color, "MID/HIGH CV jack"))
    svg.append(render_text(ddin_cond_bold, "LOW", x1, 112.000, s_jack, label_color, "LOW OUT jack"))
    svg.append(render_text(ddin_cond_bold, "MID", x2, 112.000, s_jack, label_color, "MID OUT jack"))
    svg.append(render_text(ddin_cond_bold, "HIGH", x3, 112.000, s_jack, label_color, "HIGH OUT jack"))
    svg.append(f'')
    
    svg.append('</svg>')
    return "\n".join(svg)

def render_metamodule_asset(svg_path, out_png_path):
    # MetaModule standard bitmap: height 240 px, width proportional (16 HP = 152 px)
    target_h = 240
    target_w = int(round(81.280 / 128.500 * 240)) # 152 px
    cairosvg.svg2png(url=svg_path, write_to=out_png_path, output_width=target_w, output_height=target_h)

def main():
    skins = [
        {
            'name': 'Medium',
            'filename': 'res/Hecate.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e',
            'disp_border': '#333333',
            'disp_bg': '#141416'
        },
        {
            'name': 'Light',
            'filename': 'res/Hecate_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'label_color': '#222222',
            'line_color': '#222222',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8',
            'disp_border': '#888888',
            'disp_bg': '#18181c'
        },
        {
            'name': 'Dark',
            'filename': 'res/Hecate_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'label_color': '#ffffff',
            'line_color': '#ffffff',
            'hi_color': '#585858',
            'sh_color': '#202020',
            'disp_border': '#222222',
            'disp_bg': '#0f0f12'
        }
    ]
    
    os.makedirs('res', exist_ok=True)
    os.makedirs('metamodule/assets', exist_ok=True)
    os.makedirs('scratch', exist_ok=True)
    
    for skin in skins:
        svg_content = generate_hecate_svg(skin)
        with open(skin['filename'], 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Generated {skin['filename']}")
        
    render_metamodule_asset('res/Hecate.svg', 'metamodule/assets/Hecate.png')
    render_metamodule_asset('res/Hecate.svg', 'scratch/hecate_verify.png')
    print("Rendered metamodule/assets/Hecate.png and scratch/hecate_verify.png successfully.")

if __name__ == '__main__':
    main()
