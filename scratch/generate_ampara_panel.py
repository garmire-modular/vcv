import os
import re
import sys

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

def render_qs_text(font, text, center_x, baseline_y, scale, fill, comment=""):
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

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    # Panel dimensions: 20 HP (101.60 mm x 128.5 mm)
    # MetaModule pixel width: 20 HP = 190 px (9.5 px/HP), Height = 240 px
    panel_w = 101.60
    panel_h = 128.50

    # Title "ampara" in Node.otf (scale 0.0048, baseline 7.620, centered in 101.60 mm)
    scale = 0.0048
    text = "ampara"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (panel_w - total_w) / 2.0

    ampara_title_block = ['  <!-- Label: "ampara" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            ampara_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.26.0"
    version_block = [
        '  <!-- Label: "v2.26.0" -->',
        render_qs_text(qs_reg_font, "v2.26.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.26.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 20 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (New module default placeholder: #5d5d5d neutral gray per AGENTS.md 6.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 56.50mm) -->',
        f'  <line x1="2.54" y1="56.50" x2="{panel_w - 2.54:.2f}" y2="56.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 84.50mm) -->',
        f'  <line x1="2.54" y1="84.50" x2="{panel_w - 2.54:.2f}" y2="84.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(ampara_title_block)
    svg_parts.extend(version_block)

    # 6 Columns across 101.60 mm
    # Margin = 12.00 mm, pitch = 15.52 mm
    cols = [12.00 + i * 15.52 for i in range(6)]
    # cols = [12.00, 27.52, 43.04, 58.56, 74.08, 89.60]

    # Row 1 Knobs: FREQ (with RANGE button/LED), FINE, SHAPE, WARP, WINDOW (plus stretch on row 2)
    # Specified order:
    # Row 1 Knobs: Freq (Range) Fine Shape Warp Window
    # Row 2 Knobs: Routing Amp Rise Fall Phase Stretch

    # Button between Col 0 and Col 1: (12.00 + 27.52)/2 = 19.76 mm
    btn_x = (cols[0] + cols[1]) / 2.0
    # Range button label: RANGE at baseline Y = 11.500
    svg_parts.append(render_qs_text(qs_font, "RANGE", btn_x, 11.500, 0.001600, "#2c2c2c", "RANGE Button Label"))

    # Row 1 Knob Labels (Center Y = 21.59, label Y = 14.500)
    svg_parts.append(render_qs_text(qs_font, "FREQ", cols[0], 14.500, 0.002500, "#1c1c1c", "FREQ Label"))
    svg_parts.append(render_qs_text(qs_font, "FINE", cols[1], 14.500, 0.002500, "#1c1c1c", "FINE Label"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", cols[2], 14.500, 0.002500, "#1c1c1c", "SHAPE Label"))
    svg_parts.append(render_qs_text(qs_font, "WARP", cols[3], 14.500, 0.002500, "#1c1c1c", "WARP Label"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", cols[4], 14.500, 0.002100, "#1c1c1c", "WINDOW Label"))

    # Row 2 Knobs: ROUTING, AMP, RISE, FALL, PHASE, STRETCH (Center Y = 43.00, label Y = 35.800)
    svg_parts.append(render_qs_text(qs_font, "ROUTING", cols[0], 35.800, 0.002000, "#1c1c1c", "ROUTING Label"))
    svg_parts.append(render_qs_text(qs_font, "AMP", cols[1], 35.800, 0.002500, "#1c1c1c", "AMP Label"))
    svg_parts.append(render_qs_text(qs_font, "RISE", cols[2], 35.800, 0.002500, "#1c1c1c", "RISE Label"))
    svg_parts.append(render_qs_text(qs_font, "FALL", cols[3], 35.800, 0.002500, "#1c1c1c", "FALL Label"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", cols[4], 35.800, 0.002300, "#1c1c1c", "PHASE Label"))
    svg_parts.append(render_qs_text(qs_font, "STRETCH", cols[5], 35.800, 0.002000, "#1c1c1c", "STRETCH Label"))

    # Zone 3: CV Attenuverter Trimpots (6 columns)
    # Row 1: Left Input, Right Input, Freq, Shape, Warp, Window (Center Y = 63.50, label Y = 59.500)
    svg_parts.append(render_qs_text(qs_font, "LEFT IN", cols[0], 59.500, 0.001600, "#2c2c2c", "LEFT IN Trim"))
    svg_parts.append(render_qs_text(qs_font, "RIGHT IN", cols[1], 59.500, 0.001600, "#2c2c2c", "RIGHT IN Trim"))
    svg_parts.append(render_qs_text(qs_font, "FREQ", cols[2], 59.500, 0.001800, "#2c2c2c", "FREQ Trim"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", cols[3], 59.500, 0.001800, "#2c2c2c", "SHAPE Trim"))
    svg_parts.append(render_qs_text(qs_font, "WARP", cols[4], 59.500, 0.001800, "#2c2c2c", "WARP Trim"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", cols[5], 59.500, 0.001500, "#2c2c2c", "WINDOW Trim"))

    # Row 2: Routing, Amp, Rise, Fall, Phase, Stretch (Center Y = 75.50, label Y = 71.500)
    svg_parts.append(render_qs_text(qs_font, "ROUTING", cols[0], 71.500, 0.001500, "#2c2c2c", "ROUTING Trim"))
    svg_parts.append(render_qs_text(qs_font, "AMP", cols[1], 71.500, 0.001800, "#2c2c2c", "AMP Trim"))
    svg_parts.append(render_qs_text(qs_font, "RISE", cols[2], 71.500, 0.001800, "#2c2c2c", "RISE Trim"))
    svg_parts.append(render_qs_text(qs_font, "FALL", cols[3], 71.500, 0.001800, "#2c2c2c", "FALL Trim"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", cols[4], 71.500, 0.001700, "#2c2c2c", "PHASE Trim"))
    svg_parts.append(render_qs_text(qs_font, "STRETCH", cols[5], 71.500, 0.001500, "#2c2c2c", "STRETCH Trim"))

    # Zone 4: Jacks
    # Row 1 Jacks: Left Input, Right Input, Freq, Shape, Warp, Window (Center Y = 94.00, label Y = 89.500)
    svg_parts.append(render_qs_text(qs_font, "LEFT IN", cols[0], 89.500, 0.001700, "#1c1c1c", "LEFT IN Jack"))
    svg_parts.append(render_qs_text(qs_font, "RIGHT IN", cols[1], 89.500, 0.001700, "#1c1c1c", "RIGHT IN Jack"))
    svg_parts.append(render_qs_text(qs_font, "FREQ", cols[2], 89.500, 0.001800, "#2c2c2c", "FREQ Jack"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", cols[3], 89.500, 0.001800, "#2c2c2c", "SHAPE Jack"))
    svg_parts.append(render_qs_text(qs_font, "WARP", cols[4], 89.500, 0.001800, "#2c2c2c", "WARP Jack"))
    svg_parts.append(render_qs_text(qs_font, "WINDOW", cols[5], 89.500, 0.001500, "#2c2c2c", "WINDOW Jack"))

    # Row 2 Jacks: Routing, Amp, Rise, Fall, Phase, Stretch (Center Y = 106.00, label Y = 101.500)
    svg_parts.append(render_qs_text(qs_font, "ROUTING", cols[0], 101.500, 0.001500, "#2c2c2c", "ROUTING Jack"))
    svg_parts.append(render_qs_text(qs_font, "AMP", cols[1], 101.500, 0.001800, "#2c2c2c", "AMP Jack"))
    svg_parts.append(render_qs_text(qs_font, "RISE", cols[2], 101.500, 0.001800, "#2c2c2c", "RISE Jack"))
    svg_parts.append(render_qs_text(qs_font, "FALL", cols[3], 101.500, 0.001800, "#2c2c2c", "FALL Jack"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", cols[4], 101.500, 0.001700, "#2c2c2c", "PHASE Jack"))
    svg_parts.append(render_qs_text(qs_font, "STRETCH", cols[5], 101.500, 0.001500, "#2c2c2c", "STRETCH Jack"))

    # Row 3 Jacks (Fixed Y = 118.00): Sync Input, X Out, Y Out, Sync Output
    # Centered symmetrically across 4 positions:
    # Margin = 20.00 mm, pitch = 20.53 mm -> [20.00, 40.53, 61.07, 81.60] mm
    bottom_jacks = [20.00, 40.53, 61.07, 81.60]
    svg_parts.append(render_qs_text(qs_font, "SYNC IN", bottom_jacks[0], 113.500, 0.001900, "#2c2c2c", "SYNC IN Jack"))
    svg_parts.append(render_qs_text(qs_font, "X", bottom_jacks[1], 113.500, 0.002400, "#1c1c1c", "X Jack"))
    svg_parts.append(render_qs_text(qs_font, "Y", bottom_jacks[2], 113.500, 0.002400, "#1c1c1c", "Y Jack"))
    svg_parts.append(render_qs_text(qs_font, "SYNC OUT", bottom_jacks[3], 113.500, 0.001900, "#2c2c2c", "SYNC OUT Jack"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/Ampara.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully.")

    # Render verification bitmap (190 x 240 at 4x resolution = 760 x 960)
    os.makedirs('scratch', exist_ok=True)
    verify_png = 'scratch/ampara_verify.png'
    cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=190 * 4, output_height=240 * 4)

    # Render MetaModule asset (190 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to='metamodule/assets/Ampara.png', output_width=190, output_height=240)
    print("Rendered metamodule/assets/Ampara.png successfully.")

    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (190 * 4) / panel_w

    # Row 1 Knobs: 5 knobs at cols[0..4]
    kr = 4.75 * r_px
    for kx in cols[:5]:
        draw.ellipse([kx * r_px - kr, 21.59 * r_px - kr, kx * r_px + kr, 21.59 * r_px + kr], outline='#ffaa00', width=2)
    # Row 2 Knobs: 6 knobs at cols[0..5]
    for kx in cols:
        draw.ellipse([kx * r_px - kr, 43.00 * r_px - kr, kx * r_px + kr, 43.00 * r_px + kr], outline='#ffaa00', width=2)

    # Range Button and LED at btn_x
    led_r = 1.5 * r_px
    draw.ellipse([btn_x * r_px - led_r, 15.50 * r_px - led_r, btn_x * r_px + led_r, 15.50 * r_px + led_r], outline='#d35fb7', width=2)
    btn_r = 2.0 * r_px
    draw.ellipse([btn_x * r_px - btn_r, 21.59 * r_px - btn_r, btn_x * r_px + btn_r, 21.59 * r_px + btn_r], outline='#f0e442', width=2)

    # Trimpots: 6 cols at Y = 63.50 and 75.50 mm
    tr = 2.5 * r_px
    for ty in [63.50, 75.50]:
        for tx in cols:
            draw.ellipse([tx * r_px - tr, ty * r_px - tr, tx * r_px + tr, ty * r_px + tr], outline='#00ff88', width=2)

    # Jacks: Row 1 (94.00), Row 2 (106.00)
    jr = 4.15 * r_px
    for jy in [94.00, 106.00]:
        for jx in cols:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00e5ff', width=2)

    # Row 3 (118.00 mm): 4 jacks
    for jx in bottom_jacks:
        draw.ellipse([jx * r_px - jr, 118.00 * r_px - jr, jx * r_px + jr, 118.00 * r_px + jr], outline='#00e5ff', width=2)

    im.save(verify_png)
    print(f"Rendered verification bitmap: {verify_png}")

if __name__ == '__main__':
    main()
