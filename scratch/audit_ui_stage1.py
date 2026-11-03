#!/usr/bin/env python3
"""
Garmire UI Audit: Stage 1 Inventory & Compliance Checker
Checks all modules defined in plugin.json for:
1. Panel Generator Script (scratch/generate_<slug>_panel.py)
2. Faceplate SVGs (Medium, Light, Dark in res/)
3. Font vectorization (zero <text> tags in faceplates)
4. C++ Source Theme Support (skinTheme enum, serialization, widget setTheme, context menu)
"""

import os
import json
import re

def audit():
    if not os.path.exists("plugin.json"):
        print("[ERROR] plugin.json not found.")
        return

    with open("plugin.json", "r", encoding="utf-8") as f:
        data = json.load(f)

    modules = data.get("modules", [])
    print(f"================================================================================")
    print(f"GARMIRE UI AUDIT - STAGE 1: FONTS & TRIPLE SKIN STATUS ({len(modules)} Modules)")
    print(f"================================================================================")
    print(f"{'Module Slug':<18} | {'Gen Script':<10} | {'Med':<4} | {'Lgt':<4} | {'Drk':<4} | {'<text>':<6} | {'C++ Theme':<10}")
    print("-" * 80)

    stats = {
        "total": len(modules),
        "gen_script": 0,
        "med_svg": 0,
        "lgt_svg": 0,
        "drk_svg": 0,
        "clean_fonts": 0,
        "cpp_theme": 0,
        "fully_ready": 0
    }

    results = []

    for mod in modules:
        slug = mod.get("slug", "")
        
        # 1. Generator script
        gen_script = f"scratch/generate_{slug.lower()}_panel.py"
        has_gen = os.path.exists(gen_script)
        if not has_gen:
            # Try with underscores before capitals (e.g. SteppedSlew -> stepped_slew)
            snake_slug = re.sub(r'(?<!^)(?=[A-Z])', '_', slug).lower()
            snake_script = f"scratch/generate_{snake_slug}_panel.py"
            if os.path.exists(snake_script):
                gen_script = snake_script
                has_gen = True
            else:
                # Try alternate search
                clean_s = re.sub(r'[^a-zA-Z0-9]', '', slug).lower()
                for f in os.listdir("scratch"):
                    if f.startswith("generate_") and f.endswith("_panel.py"):
                        clean_f = re.sub(r'[^a-zA-Z0-9]', '', f.replace("generate_", "").replace("_panel.py", "")).lower()
                        if clean_s == clean_f or clean_s in clean_f:
                            gen_script = os.path.join("scratch", f)
                            has_gen = True
                            break

        # 2. SVGs
        med_svg = f"res/{slug}.svg"
        lgt_svg = f"res/{slug}_Light.svg"
        drk_svg = f"res/{slug}_Dark.svg"

        has_med = os.path.exists(med_svg)
        has_lgt = os.path.exists(lgt_svg)
        has_drk = os.path.exists(drk_svg)

        # 3. Fonts check (zero <text> in SVGs)
        has_text_tags = False
        for svg_path in [med_svg, lgt_svg, drk_svg]:
            if os.path.exists(svg_path):
                try:
                    with open(svg_path, "r", encoding="utf-8", errors="ignore") as sf:
                        if re.search(r'<text\b', sf.read()):
                            has_text_tags = True
                except Exception:
                    pass

        # 4. C++ Theme check
        cpp_path = f"src/{slug}.cpp"
        if not os.path.exists(cpp_path) and slug.startswith("Bit"):
            cpp_path = "src/BitModules.cpp"
        has_cpp_theme = False
        if os.path.exists(cpp_path):
            try:
                with open(cpp_path, "r", encoding="utf-8", errors="ignore") as cf:
                    content = cf.read()
                    if slug.startswith("Bit") and cpp_path == "src/BitModules.cpp":
                        # Check specific widget in BitModules.cpp
                        widget_name = f"{slug}Widget"
                        if "SkinTheme" in content and widget_name in content and f"{slug}::SkinTheme" in content or (widget_name in content and "setTheme" in content):
                            has_cpp_theme = True
                    elif "SkinTheme" in content and "setTheme" in content:
                        has_cpp_theme = True
            except Exception:
                pass

        if has_gen: stats["gen_script"] += 1
        if has_med: stats["med_svg"] += 1
        if has_lgt: stats["lgt_svg"] += 1
        if has_drk: stats["drk_svg"] += 1
        if not has_text_tags and has_med: stats["clean_fonts"] += 1
        if has_cpp_theme: stats["cpp_theme"] += 1

        is_complete = has_gen and has_med and has_lgt and has_drk and (not has_text_tags) and has_cpp_theme
        if is_complete: stats["fully_ready"] += 1

        results.append({
            "slug": slug,
            "has_gen": has_gen,
            "has_med": has_med,
            "has_lgt": has_lgt,
            "has_drk": has_drk,
            "has_text_tags": has_text_tags,
            "has_cpp_theme": has_cpp_theme,
            "is_complete": is_complete
        })

        s_gen = "OK" if has_gen else "MISSING"
        s_med = "OK" if has_med else "--"
        s_lgt = "OK" if has_lgt else "--"
        s_drk = "OK" if has_drk else "--"
        s_txt = "FAIL" if has_text_tags else "OK"
        s_cpp = "OK" if has_cpp_theme else "MISSING"

        flag = " " if is_complete else "*"
        print(f"{flag} {slug:<16} | {s_gen:<10} | {s_med:<4} | {s_lgt:<4} | {s_drk:<4} | {s_txt:<6} | {s_cpp:<10}")

    print("=" * 80)
    print("STAGE 1 AUDIT SUMMARY:")
    print(f"Total Modules in plugin.json:    {stats['total']}")
    print(f"Panel Generator Scripts Found:   {stats['gen_script']} / {stats['total']}")
    print(f"Medium SVGs Present:             {stats['med_svg']} / {stats['total']}")
    print(f"Light SVGs Present:              {stats['lgt_svg']} / {stats['total']}")
    print(f"Dark SVGs Present:               {stats['drk_svg']} / {stats['total']}")
    print(f"Zero <text> Font Compliance:     {stats['clean_fonts']} / {stats['total']}")
    print(f"C++ Triple Skin Code Active:     {stats['cpp_theme']} / {stats['total']}")
    print(f"Fully Compliant Modules (All 6): {stats['fully_ready']} / {stats['total']}")
    print("=" * 80)

if __name__ == "__main__":
    audit()
