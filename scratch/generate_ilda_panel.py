import json
import os
from pathlib import Path
import re

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


def get_palette_badge_svg(module_name="Ilda"):
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


def generate_ilda_svg(skin):
    panel_w = 71.120   # 14 HP = 14 * 5.08 mm
    panel_h = 128.500  # Standard Eurorack 3U
    mid_x = panel_w / 2.0

    # 5-Column Grid (Outer margins 7.00 mm, Center 35.56 mm, Pitch 14.280 mm)
    col5_x = [7.000, 21.280, 35.560, 49.840, 64.120]

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
        f'  <!-- Panel Background: 14 HP ({skin["name"]} Skin) -->',
        f'  <rect x="0" y="0" width="{panel_w:.3f}" height="{panel_h:.3f}" fill="{bg_color}" stroke="none"/>',
        get_palette_badge_svg("Ilda"),
        '',
        '  <!-- ZONE 1: HEADER & TITLE -->',
        render_text(ddin_bold, "ILDA", mid_x, 8.000, s_title, title_color, "ILDA"),
        render_text(qs_reg_font, "v1.0.0", panel_w - 2.150, 8.000, s_version, version_color, "v1.0.0", align="right"),
        '',
        '  <!-- Large Live Laser Preview Bezel (X = 7.40..63.72 mm, Y = 11.00..41.00 mm) -->',
        f'  <rect x="7.400" y="11.000" width="56.320" height="30.000" rx="1.500" fill="#101014" stroke="{ring_color}" stroke-width="0.350"/>',
        '',
        '  <!-- ZONE 2: PRIMARY CONTROLS (5 Columns on One Line, Center Y = 52.00 mm, Label Y = 46.250 mm) -->',
        render_text(ddin_cond_bold, "FREQ", col5_x[0], 46.250, s_knob, label_color, "FREQ Knob"),
        render_text(ddin_cond_bold, "RANGE", col5_x[1], 46.250, s_knob, label_color, "RANGE Button"),
        render_text(ddin_cond_bold, "FINE", col5_x[2], 46.250, s_knob, label_color, "FINE Knob"),
        render_text(ddin_cond_bold, "SCALE", col5_x[3], 46.250, s_knob, label_color, "SCALE Knob"),
        render_text(ddin_cond_bold, "ROTATE", col5_x[4], 46.250, s_knob, label_color, "ROTATE Knob"),
        '  <!-- Range Button Indicator Ring Track (R = 4.12mm, Stroke = 1.40mm) -->',
        f'  <circle cx="{col5_x[1]:.3f}" cy="52.000" r="4.120" fill="none" stroke="{ring_color}" stroke-width="1.400"/>',
        '',
        render_section_divider(63.000, panel_w, hi_color, sh_color),
        '',
        '  <!-- ZONE 3: CV ATTENUVERTERS (Center Y = 73.00 mm, Label Y = 69.000 mm, Aligned Over Scale/Rotate CV) -->',
        render_text(ddin_cond, "SCALE", col5_x[3], 69.000, s_trim, label_color, "SCALE CV Depth"),
        render_text(ddin_cond, "ROTATE", col5_x[4], 69.000, s_trim, label_color, "ROTATE CV Depth"),
        '',
        render_section_divider(81.000, panel_w, hi_color, sh_color),
        '',
        '  <!-- ZONE 4: I/O JACKS (3 Rows x 5 Columns, Bottom Anchor = 117.00 mm, Pitch = 13.00 mm) -->',
        '  <!-- Jack Row 1 (Inputs, Center Y = 91.00 mm, Label Y = 86.000 mm) -->',
        render_text(ddin_cond, "FREQ", col5_x[0], 86.000, s_jack, label_color, "FREQ CV"),
        render_text(ddin_cond, "SYNC IN", col5_x[1], 86.000, s_jack, label_color, "SYNC IN"),
        render_text(ddin_cond, "SCALE", col5_x[3], 86.000, s_jack, label_color, "SCALE CV"),
        render_text(ddin_cond, "ROTATE", col5_x[4], 86.000, s_jack, label_color, "ROTATE CV"),
        '',
        '  <!-- Jack Row 2 (Color & Audio Outputs, Center Y = 104.00 mm, Label Y = 99.000 mm) -->',
        render_text(ddin_cond, "R", col5_x[0], 99.000, s_jack, label_color, "R OUT"),
        render_text(ddin_cond, "G", col5_x[1], 99.000, s_jack, label_color, "G OUT"),
        render_text(ddin_cond, "B", col5_x[2], 99.000, s_jack, label_color, "B OUT"),
        render_text(ddin_cond, "AUDIO L", col5_x[3], 99.000, s_jack, label_color, "AUDIO L OUT"),
        render_text(ddin_cond, "AUDIO R", col5_x[4], 99.000, s_jack, label_color, "AUDIO R OUT"),
        '',
        '  <!-- Jack Row 3 (XYZ & Sync Output, Center Y = 117.00 mm, Label Y = 112.000 mm) -->',
        render_text(ddin_cond, "X", col5_x[0], 112.000, s_jack, label_color, "X OUT"),
        render_text(ddin_cond, "Y", col5_x[1], 112.000, s_jack, label_color, "Y OUT"),
        render_text(ddin_cond, "Z", col5_x[2], 112.000, s_jack, label_color, "Z OUT"),
        render_text(ddin_cond, "SYNC OUT", col5_x[4], 112.000, s_jack, label_color, "SYNC OUT"),
        '</svg>'
    ]
    return "\n".join(svg) + "\n"


def main():
    skins = [
        {
            'name': 'Medium',
            'filename': 'res/Ilda.svg',
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
            'filename': 'res/Ilda_Light.svg',
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
            'filename': 'res/Ilda_Dark.svg',
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
        svg_content = generate_ilda_svg(skin)
        with open(skin['filename'], 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Generated {skin['filename']} ({skin['name']} skin) successfully.")

    # Render 240px MetaModule asset (14 HP = 71.12 mm -> round(240 * 71.12 / 128.50) = 133 px)
    panel_w = 71.120
    panel_h = 128.500
    target_h = 240
    target_w = round(target_h * (panel_w / panel_h))
    os.makedirs('metamodule/assets', exist_ok=True)
    cairosvg.svg2png(url='res/Ilda.svg', write_to='metamodule/assets/Ilda.png',
                     output_width=target_w, output_height=target_h)
    print(f"Rendered metamodule/assets/Ilda.png ({target_w}x{target_h}) successfully.")

    # Render 4x verification bitmap with hardware footprints overlaid
    verify_png = 'scratch/ilda_verify.png'
    cairosvg.svg2png(url='res/Ilda.svg', write_to=verify_png,
                     output_width=target_w * 4, output_height=target_h * 4)
    im = Image.open(verify_png)
    draw = ImageDraw.Draw(im)
    r_px = (target_w * 4) / panel_w

    col5_x = [7.000, 21.280, 35.560, 49.840, 64.120]

    # Row 1 Primary Knobs / Range Button at Y = 52.00 mm
    kr = 4.82 * r_px
    for c_idx, kx in enumerate(col5_x):
        ky = 52.000
        if c_idx == 1:
            br = 2.60 * r_px
            rr = 4.12 * r_px
            draw.ellipse([kx * r_px - br, ky * r_px - br, kx * r_px + br, ky * r_px + br], outline='#f0e442', width=2)
            draw.ellipse([kx * r_px - rr, ky * r_px - rr, kx * r_px + rr, ky * r_px + rr], outline='#d35fb7', width=2)
        else:
            draw.ellipse([kx * r_px - kr, ky * r_px - kr, kx * r_px + kr, ky * r_px + kr], outline='#ffaa00', width=2)

    # Zone 3 Trimpots (Col 3 & Col 4 at Y = 73.00 mm)
    tr = 3.04 * r_px
    for tx in [col5_x[3], col5_x[4]]:
        draw.ellipse([tx * r_px - tr, 73.000 * r_px - tr, tx * r_px + tr, 73.000 * r_px + tr], outline='#00ff88', width=2)

    # Zone 4 Jacks (13 active slots)
    jr = 3.89 * r_px
    jack_coords = [
        (col5_x[0], 91.000), (col5_x[1], 91.000), (col5_x[3], 91.000), (col5_x[4], 91.000),
        (col5_x[0], 104.000), (col5_x[1], 104.000), (col5_x[2], 104.000), (col5_x[3], 104.000), (col5_x[4], 104.000),
        (col5_x[0], 117.000), (col5_x[1], 117.000), (col5_x[2], 117.000), (col5_x[4], 117.000),
    ]
    for jx, jy in jack_coords:
        draw.ellipse([jx * r_px - jr, jy * r_px - jr, jx * r_px + jr, jy * r_px + jr], outline='#00aaff', width=2)

    im.save(verify_png)
    print(f"Saved verification image to {verify_png}.")


if __name__ == '__main__':
    main()
