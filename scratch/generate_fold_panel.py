import os
import re
import sys
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

def render_font_text(font, text, center_x, baseline_y, scale, fill, comment=""):
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
    panel_w = 30.48
    panel_h = 128.50

    # Scales per docs/TYPOGRAPHY.md
    title_scale = 16.0 * 25.4 / (72.0 * 1000.0)  # 0.005644
    knob_scale  = 10.0 * 25.4 / (72.0 * 1000.0)  # 0.003528
    trim_scale  = 8.0 * 25.4 / (72.0 * 1000.0)   # 0.002822
    ver_scale   = 0.001600                        # 4.5 pt

    # Fit title width if needed
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    raw_w = sum(hmtx[cmap[ord(c)]][0] * title_scale if ord(c) in cmap else 500 * title_scale for c in "FOLD")
    if raw_w > (panel_w - 3.0):
        title_scale = title_scale * ((panel_w - 3.0) / raw_w)

    title_block = [
        '  <!-- Label: "FOLD" (D-DIN-Bold) -->',
        render_font_text(ddin_bold, "FOLD", panel_w / 2.0, 7.620, title_scale, "#ffffff", "FOLD")
    ]

    version_block = [
        '  <!-- Label: "v2.0.0" -->',
        render_font_text(qs_reg_font, "v2.0.0", panel_w / 2.0, 10.414, ver_scale, "#aaaaaa", "v2.0.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#6e6e6e"/>',
        '''  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="28.448" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="48.248" width="1.350" height="19.558" fill="#442288" stroke="none"/>
    <rect x="0.000" y="67.806" width="1.350" height="40.894" fill="#B8A0E8" stroke="none"/>
  </g>''',
        '',
        '  <!-- Delineator Line 1 -->',
        '  <line x1="2.54" y1="71.12" x2="27.94" y2="71.12" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 -->',
        '  <line x1="2.54" y1="85.09" x2="27.94" y2="85.09" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # Main Knobs (D-DINCondensed-Bold, 10pt = 0.003528, preserving baseline Ys)
    svg_parts.append(render_font_text(ddin_cond_bold, "X", 7.62, 13.070, knob_scale, "#1c1c1c", "X"))
    svg_parts.append(render_font_text(ddin_cond_bold, "Y", 22.86, 13.070, knob_scale, "#1c1c1c", "Y"))

    # Attenuverters (D-DINCondensed, 8pt = 0.002822, preserving baseline Ys)
    svg_parts.append(render_font_text(ddin_cond, "X CV", 7.62, 74.210, trim_scale, "#2c2c2c", "X CV"))
    svg_parts.append(render_font_text(ddin_cond, "Y CV", 22.86, 74.210, trim_scale, "#2c2c2c", "Y CV"))

    # I/O Jacks (D-DINCondensed, 8pt = 0.002822, preserving baseline Ys)
    svg_parts.append(render_font_text(ddin_cond, "X", 7.62, 88.508, trim_scale, "#1c1c1c", "X"))
    svg_parts.append(render_font_text(ddin_cond, "Y", 22.86, 88.508, trim_scale, "#1c1c1c", "Y"))
    svg_parts.append(render_font_text(ddin_cond, "IN", 15.24, 92.500, trim_scale, "#2c2c2c", "IN"))
    svg_parts.append(render_font_text(ddin_cond, "CV", 15.24, 102.000, trim_scale, "#2c2c2c", "CV"))
    svg_parts.append(render_font_text(ddin_cond, "OUT", 15.24, 116.500, trim_scale, "#1c1c1c", "OUT"))

    svg_parts.append('</svg>\n')

    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('Fold', "\n".join(svg_parts) + "\n")
    print("Generated res/Fold.svg with D-DIN font hierarchy successfully.")

if __name__ == '__main__':
    main()
