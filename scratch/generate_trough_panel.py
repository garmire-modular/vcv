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

    # Title "trough" in Node.otf (scale 0.0048, baseline 7.620)
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    scale = 0.0048
    text = "trough"
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    title_block = ['  <!-- Label: "trough" -->']
    curr_x = start_x
    for c in text:
        w = hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.17.0"
    v217_version_block = [
        render_qs_text(qs_reg_font, "v2.17.0", 15.24, 10.414, 0.001600, "#aaaaaa", "v2.17.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Centered 88.9mm x 1.35mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="40.894" fill="#FFFFFF" stroke="none"/>',
        '    <rect x="0.000" y="60.694" width="1.350" height="28.448" fill="#882255" stroke="none"/>',
        '    <rect x="0.000" y="89.142" width="1.350" height="19.558" fill="#B8A0E8" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Above Attenuverters at Y = 52.50mm) -->',
        '  <line x1="2.54" y1="52.50" x2="27.94" y2="52.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Above I/O Jacks at Y = 80.50mm) -->',
        '  <line x1="2.54" y1="80.50" x2="27.94" y2="80.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(v217_version_block)

    # Top Knobs X & Y headers (Y = 13.070, scale = 0.003000)
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 13.070, 0.003000, "#1c1c1c", "X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 13.070, 0.003000, "#1c1c1c", "Y"))

    # Centered Row 2 TILT label (Y = 34.500)
    svg_parts.append(render_qs_text(qs_font, "TILT", 15.24, 34.500, 0.003000, "#1c1c1c", "TILT"))

    # Trimpot Row 1 TROUGH labels (Y = 56.500, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "TROUGH", 7.62, 56.500, 0.002000, "#2c2c2c", "X TROUGH"))
    svg_parts.append(render_qs_text(qs_font, "TROUGH", 22.86, 56.500, 0.002000, "#2c2c2c", "Y TROUGH"))

    # Trimpot Row 2 TILT labels (Y = 68.500, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "TILT", 7.62, 68.500, 0.002000, "#2c2c2c", "X TILT"))
    svg_parts.append(render_qs_text(qs_font, "TILT", 22.86, 68.500, 0.002000, "#2c2c2c", "Y TILT"))

    # Jack Row 1 (Inputs): X, Y (Y = 88.000, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 88.000, 0.002400, "#1c1c1c", "X IN"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 88.000, 0.002400, "#1c1c1c", "Y IN"))

    # Jack Row 2 (Trough CV): TROUGH (Y = 97.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "TROUGH", 15.24, 97.500, 0.002400, "#2c2c2c", "TROUGH CV"))

    # Jack Row 3 (Tilt CV): TILT (Y = 107.000, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "TILT", 15.24, 107.000, 0.002400, "#2c2c2c", "TILT CV"))

    # Jack Row 4 (Outputs): OUT (Y = 116.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 116.500, 0.002400, "#1c1c1c", "OUT"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    with open('res/Trough.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print("Generated res/Trough.svg successfully.")

if __name__ == '__main__':
    main()
