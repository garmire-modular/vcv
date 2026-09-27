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
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            pd = get_simplified_glyph_path(font, c)
            res.append(f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({scale:.6f}, {-scale:.6f})"><path d="{pd}" fill="{fill}"/></g>')
        curr += w
    return "\n".join(res)

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')

    # Title "chromance" in Node.otf (scale 0.0048, baseline 7.620, centered on 10 HP = 50.80mm)
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    scale = 0.0048
    text = "chromance"
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (50.80 - total_w) / 2.0

    chromance_title_block = ['  <!-- Label: "chromance" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            chromance_title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version "v2.10.0"
    v210_version_block = [render_qs_text(qs_font, "v2.10.0", 25.40, 10.414, 0.001600, "#aaaaaa", "Version v2.10.0")]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="50.80mm" height="128.5mm" viewBox="0 0 50.80 128.5">',
        '  <!-- Panel Background: 10 HP -->',
        '  <rect width="50.80" height="128.5" fill="#7c7c7c"/>',
        '',
        '  <!-- Delineator Line 1 (Positioned at Y = 52.50mm) -->',
        '  <line x1="2.54" y1="52.50" x2="48.26" y2="52.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Positioned at Y = 80.50mm) -->',
        '  <line x1="2.54" y1="80.50" x2="48.26" y2="80.50" stroke="#999999" stroke-width="0.176"/>',
        '',

        ''
    ]
    svg_parts.extend(chromance_title_block)
    svg_parts.extend(v210_version_block)

    # Main Knobs (3 Columns centered at X1 = 10.16, X2 = 25.40, X3 = 40.64)
    # Row 1 Main Knobs (Frequency Group): FREQ (10.16), FINE (25.40), MULT/DIV (40.64) (Center Y = 21.59mm, Header Y = 13.070mm, scale 0.003000)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 10.16, 13.070, 0.003000, "#1c1c1c", "FREQ"))
    svg_parts.append(render_qs_text(qs_font, "FINE", 25.40, 13.070, 0.003000, "#1c1c1c", "FINE"))
    svg_parts.append(render_qs_text(qs_font, "MULT/DIV", 40.64, 13.070, 0.003000, "#1c1c1c", "MULT/DIV"))

    # Row 2 Main Knobs (Waveform & Output Group): WAVE (10.16), PHASE (25.40), AMP (40.64) (Center Y = 43.00mm, Header Y = 34.500mm, scale 0.003000)
    svg_parts.append(render_qs_text(qs_font, "WAVE", 10.16, 34.500, 0.003000, "#1c1c1c", "WAVE"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", 25.40, 34.500, 0.003000, "#1c1c1c", "PHASE"))
    svg_parts.append(render_qs_text(qs_font, "AMP", 40.64, 34.500, 0.003000, "#1c1c1c", "AMP"))

    # Section Header CV at Center X = 25.40, Y = 56.500mm
    svg_parts.append(render_qs_text(qs_font, "CV", 25.40, 56.500, 0.002000, "#1c1c1c", "CV CENTER LABEL"))

    # Attenuverters & S/T&H Grid (4 Columns: 6.35, 19.05, 31.75, 44.45)
    # Row 1: FREQ (6.35), FINE (19.05), MULT (31.75), S/T&H (44.45) (Center Y = 61.50mm, Label Y = 57.500mm)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 6.35, 57.500, 0.001800, "#2c2c2c", "FREQ ATTV"))
    svg_parts.append(render_qs_text(qs_font, "FINE", 19.05, 57.500, 0.001800, "#2c2c2c", "FINE ATTV"))
    svg_parts.append(render_qs_text(qs_font, "MULT", 31.75, 57.500, 0.001800, "#2c2c2c", "MULT ATTV"))
    svg_parts.append(render_qs_text(qs_font, "S/T&H", 44.45, 57.500, 0.001800, "#2c2c2c", "S/T&H MODE"))

    # Row 2: WAVE (6.35), PHASE (19.05), AMP (31.75), RATE (44.45) (Center Y = 72.50mm, Label Y = 68.500mm)
    svg_parts.append(render_qs_text(qs_font, "WAVE", 6.35, 68.500, 0.001800, "#2c2c2c", "WAVE ATTV"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", 19.05, 68.500, 0.001800, "#2c2c2c", "PHASE ATTV"))
    svg_parts.append(render_qs_text(qs_font, "AMP", 31.75, 68.500, 0.001800, "#2c2c2c", "AMP ATTV"))
    svg_parts.append(render_qs_text(qs_font, "RATE", 44.45, 68.500, 0.001800, "#2c2c2c", "S/T&H RATE"))

    # Jack Row 1 (Frequency CV Inputs, Center Y = 92.00mm): FREQ (10.16), FINE (25.40), MULT (40.64) (Label Y = 85.500mm)
    svg_parts.append(render_qs_text(qs_font, "FREQ", 10.16, 85.500, 0.002400, "#2c2c2c", "FREQ IN"))
    svg_parts.append(render_qs_text(qs_font, "FINE", 25.40, 85.500, 0.002400, "#2c2c2c", "FINE IN"))
    svg_parts.append(render_qs_text(qs_font, "MULT", 40.64, 85.500, 0.002400, "#2c2c2c", "MULT IN"))

    # Jack Row 2 (Waveform & Output CV Inputs, Center Y = 105.00mm): WAVE (10.16), PHASE (25.40), AMP (40.64) (Label Y = 99.800mm)
    svg_parts.append(render_qs_text(qs_font, "WAVE", 10.16, 99.800, 0.002400, "#2c2c2c", "WAVE IN"))
    svg_parts.append(render_qs_text(qs_font, "PHASE", 25.40, 99.800, 0.002400, "#2c2c2c", "PHASE IN"))
    svg_parts.append(render_qs_text(qs_font, "AMP", 40.64, 99.800, 0.002400, "#2c2c2c", "AMP IN"))

    # Jack Row 3 (Bottom Row Output & Auxiliary Inputs, Center Y = 118.00mm): SYNC IN (10.16), OUT (25.40), S/T&H IN (40.64) (Label Y = 112.800mm)
    svg_parts.append(render_qs_text(qs_font, "SYNC IN", 10.16, 112.800, 0.002400, "#2c2c2c", "SYNC IN"))
    svg_parts.append(render_qs_text(qs_font, "OUT", 25.40, 112.800, 0.002400, "#1c1c1c", "OUT"))
    svg_parts.append(render_qs_text(qs_font, "S/T&H IN", 40.64, 112.800, 0.002400, "#2c2c2c", "S/T&H IN"))

    svg_parts.append('</svg>')

    with open('res/Chromance.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print("Generated res/Chromance.svg successfully.")

if __name__ == '__main__':
    main()
