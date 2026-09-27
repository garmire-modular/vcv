import os
import re
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
    widths = [hmtx[cmap[ord(c)]][0] * scale for c in text]
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

    # Title "ants" in Node.otf (scale 0.0048, baseline 7.620)
    hmtx = node_font['hmtx']
    scale = 0.0048
    text = "ants"
    total_w = sum(hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    ants_title_block = ['  <!-- Label: "ants" -->']
    curr_x = start_x
    for c in text:
        w = hmtx[c][0] * scale
        path_d = get_simplified_glyph_path(node_font, c)
        ants_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v1.0.2"
    v102_version_block = [
        '  <!-- Label: "v1.0.2" -->',
        render_qs_text(qs_reg_font, "v1.0.2", 15.24, 10.414, 0.001600, "#aaaaaa", "v1.0.2")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#7c7c7c"/>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters) -->',
        '  <line x1="2.54" y1="52.50" x2="27.94" y2="52.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks) -->',
        '  <line x1="2.54" y1="80.50" x2="27.94" y2="80.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(ants_title_block)
    svg_parts.extend(v102_version_block)

    # Top Knobs Row 1 Labels (Y = 13.070): COUNT (X=7.62) & LENGTH (X=22.86)
    svg_parts.append(render_qs_text(qs_font, "COUNT", 7.62, 13.070, 0.002400, "#1c1c1c", "COUNT"))
    svg_parts.append(render_qs_text(qs_font, "LENGTH", 22.86, 13.070, 0.002400, "#1c1c1c", "LENGTH"))

    # Knob Row 2 Labels (Y = 34.500): POS (X=7.62) & VAR (X=22.86)
    svg_parts.append(render_qs_text(qs_font, "POS", 7.62, 34.500, 0.002400, "#1c1c1c", "POS"))
    svg_parts.append(render_qs_text(qs_font, "VAR", 22.86, 34.500, 0.002400, "#1c1c1c", "VAR"))

    # Trimpot Row 1 Labels (Y = 56.500, scale = 0.002000): COUNT & LENGTH
    svg_parts.append(render_qs_text(qs_font, "COUNT", 7.62, 56.500, 0.002000, "#2c2c2c", "COUNT TRIM"))
    svg_parts.append(render_qs_text(qs_font, "LENGTH", 22.86, 56.500, 0.002000, "#2c2c2c", "LENGTH TRIM"))

    # Trimpot Row 2 Labels (Y = 68.500, scale = 0.002000): POS & VAR
    svg_parts.append(render_qs_text(qs_font, "POS", 7.62, 68.500, 0.002000, "#2c2c2c", "POS TRIM"))
    svg_parts.append(render_qs_text(qs_font, "VAR", 22.86, 68.500, 0.002000, "#2c2c2c", "VAR TRIM"))

    # Jack Row 1: X, Y (Y = 84.000), IN (Y = 88.000)
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 84.000, 0.003000, "#1c1c1c", "X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 84.000, 0.003000, "#1c1c1c", "Y"))
    svg_parts.append(render_qs_text(qs_font, "IN", 15.24, 88.000, 0.002400, "#2c2c2c", "IN"))

    # Jack Row 2: Standard centered CV text between the first two CV jacks (Y = 97.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "CV", 15.24, 97.500, 0.002400, "#2c2c2c", "CV"))

    # Jack Row 4: OUT (Y = 116.500)
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 116.500, 0.002400, "#2c2c2c", "OUT"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    with open('res/Ants.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print("Generated res/Ants.svg with v1.0.2.")

if __name__ == '__main__':
    main()
