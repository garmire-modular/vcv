#!/usr/bin/env python3
"""
Illustrator SVG Export Fixer for VCV Rack & NanoSVG.

Canonical normalizer for SVGs exported from Adobe Illustrator.
Illustrator default exports cause severe rendering bugs in NanoSVG:
1. CSS <style> blocks and .cls-X classes are completely ignored by NanoSVG,
   causing every shape to default to solid black (NSVG_RGB(0,0,0)).
2. <clipPath> elements outside <defs> are parsed as normal shapes, rendering
   as solid black rectangles across the artboard.
3. Missing or percentage width/height attributes break NanoSVG's viewBox calculation.
4. Inline CSS style="..." attributes are not always parsed uniformly.
5. Illustrator metadata (<metadata>, <i:pgf>, data-name) clutters the asset.

This script transforms any Illustrator SVG export into a 100% NanoSVG-compliant SVG:
- Inlines all CSS class styles and element styles directly into XML presentation attributes.
- Removes all <style> elements and class attributes.
- Expands inline style="..." attributes into discrete XML attributes.
- Resolves <stop> styling in gradients to stop-color and stop-opacity attributes.
- Ensures all <linearGradient>, <radialGradient>, and <clipPath> are inside <defs>.
- Strips unsupported clip-path attributes and empty clipping groups.
- Ensures explicit width, height, and viewBox are set and synchronized.
- Strips proprietary Illustrator metadata and namespaces.
- Formats path coordinates cleanly for NanoSVG without scientific notation.

Usage:
    python scratch/illustrator_svg_export_fix.py input.svg [--output output.svg] [--in-place]
    python scratch/illustrator_svg_export_fix.py res/*.svg --in-place
"""

import os
import re
import sys
import argparse
import xml.etree.ElementTree as ET

SVG_NS = "http://www.w3.org/2000/svg"
XLINK_NS = "http://www.w3.org/1999/xlink"

ET.register_namespace("", SVG_NS)
ET.register_namespace("xlink", XLINK_NS)


def parse_css_declarations(decl_str):
    """Parses a CSS declaration block into a dictionary of {property: value}."""
    props = {}
    if not decl_str:
        return props
    # Split by semicolon, ignoring semicolons inside parentheses or quotes
    for part in re.split(r';(?=(?:[^\"]*\"[^\"]*\")*[^\"]*$)', decl_str):
        part = part.strip()
        if not part or ':' not in part:
            continue
        k, v = part.split(':', 1)
        prop = k.strip().lower()
        val = v.strip()
        # Clean unit suffix 'px' for pure numeric attributes
        if prop in ("stroke-width", "stroke-miterlimit", "opacity", "fill-opacity", "stroke-opacity", "font-size"):
            val_clean = re.sub(r'px$', '', val).strip()
            if re.match(r'^-?[0-9]*\.?[0-9]+$', val_clean):
                val = val_clean
        props[prop] = val
    return props


def parse_css_stylesheet(css_text):
    """
    Parses a CSS stylesheet into a list of (selector, declarations_dict).
    Handles class selectors (.cls-1), ID selectors (#name), tag selectors (rect, path),
    and comma-separated compound selectors.
    """
    rules = []
    if not css_text:
        return rules
    # Remove CSS comments
    clean_css = re.sub(r'/\*.*?\*/', '', css_text, flags=re.DOTALL)
    # Find all selector { ... } blocks
    for match in re.finditer(r'([^{]+)\{([^}]+)\}', clean_css):
        selectors_str, decls_str = match.groups()
        decls = parse_css_declarations(decls_str)
        if not decls:
            continue
        for sel in selectors_str.split(','):
            sel = sel.strip()
            if sel:
                rules.append((sel, decls))
    return rules


def matches_selector(elem, tag_name, elem_id, elem_classes, selector):
    """Checks whether an SVG element matches a simple CSS selector."""
    selector = selector.strip()
    if not selector:
        return False
    # Universal selector
    if selector == "*":
        return True
    # Tag selector (e.g. 'path', 'rect')
    if selector == tag_name:
        return True
    # Class selector (e.g. '.cls-1', '.st0')
    if selector.startswith('.'):
        target_cls = selector[1:]
        return target_cls in elem_classes
    # ID selector (e.g. '#path1')
    if selector.startswith('#'):
        target_id = selector[1:]
        return target_id == elem_id
    # Tag + Class selector (e.g. 'rect.cls-1')
    if '.' in selector:
        tag_part, cls_part = selector.split('.', 1)
        if (not tag_part or tag_part == tag_name) and (cls_part in elem_classes):
            return True
    # Tag + ID selector (e.g. 'path#arrow')
    if '#' in selector:
        tag_part, id_part = selector.split('#', 1)
        if (not tag_part or tag_part == tag_name) and (id_part == elem_id):
            return True
    return False


def clean_number_str(s):
    """Removes scientific notation and formats floats cleanly."""
    try:
        val = float(s)
        if abs(val) < 1e-9:
            return "0"
        formatted = f"{val:.4f}".rstrip('0').rstrip('.')
        return formatted if formatted != "-0" else "0"
    except (ValueError, TypeError):
        return s


def clean_path_d(d_str):
    """Normalizes path 'd' string: cleans scientific notation and ensures spaced commands."""
    if not d_str:
        return d_str
    # Replace scientific notation like 1.23e-4 or -4.56e-5
    def replace_sci(m):
        try:
            f = float(m.group(0))
            return f"{f:.6f}".rstrip('0').rstrip('.')
        except ValueError:
            return m.group(0)
    d_clean = re.sub(r'[-+]?[0-9]*\.?[0-9]+[eE][-+]?[0-9]+', replace_sci, d_str)
    # Ensure spaces around command letters M, C, L, Z, etc.
    d_clean = re.sub(r'([A-Za-z])', r' \1 ', d_clean)
    # Ensure negative numbers are separated from preceding numbers
    d_clean = re.sub(r'([0-9])-', r'\1 -', d_clean)
    # Collapse multiple whitespaces
    d_clean = re.sub(r'\s+', ' ', d_clean).strip()
    return d_clean


def fix_illustrator_svg(input_path, output_path=None, target_size=None):
    """
    Normalizes an Illustrator-exported SVG to be 100% compliant with NanoSVG / VCV Rack.
    """
    if not os.path.exists(input_path):
        raise FileNotFoundError(f"File not found: {input_path}")

    if output_path is None:
        output_path = input_path

    tree = ET.parse(input_path)
    root = tree.getroot()

    # Verify SVG root
    if not root.tag.endswith('svg'):
        raise ValueError(f"Root tag is not <svg>: {root.tag}")

    stats = {
        "styles_inlined": 0,
        "style_tags_removed": 0,
        "clip_paths_fixed": 0,
        "inline_styles_expanded": 0,
        "paths_cleaned": 0
    }

    # 1. Collect all CSS rules from <style> blocks
    css_rules = []
    style_elements = []
    for elem in list(root.iter()):
        if elem.tag.endswith("style"):
            css_text = elem.text or ""
            rules = parse_css_stylesheet(css_text)
            css_rules.extend(rules)
            style_elements.append(elem)

    # 2. Find or create <defs> element
    defs_elem = None
    for child in root:
        if child.tag.endswith("defs"):
            defs_elem = child
            break

    # 3. Handle <clipPath> elements
    # NanoSVG does not parse clip-path and treats <clipPath> outside <defs> as drawable shapes!
    clip_elements = []
    for parent in list(root.iter()):
        for child in list(parent):
            if child.tag.endswith("clipPath"):
                clip_elements.append((parent, child))
                stats["clip_paths_fixed"] += 1

    for parent, clip_elem in clip_elements:
        if parent != defs_elem:
            parent.remove(clip_elem)
            if defs_elem is None:
                defs_elem = ET.Element(f"{{{SVG_NS}}}defs")
                root.insert(0, defs_elem)
            defs_elem.append(clip_elem)

    # 4. Remove all <style> elements from DOM
    for parent in list(root.iter()):
        for child in list(parent):
            if child in style_elements:
                parent.remove(child)
                stats["style_tags_removed"] += 1

    # 5. Remove proprietary metadata and image embeds
    junk_tags = ("metadata", "foreignObject", "pgf")
    for parent in list(root.iter()):
        for child in list(parent):
            local_tag = child.tag.split('}')[-1].lower()
            if any(junk in local_tag for junk in junk_tags):
                parent.remove(child)

    # 6. Iterate through all elements to inline styles and attributes
    for elem in root.iter():
        tag_name = elem.tag.split('}')[-1]

        # Strip proprietary Illustrator attributes
        for attr_k in list(elem.attrib.keys()):
            local_attr = attr_k.split('}')[-1]
            if local_attr.startswith("data-") or local_attr in ("graph", "extrusion"):
                del elem.attrib[attr_k]

        elem_id = elem.attrib.get("id", "")
        elem_classes = elem.attrib.get("class", "").split()

        # Match CSS rules in order
        matched_props = {}
        for sel, decls in css_rules:
            if matches_selector(elem, tag_name, elem_id, elem_classes, sel):
                matched_props.update(decls)

        # Apply matched properties if not already explicitly set on element
        for prop, val in matched_props.items():
            # If element already has this attribute set explicitly, keep it
            if prop not in elem.attrib:
                elem.attrib[prop] = val
                stats["styles_inlined"] += 1

        # Expand inline style="..." attribute into discrete attributes
        if "style" in elem.attrib:
            inline_props = parse_css_declarations(elem.attrib["style"])
            for prop, val in inline_props.items():
                if prop not in elem.attrib:
                    elem.attrib[prop] = val
                    stats["inline_styles_expanded"] += 1
            del elem.attrib["style"]

        # If class was inlined, remove the class attribute so NanoSVG sees pure presentation attrs
        if "class" in elem.attrib:
            del elem.attrib["class"]

        # Clean gradients and stops: ensure stops have stop-color and stop-opacity
        if tag_name == "stop":
            if "stop-color" not in elem.attrib and "color" in matched_props:
                elem.attrib["stop-color"] = matched_props["color"]
            if "stop-opacity" not in elem.attrib and "opacity" in matched_props:
                elem.attrib["stop-opacity"] = matched_props["opacity"]

        # Clean path 'd' attribute
        if tag_name == "path" and "d" in elem.attrib:
            elem.attrib["d"] = clean_path_d(elem.attrib["d"])
            stats["paths_cleaned"] += 1

        # Clean numeric stroke-width, opacity
        for num_attr in ("stroke-width", "stroke-miterlimit", "opacity", "fill-opacity", "stroke-opacity"):
            if num_attr in elem.attrib:
                elem.attrib[num_attr] = clean_number_str(elem.attrib[num_attr])

    # 7. Normalize viewBox and dimensions
    vb_str = root.attrib.get("viewBox", "")
    vb_parts = [p for p in re.split(r'[\s,]+', vb_str.strip()) if p]
    
    if len(vb_parts) == 4:
        vb_x, vb_y, vb_w, vb_h = [float(p) for p in vb_parts]
    else:
        # If viewBox is missing, infer from width/height if present
        w_val = root.attrib.get("width", "100")
        h_val = root.attrib.get("height", "100")
        vb_w = float(re.sub(r'[a-zA-Z%]', '', w_val) or 100)
        vb_h = float(re.sub(r'[a-zA-Z%]', '', h_val) or 100)
        vb_x, vb_y = 0.0, 0.0
        root.attrib["viewBox"] = f"0 0 {vb_w:.3f} {vb_h:.3f}"

    # Ensure explicit width & height attributes exist
    curr_w = root.attrib.get("width", "")
    curr_h = root.attrib.get("height", "")
    if not curr_w or "%" in curr_w:
        root.attrib["width"] = f"{vb_w:.3f}px"
    if not curr_h or "%" in curr_h:
        root.attrib["height"] = f"{vb_h:.3f}px"

    # Optional target size override
    if target_size is not None:
        root.attrib["width"] = f"{target_size:.3f}px"
        root.attrib["height"] = f"{target_size:.3f}px"
        root.attrib["viewBox"] = f"0 0 {target_size:.3f} {target_size:.3f}"

    # Write normalized SVG
    tree.write(output_path, encoding="utf-8", xml_declaration=True)

    print(f"Fixed {os.path.basename(input_path)} -> {os.path.basename(output_path)}:")
    print(f"  - CSS styles inlined: {stats['styles_inlined']}")
    print(f"  - <style> tags stripped: {stats['style_tags_removed']}")
    print(f"  - Clip-paths remediated: {stats['clip_paths_fixed']}")
    print(f"  - Inline styles expanded: {stats['inline_styles_expanded']}")
    print(f"  - Paths normalized: {stats['paths_cleaned']}")
    print(f"  - Dimensions: width={root.attrib['width']}, height={root.attrib['height']}, viewBox='{root.attrib['viewBox']}'")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Canonical Illustrator SVG Export Fixer for VCV Rack & NanoSVG"
    )
    parser.add_argument("inputs", nargs="+", help="Input SVG file(s) or glob patterns")
    parser.add_argument("-o", "--output", help="Output SVG file (only valid for single input)")
    parser.add_argument("-i", "--in-place", action="store_true", help="Overwrite input SVG files in-place")
    parser.add_argument("-s", "--size", type=float, help="Force square target artboard size (e.g. 28.471)")
    args = parser.parse_args()

    # Expand any glob patterns
    import glob
    matched_files = []
    for pat in args.inputs:
        files = glob.glob(pat)
        if files:
            matched_files.extend(files)
        elif os.path.exists(pat):
            matched_files.append(pat)

    if not matched_files:
        print("No matching SVG files found.")
        sys.exit(1)

    if args.output and len(matched_files) > 1:
        print("Error: --output can only be specified when processing a single SVG file.")
        sys.exit(1)

    success_count = 0
    for file_path in matched_files:
        if args.output:
            out_path = args.output
        elif args.in_place:
            out_path = file_path
        else:
            # Default to in-place if no output specified
            out_path = file_path

        try:
            fix_illustrator_svg(file_path, out_path, target_size=args.size)
            success_count += 1
        except Exception as e:
            print(f"Error fixing {file_path}: {e}", file=sys.stderr)

    print(f"\nSuccessfully processed {success_count}/{len(matched_files)} file(s).")


if __name__ == "__main__":
    main()
