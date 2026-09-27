import re

def main():
    with open('res/Stretch.svg', 'r', encoding='utf-8') as f:
        stretch_svg = f.read()

    with open('res/Scale.svg', 'r', encoding='utf-8') as f:
        scale_svg = f.read()

    with open('res/Rotate.svg', 'r', encoding='utf-8') as f:
        rotate_svg = f.read()

    # Extract paths from SVGs
    # Stretch.svg title glyphs: s=l14, t=l15, r=l16, e=l17, t=l18, c=l19, h=l20
    stretch_lines = stretch_svg.splitlines()
    s_path = re.search(r'path d="([^"]+)"', stretch_lines[13]).group(1) # line 14 (0-indexed 13)
    h_path = re.search(r'path d="([^"]+)"', stretch_lines[19]).group(1) # line 20 (0-indexed 19)
    e_path = re.search(r'path d="([^"]+)"', stretch_lines[16]).group(1) # line 17 (0-indexed 16)
    
    # Scale.svg line 16 (0-indexed 15) is 'a'
    scale_lines = scale_svg.splitlines()
    a_path = re.search(r'path d="([^"]+)"', scale_lines[15]).group(1)
    
    # Rotate.svg line 14 (0-indexed 13) is 'r'
    rotate_lines = rotate_svg.splitlines()
    r_path = re.search(r'path d="([^"]+)"', rotate_lines[13]).group(1)
    
    # Version '0' path from Stretch.svg line 25 (0-indexed 24)
    zero_version_path = re.search(r'path d="([^"]+)"', stretch_lines[24]).group(1)

    # Build title "shear"
    shear_title_block = [
        '  <!-- Label: "shear" -->',
        f'    <g transform="translate(10.186, 7.620) scale(0.004800, -0.004800)"><path d="{s_path}" fill="#ffffff"/></g>',
        f'    <g transform="translate(12.350, 7.620) scale(0.004800, -0.004800)"><path d="{h_path}" fill="#ffffff"/></g>',
        f'    <g transform="translate(14.515, 7.620) scale(0.004800, -0.004800)"><path d="{e_path}" fill="#ffffff"/></g>',
        f'    <g transform="translate(16.771, 7.620) scale(0.004800, -0.004800)"><path d="{a_path}" fill="#ffffff"/></g>',
        f'    <g transform="translate(18.936, 7.620) scale(0.004800, -0.004800)"><path d="{r_path}" fill="#ffffff"/></g>'
    ]

    # Build version "v1.0.0"
    v100_version_block = [
        '  <!-- Label: "v1.0.0" -->',
        stretch_lines[21], # v
        stretch_lines[22], # 1
        stretch_lines[23], # .
        stretch_lines[24], # 0
        stretch_lines[25], # .
        f'    <g transform="translate(16.333, 10.414) scale(0.001600, -0.001600)"><path d="{zero_version_path}" fill="#aaaaaa"/></g>' # 0
    ]

    # Replace title & version in stretch_lines
    # stretch_lines[0..11] are header
    # stretch_lines[12] is <!-- Label: "stretch" -->
    # stretch_lines[13..19] are title paths
    # stretch_lines[20] is <!-- Label: "v1.0.2" -->
    # stretch_lines[21..26] are version paths
    # stretch_lines[27..end] are all the exact label paths (X, Y, X CV, Y CV, IN, CV, OUT)
    
    new_svg_lines = stretch_lines[:12] + shear_title_block + v100_version_block + stretch_lines[27:]
    
    with open('res/Shear.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(new_svg_lines) + "\n")
    print("Created res/Shear.svg directly from master template SVGs.")

if __name__ == '__main__':
    main()
