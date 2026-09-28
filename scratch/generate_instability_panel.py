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

    # Title "instability" in Node.otf (scale 0.0048, baseline 7.620, centered on 6 HP = 30.48mm)
    hmtx = node_font['hmtx']
    scale = 0.0048
    text = "instability"
    total_w = sum(hmtx[c][0] * scale for c in text)
    start_x = (30.48 - total_w) / 2.0

    title_block = ['  <!-- Label: "instability" -->']
    curr_x = start_x
    for c in text:
        w = hmtx[c][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version "v2.24.0" in Quicksand-Regular (scale 0.001600, baseline 10.414, centered on 15.24mm)
    v224_version_block = [render_qs_text(qs_reg_font, "v2.24.0", 15.24, 10.414, 0.001600, "#aaaaaa", "v2.24.0")]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Centered 88.9mm x 1.35mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="50.673" fill="#FFFFFF" stroke="none"/>',
        '    <rect x="0.000" y="70.473" width="1.350" height="38.227" fill="#005566" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Above Attenuverters at Y = 59.50mm) -->',
        '  <line x1="2.54" y1="59.50" x2="27.94" y2="59.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Above I/O Jacks at Y = 90.00mm) -->',
        '  <line x1="2.54" y1="90.00" x2="27.94" y2="90.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(v224_version_block)

    # Row 1 Knobs: FREQ (X = 7.62mm) & PHASE (X = 22.86mm) (Center Y = 21.59mm, Baseline Y = 13.070, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 7.62, 13.070, 0.002400, "#1c1c1c", "FREQ"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", 22.86, 13.070, 0.002400, "#1c1c1c", "PHASE"))

    # Row 2 Knob: AMP (X = 15.24mm) (Center Y = 43.00mm, Baseline Y = 34.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "AMP", 15.24, 34.500, 0.002400, "#1c1c1c", "AMP"))

    # Trimpot Row 1: FREQ (X = 7.62mm) & PHASE (X = 22.86mm) (Center Y = 69.00mm, Baseline Y = 64.500, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 7.62, 64.500, 0.002000, "#2c2c2c", "FREQ TRIM"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", 22.86, 64.500, 0.002000, "#2c2c2c", "PHASE TRIM"))

    # Trimpot Row 2: AMP (X = 15.24mm) (Center Y = 81.00mm, Baseline Y = 76.500, scale = 0.002000)
    svg_parts.append(render_qs_text(qs_font, "AMP", 15.24, 76.500, 0.002000, "#2c2c2c", "AMP TRIM"))

    # Jack Row 1 (CV Rate Inputs): FREQ (X = 7.62mm) & PHASE (X = 22.86mm) (Center Y = 99.00mm, Baseline Y = 97.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 7.62, 97.500, 0.002400, "#2c2c2c", "FREQ CV"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", 22.86, 97.500, 0.002400, "#2c2c2c", "PHASE CV"))

    # Jack Row 2 (CV Rate Input): AMP (X = 15.24mm) (Center Y = 108.50mm, Baseline Y = 107.000, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "AMP", 15.24, 107.000, 0.002400, "#2c2c2c", "AMP CV"))

    # Jack Row 3 (Signal I/O): IN (X = 7.62mm) & OUT (X = 22.86mm) (Center Y = 118.00mm, Baseline Y = 116.500, scale = 0.002400)
    svg_parts.append(render_qs_text(qs_font, "IN", 7.62, 116.500, 0.002400, "#1c1c1c", "IN"))
    svg_parts.append(render_qs_text(qs_font, "OUT", 22.86, 116.500, 0.002400, "#1c1c1c", "OUT"))

    svg_parts.append('</svg>')

    with open('res/Instability.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print("Generated res/Instability.svg successfully.")

if __name__ == '__main__':
    main()
