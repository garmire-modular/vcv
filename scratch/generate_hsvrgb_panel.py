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

    # Title "hsv / hsl" in Node.otf (scale 0.0048, baseline 7.620, centered on 6 HP = 30.48mm)
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    scale = 0.0048
    text = "hsv / hsl"
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    title_block = ['  <!-- Label: "hsv / hsl" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version "v2.17.0"
    v217_version_block = [render_qs_text(qs_font, "v2.17.0", 15.24, 10.414, 0.001600, "#aaaaaa", "Version v2.17.0")]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#7c7c7c"/>',
        '',
        '  <!-- Delineator Line 1 (Above Color Preview Swatch at Y = 66.00mm) -->',
        '  <line x1="2.54" y1="66.00" x2="27.94" y2="66.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Color Display Swatch (19.05mm x 3.81mm [0.15in] at center Y = 71.58mm) -->',
        '  <rect x="5.715" y="69.675" width="19.05" height="3.81" rx="1.0" ry="1.0" fill="#ff0000" stroke="#2c2c2c" stroke-width="0.3"/>',
        '',
        '  <!-- Delineator Line 2 (Above Attenuverters at Y = 76.00mm) -->',
        '  <line x1="2.54" y1="76.00" x2="27.94" y2="76.00" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 3 (Above I/O Jacks at Y = 91.50mm) -->',
        '  <line x1="2.54" y1="91.50" x2="27.94" y2="91.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(v217_version_block)

    # Row 1 Knob: HUE (Y = 13.070, scale = 0.003000)
    svg_parts.append(render_qs_text(qs_font, "HUE", 15.24, 13.070, 0.003000, "#1c1c1c", "HUE"))

    # Row 2 Knob: SAT (Y = 31.480, scale = 0.003000)
    svg_parts.append(render_qs_text(qs_font, "SAT", 15.24, 31.480, 0.003000, "#1c1c1c", "SAT"))

    # Row 3 Knob: VAL / LUM (Y = 49.890, scale = 0.003000)
    svg_parts.append(render_qs_text(qs_font, "VAL / LUM", 15.24, 49.890, 0.003000, "#1c1c1c", "VAL / LUM"))

    # Small & offset mode switch labels near Knob 3 (VAL) at X = 24.48mm:
    svg_parts.append(render_qs_text(qs_font, "HSV", 24.48, 53.500, 0.001800, "#2c2c2c", "HSV MODE"))
    svg_parts.append(render_qs_text(qs_font, "HSL", 24.48, 63.500, 0.001800, "#2c2c2c", "HSL MODE"))

    # Trimpot Row: HUE, SAT, VAL (Center Y = 83.000, baseline Y = 78.800, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "HUE", 6.00, 78.800, 0.002000, "#2c2c2c", "HUE CV"))
    svg_parts.append(render_qs_text(qs_font, "SAT", 15.24, 78.800, 0.002000, "#2c2c2c", "SAT CV"))
    svg_parts.append(render_qs_text(qs_font, "VAL", 24.48, 78.800, 0.002000, "#2c2c2c", "VAL CV"))

    # Jack Row 1 (CV Inputs): HUE, SAT, VAL (Center Y = 105.410, baseline Y = 103.910, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "HUE", 6.00, 103.910, 0.002400, "#2c2c2c", "HUE IN"))
    svg_parts.append(render_qs_text(qs_font, "SAT", 15.24, 103.910, 0.002400, "#2c2c2c", "SAT IN"))
    svg_parts.append(render_qs_text(qs_font, "VAL", 24.48, 103.910, 0.002400, "#2c2c2c", "VAL IN"))

    # Jack Row 2 (Outputs): R, G, B (Center Y = 116.840, baseline Y = 115.340, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "R", 6.00, 115.340, 0.002400, "#1c1c1c", "R OUT"))
    svg_parts.append(render_qs_text(qs_font, "G", 15.24, 115.340, 0.002400, "#1c1c1c", "G OUT"))
    svg_parts.append(render_qs_text(qs_font, "B", 24.48, 115.340, 0.002400, "#1c1c1c", "B OUT"))

    svg_parts.append('</svg>')

    with open('res/HsvRgb.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print("Generated res/HsvRgb.svg successfully.")

if __name__ == '__main__':
    main()
