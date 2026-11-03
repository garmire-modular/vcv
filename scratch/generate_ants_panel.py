import os
import re
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops

ddin_bold = TTFont('res/D-DIN-Bold.otf')
ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
ddin_cond = TTFont('res/D-DINCondensed.otf')
qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

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
    if ('v' in text and '.' in text) or 'version' in comment.lower():
        use_font = qs_reg_font
        use_scale = 0.001600
    elif font == ddin_bold:
        use_font = ddin_bold
        use_scale = scale
    elif baseline_y < 52.0:
        use_font = ddin_cond_bold
        use_scale = 10.0 * 25.4 / (72.0 * 1000.0)  # 10pt = 0.003528
    else:
        use_font = ddin_cond
        use_scale = 8.0 * 25.4 / (72.0 * 1000.0)   # 8pt = 0.002822
    
    cmap = use_font.getBestCmap()
    hmtx = use_font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * use_scale if ord(c) in cmap else 500 * use_scale for c in text]
    total_w = sum(widths)
    start_x = center_x - total_w / 2.0
    res = []
    if comment:
        res.append(f'  <!-- Label: "{comment}" -->')
    curr = start_x
    for c, w in zip(text, widths):
        if c != ' ':
            pd = get_simplified_glyph_path(use_font, c)
            res.append(f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({use_scale:.6f}, {-use_scale:.6f})"><path d="{pd}" fill="{fill}"/></g>')
        curr += w
    return "\n".join(res)

def main():
    panel_w = 30.48
    title_scale = 16.0 * 25.4 / (72.0 * 1000.0)

    # Title "ANTS" in D-DIN-Bold (16pt, scale 0.005644, baseline 7.620)
    ants_title_block = [
        '  <!-- Label: "ANTS" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "ANTS", panel_w / 2.0, 7.620, title_scale, "#ffffff", "ANTS")
    ]

    # Version tag "v2.0.0"
    v102_version_block = [
        '  <!-- Label: "v2.0.0" -->',
        render_qs_text(qs_reg_font, "v2.0.0", 15.24, 10.414, 0.001600, "#aaaaaa", "v2.0.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Centered 88.9mm x 1.35mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="16.002" fill="#FFFFFF" stroke="none"/>',
        '    <rect x="0.000" y="35.802" width="1.350" height="44.450" fill="#442288" stroke="none"/>',
        '    <rect x="0.000" y="80.252" width="1.350" height="28.448" fill="#FF8866" stroke="none"/>',
        '  </g>',
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
    svg_parts.append(render_qs_text(None, "COUNT", 7.62, 13.070, 0.002400, "#1c1c1c", "COUNT"))
    svg_parts.append(render_qs_text(None, "LENGTH", 22.86, 13.070, 0.002400, "#1c1c1c", "LENGTH"))

    # Knob Row 2 Labels (Y = 34.500): POS (X=7.62) & VAR (X=22.86)
    svg_parts.append(render_qs_text(None, "POS", 7.62, 34.500, 0.002400, "#1c1c1c", "POS"))
    svg_parts.append(render_qs_text(None, "VAR", 22.86, 34.500, 0.002400, "#1c1c1c", "VAR"))

    # Trimpot Row 1 Labels (Y = 56.500, scale = 0.002000): COUNT & LENGTH
    svg_parts.append(render_qs_text(None, "COUNT", 7.62, 56.500, 0.002000, "#2c2c2c", "COUNT TRIM"))
    svg_parts.append(render_qs_text(None, "LENGTH", 22.86, 56.500, 0.002000, "#2c2c2c", "LENGTH TRIM"))

    # Trimpot Row 2 Labels (Y = 68.500, scale = 0.002000): POS & VAR
    svg_parts.append(render_qs_text(None, "POS", 7.62, 68.500, 0.002000, "#2c2c2c", "POS TRIM"))
    svg_parts.append(render_qs_text(None, "VAR", 22.86, 68.500, 0.002000, "#2c2c2c", "VAR TRIM"))

    # Jack Row 1: X, Y (Y = 84.000), IN (Y = 88.000)
    svg_parts.append(render_qs_text(None, "X", 7.62, 84.000, 0.003000, "#1c1c1c", "X"))
    svg_parts.append(render_qs_text(None, "Y", 22.86, 84.000, 0.003000, "#1c1c1c", "Y"))
    svg_parts.append(render_qs_text(None, "IN", 15.24, 88.000, 0.002400, "#2c2c2c", "IN"))

    # Jack Row 2: Standard centered CV text between the first two CV jacks (Y = 97.500, scale = 0.002400)
    svg_parts.append(render_qs_text(None, "CV", 15.24, 97.500, 0.002400, "#2c2c2c", "CV"))

    # Jack Row 4: OUT (Y = 116.500)
    svg_parts.append(render_qs_text(None, "OUT", 15.24, 116.500, 0.002400, "#2c2c2c", "OUT"))

    svg_parts.append('</svg>\n')

    import sys
    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('Ants', "\n".join(svg_parts) + "\n")

if __name__ == '__main__':
    main()
