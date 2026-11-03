import os
import re
import subprocess
import sys

# Badges and label specs for all 9 spatial transform modules
spatial_modules = [
    {
        'slug': 'Scale',
        'title': 'SCALE',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="40.894" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="60.694" width="1.350" height="28.448" fill="#005566" stroke="none"/>
    <rect x="0.000" y="89.142" width="1.350" height="19.558" fill="#EBEC72" stroke="none"/>
  </g>""",
        'delineators': (71.12, 85.09),
        'k1': ('X', 7.62, 13.070), 'k2': ('Y', 22.86, 13.070),
        'trims': [('X CV', 7.62, 74.210), ('Y CV', 22.86, 74.210)],
        'jacks': [('X', 7.62, 88.508), ('Y', 22.86, 88.508), ('IN', 15.24, 92.500), ('CV', 15.24, 102.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Position',
        'title': 'POSITION',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="16.002" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="35.802" width="1.350" height="44.450" fill="#442288" stroke="none"/>
    <rect x="0.000" y="80.252" width="1.350" height="28.448" fill="#56B4E9" stroke="none"/>
  </g>""",
        'delineators': (71.12, 85.09),
        'k1': ('X', 7.62, 13.070), 'k2': ('Y', 22.86, 13.070),
        'trims': [('X CV', 7.62, 74.210), ('Y CV', 22.86, 74.210)],
        'jacks': [('X', 7.62, 88.508), ('Y', 22.86, 88.508), ('IN', 15.24, 92.500), ('CV', 15.24, 102.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Rotate',
        'title': 'ROTATE',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="28.448" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="48.248" width="1.350" height="16.002" fill="#0072B2" stroke="none"/>
    <rect x="0.000" y="64.250" width="1.350" height="44.450" fill="#EBEC72" stroke="none"/>
  </g>""",
        'delineators': (71.12, 85.09),
        'k1': ('ROTATE', 7.62, 13.070), 'k2': ('AUTO', 22.86, 13.070),
        'trims': [('ROT CV', 7.62, 74.210), ('AUTO CV', 22.86, 74.210)],
        'jacks': [('X', 7.62, 88.508), ('Y', 22.86, 88.508), ('IN', 15.24, 92.500), ('CV', 15.24, 102.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Spin',
        'title': 'SPIN',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="28.448" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="48.248" width="1.350" height="16.002" fill="#0072B2" stroke="none"/>
    <rect x="0.000" y="64.250" width="1.350" height="44.450" fill="#FF8866" stroke="none"/>
  </g>""",
        'delineators': (71.12, 85.09),
        'k1': ('X', 7.62, 13.070), 'k2': ('Y', 22.86, 13.070),
        'trims': [('X CV', 7.62, 74.210), ('Y CV', 22.86, 74.210)],
        'jacks': [('X', 7.62, 88.508), ('Y', 22.86, 88.508), ('IN', 15.24, 92.500), ('CV', 15.24, 102.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Fold',
        'title': 'FOLD',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="28.448" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="48.248" width="1.350" height="19.558" fill="#442288" stroke="none"/>
    <rect x="0.000" y="67.806" width="1.350" height="40.894" fill="#B8A0E8" stroke="none"/>
  </g>""",
        'delineators': (71.12, 85.09),
        'k1': ('X', 7.62, 13.070), 'k2': ('Y', 22.86, 13.070),
        'trims': [('X CV', 7.62, 74.210), ('Y CV', 22.86, 74.210)],
        'jacks': [('X', 7.62, 88.508), ('Y', 22.86, 88.508), ('IN', 15.24, 92.500), ('CV', 15.24, 102.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Stretch',
        'title': 'STRETCH',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="42.672" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="62.472" width="1.350" height="17.780" fill="#005566" stroke="none"/>
    <rect x="0.000" y="80.252" width="1.350" height="28.448" fill="#B8A0E8" stroke="none"/>
  </g>""",
        'delineators': (71.12, 85.09),
        'k1': ('X', 7.62, 13.070), 'k2': ('Y', 22.86, 13.070),
        'trims': [('X CV', 7.62, 74.210), ('Y CV', 22.86, 74.210)],
        'jacks': [('X', 7.62, 88.508), ('Y', 22.86, 88.508), ('IN', 15.24, 92.500), ('CV', 15.24, 102.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Shear',
        'title': 'SHEAR',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="28.448" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="48.248" width="1.350" height="16.002" fill="#D55E00" stroke="none"/>
    <rect x="0.000" y="64.250" width="1.350" height="44.450" fill="#B8A0E8" stroke="none"/>
  </g>""",
        'delineators': (71.12, 85.09),
        'k1': ('X', 7.62, 13.070), 'k2': ('Y', 22.86, 13.070),
        'trims': [('X CV', 7.62, 74.210), ('Y CV', 22.86, 74.210)],
        'jacks': [('X', 7.62, 88.508), ('Y', 22.86, 88.508), ('IN', 15.24, 93.260), ('CV', 15.24, 104.690), ('OUT', 15.24, 116.120)]
    },
    {
        'slug': 'Smooth',
        'title': 'SMOOTH',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="16.002" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="35.802" width="1.350" height="44.450" fill="#005566" stroke="none"/>
    <rect x="0.000" y="80.252" width="1.350" height="28.448" fill="#FF8866" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'k1': ('SMOOTH', 7.62, 13.070), 'k2': ('SLOPE', 22.86, 13.070),
        'trims': [('SMOOTH', 7.62, 56.500), ('SLOPE', 22.86, 56.500)],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('SMOOTH', 15.24, 97.500), ('SLOPE', 15.24, 107.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Wiggle',
        'title': 'WIGGLE',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="28.448" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="48.248" width="1.350" height="42.672" fill="#CC79A7" stroke="none"/>
    <rect x="0.000" y="90.920" width="1.350" height="17.780" fill="#B8A0E8" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'k1': ('AMT.', 7.62, 13.070), 'k2': ('RATE', 22.86, 13.070),
        'trims': [('DEPTH CV', 7.62, 56.500), ('Y DEPTH CV', 22.86, 56.500), ('RATE CV', 7.62, 68.500), ('Y RATE CV', 22.86, 68.500)],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('AMT.', 15.24, 97.500), ('RATE', 15.24, 107.000), ('OUT', 15.24, 116.500)]
    }
]

template = """import os
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
    path_d = re.sub(r'(\\d+\\.\\d+)', lambda m: f'{{float(m.group(1)):.1f}}', path_d)
    path_d = re.sub(r'([MLCQZHVmlcqzhv])', r' \\1 ', path_d)
    path_d = re.sub(r'\\s+', ' ', path_d).strip()
    return path_d

def render_font_text(font, text, center_x, baseline_y, scale, fill, comment=""):
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else 500 * scale for c in text]
    total_w = sum(widths)
    start_x = center_x - total_w / 2.0
    res = []
    if comment:
        res.append(f'  <!-- Label: "{{comment}}" -->')
    curr = start_x
    for c, w in zip(text, widths):
        if c != ' ':
            pd = get_simplified_glyph_path(font, c)
            res.append(f'    <g transform="translate({{curr:.3f}}, {{baseline_y:.3f}}) scale({{scale:.6f}}, {{-scale:.6f}})"><path d="{{pd}}" fill="{{fill}}"/></g>')
        curr += w
    return "\\n".join(res)

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
    raw_w = sum(hmtx[cmap[ord(c)]][0] * title_scale if ord(c) in cmap else 500 * title_scale for c in "{title}")
    if raw_w > (panel_w - 3.0):
        title_scale = title_scale * ((panel_w - 3.0) / raw_w)

    title_block = [
        '  <!-- Label: "{title}" (D-DIN-Bold) -->',
        render_font_text(ddin_bold, "{title}", panel_w / 2.0, 7.620, title_scale, "#ffffff", "{title}")
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
        '''{badge}''',
        '',
        '  <!-- Delineator Line 1 -->',
        '  <line x1="2.54" y1="{delin1:.2f}" x2="27.94" y2="{delin1:.2f}" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 -->',
        '  <line x1="2.54" y1="{delin2:.2f}" x2="27.94" y2="{delin2:.2f}" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # Main Knobs (D-DINCondensed-Bold, 10pt = 0.003528, preserving baseline Ys)
    svg_parts.append(render_font_text(ddin_cond_bold, "{k1_text}", {k1_x:.2f}, {k1_y:.3f}, knob_scale, "#1c1c1c", "{k1_text}"))
    svg_parts.append(render_font_text(ddin_cond_bold, "{k2_text}", {k2_x:.2f}, {k2_y:.3f}, knob_scale, "#1c1c1c", "{k2_text}"))

    # Attenuverters (D-DINCondensed, 8pt = 0.002822, preserving baseline Ys)
{trims_code}

    # I/O Jacks (D-DINCondensed, 8pt = 0.002822, preserving baseline Ys)
{jacks_code}

    svg_parts.append('</svg>\\n')

    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('{slug}', "\\n".join(svg_parts) + "\\n")
    print("Generated res/{slug}.svg with D-DIN font hierarchy successfully.")

if __name__ == '__main__':
    main()
"""

for m in spatial_modules:
    slug = m['slug']
    delin1, delin2 = m['delineators']
    k1_text, k1_x, k1_y = m['k1']
    k2_text, k2_x, k2_y = m['k2']
    
    trims_lines = []
    for txt, x, y in m['trims']:
        trims_lines.append(f'    svg_parts.append(render_font_text(ddin_cond, "{txt}", {x:.2f}, {y:.3f}, trim_scale, "#2c2c2c", "{txt}"))')
    trims_code = "\n".join(trims_lines)

    jacks_lines = []
    for txt, x, y in m['jacks']:
        col = "#1c1c1c" if txt in ['X', 'Y', 'OUT'] else "#2c2c2c"
        jacks_lines.append(f'    svg_parts.append(render_font_text(ddin_cond, "{txt}", {x:.2f}, {y:.3f}, trim_scale, "{col}", "{txt}"))')
    jacks_code = "\n".join(jacks_lines)

    code = template.format(
        slug=slug,
        title=m['title'],
        badge=m['badge'],
        delin1=delin1,
        delin2=delin2,
        k1_text=k1_text, k1_x=k1_x, k1_y=k1_y,
        k2_text=k2_text, k2_x=k2_x, k2_y=k2_y,
        trims_code=trims_code,
        jacks_code=jacks_code
    )

    fpath = f"scratch/generate_{slug.lower()}_panel.py"
    with open(fpath, "w", encoding="utf-8") as f:
        f.write(code)
    print(f"Written {fpath}")

    r = subprocess.run(["python", fpath], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"ERROR in {fpath}: {r.stderr}")
        sys.exit(1)
    print(f"Successfully generated {slug}")
