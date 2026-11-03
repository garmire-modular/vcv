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
    widths = [hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else 500 * scale for c in text]
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
    ddin_bold = TTFont('res/D-DIN-Bold.otf')
    ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
    ddin_cond = TTFont('res/D-DINCondensed.otf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    # Typography scales per docs/TYPOGRAPHY.md
    title_scale = 16.0 * 25.4 / (72.0 * 1000.0)  # 0.005644
    knob_scale  = 10.0 * 25.4 / (72.0 * 1000.0)  # 0.003528
    trim_scale  = 8.0 * 25.4 / (72.0 * 1000.0)   # 0.002822
    ver_scale   = 0.001600                        # 4.5 pt

    # Title "CHROMANCE" in D-DIN-Bold (scale 0.005644, baseline 7.620, centered on 10 HP = 50.80mm)
    chromance_title_block = [
        render_qs_text(ddin_bold, "CHROMANCE", 50.80 / 2.0, 7.620, title_scale, "#ffffff", "CHROMANCE")
    ]

    # Version "v2.0.0" (keep existing baseline 10.414)
    v210_version_block = [
        render_qs_text(qs_reg_font, "v2.0.0", 25.40, 10.414, ver_scale, "#aaaaaa", "Version v2.0.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="50.80mm" height="128.5mm" viewBox="0 0 50.80 128.5">',
        '  <!-- Panel Background: 10 HP -->',
        '  <rect width="50.80" height="128.5" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (Centered 88.9mm x 1.35mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="44.450" fill="#1A1A1A" stroke="none"/>',
        '    <rect x="0.000" y="64.250" width="1.350" height="28.448" fill="#F0E442" stroke="none"/>',
        '    <rect x="0.000" y="92.698" width="1.350" height="16.002" fill="#CC79A7" stroke="none"/>',
        '  </g>',
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

    # Main Knobs (D-DINCondensed-Bold, 10pt = 0.003528, preserving baseline Ys)
    # Row 1 Main Knobs: FREQ (10.16), FINE (25.40), MULT/DIV (40.64) (Header Y = 13.070mm)
    svg_parts.append(render_qs_text(ddin_cond_bold, "FREQ", 10.16, 13.070, knob_scale, "#1c1c1c", "FREQ"))
    svg_parts.append(render_qs_text(ddin_cond_bold, "FINE", 25.40, 13.070, knob_scale, "#1c1c1c", "FINE"))
    svg_parts.append(render_qs_text(ddin_cond_bold, "MULT/DIV", 40.64, 13.070, knob_scale, "#1c1c1c", "MULT/DIV"))

    # Row 2 Main Knobs: WAVE (10.16), PHASE (25.40), AMP (40.64) (Header Y = 34.500mm)
    svg_parts.append(render_qs_text(ddin_cond_bold, "WAVE", 10.16, 34.500, knob_scale, "#1c1c1c", "WAVE"))
    svg_parts.append(render_qs_text(ddin_cond_bold, "PHASE", 25.40, 34.500, knob_scale, "#1c1c1c", "PHASE"))
    svg_parts.append(render_qs_text(ddin_cond_bold, "AMP", 40.64, 34.500, knob_scale, "#1c1c1c", "AMP"))

    # Section Header CV at Center X = 25.40, Y = 56.500mm (D-DINCondensed, 8pt)
    svg_parts.append(render_qs_text(ddin_cond, "CV", 25.40, 56.500, trim_scale, "#1c1c1c", "CV CENTER LABEL"))

    # Attenuverters (D-DINCondensed, 8pt = 0.002822, preserving baseline Ys)
    # Row 1: FREQ (6.35), FINE (19.05), MULT (31.75), S/T&H (44.45) (Label Y = 57.500mm)
    svg_parts.append(render_qs_text(ddin_cond, "FREQ", 6.35, 57.500, trim_scale, "#2c2c2c", "FREQ ATTV"))
    svg_parts.append(render_qs_text(ddin_cond, "FINE", 19.05, 57.500, trim_scale, "#2c2c2c", "FINE ATTV"))
    svg_parts.append(render_qs_text(ddin_cond, "MULT", 31.75, 57.500, trim_scale, "#2c2c2c", "MULT ATTV"))
    svg_parts.append(render_qs_text(ddin_cond, "S/T&H", 44.45, 57.500, trim_scale, "#2c2c2c", "S/T&H MODE"))

    # Row 2: WAVE (6.35), PHASE (19.05), AMP (31.75), RATE (44.45) (Label Y = 68.500mm)
    svg_parts.append(render_qs_text(ddin_cond, "WAVE", 6.35, 68.500, trim_scale, "#2c2c2c", "WAVE ATTV"))
    svg_parts.append(render_qs_text(ddin_cond, "PHASE", 19.05, 68.500, trim_scale, "#2c2c2c", "PHASE ATTV"))
    svg_parts.append(render_qs_text(ddin_cond, "AMP", 31.75, 68.500, trim_scale, "#2c2c2c", "AMP ATTV"))
    svg_parts.append(render_qs_text(ddin_cond, "RATE", 44.45, 68.500, trim_scale, "#2c2c2c", "S/T&H RATE"))

    # I/O Jacks (D-DINCondensed, 8pt = 0.002822, preserving baseline Ys)
    # Jack Row 1: FREQ (10.16), FINE (25.40), MULT (40.64) (Label Y = 85.500mm)
    svg_parts.append(render_qs_text(ddin_cond, "FREQ", 10.16, 85.500, trim_scale, "#2c2c2c", "FREQ IN"))
    svg_parts.append(render_qs_text(ddin_cond, "FINE", 25.40, 85.500, trim_scale, "#2c2c2c", "FINE IN"))
    svg_parts.append(render_qs_text(ddin_cond, "MULT", 40.64, 85.500, trim_scale, "#2c2c2c", "MULT IN"))

    # Jack Row 2: WAVE (10.16), PHASE (25.40), AMP (40.64) (Label Y = 99.800mm)
    svg_parts.append(render_qs_text(ddin_cond, "WAVE", 10.16, 99.800, trim_scale, "#2c2c2c", "WAVE IN"))
    svg_parts.append(render_qs_text(ddin_cond, "PHASE", 25.40, 99.800, trim_scale, "#2c2c2c", "PHASE IN"))
    svg_parts.append(render_qs_text(ddin_cond, "AMP", 40.64, 99.800, trim_scale, "#2c2c2c", "AMP IN"))

    # Jack Row 3: SYNC IN (10.16), OUT (25.40), S/T&H IN (40.64) (Label Y = 112.800mm)
    svg_parts.append(render_qs_text(ddin_cond, "SYNC IN", 10.16, 112.800, trim_scale, "#2c2c2c", "SYNC IN"))
    svg_parts.append(render_qs_text(ddin_cond, "OUT", 25.40, 112.800, trim_scale, "#1c1c1c", "OUT"))
    svg_parts.append(render_qs_text(ddin_cond, "S/T&H IN", 40.64, 112.800, trim_scale, "#2c2c2c", "S/T&H IN"))

    svg_parts.append('</svg>')

    import sys
    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('Chromance', "\n".join(svg_parts) + "\n")

if __name__ == '__main__':
    main()
