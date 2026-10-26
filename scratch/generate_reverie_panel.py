import os
import re

if os.name == 'nt':
    msys_bin = r'C:\msys64\mingw64\bin'
    if os.path.isdir(msys_bin):
        os.add_dll_directory(msys_bin)
        os.environ['PATH'] = msys_bin + os.pathsep + os.environ.get('PATH', '')

import cairosvg
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
import pathops

MODULE_NAME = "Reverie"
VERSION = "v1.1.1"
HP = 20
W = HP * 5.08  # 101.60 mm
H = 128.50     # 3U height

FONT_TITLE = "res/D-DIN-Bold.otf"
FONT_KNOB  = "res/D-DINCondensed-Bold.otf"
FONT_LABEL = "res/D-DINCondensed.otf"
FONT_VER   = "res/Quicksand-Regular.ttf"

SCALE_16PT  = 0.005644
SCALE_10PT  = 0.003528
SCALE_8PT   = 0.002822
SCALE_4_5PT = 0.001600


def clean_svg_path_numbers(d: str) -> str:
    if not d:
        return ""
    d = re.sub(r'([MCLZ])', r' \1 ', d)
    tokens = d.split()
    out = []
    for t in tokens:
        if t in ('M', 'C', 'L', 'Z'):
            out.append(t)
        else:
            try:
                val = float(t)
                out.append(f"{val:.1f}")
            except ValueError:
                out.append(t)
    return " ".join(out)


def text_to_svg_path(text: str, font_path: str, scale: float, cx: float, baseline_y: float, align: str = 'center', tracking: float = 0.0) -> str:
    font = TTFont(font_path)
    glyph_set = font.getGlyphSet()
    cmap = font.getBestCmap()
    hmtx = font['hmtx']

    glyphs = []
    total_adv = 0.0
    for i, ch in enumerate(text):
        gid = cmap.get(ord(ch))
        if not gid:
            gid = cmap.get(ord(' '))
        adv, _ = hmtx[gid]
        glyphs.append((gid, adv))
        total_adv += adv * scale
        if i < len(text) - 1:
            total_adv += tracking

    if align == 'center':
        start_x = cx - total_adv / 2.0
    elif align == 'right':
        start_x = cx - total_adv
    else:
        start_x = cx

    combined = pathops.Path()
    curr_x = start_x

    for i, (gid, adv) in enumerate(glyphs):
        g = glyph_set[gid]
        svg_pen = SVGPathPen(glyph_set)
        t_pen = TransformPen(svg_pen, (scale, 0, 0, -scale, curr_x, baseline_y))
        g.draw(t_pen)
        cmds = svg_pen.getCommands()
        if cmds:
            p = pathops.Path()
            svg_pen_pathops(g, (scale, 0, 0, -scale, curr_x, baseline_y), p)
            combined.addPath(p)
        curr_x += adv * scale + tracking

    simplified = pathops.simplify(combined)
    svg_out = SVGPathPen(None)
    simplified.draw(svg_out)
    return clean_svg_path_numbers(svg_out.getCommands())


def svg_pen_pathops(glyph, transform, pathops_path):
    pen = pathops_path.getPen()
    t_pen = TransformPen(pen, transform)
    glyph.draw(t_pen)


def render_skin(skin: dict):
    bg_color    = skin['bg_color']
    title_color = skin['title_color']
    label_color = skin['label_color']
    hi_color    = skin['hi_color']
    sh_color    = skin['sh_color']
    ring_color  = skin['ring_color']
    out_box_fill = skin['out_box_fill']
    out_box_text = skin['out_box_text']

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H:.2f}mm" viewBox="0 0 {W:.3f} {H:.3f}">')

    # Background
    svg.append(f'  <rect x="0" y="0" width="{W:.3f}" height="{H:.3f}" fill="{bg_color}" />')

    # Palette Badge
    svg.append('  <g id="palette-badge">')
    svg.append('    <rect x="0" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />')
    svg.append('  </g>')

    # Header: Module Title (centered at W/2 = 50.80 mm) & Version Tag
    title_d = text_to_svg_path("REVERIE", FONT_TITLE, SCALE_16PT, W / 2.0, 8.000, align='center', tracking=0.35)
    ver_d   = text_to_svg_path(VERSION, FONT_VER, SCALE_4_5PT, W - 2.15, 8.000, align='right')
    svg.append(f'  <path d="{title_d}" fill="{title_color}" />')
    svg.append(f'  <path d="{ver_d}" fill="#aaaaaa" />')

    # =========================================================================
    # LEFT CONTROLS (Columns: 7.00, 22.65, 38.30 mm)
    # =========================================================================
    X_K1, X_K2, X_K3 = 7.00, 22.65, 38.30

    # Row 1 Knobs/Button (Y = 18.00 mm, Label baseline Y = 12.25 mm): FREQ, RANGE, FINE
    Y_R1 = 18.00
    Y_R1_LBL = Y_R1 - 5.75  # 12.25 mm
    for x_pos, lbl in [(X_K1, "FREQ"), (X_K2, "RANGE"), (X_K3, "FINE")]:
        d = text_to_svg_path(lbl, FONT_KNOB, SCALE_10PT, x_pos, Y_R1_LBL, align='center')
        svg.append(f'  <path d="{d}" fill="{label_color}" />')

    # Range button trim ring at (22.65, 18.00)
    svg.append(f'  <circle cx="{X_K2:.3f}" cy="{Y_R1:.3f}" r="4.050" fill="{ring_color}" stroke="none" />')

    # Row 2 Knobs/Button (Y = 34.00 mm, Label baseline Y = 28.25 mm): CYCLES, RECORD, MORPH
    Y_R2 = 34.00
    Y_R2_LBL = Y_R2 - 5.75  # 28.25 mm
    for x_pos, lbl in [(X_K1, "CYCLES"), (X_K2, "RECORD"), (X_K3, "MORPH")]:
        d = text_to_svg_path(lbl, FONT_KNOB, SCALE_10PT, x_pos, Y_R2_LBL, align='center')
        svg.append(f'  <path d="{d}" fill="{label_color}" />')

    # Record button trim ring at (22.65, 34.00)
    svg.append(f'  <circle cx="{X_K2:.3f}" cy="{Y_R2:.3f}" r="4.050" fill="{ring_color}" stroke="none" />')

    # Delineator Line 1 above Row 3 (Y = 41.20 mm, left control zone)
    y_delin1 = 41.20
    svg.append(f'  <rect x="5.000" y="{y_delin1 - 0.150:.3f}" width="37.500" height="0.150" fill="{hi_color}" />')
    svg.append(f'  <rect x="5.000" y="{y_delin1:.3f}" width="37.500" height="0.150" fill="{sh_color}" />')

    # Row 3 Trimpot-sized Knobs (Y = 50.00 mm, Label baseline Y = 46.00 mm):
    # X above 7.00 mm, Y above 22.65 mm, MORPH above 38.30 mm, and SCAN between X and Y knobs (14.825, 51.00)
    Y_R3 = 50.00
    Y_R3_LBL = Y_R3 - 4.00  # 46.00 mm
    for x_pos, lbl in [(X_K1, "X"), (X_K2, "Y"), (X_K3, "MORPH")]:
        d = text_to_svg_path(lbl, FONT_LABEL, SCALE_8PT, x_pos, Y_R3_LBL, align='center')
        svg.append(f'  <path d="{d}" fill="{label_color}" />')
    d_scan = text_to_svg_path("SCAN", FONT_LABEL, SCALE_8PT, (X_K1 + X_K2) / 2.0, Y_R3 + 1.00, align='center')
    svg.append(f'  <path d="{d_scan}" fill="{label_color}" />')

    # Delineator Line 2 Left (above Left Jack Zone at Y = 66.00 - 10.00 = 56.00 mm)
    y_delin2_left = 56.00
    svg.append(f'  <rect x="5.000" y="{y_delin2_left - 0.150:.3f}" width="37.500" height="0.150" fill="{hi_color}" />')
    svg.append(f'  <rect x="5.000" y="{y_delin2_left:.3f}" width="37.500" height="0.150" fill="{sh_color}" />')

    # Delineator Line 2 Right (between 4x4 Preset Matrix and 4x4 Gate Grid at Y = 78.00 - 10.00 = 68.00 mm)
    y_delin2_right = 68.00
    svg.append(f'  <rect x="44.800" y="{y_delin2_right - 0.150:.3f}" width="{W - 44.800:.3f}" height="0.150" fill="{hi_color}" />')
    svg.append(f'  <rect x="44.800" y="{y_delin2_right:.3f}" width="{W - 44.800:.3f}" height="0.150" fill="{sh_color}" />')

    # =========================================================================
    # LEFT CONTROL JACKS (4 columns: 7.00, 17.43, 27.87, 38.30 mm)
    # 5 rows: 66.00, 78.75, 91.50, 104.25, 117.00 mm
    # =========================================================================
    L_COLS = [7.00, 17.43, 27.87, 38.30]
    L_ROWS = [66.00, 78.75, 91.50, 104.25, 117.00]

    # Subtle shaded backing pill for OUTPUT jacks (enclosing both label and jack cleanly):
    # Row 2 (91.50): cols 1, 2 (X Out, Y Out)
    # Row 3 (104.25): cols 0, 1, 2 (R Out, G Out, B Out)
    # Row 4 (117.00): cols 0, 1 (Z/I Out, SYNC Out)
    svg.append(f'  <rect x="12.600" y="83.400" width="20.100" height="12.750" rx="1.500" fill="{out_box_fill}" opacity="0.85" />')
    svg.append(f'  <rect x="2.600" y="96.150" width="30.100" height="12.750" rx="1.500" fill="{out_box_fill}" opacity="0.85" />')
    svg.append(f'  <rect x="2.600" y="108.900" width="19.700" height="13.000" rx="1.500" fill="{out_box_fill}" opacity="0.85" />')

    left_jack_labels = [
        # Row 0 (Y = 66.00 mm): SYNC, X, Y, FREQ
        (L_COLS[0], L_ROWS[0], "SYNC", False),
        (L_COLS[1], L_ROWS[0], "X", False),
        (L_COLS[2], L_ROWS[0], "Y", False),
        (L_COLS[3], L_ROWS[0], "FREQ", False),
        # Row 1 (Y = 78.75 mm): R, G, B, MORPH
        (L_COLS[0], L_ROWS[1], "R", False),
        (L_COLS[1], L_ROWS[1], "G", False),
        (L_COLS[2], L_ROWS[1], "B", False),
        (L_COLS[3], L_ROWS[1], "MORPH", False),
        # Row 2 (Y = 91.50 mm): Z/I, X (Out), Y (Out), X SCAN
        (L_COLS[0], L_ROWS[2], "Z/I", False),
        (L_COLS[1], L_ROWS[2], "X", True),
        (L_COLS[2], L_ROWS[2], "Y", True),
        (L_COLS[3], L_ROWS[2], "X SCAN", False),
        # Row 3 (Y = 104.25 mm): R (Out), G (Out), B (Out), Y SCAN
        (L_COLS[0], L_ROWS[3], "R", True),
        (L_COLS[1], L_ROWS[3], "G", True),
        (L_COLS[2], L_ROWS[3], "B", True),
        (L_COLS[3], L_ROWS[3], "Y SCAN", False),
        # Row 4 (Y = 117.00 mm): Z/I (Out), SYNC (Out), RECORD, RANDOM
        (L_COLS[0], L_ROWS[4], "Z/I", True),
        (L_COLS[1], L_ROWS[4], "SYNC", True),
        (L_COLS[2], L_ROWS[4], "RECORD", False),
        (L_COLS[3], L_ROWS[4], "RANDOM", False),
    ]

    for jx, jy, jlbl, is_out in left_jack_labels:
        col = out_box_text if is_out else label_color
        d = text_to_svg_path(jlbl, FONT_LABEL, SCALE_8PT, jx, jy - 5.00, align='center')
        svg.append(f'  <path d="{d}" fill="{col}" />')

    # =========================================================================
    # RIGHT 4x4 PRESET MATRIX & 4x4 GATE INPUT GRID
    # Columns: 52.00, 66.20, 80.40, 94.60 mm
    # =========================================================================
    G_COLS = [52.00, 66.20, 80.40, 94.60]
    BTN_ROWS = [18.00, 31.00, 44.00, 57.00]
    GATE_ROWS = [78.00, 91.00, 104.00, 117.00]

    # Subtle recessed frame behind the 4x4 button matrix (for MetaModule/SVG preview)
    svg.append(f'  <rect x="44.800" y="10.800" width="54.800" height="53.400" rx="2.400" fill="{sh_color}" opacity="0.28" />')

    # Gate input labels 1..16 (at Y - 5.00 mm above each gate jack)
    for r in range(4):
        gy = GATE_ROWS[r]
        for c in range(4):
            gx = G_COLS[c]
            slot_num = r * 4 + c + 1
            d = text_to_svg_path(str(slot_num), FONT_LABEL, SCALE_8PT, gx, gy - 5.00, align='center')
            svg.append(f'  <path d="{d}" fill="{label_color}" />')

    svg.append('</svg>')

    with open(skin['filename'], 'w', encoding='utf-8') as f:
        f.write("\n".join(svg) + "\n")
    print(f"Generated {skin['filename']}")


def main():
    skins = [
        {
            'name': 'Medium',
            'filename': f'res/{MODULE_NAME}.svg',
            'bg_color': '#6e6e6e',
            'title_color': '#ffffff',
            'label_color': '#222222',
            'hi_color': '#999999',
            'sh_color': '#5e5e5e',
            'ring_color': '#484848',
            'out_box_fill': '#525252',
            'out_box_text': '#eeeeee'
        },
        {
            'name': 'Light',
            'filename': f'res/{MODULE_NAME}_Light.svg',
            'bg_color': '#c6c6c6',
            'title_color': '#333333',
            'label_color': '#222222',
            'hi_color': '#e4e4e4',
            'sh_color': '#a8a8a8',
            'ring_color': '#9e9e9e',
            'out_box_fill': '#a4a4a4',
            'out_box_text': '#111111'
        },
        {
            'name': 'Dark',
            'filename': f'res/{MODULE_NAME}_Dark.svg',
            'bg_color': '#333333',
            'title_color': '#c6c6c6',
            'label_color': '#ffffff',
            'hi_color': '#585858',
            'sh_color': '#202020',
            'ring_color': '#222222',
            'out_box_fill': '#222222',
            'out_box_text': '#ffffff'
        }
    ]

    for s in skins:
        render_skin(s)

    os.makedirs("scratch", exist_ok=True)
    os.makedirs("metamodule/assets", exist_ok=True)
    cairosvg.svg2png(url=f"res/{MODULE_NAME}.svg", write_to="scratch/reverie_verify.png", output_height=720)
    cairosvg.svg2png(url=f"res/{MODULE_NAME}.svg", write_to=f"metamodule/assets/{MODULE_NAME}.png", output_height=240)
    print(f"Rendered scratch/reverie_verify.png and metamodule/assets/{MODULE_NAME}.png")


if __name__ == '__main__':
    main()
