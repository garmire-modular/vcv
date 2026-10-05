import os
import re
import sys

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
import resvg_py

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

MODULES = [
    {
        "slug": "BitMorton",
        "title": "bit morton",
        "p1": "SHIFT", "p2": "STRIDE",
        "p3": "MORPH", "p4": "MIX"
    },
    {
        "slug": "BitReverse",
        "title": "bit reverse",
        "p1": "WIDTH", "p2": "OFFSET",
        "p3": "SKEW", "p4": "MIX"
    },
    {
        "slug": "BitTranspose",
        "title": "bit transpose",
        "p1": "PLANE A", "p2": "PLANE B",
        "p3": "CYCLE", "p4": "MIX"
    },
    {
        "slug": "BitValanche",
        "title": "bit valanche",
        "p1": "MASK", "p2": "SHIFT",
        "p3": "BORROW", "p4": "MIX"
    },
    {
        "slug": "BitPermute",
        "title": "bit permute",
        "p1": "MODE", "p2": "ROTATE",
        "p3": "STRIDE", "p4": "MIX"
    },
    {
        "slug": "BitGrayBin",
        "title": "bit graybin",
        "p1": "DEPTH", "p2": "MODE",
        "p3": "TAP", "p4": "MIX"
    },
    {
        "slug": "BitGalois",
        "title": "bit galois",
        "p1": "POLY", "p2": "ALPHA",
        "p3": "MODE", "p4": "MIX"
    },
    {
        "slug": "Bitomata",
        "title": "bitomata",
        "p1": "RULE", "p2": "STEPS",
        "p3": "INJECT", "p4": "MIX"
    },
    {
        "slug": "BitHamming",
        "title": "bit hamming",
        "p1": "GAIN", "p2": "MODE",
        "p3": "MUTUAL", "p4": "MIX"
    }
]

def generate_panel_svg(m, node_font, qs_font, qs_reg_font):
    panel_w = 30.48
    panel_h = 128.50
    center_x = panel_w / 2.0  # 15.24 mm

    # Title in Node.otf (scale 0.0044, baseline 7.620)
    hmtx = node_font['hmtx']
    cmap_node = node_font.getBestCmap()
    scale_title = 0.0040 if len(m['title']) > 11 else 0.0044
    text = m['title']
    
    widths = []
    for c in text:
        if c == ' ':
            widths.append(500 * scale_title)
        else:
            gname = cmap_node[ord(c)] if cmap_node and ord(c) in cmap_node else c
            widths.append(hmtx[gname][0] * scale_title)
    total_w = sum(widths)
    start_x = (panel_w - total_w) / 2.0

    title_block = [f'  <!-- Label: "{m["title"]}" -->']
    curr_x = start_x
    for c, w in zip(text, widths):
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale_title:.6f}, {-scale_title:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.26.0"
    version_block = [
        '  <!-- Label: "v2.26.0" -->',
        render_qs_text(qs_reg_font, "v2.26.0", center_x, 10.414, 0.001600, "#aaaaaa", "v2.26.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.2f}mm" height="{panel_h:.2f}mm" viewBox="0 0 {panel_w:.2f} {panel_h:.2f}">',
        '  <!-- Panel Background: 6 HP -->',
        f'  <rect width="{panel_w:.2f}" height="{panel_h:.2f}" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge: Default Placeholder #5d5d5d (Centered 88.9mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters at Y = 52.50mm) -->',
        '  <line x1="2.54" y1="52.50" x2="27.94" y2="52.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks at Y = 80.50mm) -->',
        '  <line x1="2.54" y1="80.50" x2="27.94" y2="80.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # Knob Columns: X1 = 7.62 mm, X2 = 22.86 mm
    x1 = 7.62
    x2 = 22.86

    # Top Knobs Row 1 Labels (Y = 13.070)
    svg_parts.append(render_qs_text(qs_font, m['p1'], x1, 13.070, 0.002200, "#1c1c1c", m['p1']))
    svg_parts.append(render_qs_text(qs_font, m['p2'], x2, 13.070, 0.002200, "#1c1c1c", m['p2']))

    # Knob Row 2 Labels (Y = 34.500)
    svg_parts.append(render_qs_text(qs_font, m['p3'], x1, 34.500, 0.002200, "#1c1c1c", m['p3']))
    svg_parts.append(render_qs_text(qs_font, m['p4'], x2, 34.500, 0.002200, "#1c1c1c", m['p4']))

    # Trimpot Row 1 Labels (Y = 56.500, scale = 0.001800)
    svg_parts.append(render_qs_text(qs_font, m['p1'], x1, 56.500, 0.001800, "#2c2c2c", f"{m['p1']} TRIM"))
    svg_parts.append(render_qs_text(qs_font, m['p2'], x2, 56.500, 0.001800, "#2c2c2c", f"{m['p2']} TRIM"))

    # Trimpot Row 2 Labels (Y = 68.500, scale = 0.001800)
    svg_parts.append(render_qs_text(qs_font, m['p3'], x1, 68.500, 0.001800, "#2c2c2c", f"{m['p3']} TRIM"))
    svg_parts.append(render_qs_text(qs_font, m['p4'], x2, 68.500, 0.001800, "#2c2c2c", f"{m['p4']} TRIM"))

    # Jack Row 1: X IN, Y IN (Y = 84.000 for X/Y, Y = 88.000 for centered IN)
    svg_parts.append(render_qs_text(qs_font, "X", x1, 84.000, 0.003000, "#1c1c1c", "X"))
    svg_parts.append(render_qs_text(qs_font, "Y", x2, 84.000, 0.003000, "#1c1c1c", "Y"))
    svg_parts.append(render_qs_text(qs_font, "IN", center_x, 88.000, 0.002400, "#2c2c2c", "IN"))

    # Jack Row 2: Centered CV text between CV Row 1 jacks (Y = 97.500)
    svg_parts.append(render_qs_text(qs_font, "CV", center_x, 97.500, 0.002400, "#2c2c2c", "CV"))

    # Jack Row 3: Centered CV text between CV Row 2 jacks (Y = 107.000)
    svg_parts.append(render_qs_text(qs_font, "CV", center_x, 107.000, 0.002400, "#2c2c2c", "CV"))

    # Jack Row 4: Centered OUT text (Y = 116.500)
    svg_parts.append(render_qs_text(qs_font, "OUT", center_x, 116.500, 0.002400, "#2c2c2c", "OUT"))

    svg_parts.append('</svg>\n')
    return "\n".join(svg_parts)

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    for m in MODULES:
        slug = m['slug']
        svg_content = generate_panel_svg(m, node_font, qs_font, qs_reg_font)
        svg_path = f'res/{slug}.svg'
        with open(svg_path, 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Generated {svg_path}")

        # Write bespoke generator script in scratch/
        gen_script_path = f'scratch/generate_{slug.lower()}_panel.py'
        # Self-contained individual generator script
        script_body = f"""import os
import re
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops
import resvg_py

# Auto-generated panel generator for {slug}
from generate_bitmodules_panels import MODULES, generate_panel_svg

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')
    m = next(item for item in MODULES if item["slug"] == "{slug}")
    svg_content = generate_panel_svg(m, node_font, qs_font, qs_reg_font)
    with open('res/{slug}.svg', 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print("Generated res/{slug}.svg")

    svg_mod = re.sub(r'width="[0-9.]+mm"', 'width="57"', svg_content)
    svg_mod = re.sub(r'height="[0-9.]+mm"', 'height="240"', svg_mod)
    png_bytes = resvg_py.svg_to_bytes(svg_string=svg_mod, width=57, height=240)
    with open('metamodule/assets/{slug}.png', 'wb') as pf:
        pf.write(png_bytes)
    print("Rendered metamodule/assets/{slug}.png")

if __name__ == '__main__':
    main()
"""
        with open(gen_script_path, 'w', encoding='utf-8') as gf:
            gf.write(script_body)

        # Render 6HP MetaModule PNG (57 x 240 px)
        svg_mod = re.sub(r'width="[0-9.]+mm"', 'width="57"', svg_content)
        svg_mod = re.sub(r'height="[0-9.]+mm"', 'height="240"', svg_mod)
        png_bytes = resvg_py.svg_to_bytes(svg_string=svg_mod, width=57, height=240)
        png_path = f'metamodule/assets/{slug}.png'
        with open(png_path, 'wb') as pf:
            pf.write(png_bytes)
        print(f"Rendered {png_path} (57x240)")

if __name__ == '__main__':
    main()
