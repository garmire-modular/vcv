import json
import os
from pathlib import Path
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
import cairosvg
from PIL import Image, ImageDraw


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


ddin_bold = TTFont('res/D-DIN-Bold.otf')
ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
ddin_cond = TTFont('res/D-DINCondensed.otf')
qs_reg_font = TTFont('res/Quicksand-Regular.ttf')


def render_text(font, text, center_x, baseline_y, scale, fill, comment="", align="center"):
    text_str = str(text)
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * scale if ord(c) in cmap else 500 * scale for c in text_str]
    total_w = sum(widths)
    if align == "right":
        start_x = center_x - total_w
    elif align == "left":
        start_x = center_x
    else:
        start_x = center_x - total_w / 2.0

    res = []
    if comment:
        res.append(f'  <!-- Label: "{comment}" -->')
    curr = start_x
    for c, w in zip(text_str, widths):
        if c != ' ':
            pd = get_simplified_glyph_path(font, c)
            res.append(
                f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({scale:.6f}, {-scale:.6f})">'
                f'<path d="{pd}" fill="{fill}"/></g>'
            )
        curr += w
    return "\n".join(res)


def get_palette_badge_svg(module_name="Silvia"):
    palette_path = Path("scratch/palette/palette.json")
    if palette_path.exists():
        try:
            with open(palette_path, "r", encoding="utf-8") as f:
                pal_data = json.load(f)
            if module_name in pal_data:
                entry = pal_data[module_name]
                geom = entry.get("badgeGeometry", {})
                bw = geom.get("widthMm", 2.54)
                start_y = geom.get("y", 19.8)
                start_x = geom.get("x", 0.0)
                bands = entry.get("bands", [])
                if bands:
                    lines = ['  <!-- Left Edge Color Badge (from scratch/palette/palette.json) -->',
                             '  <g id="palette-badge">']
                    curr_y = start_y
                    for b in bands:
                        bh = b.get("heightMm", 0.0)
                        hex_col = b.get("hex", b.get("color", "#5d5d5d"))
                        lines.append(
                            f'    <rect x="{start_x:.3f}" y="{curr_y:.3f}" width="{bw:.3f}" height="{bh:.3f}" '
                            f'fill="{hex_col}" stroke="none"/>'
                        )
                        curr_y += bh
                    lines.append('  </g>')
                    return "\n".join(lines)
        except Exception as e:
            print(f"Warning: could not load palette.json: {e}")
    return '''  <!-- Left Edge Color Badge (Placeholder #5d5d5d) -->
  <g id="palette-badge">
    <rect x="0.000" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none"/>
  </g>'''


def render_section_divider(center_y, panel_w, hi_color, sh_color):
    divider_w = panel_w * 0.80
    x_start = panel_w - divider_w
    top_y = center_y - 0.150
    bot_y = center_y
    return f'''  <!-- Section Divider at Center Y = {center_y:.2f}mm (Width = {divider_w:.3f}mm, Right-Aligned, Total Height = 0.300mm) -->
  <g>
    <rect x="{x_start:.3f}" y="{top_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{hi_color}"/>
    <rect x="{x_start:.3f}" y="{bot_y:.3f}" width="{divider_w:.3f}" height="0.150" fill="{sh_color}"/>
  </g>'''


def generate_silvia_svg(skin):
    panel_w = 50.800   # 10 HP = 10 * 5.08 mm
    panel_h = 128.500  # Standard Eurorack 3U
    mid_x = panel_w / 2.0

    # 3-Column Primary Grid (Outer margins 7.00 mm, Center 25.40 mm)
    col3_x = [7.000, 25.400, 43.800]
    # 4-Column Lower Grid (Outer margins 7.00 mm, Pitch 12.267 mm)
    col4_x = [7.000, 19.267, 31.533, 43.800]

    bg_color = skin['bg_color']
    title_color = skin['title_color']
    version_color = skin['version_color']
    label_color = skin['label_color']
    hi_color = skin['hi_color']
    sh_color = skin['sh_color']
    ring_color = skin['ring_color']

    # Mandatory TYPOGRAPHY.md Scale Factors
    s_title = 0.005644     # 16 pt D-DIN-Bold
    s_version = 0.001600   # 4.5 pt Quicksand-Regular
    s_knob = 0.003528      # 10 pt D-DINCondensed-Bold
    s_trim = 0.002822      # 8 pt D-DINCondensed
    s_jack = 0.002822      # 8 pt D-DINCondensed

    svg = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{panel_w:.3f}mm" height="{panel_h:.3f}mm" viewBox="0 0 {panel_w:.3f} {panel_h:.3f}">',
        f'  <!-- Panel Background: 10 HP ({skin["name"]} Skin) -->',
        f'  <rect x="0" y="0" width="{panel_w:.3f}" height="{panel_h:.3f}" fill="{bg_color}" stroke="none"/>',
        get_palette_badge_svg("Silvia"),
        '',
        '  <!-- ZONE 1: HEADER & TITLE -->',
        render_text(ddin_bold, "SILVIA", mid_x, 8.000, s_title, title_color, "SILVIA"),
        render_text(qs_reg_font, "v1.0.0", panel_w - 2.150, 8.000, s_version, version_color, "v1.0.0", align="right"),
        '',
        '  <!-- ZONE 2: PRIMARY KNOBS (3 Columns) -->',
        '  <!-- Row 1 (Center Y = 18.00 mm, Label Y = 12.250 mm): FREQ, RANGE, FINE -->',
        render_text(ddin_cond_bold, "FREQ", col3_x[0], 12.250, s_knob, label_color, "FREQ Knob"),
        render_text(ddin_cond_bold, "RANGE", col3_x[1], 12.250, s_knob, label_color, "RANGE Button"),
        render_text(ddin_cond_bold, "FINE", col3_x[2], 12.250, s_knob, label_color, "FINE Knob"),
        f'  <!-- Range Button Indicator Ring Track (R = 4.12mm, Stroke = 1.40mm) -->',
        f'  <circle cx="{col3_x[1]:.3f}" cy="18.000" r="4.120" fill="none" stroke="{ring_color}" stroke-width="1.400"/>',
        '',
        '  <!-- Row 2 (Center Y = 34.00 mm, Label Y = 28.250 mm): SCALE, ROTATE, REVOLVE -->',
        render_text(ddin_cond_bold, "SCALE", col3_x[0], 28.250, s_knob, label_color, "SCALE Knob"),
        render_text(ddin_cond_bold, "ROTATE", col3_x[1], 28.250, s_knob, label_color, "ROTATE Knob"),
        render_text(ddin_cond_bold, "REVOLVE", col3_x[2], 28.250, s_knob, label_color, "REVOLVE Knob"),
        '',
        '  <!-- SVG Trajectory Preview Bezel (X = 7.40..43.40 mm, Y = 43.00..67.00 mm) -->',
        f'  <rect x="7.400" y="43.000" width="36.000" height="24.000" rx="1.500" fill="#121216" stroke="{ring_color}" stroke-width="0.350"/>',
        '',
        render_section_divider(72.000, panel_w, hi_color, sh_color),
        '',
        '  <!-- ZONE 3: CV ATTENUVERTERS (4 Columns, Center Y = 82.00 mm, Label Y = 78.000 mm) -->',
        render_text(ddin_cond, "SCALE X", col4_x[0], 78.000, s_trim, label_color, "SCALE X CV Depth"),
        render_text(ddin_cond, "SCALE Y", col4_x[1], 78.000, s_trim, label_color, "SCALE Y CV Depth"),
        render_text(ddin_cond, "ROTATE", col4_x[2], 78.000, s_trim, label_color, "ROTATE CV Depth"),
        render_text(ddin_cond, "REVOLVE", col4_x[3], 78.000, s_trim, label_color, "REVOLVE CV Depth"),
        '',
        render_section_divider(94.000, panel_w, hi_color, sh_color),
        '',
        '  <!-- ZONE 4: I/O JACKS (4 Columns, Pitch = 13.00 mm, Bottom Anchor = 117.00 mm) -->',
        '  <!-- Jack Row 1 (CV Inputs, Center Y = 104.00 mm, Label Y = 99.000 mm) -->',
        render_text(ddin_cond, "SCALE X", col4_x[0], 99.000, s_jack, label_color, "SCALE X CV"),
        render_text(ddin_cond, "SCALE Y", col4_x[1], 99.000, s_jack, label_color, "SCALE Y CV"),
        render_text(ddin_cond, "ROTATE", col4_x[2], 99.000, s_jack, label_color, "ROTATE CV"),
        render_text(ddin_cond, "REVOLVE", col4_x[3], 99.000, s_jack, label_color, "REVOLVE CV"),
        '',
        '  <!-- Jack Row 2 (Sync & Outputs, Center Y = 117.00 mm, Label Y = 112.000 mm) -->',
        render_text(ddin_cond, "SYNC IN", col4_x[0], 112.000, s_jack, label_color, "SYNC IN"),
        render_text(ddin_cond, "X", col4_x[1], 112.000, s_jack, label_color, "X OUT"),
        render_text(ddin_cond, "Y", col4_x[2], 112.000, s_jack, label_color, "Y OUT"),
        render_text(ddin_cond, "SYNC OUT", col4_x[3], 112.000, s_jack, label_color, "SYNC OUT"),
        '</svg>'
    ]
    return "\n".join(svg) + "\n"


def main():
    skins = [
        {
            'name': 'Medium',
            'filename': 'res/Silvia.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'version_color': '#aaaaaa',
            'label_color': '#222222',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e',
            'ring_color': '#484848',
        },
        {
            'name': 'Light',
            'filename': 'res/Silvia_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'version_color': '#666666',
            'label_color': '#222222',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8',
            'ring_color': '#9e9e9e',
        },
        {
            'name': 'Dark',
            'filename': 'res/Silvia_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'version_color': '#888888',
            'label_color': '#ffffff',
            'hi_color': '#585858',
            'sh_color': '#202020',
            'ring_color': '#222222',
        },
    ]

    os.makedirs('res', exist_ok=True)
    for skin in skins:
        svg_content = generate_silvia_svg(skin)
        with open(skin['filename'], 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Generated {skin['filename']} ({skin['name']} skin) successfully.")

    # Render 240px MetaModule asset (10 HP = 50.80 mm -> round(240 * 50.80 / 128.50) = 95 px)
    panel_w = 50.800
    panel_h = 128.500
    target_h = 240
    target_w = round(target_h * (panel_w / panel_h))
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url='res/Silvia.svg', write_to='metamodule/assets/Silvia.png',
                     output_width=target_w, output_height=target_h)
    print(f"Rendered metamodule/assets/Silvia.png ({target_w}x{target_h}) successfully.")

    # Render 4x verification bitmap with hardware footprints overlaid
    verify_png = 'scratch/silvia_verify.png'
    cairosvg.svg2png(url='res/Silvia.svg', write_to=verify_png,
                     output_width=target_w * 4, output_height=target_h * 4)
    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (target_w * 4) / panel_w

    col3_x = [7.000, 25.400, 43.800]
    col4_x = [7.000, 19.267, 31.533, 43.800]

    # Row 1 & Row 2 Knobs (R = 4.82 mm) + Range Button (R = 2.60 mm) & Ring (R = 4.12 mm)
    kr = 4.82 * r_px
    for r_idx, ky in enumerate([18.000, 34.000]):
        for c_idx, kx in enumerate(col3_x):
            if r_idx == 0 and c_idx == 1:
                br = 2.60 * r_px
                rr = 4.12 * r_px
                draw.ellipse([kx * r_px - br, ky * r_px - br, kx * r_px + br, ky * r_px + br], outline='#f0e442', width=2)
                draw.ellipse([kx * r_px - rr, ky * r_px - rr, kx * r_px + rr, ky * r_px + rr], outline='#d35fb7', width=2)
            else:
                draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Zone 3 Trimpots (R = 3.04 mm)
    tr = 3.04 * r_px
    for tx in col4_x:
        draw.ellipse([tx * r_px - tr, 82.000 * r_px - tr, tx * r_px + tr, 82.000 * r_px + tr], outline='#00ff88', width=2)

    # Zone 4 Jacks (R = 3.89 mm)
    jr = 3.89 * r_px
    for jy in [104.000, 117.000]:
        for jx in col4_x:
            draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00aaff', width=2)

    im.save(verify_png)
    print(f"Saved verification image to {verify_png}.")


if __name__ == '__main__':
    main()
