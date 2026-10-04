import os
import re
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops

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

    # 10 HP Dimensions: 50.80 mm x 128.50 mm
    panel_w = 50.80
    panel_h = 128.50

    # 3 Primary Columns (Knobs, Attenuverters, CV Jacks)
    col_1 = 9.400
    col_2 = 25.400
    col_3 = 41.400

    # 4 Port Columns (Jack Rows 3 & 4)
    p_col_1 = 7.900
    p_col_2 = 19.567
    p_col_3 = 31.233
    p_col_4 = 42.900

    # Title "stepped slew" in Node.otf (scale 0.0048, baseline 7.620, centered at x = 30.48)
    cmap_node = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    scale_title = 0.0048
    title_text = "stepped slew"
    
    widths = []
    for c in title_text:
        if c == ' ':
            widths.append(500 * scale_title)
        else:
            gname = cmap_node[ord(c)] if cmap_node and ord(c) in cmap_node else c
            widths.append(hmtx[gname][0] * scale_title)
    total_w = sum(widths)
    start_x = (panel_w - total_w) / 2.0

    title_block = ['  <!-- Label: "stepped slew" -->']
    curr_x = start_x
    for c, w in zip(title_text, widths):
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale_title:.6f}, {-scale_title:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version "v2.26.0" in Quicksand-Regular (scale 0.001600, baseline 10.414, centered on 30.48)
    version_block = [render_qs_text(qs_reg_font, "v2.26.0", col_2, 10.414, 0.001600, "#aaaaaa", "v2.26.0")]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        '  <!-- Panel Background: 10 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge: Default Placeholder #5d5d5d (Centered 88.9mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Above Attenuverters at Y = 51.50mm) -->',
        f'  <line x1="3.00" y1="51.50" x2="{panel_w - 3.00:.2f}" y2="51.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Above I/O Jacks at Y = 80.50mm) -->',
        f'  <line x1="3.00" y1="80.50" x2="{panel_w - 3.00:.2f}" y2="80.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # ---------------- Zone 2: Primary Knobs ----------------
    # Top Column Headers (Baseline Y = 13.070, scale 0.0024)
    svg_parts.append(render_qs_text(qs_font, "TIME", col_1, 13.070, 0.002400, "#1c1c1c", "TIME"))
    svg_parts.append(render_qs_text(qs_font, "SHAPE", col_2, 13.070, 0.002400, "#1c1c1c", "SHAPE"))
    svg_parts.append(render_qs_text(qs_font, "STEPS", col_3, 13.070, 0.002400, "#1c1c1c", "STEPS"))

    # Section Headers for Rows 1 & 2
    svg_parts.append(render_qs_text(qs_font, "UP", 4.80, 22.500, 0.002000, "#2c2c2c", "UP"))
    svg_parts.append(render_qs_text(qs_font, "DOWN", 4.80, 43.900, 0.002000, "#2c2c2c", "DOWN"))

    # ---------------- Zone 3: CV Attenuverters ----------------
    # Row 1 Attenuverters (Center Y = 58.00, Baseline Y = 53.50)
    svg_parts.append(render_qs_text(qs_font, "UP CV", col_2, 53.500, 0.002000, "#2c2c2c", "UP CV"))

    # Row 2 Attenuverters (Center Y = 70.00, Baseline Y = 65.50)
    svg_parts.append(render_qs_text(qs_font, "DOWN CV", col_2, 65.500, 0.002000, "#2c2c2c", "DOWN CV"))

    # ---------------- Zone 4: I/O Jacks ----------------
    # Jack Row 1 (UP CV Inputs, Center Y = 89.50, Baseline Y = 82.50)
    svg_parts.append(render_qs_text(qs_font, "UP CV IN", col_2, 82.500, 0.002000, "#2c2c2c", "UP CV IN"))

    # Jack Row 2 (DOWN CV Inputs, Center Y = 99.00, Baseline Y = 93.80)
    svg_parts.append(render_qs_text(qs_font, "DOWN CV IN", col_2, 93.800, 0.002000, "#2c2c2c", "DOWN CV IN"))

    # Jack Row 3 (Gates / Trigs, Center Y = 108.50, Baseline Y = 103.80)
    svg_parts.append(render_qs_text(qs_font, "UP", p_col_1, 103.800, 0.002000, "#2c2c2c", "UP GATE"))
    svg_parts.append(render_qs_text(qs_font, "EOU", p_col_2, 103.800, 0.002000, "#2c2c2c", "EOU TRIG"))
    svg_parts.append(render_qs_text(qs_font, "DOWN", p_col_3, 103.800, 0.002000, "#2c2c2c", "DOWN GATE"))
    svg_parts.append(render_qs_text(qs_font, "EOD", p_col_4, 103.800, 0.002000, "#2c2c2c", "EOD TRIG"))

    # Jack Row 4 (Fixed Bottom Center Y = 118.00, Baseline Y = 113.30)
    svg_parts.append(render_qs_text(qs_font, "IN", p_col_1, 113.300, 0.002200, "#1c1c1c", "IN JACK"))
    svg_parts.append(render_qs_text(qs_font, "SLEW", p_col_2, 113.300, 0.002200, "#1c1c1c", "SLEW JACK"))
    svg_parts.append(render_qs_text(qs_font, "STEP", p_col_3, 113.300, 0.002200, "#1c1c1c", "STEP JACK"))
    svg_parts.append(render_qs_text(qs_font, "TRIG", p_col_4, 113.300, 0.002200, "#1c1c1c", "STEP TRIG"))

    svg_parts.append('</svg>\n')
    svg_content = "\n".join(svg_parts)

    out_file = 'res/SteppedSlew.svg'
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print(f"Generated {out_file} successfully.")

if __name__ == '__main__':
    main()
