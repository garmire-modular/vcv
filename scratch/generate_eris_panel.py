import os
import re
import sys
from pathlib import Path

# Ensure MSYS2 DLLs are found for cairosvg / libcairo
msys_bin = r'C:\msys64\mingw64\bin'
if os.path.exists(msys_bin):
    os.environ['PATH'] = msys_bin + os.path.pathsep + os.environ.get('PATH', '')
    if hasattr(os, 'add_dll_directory'):
        try:
            os.add_dll_directory(msys_bin)
        except Exception:
            pass

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops
import cairosvg
from PIL import Image

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

def render_section_divider(center_y, panel_w, hi_color="#999999", sh_color="#505050"):
    divider_w = panel_w * 0.81
    x_start = panel_w - divider_w
    top_y = center_y - 0.150
    bot_y = center_y
    return f'''  <!-- Section Divider at Center Y = {center_y:.2f}mm (Width = {divider_w:.3f}mm, Right-Aligned, Total Height = 0.300mm) -->
  <g>
    <!-- Top Highlight Line ({hi_color}) -->
    <rect x="{x_start:.3f}" y="{top_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{hi_color}"/>
    <!-- Bottom Shadow Line ({sh_color}) -->
    <rect x="{x_start:.3f}" y="{bot_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{sh_color}"/>
  </g>'''

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    # Panel dimensions: 14 HP (71.12 mm x 128.50 mm)
    panel_w = 71.12
    panel_h = 128.50

    # Title "eris" in Node.otf (scale 0.0048, baseline 7.620, centered in 71.12 mm)
    scale = 0.0048
    text = "eris"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else hmtx[c][0] * scale for c in text)
    start_x = (panel_w - total_w) / 2.0

    title_block = ['  <!-- Label: "eris" -->']
    curr_x = start_x
    for c in text:
        gname = cmap[ord(c)] if ord(c) in cmap else c
        w = hmtx[gname][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v1.0.3"
    version_block = [
        '  <!-- Label: "v1.0.3" -->',
        render_qs_text(qs_reg_font, "v1.0.3", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v1.0.3")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.1f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.1f}">',
        f'  <!-- Panel Background: 14 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.1f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge (New module default placeholder: #5d5d5d neutral gray per AGENTS.md 7.6.2) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        render_section_divider(57.00, panel_w),
        '',
        render_section_divider(82.00, panel_w),
        '',
        '  <!-- Range Button Indicator Ring Track (Center X = 26.23mm, Y = 20.00mm, R = 4.12mm, Stroke = 1.40mm) -->',
        '  <circle cx="26.230" cy="20.000" r="4.120" fill="none" stroke="#3a3a3a" stroke-width="1.400"/>',
        '',
        '  <!-- 2mm Red Light Bezel (Horizontally centered between Reseed and Rate: X = 53.725mm, Y = 37.00mm, D = 2.0mm) -->',
        '  <circle cx="53.725" cy="37.000" r="1.300" fill="#2b2b2b" stroke="#1c1c1c" stroke-width="0.250"/>',
        '  <circle cx="53.725" cy="37.000" r="1.000" fill="#4a1515"/>',
        '',
        '  <!-- Grouping Line between RESEED and RATE labels (Center Y = 30.20mm) -->',
        '  <line x1="51.000" y1="30.200" x2="56.400" y2="30.200" stroke="#3a3a3a" stroke-width="0.300"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # 4 Columns across 71.12 mm: x = [8.56, 26.23, 44.89, 62.56] mm
    col_x = [8.56, 26.23, 44.89, 62.56]

    # Row 1 Knobs & Controls (Center Y = 20.00, label baseline Y = 14.500)
    # Col 0: FREQ, Col 1: RANGE, Col 2: FINE, Col 3: MODEL
    svg_parts.append(render_qs_text(qs_font, "FREQ", col_x[0], 14.500, 0.002800, "#1c1c1c", "FREQ Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "RANGE", col_x[1], 14.500, 0.002400, "#1c1c1c", "RANGE Button Label"))
    svg_parts.append(render_qs_text(qs_font, "FINE", col_x[2], 14.500, 0.002800, "#1c1c1c", "FINE Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "MODEL", col_x[3], 14.500, 0.002800, "#1c1c1c", "MODEL Knob Label"))

    # Row 2 Knobs & Controls (Center Y = 37.00, label baseline Y = 31.500)
    # Col 0: COMPLEX, Col 1: ORBIT, Col 2: RESEED (button), Col 3: RATE
    svg_parts.append(render_qs_text(qs_font, "COMPLEX", col_x[0], 31.500, 0.002300, "#1c1c1c", "COMPLEX Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "ORBIT", col_x[1], 31.500, 0.002500, "#1c1c1c", "ORBIT Knob Label"))
    svg_parts.append(render_qs_text(qs_font, "RESEED", col_x[2], 31.500, 0.002300, "#1c1c1c", "RESEED Button Label"))
    svg_parts.append(render_qs_text(qs_font, "RATE", col_x[3], 31.500, 0.002500, "#1c1c1c", "RATE Knob Label"))

    # Zone 3: CV Attenuverter Trimpots
    # Row 1 Trimpots (4 trimpots): FREQ, FM, MODEL, COMPLEX (Center Y = 66.00, label baseline Y = 62.000)
    svg_parts.append(render_qs_text(qs_font, "FREQ", col_x[0], 62.000, 0.001900, "#2c2c2c", "FREQ CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "FM", col_x[1], 62.000, 0.001900, "#2c2c2c", "FM CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "MODEL", col_x[2], 62.000, 0.001900, "#2c2c2c", "MODEL CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "COMPLEX", col_x[3], 62.000, 0.001700, "#2c2c2c", "COMPLEX CV Trim Label"))

    # Row 2 Trimpots (3 trimpots, right-aligned): ORBIT, RATE, Z OUT (Center Y = 77.00, label baseline Y = 73.000)
    svg_parts.append(render_qs_text(qs_font, "ORBIT", col_x[1], 73.000, 0.001900, "#2c2c2c", "ORBIT CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "RATE", col_x[2], 73.000, 0.001900, "#2c2c2c", "RATE CV Trim Label"))
    svg_parts.append(render_qs_text(qs_font, "Z OUT", col_x[3], 73.000, 0.001800, "#2c2c2c", "Z OUT Trim Label"))

    # Zone 4: I/O Jacks
    # Jack Row 1 (Inputs): FREQ, FM, MODEL, COMPLEX (Center Y = 93.00, label baseline Y = 87.000)
    svg_parts.append(render_qs_text(qs_font, "FREQ", col_x[0], 87.000, 0.002300, "#2c2c2c", "FREQ Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "FM", col_x[1], 87.000, 0.002300, "#2c2c2c", "FM Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "MODEL", col_x[2], 87.000, 0.002300, "#2c2c2c", "MODEL Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "COMPLEX", col_x[3], 87.000, 0.002000, "#2c2c2c", "COMPLEX Jack Label"))

    # Jack Row 2 (Inputs): ORBIT, RESEED, RATE, SYNC (Center Y = 106.50, label baseline Y = 100.500)
    svg_parts.append(render_qs_text(qs_font, "ORBIT", col_x[0], 100.500, 0.002300, "#2c2c2c", "ORBIT Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "RESEED", col_x[1], 100.500, 0.002000, "#2c2c2c", "RESEED Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "RATE", col_x[2], 100.500, 0.002300, "#2c2c2c", "RATE Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "SYNC", col_x[3], 100.500, 0.002300, "#2c2c2c", "SYNC In Jack Label"))

    # Jack Row 3 (Outputs): X, Y, Z, SYNC (Center Y = 120.00, label baseline Y = 114.000)
    svg_parts.append(render_qs_text(qs_font, "X", col_x[0], 114.000, 0.002400, "#2c2c2c", "X Output Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "Y", col_x[1], 114.000, 0.002400, "#2c2c2c", "Y Output Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "Z", col_x[2], 114.000, 0.002400, "#2c2c2c", "Z Output Jack Label"))
    svg_parts.append(render_qs_text(qs_font, "SYNC", col_x[3], 114.000, 0.002400, "#2c2c2c", "SYNC Output Jack Label"))

    svg_parts.append('</svg>\n')
    svg_content = "\n".join(svg_parts)

    out_svg = "res/Eris.svg"
    with open(out_svg, "w", encoding="utf-8") as f:
        f.write(svg_content)
    print(f"Generated {out_svg}")

    # Render PNG for MetaModule (height 240px)
    png_h = 240
    png_w = int(round(panel_w * (png_h / panel_h)))  # 14 HP => 133px
    out_png = "metamodule/assets/Eris.png"
    cairosvg.svg2png(bytestring=svg_content.encode('utf-8'),
                     write_to=out_png,
                     output_width=png_w,
                     output_height=png_h)
    print(f"Generated {out_png} ({png_w}x{png_h})")

if __name__ == "__main__":
    main()
