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

    with open('res/Stretch.svg', 'r', encoding='utf-8') as f:
        stretch_svg = f.read()

    stretch_lines = stretch_svg.splitlines()

    # Title "wiggle" in Node.otf (scale 0.0048, baseline 7.620)
    hmtx = node_font['hmtx']
    scale = 0.0048
    text = "wiggle"
    total_w = sum(hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    wiggle_title_block = ['  <!-- Label: "wiggle" -->']
    curr_x = start_x
    for c in text:
        w = hmtx[c][0] * scale
        path_d = get_simplified_glyph_path(node_font, c)
        wiggle_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    two_version_path = re.search(r'path d="([^"]+)"', stretch_lines[26]).group(1) if len(stretch_lines) > 26 else re.search(r'path d="([^"]+)"', stretch_lines[22]).group(1)
    v102_version_block = [
        '  <!-- Label: "v1.0.2" -->',
        stretch_lines[21], # v
        stretch_lines[22], # 1
        stretch_lines[23], # .
        stretch_lines[24], # 0
        stretch_lines[25], # .
        f'    <g transform="translate(16.333, 10.414) scale(0.001600, -0.001600)"><path d="{two_version_path}" fill="#aaaaaa"/></g>' # 2
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
    svg_parts.extend(wiggle_title_block)
    svg_parts.extend(v102_version_block)

    # Top Knobs X & Y labels (Y = 13.070, scale = 0.003000)
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 13.070, 0.003000, "#1c1c1c", "X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 13.070, 0.003000, "#1c1c1c", "Y"))

    # Row 2 Knob Centered RATE label (Y = 34.500, scale = 0.003000 - MATCHED SIZE!)
    svg_parts.append(render_qs_text(qs_font, "RATE", 15.24, 34.500, 0.003000, "#1c1c1c", "RATE"))

    # Trimpot Row 1 DEPTH CV labels (Y = 56.500, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "DEPTH CV", 7.62, 56.500, 0.002000, "#2c2c2c", "X DEPTH CV"))
    svg_parts.append(render_qs_text(qs_font, "DEPTH CV", 22.86, 56.500, 0.002000, "#2c2c2c", "Y DEPTH CV"))

    # Trimpot Row 2 RATE CV labels (Y = 68.500, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "RATE CV", 7.62, 68.500, 0.002000, "#2c2c2c", "X RATE CV"))
    svg_parts.append(render_qs_text(qs_font, "RATE CV", 22.86, 68.500, 0.002000, "#2c2c2c", "Y RATE CV"))

    # Jack Row 1: X, Y (Y = 84.000, scale = 0.003000), IN (Y = 88.000, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "X", 7.62, 84.000, 0.003000, "#1c1c1c", "X"))
    svg_parts.append(render_qs_text(qs_font, "Y", 22.86, 84.000, 0.003000, "#1c1c1c", "Y"))
    svg_parts.append(render_qs_text(qs_font, "IN", 15.24, 88.000, 0.002400, "#2c2c2c", "IN"))

    # Jack Row 2: AMT. (Y = 97.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "AMT.", 15.24, 97.500, 0.002400, "#2c2c2c", "AMT."))

    # Jack Row 3: RATE (Y = 107.000, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "RATE", 15.24, 107.000, 0.002400, "#2c2c2c", "RATE"))

    # Jack Row 4: OUT (Y = 116.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "OUT", 15.24, 116.500, 0.002400, "#2c2c2c", "OUT"))

    svg_parts.append('</svg>')

    with open('res/Wiggle.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print("Generated res/Wiggle.svg with v1.0.2.")

if __name__ == '__main__':
    main()
