import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen

def get_glyph_path(font, char):
    cmap = font.getBestCmap()
    gname = cmap[ord(char)]
    glyph_set = font.getGlyphSet()
    pen = SVGPathPen(glyph_set)
    glyph_set[gname].draw(pen)
    return pen.getCommands()

def render_text_group(font, text, center_x, baseline_y, scale, fill_color, comment=""):
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    
    # Calculate widths
    widths = [hmtx[cmap[ord(c)]][0] * scale for c in text]
    total_w = sum(widths)
    start_x = center_x - total_w / 2.0
    
    lines = []
    if comment:
        lines.append(f'  <!-- {comment} -->')
    
    curr_x = start_x
    for c, w in zip(text, widths):
        if c != ' ':
            path_d = get_glyph_path(font, c)
            lines.append(f'    <g transform="translate({curr_x:.3f}, {baseline_y:.3f}) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="{fill_color}"/></g>')
        curr_x += w
    return "\n".join(lines)

def main():
    node_font = TTFont('res/Node.otf')
    quicksand_font = TTFont('res/Quicksand-Medium.ttf')
    
    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#6e6e6e"/>',

        '  <!-- Left Edge Color Badge (Centered 88.9mm x 2.54mm, Flush X=0) -->',

        '  <g id="palette-badge">',

        '    <rect x="0.000" y="19.800" width="2.540" height="28.448" fill="#FFFFFF" stroke="none"/>',

        '    <rect x="0.000" y="48.248" width="2.540" height="16.002" fill="#D55E00" stroke="none"/>',

        '    <rect x="0.000" y="64.250" width="2.540" height="44.450" fill="#B8A0E8" stroke="none"/>',

        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters) -->',
        '  <line x1="2.54" y1="71.12" x2="27.94" y2="71.12" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks) -->',
        '  <line x1="2.54" y1="85.09" x2="27.94" y2="85.09" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Vector Outlines for Title (Node), Version & Labels (Quicksand-Medium) -->'
    ]
    
    # Title "shear"
    svg_parts.append(render_text_group(node_font, "shear", 15.24, 7.620, 0.004800, "#ffffff", 'Label: "shear"'))
    
    # Version "v1.0.0"
    svg_parts.append(render_text_group(quicksand_font, "v1.0.0", 15.24, 10.414, 0.001600, "#aaaaaa", 'Label: "v1.0.0"'))
    
    # Main Knob Labels
    svg_parts.append(render_text_group(quicksand_font, "X", 7.62, 13.070, 0.003000, "#1c1c1c", 'Label: "X"'))
    svg_parts.append(render_text_group(quicksand_font, "Y", 22.86, 13.070, 0.003000, "#1c1c1c", 'Label: "Y"'))
    
    # Attenuverter Labels
    svg_parts.append(render_text_group(quicksand_font, "X CV", 7.62, 74.210, 0.002400, "#2c2c2c", 'Label: "X CV"'))
    svg_parts.append(render_text_group(quicksand_font, "Y CV", 22.86, 74.210, 0.002400, "#2c2c2c", 'Label: "Y CV"'))
    
    # Jack Row 1 Labels
    svg_parts.append(render_text_group(quicksand_font, "X", 7.62, 88.508, 0.003000, "#1c1c1c", 'Label: "X"'))
    svg_parts.append(render_text_group(quicksand_font, "Y", 22.86, 88.508, 0.003000, "#1c1c1c", 'Label: "Y"'))
    svg_parts.append(render_text_group(quicksand_font, "IN", 15.24, 93.260, 0.002400, "#2c2c2c", 'Label: "IN"'))
    
    # Jack Row 2 Label
    svg_parts.append(render_text_group(quicksand_font, "CV", 15.24, 104.690, 0.002400, "#2c2c2c", 'Label: "CV"'))
    
    # Jack Row 3 Label
    svg_parts.append(render_text_group(quicksand_font, "OUT", 15.24, 116.120, 0.002400, "#2c2c2c", 'Label: "OUT"'))
    
    svg_parts.append('</svg>')
    
    os.makedirs('res', exist_ok=True)
    with open('res/Shear.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print("Generated res/Shear.svg with Quicksand-Medium font successfully.")

if __name__ == '__main__':
    main()
