"""
Apply Global #6e6e6e Background & Palette Badges
================================================
Updates all module generator scripts and faceplates to:
  1. Use #6e6e6e as the global panel background color.
  2. Append the unique vertical color band badge from palette.json flush against
     the left edge (X = 0.00mm, width = 2.54mm, height = 88.90mm from Y = 19.80mm to Y = 108.70mm).
  3. Render both res/<Module>.svg and metamodule/assets/<Module>.png synchronously.
"""

import os
import sys
import re
import json
from typing import Dict, Any

# Ensure MSYS2 DLLs are found for cairosvg / libcairo
msys_bin = r'C:\msys64\mingw64\bin'
if os.path.exists(msys_bin):
    os.environ['PATH'] = msys_bin + os.path.pathsep + os.environ.get('PATH', '')
    if hasattr(os, 'add_dll_directory'):
        try:
            os.add_dll_directory(msys_bin)
        except Exception:
            pass

import cairocffi
cairo_dll = os.path.join(msys_bin, 'libcairo-2.dll')
if os.path.exists(cairo_dll):
    cairocffi.cairo = cairocffi.ffi.dlopen(cairo_dll)
import cairosvg

PALETTE_JSON_PATH = 'scratch/palette/palette.json'

MODULE_HP: Dict[str, float] = {
    'Chromance': 10.0,
    'Ants': 6.0,
    'HsvRgb': 6.0,
    'OklchRgb': 6.0,
    'Rgb': 6.0,
    'Instability': 6.0,
    'Scale': 6.0,
    'Position': 6.0,
    'Rotate': 6.0,
    'Spin': 6.0,
    'Fold': 6.0,
    'Stretch': 6.0,
    'Shear': 6.0,
    'Smooth': 6.0,
    'Wiggle': 6.0,
    'Crest': 6.0,
    'Trough': 6.0,
    'Quake': 6.0,
    'Steps': 6.0,
    'Copycat': 6.0,
    'Petals': 6.0,
    'Knots': 6.0,
    'Vortex': 6.0,
    'Switch': 3.0,
    'Route': 3.0,
    'SumMult': 3.0,
    'SwitchXL': 15.0,
    'RouteXL': 15.0,
    'SumMixXL': 15.0,
    'Andxy': 3.0,
    'Orxy': 3.0,
    'Xorxy': 3.0,
    'ChopXL': 22.5,
    'Rescale': 6.0,
}

def ensure_rescale_palette(palette_data: Dict[str, Any]):
    """Ensures Rescale has a unique palette entry if missing."""
    if 'Rescale' not in palette_data:
        # Precision voltage scaling: White anchor, Deep Teal, Sky Blue
        palette_data['Rescale'] = {
            "module": "Rescale",
            "slug": "Rescale",
            "hp": 6,
            "mode": "Effect",
            "anchor": "white",
            "anchorPos": "top",
            "bandCount": 3,
            "bands": [
                {
                    "hex": "#FFFFFF",
                    "tier": "HIGH",
                    "weight": 35,
                    "heightMm": 31.115
                },
                {
                    "hex": "#005566",
                    "tier": "LOW",
                    "weight": 45,
                    "heightMm": 40.005
                },
                {
                    "hex": "#56B4E9",
                    "tier": "HIGH",
                    "weight": 20,
                    "heightMm": 17.780
                }
            ],
            "colors": [
                "#FFFFFF",
                "#005566",
                "#56B4E9"
            ],
            "weights": [
                35,
                45,
                20
            ],
            "badgeGeometry": {
                "widthMm": 2.54,
                "heightMm": 88.9,
                "yOffsetMm": 0,
                "x": 0,
                "y": 19.8
            },
            "panelColor": "#6E6E6E",
            "savedAt": "2026-09-27T20:00:00.000Z"
        }
        with open(PALETTE_JSON_PATH, 'w', encoding='utf-8') as f:
            json.dump(palette_data, f, indent=2)
        print("Added unique palette entry for Rescale to palette.json.")

def build_badge_svg_snippet(entry: Dict[str, Any]) -> str:
    """Generates the exact <g id=\"palette-badge\"> XML snippet."""
    geom = entry.get("badgeGeometry", {"x": 0.0, "y": 19.8, "widthMm": 2.54, "heightMm": 88.9})
    start_x = geom.get("x", 0.0)
    cur_y = geom.get("y", 19.8)
    width = geom.get("widthMm", 2.54)
    
    lines = [
        '  <!-- Left Edge Color Badge (Centered 88.9mm x 2.54mm, Flush X=0) -->',
        '  <g id="palette-badge">'
    ]
    for i, band in enumerate(entry["bands"]):
        h = band["heightMm"]
        hex_val = band["hex"]
        lines.append(f'    <rect x="{start_x:.3f}" y="{cur_y:.3f}" width="{width:.3f}" height="{h:.3f}" fill="{hex_val}" stroke="none"/>')
        cur_y += h
    lines.append('  </g>')
    return "\n".join(lines)

def update_svg_content(svg_text: str, badge_snippet: str) -> str:
    """Updates background fill to #6e6e6e and injects/replaces the palette badge."""
    content = svg_text
    
    # 1. Update background fill to #6e6e6e
    content = re.sub(
        r'(<rect\s+[^>]*?fill=")(#[0-9a-fA-F]{6}|[a-zA-Z]+)(")',
        r'\g<1>#6e6e6e\3',
        content,
        count=1
    )
    
    # 2. Remove existing palette-badge if present
    content = re.sub(
        r'\s*<!-- Left Edge Color Badge.*?-->\s*<g id="palette-badge">.*?</g>',
        '',
        content,
        flags=re.DOTALL
    )
    content = re.sub(
        r'\s*<g id="palette-badge">.*?</g>',
        '',
        content,
        flags=re.DOTALL
    )
    
    # 3. Inject new badge right after the background rect
    rect_match = re.search(r'(<rect\s+[^>]*?fill="#6e6e6e"[^>]*?>)', content)
    if rect_match:
        idx = rect_match.end()
        content = content[:idx] + "\n\n" + badge_snippet + content[idx:]
    else:
        # Fallback: inject before closing </svg>
        content = content.replace('</svg>', f'\n{badge_snippet}\n</svg>')
        
    return content

def create_generator_if_missing(slug: str, res_svg_content: str):
    """Creates a scratch/generate_<module>_panel.py if one does not exist."""
    script_path = f'scratch/generate_{slug.lower()}_panel.py'
    if not os.path.exists(script_path):
        code = f'''import os
from fontTools.ttLib import TTFont
import cairosvg

def main():
    svg_content = """{res_svg_content}"""
    os.makedirs('res', exist_ok=True)
    with open('res/{slug}.svg', 'w', encoding='utf-8') as f:
        f.write(svg_content.strip() + "\\n")
    print("Generated res/{slug}.svg")

if __name__ == '__main__':
    main()
'''
        with open(script_path, 'w', encoding='utf-8') as f:
            f.write(code)
        print(f"Created missing generator script: {script_path}")

def render_metamodule_png(slug: str, hp: float):
    svg_path = f'res/{slug}.svg'
    png_path = f'metamodule/assets/{slug}.png'
    os.makedirs('metamodule/assets', exist_ok=True)
    
    # MetaModule exact target dimensions (47.44 effective DPI, 240px height)
    mm_w = hp * 5.08
    target_w = int(round(mm_w * (240.0 / 128.50)))
    target_h = 240
    
    cairosvg.svg2png(url=svg_path, write_to=png_path, output_width=target_w, output_height=target_h)
    print(f"  -> Rendered {png_path} ({target_w}x{target_h}px)")

def main():
    with open(PALETTE_JSON_PATH, 'r', encoding='utf-8') as f:
        palette_data = json.load(f)
        
    ensure_rescale_palette(palette_data)
    
    total = len(MODULE_HP)
    updated = 0
    
    print(f"Starting global refresh: Setting background to #6e6e6e and applying palette badges across {total} modules...")
    
    for slug, hp in sorted(MODULE_HP.items()):
        svg_path = f'res/{slug}.svg'
        if not os.path.exists(svg_path):
            print(f"[SKIP] {svg_path} not found.")
            continue
            
        palette_entry = palette_data.get(slug)
        if not palette_entry:
            print(f"[WARN] No palette entry found for {slug}!")
            continue
            
        badge_xml = build_badge_svg_snippet(palette_entry)
        
        with open(svg_path, 'r', encoding='utf-8') as f:
            original_svg = f.read()
            
        new_svg = update_svg_content(original_svg, badge_xml)
        
        # Verify invariants
        assert '<text' not in new_svg, f"Invariant violation: <text> tag found in {slug}!"
        assert '#6e6e6e' in new_svg, f"Invariant violation: background #6e6e6e not set in {slug}!"
        assert 'id="palette-badge"' in new_svg, f"Invariant violation: palette-badge missing in {slug}!"
        
        with open(svg_path, 'w', encoding='utf-8') as f:
            f.write(new_svg.strip() + "\n")
            
        create_generator_if_missing(slug, new_svg)
        render_metamodule_png(slug, hp)
        
        updated += 1
        print(f"[{updated}/{total}] {slug} successfully refreshed with #6e6e6e & color badge.")

    print(f"\nAll {updated} modules have been successfully refreshed with #6e6e6e background and left-edge palette badges!")

if __name__ == '__main__':
    main()
