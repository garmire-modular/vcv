import os
import re
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops
import resvg_py

# Auto-generated panel generator for Bitomata
from generate_bitmodules_panels import MODULES, generate_panel_svg

def main():
    ddin_bold = TTFont('res/D-DIN-Bold.otf')
    ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
    ddin_cond = TTFont('res/D-DINCondensed.otf')
    qs_reg_font = TTFont('res/Quicksand-Regular.ttf')
    m = next(item for item in MODULES if item["slug"] == "Bitomata")
    svg_content = generate_panel_svg(m, ddin_bold, ddin_cond_bold, ddin_cond, qs_reg_font)
    import sys
    sys.path.insert(0, 'scratch')
    from skin_generator_helper import write_triple_skins
    write_triple_skins('Bitomata', svg_content)

    svg_mod = re.sub(r'width="[0-9.]+mm"', 'width="57"', svg_content)
    svg_mod = re.sub(r'height="[0-9.]+mm"', 'height="240"', svg_mod)
    png_bytes = resvg_py.svg_to_bytes(svg_string=svg_mod, width=57, height=240)
    with open('metamodule/assets/Bitomata.png', 'wb') as pf:
        pf.write(png_bytes)
    print("Rendered metamodule/assets/Bitomata.png")

if __name__ == '__main__':
    main()
