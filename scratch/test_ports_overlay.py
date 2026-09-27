import os
import sys

msys_bin = r'C:\msys64\mingw64\bin'
if os.path.exists(msys_bin):
    os.environ['PATH'] = msys_bin + os.path.pathsep + os.environ.get('PATH', '')
    if hasattr(os, 'add_dll_directory'):
        try:
            os.add_dll_directory(msys_bin)
        except Exception:
            pass

import cairosvg
from PIL import Image, ImageDraw

svg_path = 'res/Switch.svg'
png_path = 'scratch/switch_with_ports.png'
cairosvg.svg2png(url=svg_path, write_to=png_path, output_width=57*4, output_height=240*4)

im = Image.open(png_path)
draw = ImageDraw.Draw(im)

r_px = (57 * 4) / 30.48
jr = 4.15 * r_px

jack_ys = [30.00, 41.50, 55.50, 67.00, 81.00, 92.50, 106.50, 118.00]
cx = 15.24 * r_px

for y in jack_ys:
    cy = y * r_px
    draw.ellipse([cx - jr, cy - jr, cx + jr, cy + jr], outline='cyan', width=2)

im.save('scratch/switch_with_ports.png')
print('Saved switch_with_ports.png')
