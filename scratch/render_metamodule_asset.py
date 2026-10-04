import re
import resvg_py
from PIL import Image

def render_metamodule_asset(svg_path, out_png, width=76, height=240):
    with open(svg_path, 'r', encoding='utf-8') as f:
        svg_text = f.read()
    
    # Replace mm units on width and height with pixel dimensions for resvg
    svg_mod = re.sub(r'width="[0-9.]+mm"', f'width="{width}"', svg_text)
    svg_mod = re.sub(r'height="[0-9.]+mm"', f'height="{height}"', svg_mod)
    
    png_bytes = resvg_py.svg_to_bytes(svg_string=svg_mod, width=width, height=height)
    with open(out_png, 'wb') as f:
        f.write(png_bytes)
    print(f"Rendered {out_png} successfully: {width}x{height}, {len(png_bytes)} bytes.")

if __name__ == '__main__':
    render_metamodule_asset('res/Eunice.svg', 'metamodule/assets/Eunice.png', 76, 240)
