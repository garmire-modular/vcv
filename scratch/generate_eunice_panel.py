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

    # 8 HP Dimensions: 40.64 mm x 128.50 mm
    panel_w = 40.64
    panel_h = 128.50

    # 3 Columns
    col_a = 8.13
    col_b = 20.32
    col_c = 32.51

    # Title "eunice" in Node.otf (scale 0.0048, baseline 7.620, centered at x = 20.32)
    hmtx = node_font['hmtx']
    scale_title = 0.0048
    title_text = "eunice"
    total_w = sum(hmtx[c][0] * scale_title for c in title_text)
    start_x = (panel_w - total_w) / 2.0

    title_block = ['  <!-- Label: "eunice" -->']
    curr_x = start_x
    for c in title_text:
        w = hmtx[c][0] * scale_title
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale_title:.6f}, {-scale_title:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version "v2.26.0" in Quicksand-Regular (scale 0.001600, baseline 10.414, centered on 20.32)
    version_block = [render_qs_text(qs_reg_font, "v2.26.0", col_b, 10.414, 0.001600, "#aaaaaa", "v2.26.0")]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        '  <!-- Panel Background: 8 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge: Default Placeholder #5d5d5d (Centered 88.9mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Above Attenuverters at Y = 49.00mm) -->',
        f'  <line x1="3.00" y1="49.00" x2="{panel_w - 3.00:.2f}" y2="49.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Above I/O Jacks at Y = 77.00mm) -->',
        f'  <line x1="3.00" y1="77.00" x2="{panel_w - 3.00:.2f}" y2="77.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # ---------------- Zone 1 & 2: Primary Controls ----------------
    # Row 1 Labels (Baseline Y = 13.070, scale 0.0024)
    # Rate, Dist, Corr
    svg_parts.append(render_qs_text(qs_font, "RATE", col_a, 13.070, 0.002400, "#1c1c1c", "RATE"))
    svg_parts.append(render_qs_text(qs_font, "DIST", col_b, 13.070, 0.002400, "#1c1c1c", "DIST"))
    svg_parts.append(render_qs_text(qs_font, "CORR", col_c, 13.070, 0.002400, "#1c1c1c", "CORR"))

    # Row 2 Labels (Baseline Y = 32.500, scale 0.0024)
    # Slew Dest switch label above switch, Rise & Fall
    svg_parts.append(render_qs_text(qs_font, "SLEW", col_a, 32.500, 0.002400, "#1c1c1c", "SLEW"))
    svg_parts.append(render_qs_text(qs_font, "RISE", col_b, 32.500, 0.002400, "#1c1c1c", "RISE"))
    svg_parts.append(render_qs_text(qs_font, "FALL", col_c, 32.500, 0.002400, "#1c1c1c", "FALL"))

    # Switch position indicators for Slew Dest: SH (top), BOTH (mid), TH (bot)
    # Switch center is at Y = 40.00. Indicators at -4mm, 0mm, +4mm
    svg_parts.append(render_qs_text(qs_font, "S&H", col_a - 4.50, 37.50, 0.001600, "#2c2c2c", "S&H POS"))
    svg_parts.append(render_qs_text(qs_font, "ALL", col_a - 4.50, 41.00, 0.001600, "#2c2c2c", "ALL POS"))
    svg_parts.append(render_qs_text(qs_font, "T&H", col_a - 4.50, 44.50, 0.001600, "#2c2c2c", "T&H POS"))

    # ---------------- Zone 3: CV Attenuverters ----------------
    # Row 3 Trimpots (Center Y = 56.00, Baseline Y = 51.50)
    svg_parts.append(render_qs_text(qs_font, "RATE", col_a, 51.50, 0.002000, "#2c2c2c", "RATE TRIM"))
    svg_parts.append(render_qs_text(qs_font, "DIST", col_b, 51.50, 0.002000, "#2c2c2c", "DIST TRIM"))
    svg_parts.append(render_qs_text(qs_font, "CORR", col_c, 51.50, 0.002000, "#2c2c2c", "CORR TRIM"))

    # Row 4 Trimpots (Center Y = 68.00, Baseline Y = 63.50)
    svg_parts.append(render_qs_text(qs_font, "RISE", col_a, 63.50, 0.002000, "#2c2c2c", "RISE TRIM"))
    svg_parts.append(render_qs_text(qs_font, "FALL", col_b, 63.50, 0.002000, "#2c2c2c", "FALL TRIM"))

    # ---------------- Zone 4: I/O Jacks ----------------
    # Row 5 Jacks (Center Y = 89.50, Baseline Y = 82.50)
    svg_parts.append(render_qs_text(qs_font, "IN", col_a, 82.50, 0.002200, "#1c1c1c", "IN JACK"))
    svg_parts.append(render_qs_text(qs_font, "DIST", col_b, 82.50, 0.002200, "#1c1c1c", "DIST CV"))
    svg_parts.append(render_qs_text(qs_font, "CORR", col_c, 82.50, 0.002200, "#1c1c1c", "CORR CV"))

    # Row 6 Jacks (Center Y = 99.00, Baseline Y = 94.00)
    svg_parts.append(render_qs_text(qs_font, "RATE", col_a, 94.00, 0.002200, "#1c1c1c", "RATE CV"))
    svg_parts.append(render_qs_text(qs_font, "GATE", col_b, 94.00, 0.002200, "#1c1c1c", "GATE JACK"))
    svg_parts.append(render_qs_text(qs_font, "S&H", col_c, 94.00, 0.002200, "#1c1c1c", "S&H OUT"))

    # Row 7 Jacks (Center Y = 108.50, Baseline Y = 103.50)
    svg_parts.append(render_qs_text(qs_font, "RISE", col_a, 103.50, 0.002200, "#1c1c1c", "RISE CV"))
    svg_parts.append(render_qs_text(qs_font, "FALL", col_b, 103.50, 0.002200, "#1c1c1c", "FALL CV"))
    svg_parts.append(render_qs_text(qs_font, "T&H", col_c, 103.50, 0.002200, "#1c1c1c", "T&H OUT"))

    svg_parts.append('</svg>\n')
    svg_content = "\n".join(svg_parts)

    out_file = 'res/Eunice.svg'
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print(f"Generated {out_file} successfully.")

if __name__ == '__main__':
    main()
