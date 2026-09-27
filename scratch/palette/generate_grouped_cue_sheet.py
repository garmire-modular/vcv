import os
from PIL import Image, ImageDraw, ImageFont

def hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip('#')
    return tuple(int(hex_str[i:i+2], 16) for i in (0, 2, 4))

def simulate_deutan(r, g, b):
    # Machado et al. deuteranopia
    r_d = 0.625 * r + 0.375 * g
    g_d = 0.700 * r + 0.300 * g
    b_d = 0.300 * g + 0.700 * b
    return (min(255, max(0, int(r_d))), min(255, max(0, int(g_d))), min(255, max(0, int(b_d))))

def simulate_protan(r, g, b):
    # Machado et al. protanopia
    r_p = 0.567 * r + 0.433 * g
    g_p = 0.558 * r + 0.442 * g
    b_p = 0.242 * g + 0.758 * b
    return (min(255, max(0, int(r_p))), min(255, max(0, int(g_p))), min(255, max(0, int(b_p))))

def simulate_tritan(r, g, b):
    # Brettel et al. tritanopia
    r_t = 0.950 * r + 0.050 * g
    g_t = 0.000 * r + 0.433 * g + 0.567 * b
    b_t = 0.000 * r + 0.475 * g + 0.525 * b
    return (min(255, max(0, int(r_t))), min(255, max(0, int(g_t))), min(255, max(0, int(b_t))))

def main():
    W, H = 2020, 1560
    img = Image.new("RGBA", (W, H), (18, 20, 24, 255))
    draw = ImageDraw.Draw(img)

    # Fonts
    font_dir = "res"
    font_title = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 32)
    font_sub = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Medium.ttf"), 16)
    font_group_hdr = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 17)
    font_th = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 13)
    font_name = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 15)
    font_tag = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 11)
    font_desc = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Regular.ttf"), 13)
    font_mono_s = ImageFont.truetype("consola.ttf", 12)

    # Title Header
    draw.text((50, 26), "COLOR SIMILARITY CUE SHEET: 4 REPLACEMENTS & FULL PALETTE", fill=(255, 255, 255), font=font_title)
    draw.text((50, 68), "Grouped by Hue Family • Direct Contrast Analysis Under Normal, Deuteranopia, Protanopia & Tritanopia Vision", fill=(150, 155, 170), font=font_sub)
    draw.line([(50, 98), (W - 50, 98)], fill=(45, 48, 58), width=2)

    # Groups
    groups = [
        {
            "family": "1. ROSE, MAGENTA & WINE FAMILY (HIGH vs. LOW/MID TIER)",
            "colors": [
                { "type": "CANDIDATE 1", "name": "Pastel Rose", "hex": "#EE99AA", "tier": "HIGH", "lum": "0.439", "notes": "Soft warm rose pink. High-luminance warm pastel; clearly distinct from orchid Reddish Purple." },
                { "type": "ORIGINAL", "name": "Reddish Purple", "hex": "#CC79A7", "tier": "LOW/MID", "lum": "0.293", "notes": "Warm orchid magenta. Sits in Low tier; noticeably deeper than Pastel Rose (-0.15 Lum)." },
                { "type": "APPROVED", "name": "Deep Wine", "hex": "#882255", "tier": "LOW", "lum": "0.070", "notes": "Dark burgundy/crimson. Very dark anchor with rich red undertone." }
            ]
        },
        {
            "family": "2. PEACH, ORANGE & VERMILION FAMILY (WARM SPECTRUM)",
            "colors": [
                { "type": "CANDIDATE 2", "name": "Coral Peach", "hex": "#FF8866", "tier": "HIGH", "lum": "0.398", "notes": "Soft apricot/coral salmon. Clearly distinct from amber Orange (+pink hue) and Vermilion." },
                { "type": "ORIGINAL", "name": "Safety Orange", "hex": "#E69F00", "tier": "HIGH", "lum": "0.416", "notes": "Golden amber yellow-orange. Very warm yellow-bias." },
                { "type": "ORIGINAL", "name": "Vermilion", "hex": "#D55E00", "tier": "LOW", "lum": "0.222", "notes": "Fiery deep red-orange. Sits in Low tier (-0.18 Lum below Coral Peach)." }
            ]
        },
        {
            "family": "3. VIOLET, LAVENDER & INDIGO FAMILY (HIGH vs. DEEP LOW TIER)",
            "colors": [
                { "type": "APPROVED", "name": "Pale Lavender", "hex": "#B8A0E8", "tier": "HIGH", "lum": "0.412", "notes": "Cool periwinkle/lavender. High-luminance cool pastel tier." },
                { "type": "ORIGINAL", "name": "Cobalt Blue", "hex": "#0072B2", "tier": "LOW", "lum": "0.152", "notes": "Primary saturated blue. Mid-low luminance anchor (cyan-blue bias)." },
                { "type": "CANDIDATE 3", "name": "Imperial Violet", "hex": "#442288", "tier": "LOW", "lum": "0.042", "notes": "Deep royal violet/indigo (260° hue). Far deeper than Cobalt, bluer than Deep Wine." }
            ]
        },
        {
            "family": "4. TEAL, MINT & FOREST GREEN FAMILY (COOL SPECTRUM)",
            "colors": [
                { "type": "ORIGINAL", "name": "Bluish Green (Mint)", "hex": "#009E73", "tier": "LOW", "lum": "0.257", "notes": "Saturated sea green/teal. High green-blue saturation." },
                { "type": "APPROVED", "name": "Forest Pine", "hex": "#117733", "tier": "LOW", "lum": "0.136", "notes": "Deep evergreen leaf foliage. True natural green (-0.12 Lum darker than Mint)." },
                { "type": "CANDIDATE 4", "name": "Dark Petrol (Deep Teal)", "hex": "#005566", "tier": "LOW", "lum": "0.075", "notes": "Inky marine blue-green (190° hue). Sits between deep navy and pine with cyan undertone." }
            ]
        },
        {
            "family": "5. HIGH-LUMINANCE ANCHORS & MONOCHROME REFERENCE",
            "colors": [
                { "type": "ORIGINAL", "name": "White (Top Anchor)", "hex": "#FFFFFF", "tier": "HIGH", "lum": "1.000", "notes": "Maximum luminance anchor (Effect top band)." },
                { "type": "ORIGINAL", "name": "Electric Yellow", "hex": "#F0E442", "tier": "HIGH", "lum": "0.744", "notes": "Pure lemon yellow. Peak chromatic luminance." },
                { "type": "ORIGINAL", "name": "Sky Blue", "hex": "#56B4E9", "tier": "HIGH", "lum": "0.405", "notes": "Clear cornflower cyan-blue." },
                { "type": "ORIGINAL", "name": "Charcoal (Top Anchor)", "hex": "#1A1A1A", "tier": "LOW", "lum": "0.010", "notes": "Near-black anchor (Generate top band)." }
            ]
        }
    ]

    cur_y = 115

    # Column coordinate mapping
    col_name = 60
    col_tag = 260
    col_tier = 380
    col_panel = 490
    col_norm = 680
    col_deutan = 830
    col_protan = 980
    col_tritan = 1130
    col_notes = 1290

    for grp in groups:
        # Group Header Box
        draw.rounded_rectangle([50, cur_y, W - 50, cur_y + 30], radius=5, fill=(30, 34, 42))
        draw.text((62, cur_y + 6), grp["family"], fill=(230, 235, 245), font=font_group_hdr)
        cur_y += 38

        # Table Column Subheader
        draw.text((col_name, cur_y), "COLOR & HEX", fill=(120, 128, 145), font=font_th)
        draw.text((col_tag, cur_y), "STATUS", fill=(120, 128, 145), font=font_th)
        draw.text((col_tier, cur_y), "TIER / LUM", fill=(120, 128, 145), font=font_th)
        draw.text((col_panel, cur_y), "ON #7c7c7c PANEL", fill=(120, 128, 145), font=font_th)
        draw.text((col_norm, cur_y), "NORMAL", fill=(120, 128, 145), font=font_th)
        draw.text((col_deutan, cur_y), "DEUTAN (GREEN)", fill=(120, 128, 145), font=font_th)
        draw.text((col_protan, cur_y), "PROTAN (RED)", fill=(120, 128, 145), font=font_th)
        draw.text((col_tritan, cur_y), "TRITAN (BLUE)", fill=(120, 128, 145), font=font_th)
        draw.text((col_notes, cur_y), "DISTINCTION / PROXIMITY ANALYSIS", fill=(120, 128, 145), font=font_th)
        cur_y += 20

        # Colors in this group
        for c in grp["colors"]:
            row_h = 56
            is_cand = "CANDIDATE" in c["type"]
            is_app = (c["type"] == "APPROVED")
            
            if is_cand:
                row_bg = (32, 36, 48)
                border_col = (70, 120, 220)
            elif is_app:
                row_bg = (24, 32, 28)
                border_col = (40, 120, 80)
            else:
                row_bg = (22, 24, 30)
                border_col = None

            draw.rounded_rectangle([50, cur_y, W - 50, cur_y + row_h - 6], radius=4, fill=row_bg, outline=border_col)

            rgb = hex_to_rgb(c["hex"])
            deutan_rgb = simulate_deutan(*rgb)
            protan_rgb = simulate_protan(*rgb)
            tritan_rgb = simulate_tritan(*rgb)

            # Name & Hex
            draw.text((col_name, cur_y + 8), c["name"], fill=(255, 255, 255), font=font_name)
            draw.text((col_name, cur_y + 28), c["hex"], fill=(200, 205, 215), font=font_mono_s)

            # Tag
            if is_cand:
                tag_bg = (30, 100, 200)
            elif is_app:
                tag_bg = (20, 110, 60)
            else:
                tag_bg = (50, 55, 68)

            draw.rounded_rectangle([col_tag, cur_y + 14, col_tag + 95, cur_y + 34], radius=3, fill=tag_bg)
            draw.text((col_tag + 7, cur_y + 17), c["type"], fill=(255, 255, 255), font=font_tag)

            # Tier & Lum
            tier_col = (240, 228, 66) if "HIGH" in c["tier"] else (86, 180, 233)
            draw.text((col_tier, cur_y + 8), c["tier"], fill=tier_col, font=font_tag)
            draw.text((col_tier, cur_y + 26), f"L = {c['lum']}", fill=(160, 165, 180), font=font_mono_s)

            # #7c7c7c Panel Mount
            draw.rounded_rectangle([col_panel, cur_y + 6, col_panel + 150, cur_y + row_h - 12], radius=3, fill=(124, 124, 124))
            draw.rectangle([col_panel, cur_y + 6, col_panel + 30, cur_y + row_h - 12], fill=rgb)
            draw.text((col_panel + 40, cur_y + 15), "Panel edge", fill=(240, 240, 240), font=font_desc)

            # Vision swatches
            draw.rounded_rectangle([col_norm, cur_y + 6, col_norm + 110, cur_y + row_h - 12], radius=3, fill=rgb)
            draw.rounded_rectangle([col_deutan, cur_y + 6, col_deutan + 110, cur_y + row_h - 12], radius=3, fill=deutan_rgb)
            draw.rounded_rectangle([col_protan, cur_y + 6, col_protan + 110, cur_y + row_h - 12], radius=3, fill=protan_rgb)
            draw.rounded_rectangle([col_tritan, cur_y + 6, col_tritan + 110, cur_y + row_h - 12], radius=3, fill=tritan_rgb)

            # Notes
            draw.text((col_notes, cur_y + 15), c["notes"], fill=(200, 205, 220), font=font_desc)

            cur_y += row_h

        cur_y += 14

    # Save image
    out_dir = r"C:\Users\Pat\.gemini\antigravity\brain\1810fd88-4c00-4461-8eb4-ad0fdb55eb44"
    out_png = os.path.join(out_dir, "cue_sheet_grouped.png")
    img.save(out_png)

    scratch_png = os.path.join("scratch", "palette", "cue_sheet_grouped.png")
    img.save(scratch_png)
    print(f"Grouped cue sheet saved to {out_png} and {scratch_png}")

if __name__ == "__main__":
    main()
