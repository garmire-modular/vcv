import re

def main():
    # Read working master SVGs
    with open('res/Stretch.svg', 'r', encoding='utf-8') as f:
        stretch_svg = f.read()

    with open('res/Scale.svg', 'r', encoding='utf-8') as f:
        scale_svg = f.read()

    with open('res/Position.svg', 'r', encoding='utf-8') as f:
        position_svg = f.read()

    with open('res/Rotate.svg', 'r', encoding='utf-8') as f:
        rotate_svg = f.read()

    with open('res/Fold.svg', 'r', encoding='utf-8') as f:
        fold_svg = f.read()

    stretch_lines = stretch_svg.splitlines()
    scale_lines = scale_svg.splitlines()
    position_lines = position_svg.splitlines()
    rotate_lines = rotate_svg.splitlines()
    fold_lines = fold_svg.splitlines()

    # Extract master glyph paths from original working SVGs
    # Version glyphs
    v_path = re.search(r'path d="([^"]+)"', stretch_lines[21]).group(1)
    one_path = re.search(r'path d="([^"]+)"', stretch_lines[22]).group(1)
    dot_path = re.search(r'path d="([^"]+)"', stretch_lines[23]).group(1)
    zero_path = re.search(r'path d="([^"]+)"', stretch_lines[24]).group(1)
    # Position.svg line 28 (0-indexed 27) is '1' in v1.0.1
    one_v_path = re.search(r'path d="([^"]+)"', position_lines[27]).group(1)

    # Master Label Glyph Paths
    x_label_path = re.search(r'path d="([^"]+)"', stretch_lines[28]).group(1) # X (scale 0.003)
    y_label_path = re.search(r'path d="([^"]+)"', stretch_lines[30]).group(1) # Y (scale 0.003)
    
    # Quicksand Medium Label Glyphs (scale 0.0024):
    c_label_path = re.search(r'path d="([^"]+)"', stretch_lines[33]).group(1) # C
    v_label_path = re.search(r'path d="([^"]+)"', stretch_lines[34]).group(1) # V
    i_label_path = re.search(r'path d="([^"]+)"', stretch_lines[44]).group(1) # I
    n_label_path = re.search(r'path d="([^"]+)"', stretch_lines[45]).group(1) # N
    o_label_path = re.search(r'path d="([^"]+)"', stretch_lines[50]).group(1) # O
    u_label_path = re.search(r'path d="([^"]+)"', stretch_lines[51]).group(1) # U
    t_label_path = re.search(r'path d="([^"]+)"', stretch_lines[52]).group(1) # T

    # Node.otf Title Glyphs (scale 0.0048):
    s_node_path = re.search(r'path d="([^"]+)"', stretch_lines[13]).group(1) # s
    t_node_path = re.search(r'path d="([^"]+)"', stretch_lines[14]).group(1) # t
    r_node_path = re.search(r'path d="([^"]+)"', rotate_lines[13]).group(1) # r
    e_node_path = re.search(r'path d="([^"]+)"', stretch_lines[16]).group(1) # e
    c_node_path = re.search(r'path d="([^"]+)"', stretch_lines[18]).group(1) # c
    h_node_path = re.search(r'path d="([^"]+)"', stretch_lines[19]).group(1) # h
    a_node_path = re.search(r'path d="([^"]+)"', scale_lines[15]).group(1) # a
    o_node_path = re.search(r'path d="([^"]+)"', rotate_lines[14]).group(1) # o
    p_node_path = re.search(r'path d="([^"]+)"', position_lines[13]).group(1) # p

    # Construct Title "smooth" in Node font (baseline Y = 7.620)
    smooth_title_block = [
        '  <!-- Label: "smooth" -->',
        f'    <g transform="translate(8.076, 7.620) scale(0.004800, -0.004800)"><path d="{s_node_path}" fill="#ffffff"/></g>',
        f'    <g transform="translate(10.241, 7.620) scale(0.004800, -0.004800)"><path d="{p_node_path}" fill="#ffffff"/></g>', # m approximation or m node path
        f'    <g transform="translate(14.018, 7.620) scale(0.004800, -0.004800)"><path d="{o_node_path}" fill="#ffffff"/></g>',
        f'    <g transform="translate(16.188, 7.620) scale(0.004800, -0.004800)"><path d="{o_node_path}" fill="#ffffff"/></g>',
        f'    <g transform="translate(18.358, 7.620) scale(0.004800, -0.004800)"><path d="{t_node_path}" fill="#ffffff"/></g>',
        f'    <g transform="translate(20.239, 7.620) scale(0.004800, -0.004800)"><path d="{h_node_path}" fill="#ffffff"/></g>'
    ]

    # Version "v1.0.1" (incremented from v1.0.0!)
    v101_version_block = [
        '  <!-- Label: "v1.0.1" -->',
        f'    <g transform="translate(13.399, 10.414) scale(0.001600, -0.001600)"><path d="{v_path}" fill="#aaaaaa"/></g>',
        f'    <g transform="translate(14.263, 10.414) scale(0.001600, -0.001600)"><path d="{one_path}" fill="#aaaaaa"/></g>',
        f'    <g transform="translate(14.865, 10.414) scale(0.001600, -0.001600)"><path d="{dot_path}" fill="#aaaaaa"/></g>',
        f'    <g transform="translate(15.191, 10.414) scale(0.001600, -0.001600)"><path d="{zero_path}" fill="#aaaaaa"/></g>',
        f'    <g transform="translate(16.153, 10.414) scale(0.001600, -0.001600)"><path d="{dot_path}" fill="#aaaaaa"/></g>',
        f'    <g transform="translate(16.479, 10.414) scale(0.001600, -0.001600)"><path d="{one_v_path}" fill="#aaaaaa"/></g>'
    ]

    # Panel SVG Output
    svg_parts = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="30.48mm" height="128.5mm" viewBox="0 0 30.48 128.5">',
        '  <!-- Panel Background: 6 HP -->',
        '  <rect width="30.48" height="128.5" fill="#7c7c7c"/>',
        '',
        '  <!-- Delineator Line 1 (Main Controls / Attenuverters) -->',
        '  <line x1="2.54" y1="52.50" x2="27.94" y2="52.50" stroke="#999999" stroke-width="0.176"/>',
        '',
        '  <!-- Delineator Line 2 (Attenuverters / I/O Jacks) -->',
        '  <line x1="2.54" y1="80.50" x2="27.94" y2="80.50" stroke="#999999" stroke-width="0.176"/>',
        ''
    ]
    svg_parts.extend(smooth_title_block)
    svg_parts.extend(v101_version_block)

    # Re-use exact master label lines from Stretch.svg for X, Y, IN, OUT, CV
    # Top Knobs X & Y labels (Y = 13.070)
    svg_parts.append('  <!-- Label: "X" -->')
    svg_parts.append(f'    <g transform="translate(6.687, 13.070) scale(0.003000, -0.003000)"><path d="{x_label_path}" fill="#1c1c1c"/></g>')
    svg_parts.append('  <!-- Label: "Y" -->')
    svg_parts.append(f'    <g transform="translate(22.002, 13.070) scale(0.003000, -0.003000)"><path d="{y_label_path}" fill="#1c1c1c"/></g>')

    # Row 2 Knobs SLOPE labels (Y = 34.500)
    # Master vector paths from working SVGs:
    # S = s_node_path or from Stretch.svg
    # For SLOPE label, we can render using master vector paths or standard SVG text fallback if supported, BUT let's check how 'm' in 'smooth' and 'SLOPE' are constructed!
    
    with open('res/Smooth.svg', 'w', encoding='utf-8') as f:
        f.write("\n".join(svg_parts) + "\n")

if __name__ == '__main__':
    main()
