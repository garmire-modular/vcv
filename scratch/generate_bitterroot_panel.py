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

    # 28 HP Dimensions: 142.24 mm x 128.50 mm
    panel_w = 142.24
    panel_h = 128.50

    # 3 Block Column Centers
    col_1 = 26.000
    col_2 = 71.120
    col_3 = 116.240

    # Knobs within columns (pitch 13.0 mm)
    c1_k = [13.000, 26.000, 39.000]
    c2_k = [58.120, 71.120, 84.120]
    c3_k = [103.240, 116.240, 129.240]

    # Title "bitterroot" in Node.otf (scale 0.0048, baseline 7.620, centered at x = 71.12)
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

    # Version "v1.0.0" in Quicksand-Regular (scale 0.001600, baseline 10.414, centered on 71.12)
    version_block = [render_qs_text(qs_reg_font, "v1.0.0", col_2, 10.414, 0.001600, "#aaaaaa", "v1.0.0")]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        '  <!-- Panel Background: 28 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge: Default Placeholder #5d5d5d (Centered 88.9mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Between Zone 2 Effects & Zone 3 Master at Y = 69.50mm) -->',
        f'  <line x1="3.00" y1="69.50" x2="{panel_w - 3.00:.2f}" y2="69.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Above I/O Jacks at Y = 83.50mm) -->',
        f'  <line x1="3.00" y1="83.50" x2="{panel_w - 3.00:.2f}" y2="83.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # ---------------- Zone 2: Transformation Blocks (Rows 1, 2, 3) ----------------
    # Row 1 Block Titles & Knobs (Knobs Center Y = 23.50, Header Y = 13.80, Subheader Y = 16.50)
    svg_parts.append(render_qs_text(qs_font, "MORTON", col_1, 13.800, 0.002400, "#1c1c1c", "MORTON"))
    svg_parts.append(render_qs_text(qs_font, "REVERSE", col_2, 13.800, 0.002400, "#1c1c1c", "REVERSE"))
    svg_parts.append(render_qs_text(qs_font, "TRANSPOSE", col_3, 13.800, 0.002400, "#1c1c1c", "TRANSPOSE"))

    # Row 1 Knob Labels
    svg_parts.append(render_qs_text(qs_font, "SHIFT", c1_k[0], 16.500, 0.001600, "#2c2c2c", "SHIFT"))
    svg_parts.append(render_qs_text(qs_font, "STRIDE", c1_k[1], 16.500, 0.001600, "#2c2c2c", "STRIDE"))
    svg_parts.append(render_qs_text(qs_font, "HILBERT", c1_k[2], 16.500, 0.001600, "#2c2c2c", "HILBERT"))

    svg_parts.append(render_qs_text(qs_font, "WIDTH", c2_k[0], 16.500, 0.001600, "#2c2c2c", "WIDTH"))
    svg_parts.append(render_qs_text(qs_font, "OFFSET", c2_k[1], 16.500, 0.001600, "#2c2c2c", "OFFSET"))
    svg_parts.append(render_qs_text(qs_font, "SKEW", c2_k[2], 16.500, 0.001600, "#2c2c2c", "SKEW"))

    svg_parts.append(render_qs_text(qs_font, "PLANE A", c3_k[0], 16.500, 0.001600, "#2c2c2c", "PLANE A"))
    svg_parts.append(render_qs_text(qs_font, "PLANE B", c3_k[1], 16.500, 0.001600, "#2c2c2c", "PLANE B"))
    svg_parts.append(render_qs_text(qs_font, "CYCLE C", c3_k[2], 16.500, 0.001600, "#2c2c2c", "CYCLE C"))

    # Row 2 Block Titles & Knobs (Knobs Center Y = 42.50, Header Y = 32.80, Subheader Y = 35.50)
    svg_parts.append(render_qs_text(qs_font, "AVALANCHE", col_1, 32.800, 0.002400, "#1c1c1c", "AVALANCHE"))
    svg_parts.append(render_qs_text(qs_font, "PERMUTE", col_2, 32.800, 0.002400, "#1c1c1c", "PERMUTE"))
    svg_parts.append(render_qs_text(qs_font, "GRAY", col_3, 32.800, 0.002400, "#1c1c1c", "GRAY"))

    # Row 2 Knob Labels
    svg_parts.append(render_qs_text(qs_font, "MASK", c1_k[0], 35.500, 0.001600, "#2c2c2c", "MASK"))
    svg_parts.append(render_qs_text(qs_font, "SHIFT", c1_k[1], 35.500, 0.001600, "#2c2c2c", "SHIFT"))
    svg_parts.append(render_qs_text(qs_font, "BORROW", c1_k[2], 35.500, 0.001600, "#2c2c2c", "BORROW"))

    svg_parts.append(render_qs_text(qs_font, "MODE", c2_k[0], 35.500, 0.001600, "#2c2c2c", "MODE"))
    svg_parts.append(render_qs_text(qs_font, "ROTATE", c2_k[1], 35.500, 0.001600, "#2c2c2c", "ROTATE"))
    svg_parts.append(render_qs_text(qs_font, "STRIDE", c2_k[2], 35.500, 0.001600, "#2c2c2c", "STRIDE"))

    svg_parts.append(render_qs_text(qs_font, "DEPTH", c3_k[0], 35.500, 0.001600, "#2c2c2c", "DEPTH"))
    svg_parts.append(render_qs_text(qs_font, "MODE", c3_k[1], 35.500, 0.001600, "#2c2c2c", "MODE"))
    svg_parts.append(render_qs_text(qs_font, "TAP", c3_k[2], 35.500, 0.001600, "#2c2c2c", "TAP"))

    # Row 3 Block Titles & Knobs (Knobs Center Y = 61.50, Header Y = 51.80, Subheader Y = 54.50)
    svg_parts.append(render_qs_text(qs_font, "GALOIS", col_1, 51.800, 0.002400, "#1c1c1c", "GALOIS"))
    svg_parts.append(render_qs_text(qs_font, "AUTOMATA", col_2, 51.800, 0.002400, "#1c1c1c", "AUTOMATA"))
    svg_parts.append(render_qs_text(qs_font, "HAMMING", col_3, 51.800, 0.002400, "#1c1c1c", "HAMMING"))

    # Row 3 Knob Labels
    svg_parts.append(render_qs_text(qs_font, "POLY", c1_k[0], 54.500, 0.001600, "#2c2c2c", "POLY"))
    svg_parts.append(render_qs_text(qs_font, "ALPHA", c1_k[1], 54.500, 0.001600, "#2c2c2c", "ALPHA"))
    svg_parts.append(render_qs_text(qs_font, "POWER", c1_k[2], 54.500, 0.001600, "#2c2c2c", "POWER"))

    svg_parts.append(render_qs_text(qs_font, "RULE", c2_k[0], 54.500, 0.001600, "#2c2c2c", "RULE"))
    svg_parts.append(render_qs_text(qs_font, "STEPS", c2_k[1], 54.500, 0.001600, "#2c2c2c", "STEPS"))
    svg_parts.append(render_qs_text(qs_font, "INJECT", c2_k[2], 54.500, 0.001600, "#2c2c2c", "INJECT"))

    svg_parts.append(render_qs_text(qs_font, "GAIN", c3_k[0], 54.500, 0.001600, "#2c2c2c", "GAIN"))
    svg_parts.append(render_qs_text(qs_font, "MODE", c3_k[1], 54.500, 0.001600, "#2c2c2c", "MODE"))
    svg_parts.append(render_qs_text(qs_font, "MUTUAL", c3_k[2], 54.500, 0.001600, "#2c2c2c", "MUTUAL"))

    # ---------------- Zone 3: Master Controls & Vector Activity Display ----------------
    svg_parts.append(render_qs_text(qs_font, "SCAN X", 20.000, 72.000, 0.002000, "#1c1c1c", "SCAN X"))
    svg_parts.append(render_qs_text(qs_font, "SCAN Y", 36.000, 72.000, 0.002000, "#1c1c1c", "SCAN Y"))
    svg_parts.append(render_qs_text(qs_font, "ROUTE", 52.000, 72.000, 0.002000, "#1c1c1c", "ROUTE"))
    svg_parts.append(render_qs_text(qs_font, "GRID", 71.120, 72.000, 0.001800, "#2c2c2c", "GRID DISPLAY"))
    svg_parts.append(render_qs_text(qs_font, "Z BLANK", 122.240, 72.000, 0.002000, "#1c1c1c", "Z BLANK"))

    # ---------------- Zone 4: I/O Patch Bay ----------------
    # Row 1 CV Inputs (Center Y = 89.50, Label Y = 85.30)
    # Block A
    svg_parts.append(render_qs_text(qs_font, "SHFT", c1_k[0], 85.300, 0.001700, "#2c2c2c", "A SHIFT CV"))
    svg_parts.append(render_qs_text(qs_font, "STRD", c1_k[1], 85.300, 0.001700, "#2c2c2c", "A STRIDE CV"))
    svg_parts.append(render_qs_text(qs_font, "HLBT", c1_k[2], 85.300, 0.001700, "#2c2c2c", "A HILBERT CV"))
    # Block B
    svg_parts.append(render_qs_text(qs_font, "WDTH", c2_k[0], 85.300, 0.001700, "#2c2c2c", "B WIDTH CV"))
    svg_parts.append(render_qs_text(qs_font, "OFST", c2_k[1], 85.300, 0.001700, "#2c2c2c", "B OFFSET CV"))
    svg_parts.append(render_qs_text(qs_font, "SKEW", c2_k[2], 85.300, 0.001700, "#2c2c2c", "B SKEW CV"))
    # Block C
    svg_parts.append(render_qs_text(qs_font, "PL A", c3_k[0], 85.300, 0.001700, "#2c2c2c", "C PLANE A CV"))
    svg_parts.append(render_qs_text(qs_font, "PL B", c3_k[1], 85.300, 0.001700, "#2c2c2c", "C PLANE B CV"))
    svg_parts.append(render_qs_text(qs_font, "CYCL", c3_k[2], 85.300, 0.001700, "#2c2c2c", "C CYCLE CV"))

    # Row 2 CV Inputs (Center Y = 99.00, Label Y = 94.80)
    # Block D
    svg_parts.append(render_qs_text(qs_font, "MASK", c1_k[0], 94.800, 0.001700, "#2c2c2c", "D MASK CV"))
    svg_parts.append(render_qs_text(qs_font, "SHFT", c1_k[1], 94.800, 0.001700, "#2c2c2c", "D SHIFT CV"))
    svg_parts.append(render_qs_text(qs_font, "BORW", c1_k[2], 94.800, 0.001700, "#2c2c2c", "D BORROW CV"))
    # Block E
    svg_parts.append(render_qs_text(qs_font, "MODE", c2_k[0], 94.800, 0.001700, "#2c2c2c", "E MODE CV"))
    svg_parts.append(render_qs_text(qs_font, "ROT", c2_k[1], 94.800, 0.001700, "#2c2c2c", "E ROTATE CV"))
    svg_parts.append(render_qs_text(qs_font, "STRD", c2_k[2], 94.800, 0.001700, "#2c2c2c", "E STRIDE CV"))
    # Block F
    svg_parts.append(render_qs_text(qs_font, "DPTH", c3_k[0], 94.800, 0.001700, "#2c2c2c", "F DEPTH CV"))
    svg_parts.append(render_qs_text(qs_font, "MODE", c3_k[1], 94.800, 0.001700, "#2c2c2c", "F MODE CV"))
    svg_parts.append(render_qs_text(qs_font, "TAP", c3_k[2], 94.800, 0.001700, "#2c2c2c", "F TAP CV"))

    # Row 3 CV Inputs (Center Y = 108.50, Label Y = 104.30)
    # Block G
    svg_parts.append(render_qs_text(qs_font, "POLY", c1_k[0], 104.300, 0.001700, "#2c2c2c", "G POLY CV"))
    svg_parts.append(render_qs_text(qs_font, "ALPH", c1_k[1], 104.300, 0.001700, "#2c2c2c", "G ALPHA CV"))
    svg_parts.append(render_qs_text(qs_font, "POWR", c1_k[2], 104.300, 0.001700, "#2c2c2c", "G POWER CV"))
    # Block H
    svg_parts.append(render_qs_text(qs_font, "RULE", c2_k[0], 104.300, 0.001700, "#2c2c2c", "H RULE CV"))
    svg_parts.append(render_qs_text(qs_font, "STEP", c2_k[1], 104.300, 0.001700, "#2c2c2c", "H STEP CV"))
    svg_parts.append(render_qs_text(qs_font, "INJ", c2_k[2], 104.300, 0.001700, "#2c2c2c", "H INJECT CV"))
    # Block I
    svg_parts.append(render_qs_text(qs_font, "GAIN", c3_k[0], 104.300, 0.001700, "#2c2c2c", "I GAIN CV"))
    svg_parts.append(render_qs_text(qs_font, "MODE", c3_k[1], 104.300, 0.001700, "#2c2c2c", "I MODE CV"))
    svg_parts.append(render_qs_text(qs_font, "MUTL", c3_k[2], 104.300, 0.001700, "#2c2c2c", "I MUTUAL CV"))

    # Bottom Master Jacks (Fixed Y = 118.00, Baseline Y = 113.80)
    # IN X (13.0), IN Y (27.5), SCAN X (47.0), SCAN Y (63.0), ROUTE (79.0), OUT X (99.0), OUT Y (114.5), OUT Z (130.0)
    svg_parts.append(render_qs_text(qs_font, "IN X", 13.000, 113.800, 0.002000, "#1c1c1c", "IN X"))
    svg_parts.append(render_qs_text(qs_font, "IN Y", 27.500, 113.800, 0.002000, "#1c1c1c", "IN Y"))
    svg_parts.append(render_qs_text(qs_font, "SCAN X", 47.000, 113.800, 0.001800, "#2c2c2c", "SCAN X CV"))
    svg_parts.append(render_qs_text(qs_font, "SCAN Y", 63.000, 113.800, 0.001800, "#2c2c2c", "SCAN Y CV"))
    svg_parts.append(render_qs_text(qs_font, "ROUTE", 79.000, 113.800, 0.001800, "#2c2c2c", "ROUTE CV"))
    svg_parts.append(render_qs_text(qs_font, "OUT X", 99.000, 113.800, 0.002000, "#1c1c1c", "OUT X"))
    svg_parts.append(render_qs_text(qs_font, "OUT Y", 114.500, 113.800, 0.002000, "#1c1c1c", "OUT Y"))
    svg_parts.append(render_qs_text(qs_font, "OUT Z", 130.000, 113.800, 0.002000, "#1c1c1c", "OUT Z"))

    svg_parts.append('</svg>\n')
    svg_content = "\n".join(svg_parts)

    out_file = 'res/Bitterroot.svg'
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print(f"Generated {out_file} successfully.")

    # Render MetaModule PNG (28 HP = 266 x 240 px)
    try:
        import resvg_py
        png_path = 'metamodule/assets/Bitterroot.png'
        svg_mod = re.sub(r'width="[0-9.]+mm"', 'width="266"', svg_content)
        svg_mod = re.sub(r'height="[0-9.]+mm"', 'height="240"', svg_mod)
        png_bytes = resvg_py.svg_to_bytes(svg_string=svg_mod, width=266, height=240)
        with open(png_path, 'wb') as pf:
            pf.write(png_bytes)
        print(f"Rendered {png_path} successfully (266x240).")
    except Exception as e:
        print(f"Note: MetaModule asset rendering skipped or failed: {e}")

if __name__ == '__main__':
    main()
