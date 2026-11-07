#!/usr/bin/env python3
"""
Automated 4ms MetaModule Hardware Asset Renderer
------------------------------------------------
Automatically scans plugin.json (or res/*.svg) and renders 240px-high
non-transparent hardware faceplate PNGs to metamodule/assets/<Module>.png.

Usage:
  python scratch/render_metamodule_assets.py          # Render all modules
  python scratch/render_metamodule_assets.py Serqet   # Render specific module(s)
"""

import os
import sys
import json
import re
import xml.etree.ElementTree as ET

# Ensure MSYS2 DLLs are found for cairosvg / libcairo on Windows
msys_bin = r'C:\msys64\mingw64\bin'
if os.path.exists(msys_bin):
    os.environ['PATH'] = msys_bin + os.path.pathsep + os.environ.get('PATH', '')
    if hasattr(os, 'add_dll_directory'):
        try:
            os.add_dll_directory(msys_bin)
        except Exception:
            pass

import cairosvg

def get_svg_dimensions(svg_path):
    """Extract width and height ratio from SVG viewBox or width/height attributes."""
    try:
        tree = ET.parse(svg_path)
        root = tree.getroot()
        vb = root.get('viewBox')
        if vb:
            parts = [float(p) for p in re.split(r'[\s,]+', vb.strip()) if p]
            if len(parts) == 4:
                _, _, w, h = parts
                if h > 0:
                    return w, h
        w_attr = root.get('width', '')
        h_attr = root.get('height', '')
        w_m = re.findall(r'[\d\.]+', w_attr)
        h_m = re.findall(r'[\d\.]+', h_attr)
        if w_m and h_m:
            w, h = float(w_m[0]), float(h_m[0])
            if h > 0:
                return w, h
    except Exception as e:
        print(f"Error parsing SVG {svg_path}: {e}")
    return 5.08, 128.5  # default 1 HP ratio

def render_module(slug, assets_dir='metamodule/assets', force=False):
    svg_path = f'res/{slug}.svg'
    png_path = os.path.join(assets_dir, f'{slug}.png').replace('\\', '/')
    
    if not os.path.exists(svg_path):
        print(f"[WARN] Faceplate SVG not found: {svg_path}")
        return False
        
    w, h = get_svg_dimensions(svg_path)
    target_h = 240
    target_w = round(target_h * (w / h))
    
    # Check if rendering is necessary
    if not force and os.path.exists(png_path):
        if os.path.getmtime(png_path) >= os.path.getmtime(svg_path):
            return True

    try:
        cairosvg.svg2png(url=svg_path, write_to=png_path, output_width=target_w, output_height=target_h)
        print(f"[RENDERED] {png_path} ({target_w}x{target_h}) from {svg_path}")
        return True
    except Exception as e:
        print(f"[ERROR] Failed to render {png_path}: {e}")
        return False

def main():
    os.makedirs('metamodule/assets', exist_ok=True)
    
    # Determine target slugs
    target_slugs = []
    if len(sys.argv) > 1:
        target_slugs = [arg.replace('.svg', '').replace('res/', '') for arg in sys.argv[1:] if not arg.startswith('-')]

    if not target_slugs:
        # Load all modules from plugin.json
        if os.path.exists('plugin.json'):
            try:
                with open('plugin.json', 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    target_slugs = [mod.get('slug') for mod in data.get('modules', []) if mod.get('slug')]
            except Exception as e:
                print(f"Error loading plugin.json: {e}")
        
        # If plugin.json didn't yield any, scan res/*.svg
        if not target_slugs and os.path.exists('res'):
            target_slugs = [f[:-4] for f in os.listdir('res') if f.endswith('.svg')]

    success_count = 0
    for slug in target_slugs:
        if render_module(slug, force=True):
            success_count += 1

    print(f"\nDone: {success_count}/{len(target_slugs)} MetaModule PNG asset(s) ready in metamodule/assets/")

if __name__ == '__main__':
    main()
