import os
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

import cairosvg

os.makedirs('metamodule/assets', exist_ok=True)

modules = [
    ('Chromance', 95, 240),
    ('HsvRgb', 57, 240),
    ('OklchRgb', 57, 240),
    ('Rgb', 57, 240),
    ('Instability', 57, 240),
    ('Scale', 57, 240),
    ('Position', 57, 240),
    ('Rotate', 57, 240),
    ('Spin', 57, 240),
    ('Fold', 57, 240),
    ('Stretch', 57, 240),
    ('Shear', 57, 240),
    ('Smooth', 57, 240),
    ('Wiggle', 57, 240),
    ('Crest', 57, 240),
    ('Trough', 57, 240),
    ('Quake', 57, 240),
    ('Steps', 57, 240),
    ('Copycat', 57, 240),
    ('Petals', 57, 240),
    ('Unfold', 57, 240),
    ('Vortex', 57, 240),
    ('Ants', 57, 240),
    ('Switch', 28, 240),
]

for slug, width, height in modules:
    svg_path = f'res/{slug}.svg'
    png_path = f'metamodule/assets/{slug}.png'
    if os.path.exists(svg_path):
        cairosvg.svg2png(url=svg_path, write_to=png_path, output_width=width, output_height=height)
        print(f"Rendered {png_path} ({width}x{height})")
    else:
        print(f"Warning: {svg_path} not found")

print("All MetaModule PNG assets rendered successfully!")
