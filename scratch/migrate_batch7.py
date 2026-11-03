import re
import subprocess
import sys

batch7 = [
    ('annette', 'ANNETTE', 60.96, 'title_block', 'version_block', 'v1.0.0', ''),
    ('bitterroot', 'BITTERROOT', 162.56, 'title_block', 'version_block', 'v2.0.0', """    center_x = panel_w / 2.0  # 81.28 mm
    col_1 = 28.780
    col_2 = 81.280
    col_3 = 133.780
    c1_k = [11.530, 23.030, 34.530, 46.030]
    c2_k = [64.030, 75.530, 87.030, 98.530]
    c3_k = [116.530, 128.030, 139.530, 151.030]\n"""),
    ('crissy', 'CRISSY', 40.64, 'title_block', 'version_block', 'v1.0.0', ''),
    ('eris', 'ERIS', 71.12, 'title_block', 'version_block', 'v1.0.3', ''),
    ('ziggy', 'ZIGGY', 40.64, 'title_block', 'version_block', 'v1.1.3', ''),
]

new_render_qs_text = """ddin_bold = TTFont('res/D-DIN-Bold.otf')
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
    elif ('knob' in comment.lower() or 'button' in comment.lower() or 'header' in comment.lower()) and baseline_y < 50.0:
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

for slug, title_text, panel_w, title_var, ver_var, ver_str, extra_setup in batch7:
    fpath = f"scratch/generate_{slug}_panel.py"
    with open(fpath, "r", encoding="utf-8") as f:
        src = f.read()

    # 1. Replace render_qs_text function
    m_rqt = re.search(r'def render_qs_text\(.*?\n(?=\ndef )', src, re.DOTALL)
    assert m_rqt, f"render_qs_text not found in {fpath}"
    src = src[:m_rqt.start()] + new_render_qs_text + "\n\n" + src[m_rqt.end():]

    # 2. In main(), replace title generation block up to svg_parts = [
    m_main_head = re.search(r'def main\(\):.*?(?=svg_parts\s*=\s*\[)', src, re.DOTALL)
    assert m_main_head, f"main head not found in {fpath}"

    title_code = f"""def main():
    node_font = ddin_bold
    qs_font = ddin_cond
    panel_w = {panel_w}
    panel_h = 128.50
{extra_setup}
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
