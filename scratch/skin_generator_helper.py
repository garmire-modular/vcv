import re
import os

def apply_skin_theme(svg_content, skin_name):
    """
    Transforms a standard Medium-skin SVG faceplate into Medium, Light, or Dark variant
    strictly complying with Garmire Design Directives & Stage 1 UI Audit specifications.
    Preserves palette badges intact.
    """
    if skin_name == 'Medium':
        return svg_content

    # Isolate palette badge if present so its band colors are never altered
    badge_match = re.search(r'(<!--[^\n]*Palette Badge[^\n]*-->\s*<g id="palette-badge">.*?</g>|<g id="palette-badge">.*?</g>)', svg_content, re.DOTALL | re.IGNORECASE)
    badge_placeholder = "<!-- PALETTE_BADGE_PLACEHOLDER -->"
    if badge_match:
        badge_xml = badge_match.group(1)
        svg_content = svg_content[:badge_match.start()] + badge_placeholder + svg_content[badge_match.end():]
    else:
        badge_xml = None

    if skin_name == 'Light':
        # Background: #6e6e6e -> #c6c6c6
        svg_content = re.sub(r'fill="#6e6e6e"', 'fill="#c6c6c6"', svg_content)
        svg_content = re.sub(r'fill="#7c7c7c"', 'fill="#c6c6c6"', svg_content)
        # Title: #ffffff -> #333333
        svg_content = re.sub(r'fill="#ffffff"', 'fill="#333333"', svg_content)
        svg_content = re.sub(r'fill="#fff"', 'fill="#333333"', svg_content)
        # Version tag: #aaaaaa -> #666666
        svg_content = re.sub(r'fill="#aaaaaa"', 'fill="#666666"', svg_content)
        # Top Highlight: #999999 -> #e4e4e4
        svg_content = re.sub(r'fill="#999999"', 'fill="#e4e4e4"', svg_content)
        svg_content = re.sub(r'stroke="#999999"', 'stroke="#e4e4e4"', svg_content)
        # Bottom Shadow: #5e5e5e -> #a8a8a8
        svg_content = re.sub(r'fill="#5e5e5e"', 'fill="#a8a8a8"', svg_content)
        svg_content = re.sub(r'stroke="#5e5e5e"', 'stroke="#a8a8a8"', svg_content)
        # Button ring: #484848 -> #9e9e9e
        svg_content = re.sub(r'stroke="#484848"', 'stroke="#9e9e9e"', svg_content)
        # Dark labels stay/become #222222
        dark_grays = ['#1c1c1c', '#2c2c2c', '#111111', '#181818', '#1a1a1a', '#1e1e1e', '#242424', '#282828', '#2b2b2b', '#101412', '#151515', '#171712', '#1f1f1f', '#222', '#272727', '#333', '#333333', '#383838', '#3c3c3c']
        for c in dark_grays:
            svg_content = re.sub(f'fill="{c}"', 'fill="#222222"', svg_content)

    elif skin_name == 'Dark':
        # Background: #6e6e6e -> #333333
        svg_content = re.sub(r'fill="#6e6e6e"', 'fill="#333333"', svg_content)
        svg_content = re.sub(r'fill="#7c7c7c"', 'fill="#333333"', svg_content)
        # Title: #ffffff -> #c6c6c6
        svg_content = re.sub(r'fill="#ffffff"', 'fill="#c6c6c6"', svg_content)
        svg_content = re.sub(r'fill="#fff"', 'fill="#c6c6c6"', svg_content)
        # Version tag: #aaaaaa -> #888888
        svg_content = re.sub(r'fill="#aaaaaa"', 'fill="#888888"', svg_content)
        # Top Highlight: #999999 -> #585858
        svg_content = re.sub(r'fill="#999999"', 'fill="#585858"', svg_content)
        svg_content = re.sub(r'stroke="#999999"', 'stroke="#585858"', svg_content)
        # Bottom Shadow: #5e5e5e -> #202020
        svg_content = re.sub(r'fill="#5e5e5e"', 'fill="#202020"', svg_content)
        svg_content = re.sub(r'stroke="#5e5e5e"', 'stroke="#202020"', svg_content)
        # Button ring: #484848 -> #222222
        svg_content = re.sub(r'stroke="#484848"', 'stroke="#222222"', svg_content)
        # Dark labels & lines become #ffffff
        dark_grays = ['#1c1c1c', '#222222', '#2c2c2c', '#111111', '#181818', '#1a1a1a', '#1e1e1e', '#242424', '#282828', '#2b2b2b', '#101412', '#151515', '#171712', '#1f1f1f', '#222', '#272727', '#333', '#333333', '#383838', '#3c3c3c']
        for c in dark_grays:
            svg_content = re.sub(f'fill="{c}"', 'fill="#ffffff"', svg_content)
            svg_content = re.sub(f'stroke="{c}"', 'stroke="#ffffff"', svg_content)

    if badge_xml:
        svg_content = svg_content.replace(badge_placeholder, badge_xml)

    return svg_content

def write_triple_skins(slug, base_svg_content):
    """Writes Medium, Light, Dark SVGs for the given module slug."""
    skins = [
        ('Medium', f'res/{slug}.svg'),
        ('Light', f'res/{slug}_Light.svg'),
        ('Dark', f'res/{slug}_Dark.svg'),
    ]
    os.makedirs('res', exist_ok=True)
    for name, path in skins:
        content = apply_skin_theme(base_svg_content, name)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Generated {path} ({name} skin) successfully.")
