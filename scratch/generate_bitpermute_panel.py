import os
import re
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops
import resvg_py

# Auto-generated panel generator for BitPermute
from generate_bitmodules_panels import MODULES, generate_panel_svg

def main():
    node_font = TTFont('res/Node.otf')
    qs_font = TTFont('res/Quicksand-Medium.ttf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')
    m = next(item for item in MODULES if item["slug"] == "BitPermute")
    svg_content = generate_panel_svg(m, node_font, qs_font, qs_reg_font)
    with open('res/BitPermute.svg', 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print("Generated res/BitPermute.svg")

    svg_mod = re.sub(r'width="[0-9.]+mm"', 'width="57"', svg_content)
    svg_mod = re.sub(r'height="[0-9.]+mm"', 'height="240"', svg_mod)
    png_bytes = resvg_py.svg_to_bytes(svg_string=svg_mod, width=57, height=240)
    with open('metamodule/assets/BitPermute.png', 'wb') as pf:
        pf.write(png_bytes)
    print("Rendered metamodule/assets/BitPermute.png")

if __name__ == '__main__':
    main()
