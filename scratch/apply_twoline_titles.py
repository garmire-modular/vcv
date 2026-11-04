import re
import subprocess
import sys

# 1. Update scratch/generate_summult_panel.py
summult_path = 'scratch/generate_summult_panel.py'
with open(summult_path, 'r', encoding='utf-8') as f:
    src = f.read()

summult_old = re.search(r'    # Title "SUM/MULT".*?    v221_version_block = \[.*?    \]\n', src, re.DOTALL)
assert summult_old, "SumMult title block not found"

summult_new = """    # Title "SUM /" and "MULT" in D-DIN-Bold (12pt, scale 0.004233, baselines 5.20 and 9.20)
    title_lines = ["SUM /", "MULT"]
    base_title_scale = 12.0 * 25.4 / (72.0 * 1000.0)
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    title_scale = base_title_scale
    for t_line in title_lines:
        raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in t_line)
        if raw_w > (panel_w - 3.0):
            title_scale = min(title_scale, base_title_scale * ((panel_w - 3.0) / raw_w))

    title_block = [
        '  <!-- Label: "SUM /" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "SUM /", panel_w / 2.0, 5.200, title_scale, "#ffffff", "SUM /"),
        '  <!-- Label: "MULT" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "MULT", panel_w / 2.0, 9.200, title_scale, "#ffffff", "MULT")
    ]

    v221_version_block = [
        '  <!-- Label: "v2.0.0" -->',
        render_qs_text(qs_reg_font, "v2.0.0", panel_w / 2.0, 12.000, 0.001600, "#aaaaaa", "v2.0.0")
    ]
"""

src = src[:summult_old.start()] + summult_new + src[summult_old.end():]
with open(summult_path, 'w', encoding='utf-8') as f:
    f.write(src)
print("Updated scratch/generate_summult_panel.py")

# 2. Update logic modules: Andxy, Orxy, Xorxy
logic_modules = [
    ('andxy', 'Andxy', ["AND", "XY"]),
    ('orxy', 'Orxy', ["OR", "XY"]),
    ('xorxy', 'Xorxy', ["XOR", "XY"]),
]

for slug, class_name, lines in logic_modules:
    fpath = f'scratch/generate_{slug}_panel.py'
    with open(fpath, 'r', encoding='utf-8') as f:
        src = f.read()

    # Replace generate_3hp_logic_panel head
    m_head = re.search(r'def generate_3hp_logic_panel\(.*?\n(?=    svg_parts\s*=\s*\[)', src, re.DOTALL)
    assert m_head, f"Head not found in {fpath}"

    new_head = f"""def generate_3hp_logic_panel(module_slug, title_lines, out_pair1_labels, out_pair2_labels, version_str="v2.0.0"):
    node_font = ddin_bold
    qs_font = ddin_cond
    panel_w = 15.24

    # Title lines in D-DIN-Bold (12pt, scale 0.004233, baselines 5.20 and 9.20)
    base_title_scale = 12.0 * 25.4 / (72.0 * 1000.0)
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    title_scale = base_title_scale
    for t_line in title_lines:
        raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in t_line)
        if raw_w > (panel_w - 3.0):
            title_scale = min(title_scale, base_title_scale * ((panel_w - 3.0) / raw_w))

    title_block = [
        f'  <!-- Label: "{{title_lines[0]}}" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, title_lines[0], panel_w / 2.0, 5.200, title_scale, "#ffffff", title_lines[0]),
        f'  <!-- Label: "{{title_lines[1]}}" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, title_lines[1], panel_w / 2.0, 9.200, title_scale, "#ffffff", title_lines[1])
    ]

    version_block = [
        render_qs_text(qs_reg_font, version_str, panel_w / 2.0, 12.000, 0.001600, "#aaaaaa", version_str)
    ]
"""
    src = src[:m_head.start()] + new_head + "\n" + src[m_head.end():]

    # Replace main call
    src = re.sub(
        r'generate_3hp_logic_panel\("(\w+)",\s*["\'][^"\']+["\']',
        f'generate_3hp_logic_panel("{class_name}", {lines}',
        src
    )

    with open(fpath, 'w', encoding='utf-8') as f:
        f.write(src)
    print(f"Updated {fpath}")

# 3. Update scratch/generate_rgbivca_panel.py
rgbivca_path = 'scratch/generate_rgbivca_panel.py'
with open(rgbivca_path, 'r', encoding='utf-8') as f:
    src = f.read()

rgbivca_old = re.search(r'    # Title "RGB/I/VCA".*?    version_block = \[.*?    \]\n', src, re.DOTALL)
assert rgbivca_old, "RgbIVca title block not found"

rgbivca_new = """    # Title "RGBI" and "VCA" in D-DIN-Bold (11.5pt, scale 0.004057, baselines 4.80 and 8.40)
    title_lines = ["RGBI", "VCA"]
    base_title_scale = 11.5 * 25.4 / (72.0 * 1000.0)
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    title_scale = base_title_scale
    for t_line in title_lines:
        raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in t_line)
        if raw_w > (panel_w - 3.0):
            title_scale = min(title_scale, base_title_scale * ((panel_w - 3.0) / raw_w))

    title_block = [
        '  <!-- Label: "RGBI" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "RGBI", panel_w / 2.0, 4.800, title_scale, "#ffffff", "RGBI"),
        '  <!-- Label: "VCA" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "VCA", panel_w / 2.0, 8.400, title_scale, "#ffffff", "VCA")
    ]

    version_block = [
        '  <!-- Label: "v2.0.0" -->',
        render_qs_text(qs_reg_font, "v2.0.0", panel_w / 2.0, 10.600, 0.001600, "#aaaaaa", "v2.0.0")
    ]
"""

src = src[:rgbivca_old.start()] + rgbivca_new + src[rgbivca_old.end():]
with open(rgbivca_path, 'w', encoding='utf-8') as f:
    f.write(src)
print("Updated scratch/generate_rgbivca_panel.py")

# 4. Now run all 5 scripts to generate the SVGs and triple skins
scripts = [
    'scratch/generate_summult_panel.py',
    'scratch/generate_andxy_panel.py',
    'scratch/generate_orxy_panel.py',
    'scratch/generate_xorxy_panel.py',
    'scratch/generate_rgbivca_panel.py',
]

for s in scripts:
    r = subprocess.run(["python", s], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"ERROR executing {s}:\n{r.stderr}")
        sys.exit(1)
    print(f"Successfully ran {s}")
