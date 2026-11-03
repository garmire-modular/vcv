import os
import re
import subprocess
import sys

batch4_modules = [
    {
        'slug': 'Instability',
        'title': 'INSTABILITY',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="50.673" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="70.473" width="1.350" height="38.227" fill="#005566" stroke="none"/>
  </g>""",
        'delineators': (59.50, 90.00),
        'knobs': [('FREQ', 7.62, 13.070), ('PHASE', 22.86, 13.070), ('AMP', 15.24, 34.500)],
        'trims': [('FREQ', 7.62, 64.500), ('PHASE', 22.86, 64.500), ('AMP', 15.24, 76.500)],
        'jacks': [('FREQ', 7.62, 97.500), ('PHASE', 22.86, 97.500), ('AMP', 15.24, 107.000), ('IN', 7.62, 116.500), ('OUT', 22.86, 116.500)]
    },
    {
        'slug': 'Crest',
        'title': 'CREST',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="42.672" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="62.472" width="1.350" height="17.780" fill="#882255" stroke="none"/>
    <rect x="0.000" y="80.252" width="1.350" height="28.448" fill="#56B4E9" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'knobs': [('X', 7.62, 13.070), ('Y', 22.86, 13.070), ('TILT', 15.24, 34.500)],
        'trims': [('AMT CV', 7.62, 56.500), ('AMT CV', 22.86, 56.500), ('TILT CV', 7.62, 68.500), ('TILT CV', 22.86, 68.500)],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('AMT.', 15.24, 97.500), ('TILT', 15.24, 107.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Quake',
        'title': 'QUAKE',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="44.450" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="64.250" width="1.350" height="28.448" fill="#0072B2" stroke="none"/>
    <rect x="0.000" y="92.698" width="1.350" height="16.002" fill="#56B4E9" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'knobs': [('X', 7.62, 13.070), ('Y', 22.86, 13.070), ('SPEED', 15.24, 34.500)],
        'trims': [('AMT CV', 7.62, 56.500), ('AMT CV', 22.86, 56.500), ('SPEED CV', 7.62, 68.500), ('SPEED CV', 22.86, 68.500)],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('AMT.', 15.24, 97.500), ('SPEED', 15.24, 107.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Steps',
        'title': 'STEPS',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="17.780" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="37.580" width="1.350" height="28.448" fill="#882255" stroke="none"/>
    <rect x="0.000" y="66.028" width="1.350" height="42.672" fill="#EBEC72" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'knobs': [('X', 7.62, 13.070), ('Y', 22.86, 13.070)],
        'trims': [('STEPS', 15.24, 68.500)],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('CV', 15.24, 97.500), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Copycat',
        'title': 'COPYCAT',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="44.450" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="64.250" width="1.350" height="28.448" fill="#CC79A7" stroke="none"/>
    <rect x="0.000" y="92.698" width="1.350" height="16.002" fill="#EBEC72" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'knobs': [('COPIES', 7.62, 13.070), ('SCALE', 22.86, 13.070), ('SHIFT', 7.62, 34.500), ('ROTATE', 22.86, 34.500)],
        'trims': [('COPIES', 7.62, 56.500), ('SCALE', 15.24, 56.500), ('FLIP X', 22.86, 56.500), ('SHIFT', 7.62, 68.500), ('ROTATE', 15.24, 68.500), ('FLIP Y', 22.86, 68.500)],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('CV', 15.24, 97.500), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Petals',
        'title': 'PETALS',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="44.450" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="64.250" width="1.350" height="28.448" fill="#CC79A7" stroke="none"/>
    <rect x="0.000" y="92.698" width="1.350" height="16.002" fill="#56B4E9" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'knobs': [('X', 7.62, 13.070), ('Y', 22.86, 13.070)],
        'trims': [],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('CV', 15.24, 97.500), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Knots',
        'title': 'KNOTS',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="44.450" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="64.250" width="1.350" height="28.448" fill="#D55E00" stroke="none"/>
    <rect x="0.000" y="92.698" width="1.350" height="16.002" fill="#FF8866" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'knobs': [('X', 7.62, 13.070), ('Y', 22.86, 13.070)],
        'trims': [('KNOTS', 15.24, 68.500)],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('CV', 15.24, 97.500), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Vortex',
        'title': 'VORTEX',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="28.448" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="48.248" width="1.350" height="19.558" fill="#D55E00" stroke="none"/>
    <rect x="0.000" y="67.806" width="1.350" height="40.894" fill="#56B4E9" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'knobs': [('X', 7.62, 13.070), ('Y', 22.86, 13.070), ('COUNT', 15.24, 34.500)],
        'trims': [('DEPTH CV', 7.62, 56.500), ('DEPTH CV', 22.86, 56.500), ('COUNT CV', 7.62, 68.500), ('COUNT CV', 22.86, 68.500)],
        'jacks': [('X', 7.62, 84.000), ('Y', 22.86, 84.000), ('IN', 15.24, 88.000), ('DEPTH', 15.24, 97.500), ('COUNT', 15.24, 107.000), ('OUT', 15.24, 116.500)]
    },
    {
        'slug': 'Trough',
        'title': 'TROUGH',
        'badge': """  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="1.350" height="40.894" fill="#FFFFFF" stroke="none"/>
    <rect x="0.000" y="60.694" width="1.350" height="28.448" fill="#882255" stroke="none"/>
    <rect x="0.000" y="89.142" width="1.350" height="19.558" fill="#B8A0E8" stroke="none"/>
  </g>""",
        'delineators': (52.50, 80.50),
        'knobs': [('X', 7.62, 13.070), ('Y', 22.86, 13.070), ('TILT', 15.24, 34.500)],
        'trims': [('TROUGH', 7.62, 56.500), ('TROUGH', 22.86, 56.500), ('TILT', 7.62, 68.500), ('TILT', 22.86, 68.500)],
        'jacks': [('X', 7.62, 88.000), ('Y', 22.86, 88.000), ('TROUGH', 15.24, 97.500), ('TILT', 15.24, 107.000), ('OUT', 15.24, 116.500)]
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
{knobs_code}

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

for m in batch4_modules:
    slug = m['slug']
    delin1, delin2 = m['delineators']
    
    knobs_lines = []
    for txt, x, y in m['knobs']:
        knobs_lines.append(f'    svg_parts.append(render_font_text(ddin_cond_bold, "{txt}", {x:.2f}, {y:.3f}, knob_scale, "#1c1c1c", "{txt}"))')
    knobs_code = "\n".join(knobs_lines)

    trims_lines = []
    for txt, x, y in m['trims']:
        trims_lines.append(f'    svg_parts.append(render_font_text(ddin_cond, "{txt}", {x:.2f}, {y:.3f}, trim_scale, "#2c2c2c", "{txt}"))')
    trims_code = "\n".join(trims_lines)

    jacks_lines = []
    for txt, x, y in m['jacks']:
        col = "#1c1c1c" if txt in ['X', 'Y', 'OUT', 'IN'] else "#2c2c2c"
        jacks_lines.append(f'    svg_parts.append(render_font_text(ddin_cond, "{txt}", {x:.2f}, {y:.3f}, trim_scale, "{col}", "{txt}"))')
    jacks_code = "\n".join(jacks_lines)

    code = template.format(
        slug=slug,
        title=m['title'],
        badge=m['badge'],
        delin1=delin1,
        delin2=delin2,
        knobs_code=knobs_code,
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
