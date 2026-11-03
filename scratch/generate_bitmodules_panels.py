import os
import re
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops
import resvg_py
from skin_generator_helper import write_triple_skins

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

MODULES = [
    {
        "slug": "BitMorton",
        "title": "BIT MORTON",
        "p1": "SHIFT", "p2": "STRIDE",
        "p3": "MORPH", "p4": "MIX"
    },
    {
        "slug": "BitReverse",
        "title": "BIT REVERSE",
        "p1": "SPAN", "p2": "STEP",
        "p3": "BITS", "p4": "MIX"
    },
    {
        "slug": "BitTranspose",
        "title": "BIT TRANSPOSE",
        "p1": "GRID", "p2": "PERM",
        "p3": "DEPTH", "p4": "MIX"
    },
    {
        "slug": "BitValanche",
        "title": "BIT VALANCHE",
        "p1": "DIFF", "p2": "ROUNDS",
        "p3": "CHAOS", "p4": "MIX"
    },
    {
        "slug": "BitPermute",
        "title": "BIT PERMUTE",
        "p1": "ORDER", "p2": "ROT",
        "p3": "MASK", "p4": "MIX"
    },
    {
        "slug": "BitGrayBin",
        "title": "BIT GRAY/BIN",
        "p1": "MODE", "p2": "INV",
        "p3": "FLIP", "p4": "MIX"
    },
    {
        "slug": "BitGalois",
        "title": "BIT GALOIS",
        "p1": "POLY", "p2": "TAPS",
        "p3": "CYCLE", "p4": "MIX"
    },
    {
        "slug": "Bitomata",
        "title": "BITOMATA",
        "p1": "RULE", "p2": "GEN",
        "p3": "SEEDS", "p4": "MIX"
    },
    {
        "slug": "BitHamming",
        "title": "BIT HAMMING",
        "p1": "WEIGHT", "p2": "DIST",
        "p3": "PARITY", "p4": "MIX"
    }
]

def generate_panel_svg(m, ddin_bold, ddin_cond_bold, ddin_cond, qs_reg_font):
    panel_w = 30.48
    panel_h = 128.50
    center_x = panel_w / 2.0  # 15.24 mm

    # Title: D-DIN-Bold, 16pt (0.005644), fitted to panel if wider than 26.5mm
    base_title_scale = 16.0 * 25.4 / (72.0 * ddin_bold['head'].unitsPerEm)  # 0.005644
    text = m['title']
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in text)
    title_scale = base_title_scale
    if raw_w > 26.5:
        title_scale = base_title_scale * (26.5 / raw_w)

    title_block = [
        f'  <!-- Label: "{m["title"]}" (D-DIN-Bold) -->',
        render_font_text(ddin_bold, text, center_x, 7.620, title_scale, "#ffffff", m["title"])
    ]

    # Version tag "v2.0.0" (keep existing baseline Y = 10.414)
    version_block = [
        '  <!-- Label: "v2.0.0" -->',
        render_font_text(qs_reg_font, "v2.0.0", center_x, 10.414, 0.001600, "#aaaaaa", "v2.0.0")
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

    # Main Knobs (D-DINCondensed-Bold, 10pt = 0.003528, keep existing baseline Ys)
    knob_scale = 10.0 * 25.4 / (72.0 * ddin_cond_bold['head'].unitsPerEm)  # 0.003528
    svg_parts.append(render_font_text(ddin_cond_bold, m['p1'], x1, 13.070, knob_scale, "#1c1c1c", m['p1']))
    svg_parts.append(render_font_text(ddin_cond_bold, m['p2'], x2, 13.070, knob_scale, "#1c1c1c", m['p2']))
    svg_parts.append(render_font_text(ddin_cond_bold, m['p3'], x1, 34.500, knob_scale, "#1c1c1c", m['p3']))
    svg_parts.append(render_font_text(ddin_cond_bold, m['p4'], x2, 34.500, knob_scale, "#1c1c1c", m['p4']))

    # Trimpots (D-DINCondensed, 8pt = 0.002822, keep existing baseline Ys)
    trim_scale = 8.0 * 25.4 / (72.0 * ddin_cond['head'].unitsPerEm)  # 0.002822
    svg_parts.append(render_font_text(ddin_cond, m['p1'], x1, 56.500, trim_scale, "#2c2c2c", f"{m['p1']} TRIM"))
    svg_parts.append(render_font_text(ddin_cond, m['p2'], x2, 56.500, trim_scale, "#2c2c2c", f"{m['p2']} TRIM"))
    svg_parts.append(render_font_text(ddin_cond, m['p3'], x1, 68.500, trim_scale, "#2c2c2c", f"{m['p3']} TRIM"))
    svg_parts.append(render_font_text(ddin_cond, m['p4'], x2, 68.500, trim_scale, "#2c2c2c", f"{m['p4']} TRIM"))

    # I/O Jacks (D-DINCondensed, 8pt = 0.002822, keep existing baseline Ys)
    svg_parts.append(render_font_text(ddin_cond, "X", x1, 84.000, trim_scale, "#1c1c1c", "X"))
    svg_parts.append(render_font_text(ddin_cond, "Y", x2, 84.000, trim_scale, "#1c1c1c", "Y"))
    svg_parts.append(render_font_text(ddin_cond, "IN", center_x, 88.000, trim_scale, "#2c2c2c", "IN"))
    svg_parts.append(render_font_text(ddin_cond, "CV", center_x, 97.500, trim_scale, "#2c2c2c", "CV"))
    svg_parts.append(render_font_text(ddin_cond, "CV", center_x, 107.000, trim_scale, "#2c2c2c", "CV"))
    svg_parts.append(render_font_text(ddin_cond, "OUT", center_x, 116.500, trim_scale, "#2c2c2c", "OUT"))

    svg_parts.append('</svg>\n')
    return "\n".join(svg_parts)

def main():
    ddin_bold = TTFont('res/D-DIN-Bold.otf')
    ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
    ddin_cond = TTFont('res/D-DINCondensed.otf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    for m in MODULES:
        slug = m['slug']
        svg_content = generate_panel_svg(m, ddin_bold, ddin_cond_bold, ddin_cond, qs_reg_font)
        write_triple_skins(slug, svg_content)

        # Write bespoke generator script in scratch/
        gen_script_path = f'scratch/generate_{slug.lower()}_panel.py'
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
    ddin_bold = TTFont('res/D-DIN-Bold.otf')
    ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
    ddin_cond = TTFont('res/D-DINCondensed.otf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')
    m = next(item for item in MODULES if item["slug"] == "{slug}")
    svg_content = generate_panel_svg(m, ddin_bold, ddin_cond_bold, ddin_cond, qs_reg_font)
    import sys
    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('{slug}', svg_content)

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
