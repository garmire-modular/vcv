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

ddin_bold = TTFont('res/D-DIN-Bold.otf')
ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
ddin_cond = TTFont('res/D-DINCondensed.otf')
qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

def render_qs_text(font, text, center_x, baseline_y, scale, fill, comment="", align="center"):
    text_str = str(text)
    if ('v' in text_str and '.' in text_str) or 'version' in comment.lower():
        use_font = qs_reg_font
        use_scale = 0.001600
    elif font == ddin_bold:
        use_font = ddin_bold
        use_scale = scale
    elif ('knob' in comment.lower() or 'header' in comment.lower() or text_str in ['FREQ', 'PHASE', 'AMP', 'TIME', 'WIDTH', 'SLEW', 'SHAPE']) and baseline_y < 50.0:
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

def get_palette_badge_svg(module_name="Joy"):
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
                        bh = b.get("heightMm", 0.0)
                        hex_col = b.get("hex", "#5d5d5d")
                        lines.append(f'    <rect x="{start_x:.3f}" y="{curr_y:.3f}" width="{bw:.3f}" height="{bh:.3f}" fill="{hex_col}" stroke="none"/>')
                        curr_y += bh
                    lines.append('  </g>')
                    return "\n".join(lines)
        except Exception as e:
            print(f"Warning: could not load palette.json: {e}")
    return '''  <!-- Left Edge Color Badge (Placeholder #5d5d5d) -->
  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>
  </g>'''

def render_section_divider(center_y, panel_w, hi_color, sh_color):
    divider_w = panel_w * 0.80
    x_start = panel_w - divider_w
    top_y = center_y - 0.150
    bot_y = center_y

    return f'''  <!-- Section Divider at Center Y = {center_y:.2f}mm (Width = {divider_w:.3f}mm, Right-Aligned, Total Height = 0.300mm) -->
  <g>
    <rect x="{x_start:.3f}" y="{top_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{hi_color}"/>
    <rect x="{x_start:.3f}" y="{bot_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{sh_color}"/>
  </g>'''

def main():
    panel_w = 60.96  # 12 HP = 12 * 5.08 mm
    panel_h = 128.50 # Standard Eurorack 3U height
    col_knob_x = [7.62, 22.86, 38.10, 53.34]  # 4 columns for Knobs & Trims (15.24 mm pitch)
    col_jack_x = [7.62, 19.05, 30.48, 41.91, 53.34]  # 5 columns for Jacks (11.43 mm pitch)

    # Title "JOY" in D-DIN-Bold (16pt, scale 0.005644, baseline 7.620)
    title_scale = 16.0 * 25.4 / (72.0 * 1000.0)
    joy_title_block = [
        '  <!-- Label: "JOY" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "JOY", panel_w / 2.0, 7.620, title_scale, "#ffffff", "JOY")
    ]

    version_block = [
        '  <!-- Label: "v1.4.0" -->',
        render_qs_text(qs_reg_font, "v1.4.0", panel_w - 2.15, 8.000, 0.001600, "#aaaaaa", "v1.4.0", align="right")
    ]

    badge_svg = get_palette_badge_svg("Joy")

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 12 HP (Medium Skin) -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        badge_svg,
        '',
        render_section_divider(51.50, panel_w, "#999999", "#5e5e5e"),
        '',
        render_section_divider(74.50, panel_w, "#999999", "#5e5e5e"),
        '',
        '  <!-- Range Button Indicator Ring Track (R = 4.12mm, Stroke = 1.40mm) -->',
        f'  <circle cx="{col_knob_x[1]:.3f}" cy="18.500" r="4.120" fill="none" stroke="#3a3a3a" stroke-width="1.400"/>',
        ''
    ]
    svg_parts.extend(joy_title_block)
    svg_parts.extend(version_block)

    # ── Zone 1 & 2: Main Parameter Knobs (3 Rows of 4 Knobs) ──
    # Row 1 Labels (Knob center Y = 18.50 mm, Label Y = 12.20 mm, Scale 0.0024, Color #1c1c1c)
    row1_labels = ["FREQ", "RANGE", "FINE", "HEIGHT"]
    for i, lbl in enumerate(row1_labels):
        svg_parts.append(render_qs_text(ddin_cond_bold, lbl, col_knob_x[i], 12.200, 0.002400, "#1c1c1c", f"Row 1: {lbl}"))

    # Row 2 Labels: Dynamics & Engine (Knob center Y = 32.00 mm, Label Y = 25.70 mm)
    row2_labels = ["SMOOTH", "REFRESH", "LINES", "DECAY"]
    for i, lbl in enumerate(row2_labels):
        svg_parts.append(render_qs_text(ddin_cond_bold, lbl, col_knob_x[i], 25.700, 0.002400, "#1c1c1c", f"Row 2: {lbl}"))

    # Row 3 Labels: Transformations & Shading (Knob center Y = 45.50 mm, Label Y = 39.20 mm)
    row3_labels = ["WINDOW", "TILT", "OCCLUSION", "ZFADE"]
    for i, lbl in enumerate(row3_labels):
        svg_parts.append(render_qs_text(ddin_cond_bold, lbl, col_knob_x[i], 39.200, 0.002200, "#1c1c1c", f"Row 3: {lbl}"))

    # ── Zone 3: CV Attenuverter Trimpots (2 Rows of 4 Trimpots) ──
    # Trim Row 1: FREQ, AUDIO, HEIGHT, SMOOTH (Center Y = 58.00 mm, Label Y = 54.00 mm, Scale 0.0014)
    trim1_labels = ["FREQ", "AUDIO", "HEIGHT", "SMOOTH"]
    for i, lbl in enumerate(trim1_labels):
        svg_parts.append(render_qs_text(ddin_cond, lbl, col_knob_x[i], 54.000, 0.001400, "#2c2c2c", f"Trim 1: {lbl}"))

    # Trim Row 2: LINES, DECAY, WINDOW, TILT (Center Y = 68.00 mm, Label Y = 64.00 mm, Scale 0.0014)
    trim2_labels = ["LINES", "DECAY", "WINDOW", "TILT"]
    for i, lbl in enumerate(trim2_labels):
        svg_parts.append(render_qs_text(ddin_cond, lbl, col_knob_x[i], 64.000, 0.001400, "#2c2c2c", f"Trim 2: {lbl}"))

    # ── Zone 4: I/O Jacks (3 Rows of 5 Jacks) ──
    # Jack Row 1: SYNC, FREQ, AUDIO, LOCK, HEIGHT (Center Y = 85.00 mm, Label Y = 80.00 mm, Scale 0.0013)
    jack1_labels = ["SYNC", "FREQ", "AUDIO", "LOCK", "HEIGHT"]
    for i, lbl in enumerate(jack1_labels):
        svg_parts.append(render_qs_text(ddin_cond, lbl, col_jack_x[i], 80.000, 0.001300, "#2c2c2c", f"Jack 1: {lbl}"))

    # Jack Row 2: SMOOTH, LINES, DECAY, WINDOW, TILT (Center Y = 98.00 mm, Label Y = 93.00 mm, Scale 0.0013)
    jack2_labels = ["SMOOTH", "LINES", "DECAY", "WINDOW", "TILT"]
    for i, lbl in enumerate(jack2_labels):
        svg_parts.append(render_qs_text(ddin_cond, lbl, col_jack_x[i], 93.000, 0.001300, "#2c2c2c", f"Jack 2: {lbl}"))

    # Jack Row 3: ZFADE, X, Y, Z, SYNC (Center Y = 112.00 mm, Label Y = 107.00 mm, Scale 0.0014)
    jack3_labels = ["ZFADE", "X", "Y", "Z", "SYNC"]
    for i, lbl in enumerate(jack3_labels):
        scale = 0.001600 if lbl in ["X", "Y", "Z"] else 0.001300
        svg_parts.append(render_qs_text(ddin_cond, lbl, col_jack_x[i], 107.000, scale, "#2c2c2c", f"Jack 3: {lbl}"))

    # Centered IN / OUT port subheaders (below jacks at Y = 119.50 mm)
    svg_parts.append(render_qs_text(ddin_cond, "IN", col_jack_x[0], 119.500, 0.001400, "#2c2c2c", "SYNC IN Sub"))
    svg_parts.append(render_qs_text(ddin_cond, "OUT", (col_jack_x[1] + col_jack_x[3]) / 2.0, 119.500, 0.001600, "#2c2c2c", "OUT Sub"))
    svg_parts.append(render_qs_text(ddin_cond, "OUT", col_jack_x[4], 119.500, 0.001400, "#2c2c2c", "SYNC OUT Sub"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Joy.svg'
    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('Joy', "\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (12 HP).")

    # Render verification bitmap (114 x 240 at 4x resolution = 456 x 960)
    verify_png = 'scratch/joy_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=114 * 4, output_height=240 * 4)

    # Render MetaModule asset (114 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to='metamodule/assets/Joy.png', output_width=114, output_height=240)
    print("Rendered metamodule/assets/Joy.png successfully.")

    # Draw hardware component overlay for clearance verification
    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (114 * 4) / panel_w

    # Knobs at Y = [18.50, 32.00, 45.50]
    kr = 4.75 * r_px
    for i, ky in enumerate([18.50, 32.00, 45.50]):
        for j, kx in enumerate(col_knob_x):
            if i == 0 and j == 1:
                # Range Button with illuminated ring (R = 4.12 mm, W = 1.40 mm)
                btn_r = 2.60 * r_px
                draw.ellipse([kx * r_px - btn_r, ky * r_px - btn_r,
                              kx * r_px + btn_r, ky * r_px + btn_r],
                             outline='#f0e442', width=2)
                ring_r = 4.12 * r_px
                draw.ellipse([kx * r_px - ring_r, ky * r_px - ring_r,
                              kx * r_px + ring_r, ky * r_px + ring_r],
                             outline='#d35fb7', width=2)
            else:
                draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Trimpots at Y = [58.00, 68.00]
    tr = 2.4 * r_px
    for ty in [58.00, 68.00]:
        for tx in col_knob_x:
            draw.ellipse([tx * r_px - tr, ty * r_px - tr, tx * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks at Y = [85.00, 98.00, 112.00]
    jr = 4.0 * r_px
    for jy in [85.00, 98.00, 112.00]:
        for jx in col_jack_x:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00aaff', width=2)

    im.save(verify_png)
    print("Saved verification image to scratch/joy_verify.png.")

if __name__ == '__main__':
    main()
