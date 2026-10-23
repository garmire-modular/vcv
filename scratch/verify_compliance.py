#!/usr/bin/env python3
"""
Garmire Compliance Verification Script
-------------------------------------
Scans source files, SVG faceplates, and plugin metadata against mandatory AGENTS.md rules.
Can run against either:
  1) All files: python scratch/verify_compliance.py --all
  2) Git staged/recent changes (default): python scratch/verify_compliance.py
  3) Specific files/modules: python scratch/verify_compliance.py src/Eris.cpp res/Eris.svg
"""

import sys
import os
import re
import argparse
import subprocess

VALID_CLASSES = {"Generate", "Transform", "Utility", "Control"}

def get_git_modified_files():
    """Get unstaged + staged + recent commit modified files."""
    files = set()
    try:
        # Unstaged & staged files
        res = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, check=True)
        for line in res.stdout.splitlines():
            if len(line) > 3:
                filepath = line[3:].strip()
                # Handle renamed files "old -> new"
                if " -> " in filepath:
                    filepath = filepath.split(" -> ")[1].strip()
                files.add(filepath.replace('\\', '/'))
        
        # Files changed in the last commit (HEAD~1..HEAD)
        res_head = subprocess.run(["git", "diff", "--name-only", "HEAD~1..HEAD"], capture_output=True, text=True)
        if res_head.returncode == 0:
            for line in res_head.stdout.splitlines():
                if line.strip():
                    files.add(line.strip().replace('\\', '/'))
    except Exception:
        pass
    return files

def check_cpp_file(filepath):
    errors = []
    warnings = []
    
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    for idx, line in enumerate(lines, start=1):
        # 1. Virtual screws check
        if re.search(r'\b(ScrewSilver|ScrewBlack)\b', line):
            errors.append(f"Line {idx}: Forbidden virtual screw widget instantiated: '{line.strip()}' (AGENTS.md Section 7.5.6)")

        # 2. Redundant "input" / "output" check in configInput / configOutput
        # Match configInput(..., "...input...", ...) or configOutput(..., "...output...", ...)
        m_in = re.search(r'configInput\s*\(\s*[^,]+,\s*"([^"]+)"', line)
        if m_in:
            tooltip = m_in.group(1)
            # Check for standalone word "input" (case insensitive) or ending with " input" / " in"
            if re.search(r'\b(input|in)\b', tooltip, re.IGNORECASE):
                # Allow if part of a parameter like "Left in" only if explicitly desired, but standard says no redundant input/output
                # Specifically catch "X input", "CV input", "Gate input"
                errors.append(f"Line {idx}: Redundant 'input'/'in' in configInput tooltip \"{tooltip}\" (AGENTS.md Section 7.5.1)")

        m_out = re.search(r'configOutput\s*\(\s*[^,]+,\s*"([^"]+)"', line)
        if m_out:
            tooltip = m_out.group(1)
            if re.search(r'\b(output|out)\b', tooltip, re.IGNORECASE):
                errors.append(f"Line {idx}: Redundant 'output'/'out' in configOutput tooltip \"{tooltip}\" (AGENTS.md Section 7.5.1)")

        # 3. Attenuverter tooltip format check
        # Look for params with "CV" and "depth" or attenuverter params
        # Format must be: "[X|Y ]<Parameter> CV depth" with 'depth' lowercase and 'CV' uppercase
        m_param = re.search(r'configParam\s*\(\s*[^,]+,\s*[^,]+,\s*[^,]+,\s*[^,]+,\s*"([^"]+)"', line)
        if m_param:
            param_tooltip = m_param.group(1)
            # If tooltip contains "depth" or "depth" in name
            if re.search(r'\bcv\b.*\bdepth\b', param_tooltip, re.IGNORECASE):
                # Check strict format
                # e.g., "Depth" (uppercase) is invalid, "cv" (lowercase) is invalid
                if "CV Depth" in param_tooltip or "cv depth" in param_tooltip or "Cv depth" in param_tooltip:
                    errors.append(f"Line {idx}: Attenuverter tooltip casing invalid in \"{param_tooltip}\". Must use '<Parameter> CV depth' (AGENTS.md Section 7.5.4)")
                if re.search(r'\battenuverter\b', param_tooltip, re.IGNORECASE):
                    errors.append(f"Line {idx}: Forbidden word 'Attenuverter' in tooltip \"{param_tooltip}\" (AGENTS.md Section 7.5.4)")

        # 4. Sentence case check for all configParam tooltips
        if m_param:
            param_tooltip = m_param.group(1)
            # Split words, check that words after first word (unless an acronym/CV/VCA/LFO/RGB/X/Y) are not capitalized
            words = param_tooltip.split()
            if len(words) > 1:
                for w in words[1:]:
                    # Strip punctuation
                    clean_w = re.sub(r'[^a-zA-Z0-9]', '', w)
                    # Allowed uppercase abbreviations:
                    allowed_acronyms = {"CV", "VCA", "LFO", "VCO", "RGB", "OKLCH", "HSV", "HSL", "FM", "AM", "X", "Y", "Z", "L", "R", "I", "II", "III", "IV", "A", "B", "C", "D"}
                    if clean_w.isupper() and clean_w in allowed_acronyms:
                        continue
                    if clean_w.istitle() and clean_w.lower() not in {"and", "or", "to", "of", "in"}:
                        # Check if it looks like Title Case rather than sentence case
                        # e.g., "Frequency Modulation Depth" -> "Modulation" is capitalized
                        warnings.append(f"Line {idx}: Possible Title Case in param tooltip \"{param_tooltip}\" (word '{w}'). Use sentence case (AGENTS.md Section 7.5.5)")
                        break

    return errors, warnings

def check_svg_file(filepath):
    errors = []
    warnings = []
    
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # 1. Zero <text> elements
    if re.search(r'<text\b', content):
        errors.append(f"SVG contains forbidden `<text>` tag(s). Typography must be cubic paths (<path d=\"...\">) (AGENTS.md Section 7.5)")

    # 2. Virtual screw cutouts check in SVG
    if re.search(r'id=["\']?screw', content, re.IGNORECASE) or re.search(r'class=["\']?screw', content, re.IGNORECASE):
        errors.append(f"SVG contains screw cutouts or screw tags (AGENTS.md Section 7.5.6)")

    # 3. Initial version starting check (e.g. v2.26.0 on a panel when it should be v1.0.X)
    # Check version comment or label if present
    m_ver = re.search(r'Label:\s*"(v\d+\.\d+\.\d+)"', content)
    if m_ver:
        ver_str = m_ver.group(1)
        if ver_str.startswith("v2.26") or ver_str.startswith("v2.25"):
            warnings.append(f"Faceplate version is '{ver_str}'. Ensure module version is independent (e.g. v1.0.x) and not matching the suite version v2.26.0 (AGENTS.md Section 2)")

    return errors, warnings

def check_plugin_json(filepath):
    errors = []
    warnings = []
    try:
        import json
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        modules = data.get("modules", [])
        for mod in modules:
            slug = mod.get("slug", "Unknown")
            tags = mod.get("tags", [])
            # Check if at least one tag is in VALID_CLASSES
            found_class = any(t in VALID_CLASSES for t in tags)
            if not found_class:
                warnings.append(f"Module '{slug}' in plugin.json is missing a primary functional class tag ({', '.join(sorted(VALID_CLASSES))}) (AGENTS.md Section 5)")
    except Exception as e:
        errors.append(f"Failed to parse plugin.json: {e}")
    return errors, warnings

def main():
    parser = argparse.ArgumentParser(description="Verify codebase compliance with AGENTS.md rules.")
    parser.add_argument("--all", action="store_true", help="Scan all project files instead of just modified files.")
    parser.add_argument("files", nargs="*", help="Specific files to check.")
    args = parser.parse_args()

    target_files = []
    if args.files:
        target_files = args.files
    elif args.all:
        # Collect all src/*.cpp, res/*.svg, plugin.json
        for root, _, files in os.walk("src"):
            for f in files:
                if f.endswith(".cpp"):
                    target_files.append(os.path.join(root, f).replace('\\', '/'))
        for f in os.listdir("res"):
            if f.endswith(".svg"):
                target_files.append(os.path.join("res", f).replace('\\', '/'))
        target_files.append("plugin.json")
    else:
        git_files = get_git_modified_files()
        for f in git_files:
            if f.startswith("src/") and f.endswith(".cpp"):
                target_files.append(f)
            elif f.startswith("res/") and f.endswith(".svg"):
                target_files.append(f)
            elif f == "plugin.json":
                target_files.append(f)

    if not target_files:
        print("[OK] No modified C++, SVG, or configuration files to verify.")
        sys.exit(0)

    print(f"--- AGENTS.md Compliance Check ({len(target_files)} file(s)) ---")

    total_errors = 0
    total_warnings = 0

    for fpath in target_files:
        if not os.path.exists(fpath):
            continue
        
        errors = []
        warnings = []

        if fpath.endswith(".cpp"):
            errors, warnings = check_cpp_file(fpath)
        elif fpath.endswith(".svg"):
            errors, warnings = check_svg_file(fpath)
        elif fpath.endswith("plugin.json"):
            errors, warnings = check_plugin_json(fpath)

        if errors or warnings:
            print(f"\n[FILE] {fpath}")
            for err in errors:
                print(f"  [ERROR] {err}")
                total_errors += 1
            for warn in warnings:
                print(f"  [WARN]  {warn}")
                total_warnings += 1

    print("\n-------------------------------------------------------")
    if total_errors > 0:
        print(f"[FAILED] {total_errors} error(s), {total_warnings} warning(s) found.")
        sys.exit(1)
    elif total_warnings > 0:
        print(f"[PASSED with WARNINGS] 0 errors, {total_warnings} warning(s).")
        sys.exit(0)
    else:
        print("[PASSED] All checked files strictly comply with AGENTS.md rules.")
        sys.exit(0)

if __name__ == "__main__":
    main()
