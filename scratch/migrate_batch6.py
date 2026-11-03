import re
import subprocess
import sys

# 1. First, handle the 3 logic modules (andxy, orxy, xorxy)
logic_modules = [
    ('andxy', 'AND XY'),
    ('orxy', 'OR XY'),
    ('xorxy', 'XOR XY')
]

new_render_qs_text_logic = """ddin_bold = TTFont('res/D-DIN-Bold.otf')
ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
ddin_cond = TTFont('res/D-DINCondensed.otf')
qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

def render_qs_text(font, text, center_x, baseline_y, scale, fill, comment=""):
    text_str = str(text)
    if ('v' in text_str and '.' in text_str) or 'version' in comment.lower():
        use_font = qs_reg_font
        use_scale = 0.001600
    elif font == ddin_bold:
        use_font = ddin_bold
        use_scale = scale
    else:
        use_font = ddin_cond
        use_scale = 8.0 * 25.4 / (72.0 * 1000.0)   # 8pt = 0.002822
    
    cmap = use_font.getBestCmap()
    hmtx = use_font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * use_scale if ord(c) in cmap else 500 * use_scale for c in text_str]
    total_w = sum(widths)
    start_x = center_x - total_w / 2.0
    res = []
    if comment:
        res.append(f'  <!-- Label: "{comment}" -->')
    curr = start_x
    for c, w in zip(text_str, widths):
        if c != ' ':
            pd = get_simplified_glyph_path(use_font, c)
            res.append(f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({use_scale:.6f}, {-use_scale:.6f})"><path d="{pd}" fill="{fill}"/></g>')
        curr += w
    return "\\n".join(res)"""

for slug, title_text in logic_modules:
    fpath = f"scratch/generate_{slug}_panel.py"
    with open(fpath, "r", encoding="utf-8") as f:
        src = f.read()

    # Replace render_qs_text
    m_rqt = re.search(r'def render_qs_text\(.*?\n(?=def generate_3hp_logic_panel)', src, re.DOTALL)
    assert m_rqt, f"render_qs_text not found in {fpath}"
    src = src[:m_rqt.start()] + new_render_qs_text_logic + "\n\n" + src[m_rqt.end():]

    # In generate_3hp_logic_panel, replace title calculation block
    m_gen_head = re.search(r'def generate_3hp_logic_panel\(.*?\n(?=    svg_parts\s*=\s*\[)', src, re.DOTALL)
    assert m_gen_head, f"generate_3hp_logic_panel head not found in {fpath}"

    new_gen_head = f"""def generate_3hp_logic_panel(module_slug, title_text, out_pair1_labels, out_pair2_labels, version_str="v2.0.0"):
    node_font = ddin_bold
    qs_font = ddin_cond
    panel_w = 15.24

    # Title "{title_text}" in D-DIN-Bold (16pt, scale 0.005644, baseline 7.620)
    base_title_scale = 16.0 * 25.4 / (72.0 * 1000.0)
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in title_text)
    title_scale = base_title_scale
    if raw_w > (panel_w - 3.0):
        title_scale = base_title_scale * ((panel_w - 3.0) / raw_w)

    title_block = [
        f'  <!-- Label: "{{title_text}}" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, title_text, panel_w / 2.0, 7.620, title_scale, "#ffffff", title_text)
    ]

    version_block = [
        render_qs_text(qs_reg_font, version_str, 7.62, 10.414, 0.001600, "#aaaaaa", version_str)
    ]
"""
    src = src[:m_gen_head.start()] + new_gen_head + "\n" + src[m_gen_head.end():]

    # Ensure main passes uppercase title
    src = re.sub(r'generate_3hp_logic_panel\("(\w+)",\s*"(\w+)"', f'generate_3hp_logic_panel("\\1", "{title_text}"', src)

    with open(fpath, "w", encoding="utf-8") as f:
        f.write(src)
    print(f"Updated {fpath}")

    r = subprocess.run(["python", fpath], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"ERROR executing {fpath}:\n{r.stderr}")
        sys.exit(1)
    print(f"Successfully generated {slug}")


# 2. Now handle the remaining 6 modules in Batch 6
complex_batch6 = [
    ('lisa', 'LISA', 40.64, 'lisa_title_block', 'version_block', 'v2.0.1', ''),
    ('daisy', 'DAISY', 40.64, 'daisy_title_block', 'version_block', 'v2.0.1', ''),
    ('circe', 'CIRCE', 40.64, 'circe_title_block', 'version_block', 'v2.0.1', ''),
    ('maude', 'MAUDE', 60.96, 'maude_title_block', 'version_block', 'v2.0.1', '    col_x = [7.62, 22.86, 38.10, 53.34]\n'),
    ('eunice', 'EUNICE', 40.64, 'title_block', 'version_block', 'v2.0.0', '    col_a = 8.13\n    col_b = 20.32\n    col_c = 32.51\n'),
    ('stepped_slew', 'STEPPED SLEW', 50.80, 'title_block', 'version_block', 'v2.0.0', """    col_1 = 9.400
    col_2 = 25.400
    col_3 = 41.400
    p_col_1 = 7.900
    p_col_2 = 19.567
    p_col_3 = 31.233
    p_col_4 = 42.900\n"""),
]

new_render_qs_text_complex = """ddin_bold = TTFont('res/D-DIN-Bold.otf')
ddin_cond_bold = TTFont('res/D-DINCondensed-Bold.otf')
ddin_cond = TTFont('res/D-DINCondensed.otf')
qs_reg_font = TTFont('res/Quicksand-Regular.ttf')

def render_qs_text(font, text, center_x, baseline_y, scale, fill, comment=""):
    text_str = str(text)
    if ('v' in text_str and '.' in text_str) or 'version' in comment.lower():
        use_font = qs_reg_font
        use_scale = 0.001600
    elif font == ddin_bold:
        use_font = ddin_bold
        use_scale = scale
    elif ('knob' in comment.lower() or 'header' in comment.lower() or text_str in ['FREQ', 'PHASE', 'AMP', 'TIME', 'WIDTH', 'SLEW', 'SHAPE']) and baseline_y < 50.0:
        use_font = ddin_cond_bold
        use_scale = 10.0 * 25.4 / (72.0 * 1000.0)  # 10pt = 0.003528
    else:
        use_font = ddin_cond
        use_scale = 8.0 * 25.4 / (72.0 * 1000.0)   # 8pt = 0.002822
    
    cmap = use_font.getBestCmap()
    hmtx = use_font['hmtx']
    widths = [hmtx[cmap[ord(c)]][0] * use_scale if ord(c) in cmap else 500 * use_scale for c in text_str]
    total_w = sum(widths)
    start_x = center_x - total_w / 2.0
    res = []
    if comment:
        res.append(f'  <!-- Label: "{comment}" -->')
    curr = start_x
    for c, w in zip(text_str, widths):
        if c != ' ':
            pd = get_simplified_glyph_path(use_font, c)
            res.append(f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({use_scale:.6f}, {-use_scale:.6f})"><path d="{pd}" fill="{fill}"/></g>')
        curr += w
    return "\\n".join(res)"""

for slug, title_text, panel_w, title_var, ver_var, ver_str, extra_cols in complex_batch6:
    fpath = f"scratch/generate_{slug}_panel.py"
    with open(fpath, "r", encoding="utf-8") as f:
        src = f.read()

    # 1. Replace render_qs_text function
    m_rqt = re.search(r'def render_qs_text\(.*?\n(?=def main|\n\s*MODULES|\n\s*class)', src, re.DOTALL)
    assert m_rqt, f"render_qs_text not found in {fpath}"
    src = src[:m_rqt.start()] + new_render_qs_text_complex + "\n\n" + src[m_rqt.end():]

    # 2. In main(), replace title generation block up to svg_parts = [
    m_main_head = re.search(r'def main\(\):.*?(?=svg_parts\s*=\s*\[)', src, re.DOTALL)
    assert m_main_head, f"main head not found in {fpath}"

    title_code = f"""def main():
    node_font = ddin_bold
    qs_font = ddin_cond
    panel_w = {panel_w}
    panel_h = 128.50
{extra_cols}
    # Title "{title_text}" in D-DIN-Bold (16pt, scale 0.005644, baseline 7.620)
    base_title_scale = 16.0 * 25.4 / (72.0 * 1000.0)
    cmap = ddin_bold.getBestCmap()
    hmtx = ddin_bold['hmtx']
    raw_w = sum(hmtx[cmap[ord(c)]][0] * base_title_scale if ord(c) in cmap else 500 * base_title_scale for c in "{title_text}")
    title_scale = base_title_scale
    if raw_w > (panel_w - 3.0):
        title_scale = base_title_scale * ((panel_w - 3.0) / raw_w)

    {title_var} = [
        '  <!-- Label: "{title_text}" (D-DIN-Bold) -->',
        render_qs_text(ddin_bold, "{title_text}", panel_w / 2.0, 7.620, title_scale, "#ffffff", "{title_text}")
    ]

    {ver_var} = [
        '  <!-- Label: "{ver_str}" -->',
        render_qs_text(qs_reg_font, "{ver_str}", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "{ver_str}")
    ]

    """

    src = src[:m_main_head.start()] + title_code + src[m_main_head.end():]

    with open(fpath, "w", encoding="utf-8") as f:
        f.write(src)
    print(f"Updated {fpath}")

    r = subprocess.run(["python", fpath], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"ERROR executing {fpath}:\n{r.stderr}")
        sys.exit(1)
    print(f"Successfully generated {slug}")
