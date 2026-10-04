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

    # 32 HP Dimensions: 162.56 mm x 128.50 mm
    panel_w = 162.56
    panel_h = 128.50
    center_x = panel_w / 2.0  # 81.28 mm

    # 3 Effect Column Centers
    col_1 = 28.780
    col_2 = 81.280
    col_3 = 133.780

    # 4 Knobs within each effect column (pitch 11.5 mm, span 34.5 mm)
    c1_k = [11.530, 23.030, 34.530, 46.030]
    c2_k = [64.030, 75.530, 87.030, 98.530]
    c3_k = [116.530, 128.030, 139.530, 151.030]

    # Title "bitterroot" in Node.otf (scale 0.0048, baseline 7.620, centered at x = 81.28)
    cmap_node = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    scale_title = 0.0048
    title_text = "bitterroot"
    
    widths = []
    for c in title_text:
        if c == ' ':
            widths.append(500 * scale_title)
        else:
            gname = cmap_node[ord(c)] if cmap_node and ord(c) in cmap_node else c
            widths.append(hmtx[gname][0] * scale_title)
    total_w = sum(widths)
    start_x = (panel_w - total_w) / 2.0

    title_block = ['  <!-- Label: "bitterroot" -->']
    curr_x = start_x
    for c, w in zip(title_text, widths):
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale_title:.6f}, {-scale_title:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version "v1.1.0" in Quicksand-Regular (scale 0.001600, baseline 10.414, centered on 81.28)
    version_block = [render_qs_text(qs_reg_font, "v1.1.0", center_x, 10.414, 0.001600, "#aaaaaa", "v1.1.0")]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        '  <!-- Panel Background: 32 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge: Default Placeholder #5d5d5d (Centered 88.9mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Between Effects and Master Section at Y = 70.00mm) -->',
        f'  <line x1="3.00" y1="70.00" x2="{panel_w - 3.00:.2f}" y2="70.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Between Master Section and 12x3 CV Matrix at Y = 88.50mm) -->',
        f'  <line x1="3.00" y1="88.50" x2="{panel_w - 3.00:.2f}" y2="88.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # ---------------- Zone 2: Effect Blocks (Rows 1, 2, 3) ----------------
    # Row 1 Block Titles & Knobs (Knobs Center Y = 23.50, Header Y = 13.80, Subheader Y = 16.50)
    svg_parts.append(render_qs_text(qs_font, "MORTON", col_1, 13.800, 0.002400, "#1c1c1c", "MORTON"))
    svg_parts.append(render_qs_text(qs_font, "REVERSE", col_2, 13.800, 0.002400, "#1c1c1c", "REVERSE"))
    svg_parts.append(render_qs_text(qs_font, "TRANSPOSE", col_3, 13.800, 0.002400, "#1c1c1c", "TRANSPOSE"))

    # Row 1 Knob Labels
    svg_parts.append(render_qs_text(qs_font, "SHIFT", c1_k[0], 16.500, 0.001600, "#2c2c2c", "SHIFT"))
    svg_parts.append(render_qs_text(qs_font, "STRIDE", c1_k[1], 16.500, 0.001600, "#2c2c2c", "STRIDE"))
    svg_parts.append(render_qs_text(qs_font, "HILBERT", c1_k[2], 16.500, 0.001600, "#2c2c2c", "HILBERT"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c1_k[3], 16.500, 0.001600, "#2c2c2c", "MIX"))

    svg_parts.append(render_qs_text(qs_font, "WIDTH", c2_k[0], 16.500, 0.001600, "#2c2c2c", "WIDTH"))
    svg_parts.append(render_qs_text(qs_font, "OFFSET", c2_k[1], 16.500, 0.001600, "#2c2c2c", "OFFSET"))
    svg_parts.append(render_qs_text(qs_font, "SKEW", c2_k[2], 16.500, 0.001600, "#2c2c2c", "SKEW"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c2_k[3], 16.500, 0.001600, "#2c2c2c", "MIX"))

    svg_parts.append(render_qs_text(qs_font, "PLANE A", c3_k[0], 16.500, 0.001600, "#2c2c2c", "PLANE A"))
    svg_parts.append(render_qs_text(qs_font, "PLANE B", c3_k[1], 16.500, 0.001600, "#2c2c2c", "PLANE B"))
    svg_parts.append(render_qs_text(qs_font, "CYCLE C", c3_k[2], 16.500, 0.001600, "#2c2c2c", "CYCLE C"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c3_k[3], 16.500, 0.001600, "#2c2c2c", "MIX"))

    # Row 2 Block Titles & Knobs (Knobs Center Y = 42.50, Header Y = 32.80, Subheader Y = 35.50)
    svg_parts.append(render_qs_text(qs_font, "AVALANCHE", col_1, 32.800, 0.002400, "#1c1c1c", "AVALANCHE"))
    svg_parts.append(render_qs_text(qs_font, "PERMUTE", col_2, 32.800, 0.002400, "#1c1c1c", "PERMUTE"))
    svg_parts.append(render_qs_text(qs_font, "GRAY", col_3, 32.800, 0.002400, "#1c1c1c", "GRAY"))

    # Row 2 Knob Labels
    svg_parts.append(render_qs_text(qs_font, "MASK", c1_k[0], 35.500, 0.001600, "#2c2c2c", "MASK"))
    svg_parts.append(render_qs_text(qs_font, "SHIFT", c1_k[1], 35.500, 0.001600, "#2c2c2c", "SHIFT"))
    svg_parts.append(render_qs_text(qs_font, "BORROW", c1_k[2], 35.500, 0.001600, "#2c2c2c", "BORROW"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c1_k[3], 35.500, 0.001600, "#2c2c2c", "MIX"))

    svg_parts.append(render_qs_text(qs_font, "MODE", c2_k[0], 35.500, 0.001600, "#2c2c2c", "MODE"))
    svg_parts.append(render_qs_text(qs_font, "ROTATE", c2_k[1], 35.500, 0.001600, "#2c2c2c", "ROTATE"))
    svg_parts.append(render_qs_text(qs_font, "STRIDE", c2_k[2], 35.500, 0.001600, "#2c2c2c", "STRIDE"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c2_k[3], 35.500, 0.001600, "#2c2c2c", "MIX"))

    svg_parts.append(render_qs_text(qs_font, "DEPTH", c3_k[0], 35.500, 0.001600, "#2c2c2c", "DEPTH"))
    svg_parts.append(render_qs_text(qs_font, "MODE", c3_k[1], 35.500, 0.001600, "#2c2c2c", "MODE"))
    svg_parts.append(render_qs_text(qs_font, "TAP", c3_k[2], 35.500, 0.001600, "#2c2c2c", "TAP"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c3_k[3], 35.500, 0.001600, "#2c2c2c", "MIX"))

    # Row 3 Block Titles & Knobs (Knobs Center Y = 61.50, Header Y = 51.80, Subheader Y = 54.50)
    svg_parts.append(render_qs_text(qs_font, "GALOIS", col_1, 51.800, 0.002400, "#1c1c1c", "GALOIS"))
    svg_parts.append(render_qs_text(qs_font, "AUTOMATA", col_2, 51.800, 0.002400, "#1c1c1c", "AUTOMATA"))
    svg_parts.append(render_qs_text(qs_font, "HAMMING", col_3, 51.800, 0.002400, "#1c1c1c", "HAMMING"))

    # Row 3 Knob Labels
    svg_parts.append(render_qs_text(qs_font, "POLY", c1_k[0], 54.500, 0.001600, "#2c2c2c", "POLY"))
    svg_parts.append(render_qs_text(qs_font, "ALPHA", c1_k[1], 54.500, 0.001600, "#2c2c2c", "ALPHA"))
    svg_parts.append(render_qs_text(qs_font, "POWER", c1_k[2], 54.500, 0.001600, "#2c2c2c", "POWER"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c1_k[3], 54.500, 0.001600, "#2c2c2c", "MIX"))

    svg_parts.append(render_qs_text(qs_font, "RULE", c2_k[0], 54.500, 0.001600, "#2c2c2c", "RULE"))
    svg_parts.append(render_qs_text(qs_font, "STEPS", c2_k[1], 54.500, 0.001600, "#2c2c2c", "STEPS"))
    svg_parts.append(render_qs_text(qs_font, "INJECT", c2_k[2], 54.500, 0.001600, "#2c2c2c", "INJECT"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c2_k[3], 54.500, 0.001600, "#2c2c2c", "MIX"))

    svg_parts.append(render_qs_text(qs_font, "GAIN", c3_k[0], 54.500, 0.001600, "#2c2c2c", "GAIN"))
    svg_parts.append(render_qs_text(qs_font, "MODE", c3_k[1], 54.500, 0.001600, "#2c2c2c", "MODE"))
    svg_parts.append(render_qs_text(qs_font, "MUTUAL", c3_k[2], 54.500, 0.001600, "#2c2c2c", "MUTUAL"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c3_k[3], 54.500, 0.001600, "#2c2c2c", "MIX"))

    # ---------------- Zone 3: Master Controls & Master Jacks (Center Y = 79.50) ----------------
    # Left Side: Controls & Display (Y = 79.50, Labels Y = 72.80)
    svg_parts.append(render_qs_text(qs_font, "SCAN X", 12.000, 72.800, 0.001900, "#1c1c1c", "SCAN X"))
    svg_parts.append(render_qs_text(qs_font, "SCAN Y", 24.500, 72.800, 0.001900, "#1c1c1c", "SCAN Y"))
    svg_parts.append(render_qs_text(qs_font, "ROUTE", 37.000, 72.800, 0.001900, "#1c1c1c", "ROUTE"))
    svg_parts.append(render_qs_text(qs_font, "GRID", 49.500, 72.800, 0.001700, "#2c2c2c", "GRID DISPLAY"))
    svg_parts.append(render_qs_text(qs_font, "Z BLANK", 64.000, 72.800, 0.001900, "#1c1c1c", "Z BLANK"))

    # Right Side: Master Jacks (Center Y = 79.50, Labels Y = 72.80)
    # IN X (80.0), IN Y (90.0), SCAN X (103.0), SCAN Y (113.0), Z BLK (123.0), OUT X (136.0), OUT Y (146.0), OUT Z (156.0)
    svg_parts.append(render_qs_text(qs_font, "IN X", 80.000, 72.800, 0.001800, "#1c1c1c", "IN X"))
    svg_parts.append(render_qs_text(qs_font, "IN Y", 90.000, 72.800, 0.001800, "#1c1c1c", "IN Y"))
    svg_parts.append(render_qs_text(qs_font, "SCAN X", 103.000, 72.800, 0.001700, "#2c2c2c", "SCAN X CV"))
    svg_parts.append(render_qs_text(qs_font, "SCAN Y", 113.000, 72.800, 0.001700, "#2c2c2c", "SCAN Y CV"))
    svg_parts.append(render_qs_text(qs_font, "Z BLK", 123.000, 72.800, 0.001700, "#2c2c2c", "Z BLANK CV"))
    svg_parts.append(render_qs_text(qs_font, "OUT X", 136.000, 72.800, 0.001800, "#1c1c1c", "OUT X"))
    svg_parts.append(render_qs_text(qs_font, "OUT Y", 146.000, 72.800, 0.001800, "#1c1c1c", "OUT Y"))
    svg_parts.append(render_qs_text(qs_font, "OUT Z", 156.000, 72.800, 0.001800, "#1c1c1c", "OUT Z"))

    # ---------------- Zone 4: 12 x 3 CV Depth Jack Matrix ----------------
    # Row 1 CV Matrix (Center Y = 98.00, Label Y = 93.80)
    # Morton CV
    svg_parts.append(render_qs_text(qs_font, "SHFT", c1_k[0], 93.800, 0.001600, "#2c2c2c", "MORTON SHIFT CV"))
    svg_parts.append(render_qs_text(qs_font, "STRD", c1_k[1], 93.800, 0.001600, "#2c2c2c", "MORTON STRIDE CV"))
    svg_parts.append(render_qs_text(qs_font, "HLBT", c1_k[2], 93.800, 0.001600, "#2c2c2c", "MORTON HILBERT CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c1_k[3], 93.800, 0.001600, "#2c2c2c", "MORTON MIX CV"))
    # Reverse CV
    svg_parts.append(render_qs_text(qs_font, "WDTH", c2_k[0], 93.800, 0.001600, "#2c2c2c", "REVERSE WIDTH CV"))
    svg_parts.append(render_qs_text(qs_font, "OFST", c2_k[1], 93.800, 0.001600, "#2c2c2c", "REVERSE OFFSET CV"))
    svg_parts.append(render_qs_text(qs_font, "SKEW", c2_k[2], 93.800, 0.001600, "#2c2c2c", "REVERSE SKEW CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c2_k[3], 93.800, 0.001600, "#2c2c2c", "REVERSE MIX CV"))
    # Transpose CV
    svg_parts.append(render_qs_text(qs_font, "PL A", c3_k[0], 93.800, 0.001600, "#2c2c2c", "TRANSPOSE PLANE A CV"))
    svg_parts.append(render_qs_text(qs_font, "PL B", c3_k[1], 93.800, 0.001600, "#2c2c2c", "TRANSPOSE PLANE B CV"))
    svg_parts.append(render_qs_text(qs_font, "CYCL", c3_k[2], 93.800, 0.001600, "#2c2c2c", "TRANSPOSE CYCLE CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c3_k[3], 93.800, 0.001600, "#2c2c2c", "TRANSPOSE MIX CV"))

    # Row 2 CV Matrix (Center Y = 108.00, Label Y = 103.80)
    # Avalanche CV
    svg_parts.append(render_qs_text(qs_font, "MASK", c1_k[0], 103.800, 0.001600, "#2c2c2c", "AVALANCHE MASK CV"))
    svg_parts.append(render_qs_text(qs_font, "SHFT", c1_k[1], 103.800, 0.001600, "#2c2c2c", "AVALANCHE SHIFT CV"))
    svg_parts.append(render_qs_text(qs_font, "BORW", c1_k[2], 103.800, 0.001600, "#2c2c2c", "AVALANCHE BORROW CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c1_k[3], 103.800, 0.001600, "#2c2c2c", "AVALANCHE MIX CV"))
    # Permute CV
    svg_parts.append(render_qs_text(qs_font, "MODE", c2_k[0], 103.800, 0.001600, "#2c2c2c", "PERMUTE MODE CV"))
    svg_parts.append(render_qs_text(qs_font, "ROT", c2_k[1], 103.800, 0.001600, "#2c2c2c", "PERMUTE ROTATE CV"))
    svg_parts.append(render_qs_text(qs_font, "STRD", c2_k[2], 103.800, 0.001600, "#2c2c2c", "PERMUTE STRIDE CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c2_k[3], 103.800, 0.001600, "#2c2c2c", "PERMUTE MIX CV"))
    # Gray CV
    svg_parts.append(render_qs_text(qs_font, "DPTH", c3_k[0], 103.800, 0.001600, "#2c2c2c", "GRAY DEPTH CV"))
    svg_parts.append(render_qs_text(qs_font, "MODE", c3_k[1], 103.800, 0.001600, "#2c2c2c", "GRAY MODE CV"))
    svg_parts.append(render_qs_text(qs_font, "TAP", c3_k[2], 103.800, 0.001600, "#2c2c2c", "GRAY TAP CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c3_k[3], 103.800, 0.001600, "#2c2c2c", "GRAY MIX CV"))

    # Row 3 CV Matrix (Center Y = 118.00, Label Y = 113.80)
    # Galois CV
    svg_parts.append(render_qs_text(qs_font, "POLY", c1_k[0], 113.800, 0.001600, "#2c2c2c", "GALOIS POLY CV"))
    svg_parts.append(render_qs_text(qs_font, "ALPH", c1_k[1], 113.800, 0.001600, "#2c2c2c", "GALOIS ALPHA CV"))
    svg_parts.append(render_qs_text(qs_font, "POWR", c1_k[2], 113.800, 0.001600, "#2c2c2c", "GALOIS POWER CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c1_k[3], 113.800, 0.001600, "#2c2c2c", "GALOIS MIX CV"))
    # Automata CV
    svg_parts.append(render_qs_text(qs_font, "RULE", c2_k[0], 113.800, 0.001600, "#2c2c2c", "AUTOMATA RULE CV"))
    svg_parts.append(render_qs_text(qs_font, "STEP", c2_k[1], 113.800, 0.001600, "#2c2c2c", "AUTOMATA STEP CV"))
    svg_parts.append(render_qs_text(qs_font, "INJ", c2_k[2], 113.800, 0.001600, "#2c2c2c", "AUTOMATA INJECT CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c2_k[3], 113.800, 0.001600, "#2c2c2c", "AUTOMATA MIX CV"))
    # Hamming CV
    svg_parts.append(render_qs_text(qs_font, "GAIN", c3_k[0], 113.800, 0.001600, "#2c2c2c", "HAMMING GAIN CV"))
    svg_parts.append(render_qs_text(qs_font, "MODE", c3_k[1], 113.800, 0.001600, "#2c2c2c", "HAMMING MODE CV"))
    svg_parts.append(render_qs_text(qs_font, "MUTL", c3_k[2], 113.800, 0.001600, "#2c2c2c", "HAMMING MUTUAL CV"))
    svg_parts.append(render_qs_text(qs_font, "MIX", c3_k[3], 113.800, 0.001600, "#2c2c2c", "HAMMING MIX CV"))

    svg_parts.append('</svg>\n')
    svg_content = "\n".join(svg_parts)

    out_file = 'res/Bitterroot.svg'
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print(f"Generated {out_file} successfully.")

    # Render MetaModule PNG (32 HP = 304 x 240 px)
    try:
        import resvg_py
        png_path = 'metamodule/assets/Bitterroot.png'
        svg_mod = re.sub(r'width="[0-9.]+mm"', 'width="304"', svg_content)
        svg_mod = re.sub(r'height="[0-9.]+mm"', 'height="240"', svg_mod)
        png_bytes = resvg_py.svg_to_bytes(svg_string=svg_mod, width=304, height=240)
        with open(png_path, 'wb') as pf:
            pf.write(png_bytes)
        print(f"Rendered {png_path} successfully (304x240).")
    except Exception as e:
        print(f"Note: MetaModule asset rendering skipped or failed: {e}")

if __name__ == '__main__':
    main()
