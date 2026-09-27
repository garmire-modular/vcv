"""
Global Design Refresher & Batch Asset Pipeline
==============================================
Applies global theme changes (background color, text contrast, line colors),
adds brand logos, integrates custom hardware assets, and regenerates both:
  1. res/<Module>.svg (VCV Rack NanoSVG compliant)
  2. metamodule/assets/<Module>.png (MetaModule 240px RGB565 compliant)
"""

import os
import sys
import re
import argparse
from typing import Dict, List, Optional

# Add scratch dir to path
scratch_dir = os.path.dirname(os.path.abspath(__file__))
if scratch_dir not in sys.path:
    sys.path.insert(0, scratch_dir)

from design_harness import UITheme, THEMES, DualTargetExporter, HardwareAssetEngine, ClearanceValidator

# Module slug -> (HP width)
MODULE_SPECS: Dict[str, float] = {
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

def refresh_svg_content(svg_text: str, theme: UITheme, add_logo: bool = True, slug: str = "") -> str:
    """
    Transforms an existing SVG panel content to apply the new theme and optionally inject a logo.
    """
    content = svg_text

    # 1. Update background fill
    content = re.sub(
        r'(<rect\s+[^>]*?fill=")(#[0-9a-fA-F]{6}|[a-zA-Z]+)(")',
        rf'\g<1>{theme.background_fill}\3',
        content
    )

    # 2. Update delineator line stroke
    content = re.sub(
        r'(<line\s+[^>]*?stroke=")(#[0-9a-fA-F]{6}|[a-zA-Z]+)(")',
        rf'\g<1>{theme.delineator_stroke}\3',
        content
    )

    # 3. Add Garmire logo if requested and not already present
    if add_logo and '<!-- Garmire Brand Insignia -->' not in content:
        hp = MODULE_SPECS.get(slug, 6.0)
        cx = (hp * 5.08) / 2.0
        # Place logo at bottom anchor (Y = 124.20mm) if HP >= 6
        if hp >= 6.0:
            logo_xml = HardwareAssetEngine.render_garmire_logo(cx, 124.20, scale=0.75, fill=theme.logo_fill)
            content = content.replace('</svg>', f'\n{logo_xml}\n</svg>')

    return content

def refresh_module(slug: str, theme: UITheme, add_logo: bool = True) -> bool:
    svg_path = os.path.join('res', f'{slug}.svg')
    if not os.path.exists(svg_path):
        print(f"[SKIP] {svg_path} does not exist.")
        return False

    with open(svg_path, 'r', encoding='utf-8') as f:
        original = f.read()

    updated = refresh_svg_content(original, theme, add_logo=add_logo, slug=slug)
    hp = MODULE_SPECS.get(slug, 6.0)

    success, msg = DualTargetExporter.export(slug, updated, hp)
    print(f"[{'OK' if success else 'FAIL'}] {slug}: {msg}")
    return success

def refresh_all(theme: UITheme, add_logo: bool = True):
    print(f"=== Refreshing All Panels with Theme: {theme.name} (BG: {theme.background_fill}) ===")
    total = len(MODULE_SPECS)
    successful = 0
    for slug in sorted(MODULE_SPECS.keys()):
        if refresh_module(slug, theme, add_logo=add_logo):
            successful += 1
    print(f"=== Completed: {successful}/{total} modules updated ===")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Refresh Garmire module faceplates and MetaModule assets.")
    parser.add_argument('--theme', choices=list(THEMES.keys()), default='slate', help="Visual theme preset")
    parser.add_argument('--bg', type=str, default=None, help="Custom background color (hex, e.g. #242426)")
    parser.add_argument('--logo', action='store_true', default=False, help="Inject Garmire brand logo")
    parser.add_argument('--module', type=str, default=None, help="Single module slug to refresh")
    args = parser.parse_args()

    selected_theme = THEMES[args.theme]
    if args.bg:
        selected_theme.background_fill = args.bg

    if args.module:
        refresh_module(args.module, selected_theme, add_logo=args.logo)
    else:
        refresh_all(selected_theme, add_logo=args.logo)
