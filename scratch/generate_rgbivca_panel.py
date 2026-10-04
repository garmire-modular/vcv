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

try:
    import cairocffi
    cairo_dll = os.path.join(msys_bin, 'libcairo-2.dll')
    if os.path.exists(cairo_dll):
        cairocffi.cairo = cairocffi.ffi.dlopen(cairo_dll)
    import cairosvg
except Exception:
    cairosvg = None

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

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

    # Title "rgb/i/vca" in Node.otf (scale 0.0028 -> ~10.82mm wide, centered in 15.24 mm)
    scale = 0.0028
    text = "rgb/i/vca"
    cmap = node_font.getBestCmap()
    hmtx = node_font['hmtx']
    total_w = sum(hmtx[cmap[ord(c)]][0] * scale for c in text)
    start_x = (15.24 - total_w) / 2.0

    title_block = ['  <!-- Label: "rgb/i/vca" -->']
    curr_x = start_x
    for c in text:
        w = hmtx[cmap[ord(c)]][0] * scale
        if c != ' ':
            path_d = get_simplified_glyph_path(node_font, c)
            title_block.append(f'    <g transform="translate({curr_x:.3f}, 7.620) scale({scale:.6f}, {-scale:.6f})"><path d="{path_d}" fill="#ffffff"/></g>')
        curr_x += w

    # Version tag "v2.26.0" centered at 7.62, baseline 10.414
    version_block = [
        render_qs_text(qs_reg_font, "v2.26.0", 7.62, 10.414, 0.001400, "#aaaaaa", "v2.26.0")
    ]

    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="15.24mm" height="128.5mm" viewBox="0 0 15.24 128.5">',
        '  <!-- Panel Background: 3 HP (15.24 mm) -->',
        '  <rect width="15.24" height="128.5" fill="#6e6e6e"/>',
        '  <!-- Left Edge Color Badge: Default Placeholder #5d5d5d (88.9mm x 1.35mm, Flush X=0) -->',
        '  <g id="palette-badge">',
        '    <rect x="0.000" y="19.800" width="1.350" height="88.900" fill="#5d5d5d" stroke="none"/>',
        '  </g>',
        '',
        '  <!-- Delineator Line 1 (Between I Modulation & RGB IN at Y = 41.50mm) -->',
        '  <line x1="1.50" y1="41.50" x2="13.74" y2="41.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Between RGB IN & RGB OUT at Y = 80.00mm) -->',
        '  <line x1="1.50" y1="80.00" x2="13.74" y2="80.00" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(title_block)
    svg_parts.extend(version_block)

    # Section 1: Intensity / I Modulation
    # Knob: center Y = 21.59 mm (RoundBlackKnob r = 4.25mm)
    # Label "DEPTH": baseline Y = 13.80 mm
    svg_parts.append(render_qs_text(qs_font, "DEPTH", 7.62, 13.80, 0.002000, "#1c1c1c", "DEPTH"))
    # Jack: center Y = 35.00 mm (PJ301M r = 4.15mm)
    # Label "I": baseline Y = 29.00 mm (-6.00mm offset)
    svg_parts.append(render_qs_text(qs_font, "I", 7.62, 29.00, 0.002000, "#2c2c2c", "I"))

    # Section 2: RGB Inputs (Jacks at Y = 49.00, 61.50, 74.00, pitch 12.50mm)
    # R In (center 49.00, label at 43.00)
    svg_parts.append(render_qs_text(qs_font, "R", 7.62, 43.00, 0.001900, "#2c2c2c", "R IN"))
    # G In (center 61.50, label at 55.30)
    svg_parts.append(render_qs_text(qs_font, "G", 7.62, 55.30, 0.001900, "#2c2c2c", "G IN"))
    # B In (center 74.00, label at 67.80)
    svg_parts.append(render_qs_text(qs_font, "B", 7.62, 67.80, 0.001900, "#2c2c2c", "B IN"))

    # Section 3: RGB Outputs (Jacks at Y = 95.00, 106.50, 118.00, pitch 11.50mm)
    # Section Header "OUT": baseline Y = 84.00 mm
    svg_parts.append(render_qs_text(qs_font, "OUT", 7.62, 84.00, 0.002200, "#1c1c1c", "OUT HEADER"))
    # R Out (center 95.00, label at 88.70)
    svg_parts.append(render_qs_text(qs_font, "R", 7.62, 88.70, 0.001900, "#2c2c2c", "R OUT"))
    # G Out (center 106.50, label at 100.50)
    svg_parts.append(render_qs_text(qs_font, "G", 7.62, 100.50, 0.001900, "#2c2c2c", "G OUT"))
    # B Out (center 118.00, label at 112.00)
    svg_parts.append(render_qs_text(qs_font, "B", 7.62, 112.00, 0.001900, "#2c2c2c", "B OUT"))

    svg_parts.append('</svg>')

    os.makedirs('res', exist_ok=True)
    svg_path = 'res/RgbIVca.svg'
    with open(svg_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")
    print(f"Generated {svg_path} successfully (3 HP).")

    # Render MetaModule PNG (28 x 240)
    os.makedirs('metamodule/assets', exist_ok=True)
    mm_png = 'metamodule/assets/RgbIVca.png'
    if cairosvg:
        cairosvg.svg2png(url=svg_path, write_to=mm_png, output_width=28, output_height=240)
        print(f"Rendered MetaModule asset: {mm_png} (28x240)")

        # Render verification bitmap with overlaid controls
        verify_png = 'scratch/rgbivca_verify.png'
        cairosvg.svg2png(url=svg_path, write_to=verify_png, output_width=28 * 4, output_height=240 * 4)

        im = Image.open(verify_png)
        draw = ImageDraw.Draw(im)
        r_px = (28 * 4) / 15.24
        cx = 7.62 * r_px

        # Overlay Knob (RoundBlackKnob r = 4.25mm)
        kr = 4.25 * r_px
        ky = 21.59 * r_px
        draw.ellipse([cx - kr, ky - kr, cx + kr, ky + kr], outline='#ffaa00', width=2)

        # Overlay Jacks (PJ301M r = 4.15mm)
        jr = 4.15 * r_px
        jacks_y = [35.00, 49.00, 61.50, 74.00, 95.00, 106.50, 118.00]
        for y in jacks_y:
            cy = y * r_px
            draw.ellipse([cx - jr, cy - jr, cx + jr, cy + jr], outline='#00e5ff', width=2)
        im.save(verify_png)
        print(f"Rendered verification bitmap with controls: {verify_png}")

if __name__ == '__main__':
    main()
