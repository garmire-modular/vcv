import re
import subprocess
import sys

batch5 = [
    ('switch', 'SWITCH', 15.24, 'title_block', 'v219_version_block'),
    ('route', 'ROUTE', 15.24, 'title_block', 'v220_version_block'),
    ('summult', 'SUM/MULT', 15.24, 'title_block', 'v221_version_block'),
    ('switchxl', 'SWITCH XL', 40.64, 'title_block', 'version_block'),
    ('routexl', 'ROUTE XL', 40.64, 'title_block', 'version_block'),
    ('summixxl', 'SUM/MIX XL', 40.64, 'title_block', 'version_block'),
    ('chopxl', 'CHOP XL', 60.96, 'title_block', 'version_block'),
    ('rescale', 'RESCALE', 30.48, 'title_block', 'version_block'),
    ('bleed', 'BLEED', 40.64, 'bleed_title_block', 'version_block'),
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
    elif ('knob' in comment.lower() or 'header' in comment.lower() or text_str in ['COPIES', 'SCALE', 'SHIFT', 'ROTATE', 'FEEDBACK']) and baseline_y < 50.0:
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

for slug, title_text, panel_w, title_var, ver_var in batch5:
    fpath = f"scratch/generate_{slug}_panel.py"
    with open(fpath, "r", encoding="utf-8") as f:
        src = f.read()

    # 1. Replace render_qs_text function
    m_rqt = re.search(r'def render_qs_text\(.*?\n(?=def main|\n\s*MODULES|\n\s*class)', src, re.DOTALL)
    assert m_rqt, f"render_qs_text not found in {fpath}"
    src = src[:m_rqt.start()] + new_render_qs_text + "\n\n" + src[m_rqt.end():]

    # 2. In main(), replace title generation block up to svg_parts = [
    m_main_head = re.search(r'def main\(\):.*?(?=svg_parts\s*=\s*\[)', src, re.DOTALL)
    assert m_main_head, f"main head not found in {fpath}"
    
    # Keep custom config lines
    custom_lines = []
    for line in m_main_head.group(0).splitlines():
        line_s = line.strip()
        if any(line_s.startswith(k) for k in ['col_headers =', 'cols =', 'col_x =', 'kx_left =', 'kx_right =', 'cx_mid =', 'version_str =']):
            custom_lines.append("    " + line_s)
    has_custom = "\n".join(custom_lines)
    if has_custom:
        has_custom += "\n"

    title_code = f"""def main():
    node_font = ddin_bold
    qs_font = ddin_cond
    panel_w = {panel_w}
    panel_h = 128.50
{has_custom}
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
        '  <!-- Label: "v2.0.0" -->',
        render_qs_text(qs_reg_font, "v2.0.0", panel_w / 2.0, 10.414, 0.001600, "#aaaaaa", "v2.0.0")
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
