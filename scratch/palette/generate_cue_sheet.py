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

def main():
    W, H = 1600, 920
    img = Image.new("RGBA", (W, H), (20, 22, 26, 255))
    draw = ImageDraw.Draw(img)

    # Fonts
    font_dir = "res"
    font_title = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 32)
    font_sub = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Medium.ttf"), 17)
    font_hdr = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 14)
    font_name = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 18)
    font_desc = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Regular.ttf"), 13)
    font_mono = ImageFont.truetype("consola.ttf", 16)
    font_mono_s = ImageFont.truetype("consola.ttf", 12)

    # Title
    draw.text((50, 35), "CUE SHEET: 6 PROPOSED ACCESSIBLE COLORS", fill=(255, 255, 255), font=font_title)
    draw.text((50, 78), "Paul Tol / W3C CVD Standards • Evaluated against #7c7c7c Eurorack Panel • Normal, Deutan & Protan Simulations", fill=(150, 155, 170), font=font_sub)
    draw.line([(50, 115), (W - 50, 115)], fill=(45, 48, 58), width=2)

    # Table Headers
    cols = [
        ("COLOR NAME & HEX", 50),
        ("TIER / LUM", 280),
        ("PANEL PREVIEW (#7c7c7c)", 420),
        ("NORMAL", 640),
        ("DEUTAN (GREEN-BLIND)", 780),
        ("PROTAN (RED-BLIND)", 980),
        ("ACCESSIBILITY ROLE & NOTE", 1180)
    ]

    header_y = 135
    draw.rounded_rectangle([50, header_y, W - 50, header_y + 32], radius=4, fill=(30, 33, 40))
    for title, x in cols:
        draw.text((x + 8, header_y + 8), title, fill=(130, 138, 155), font=font_hdr)

    # Proposed Colors Data
    candidates = [
        {
            "name": "1. Bright Cyan",
            "hex": "#66CCEE",
            "tier": "HIGH",
            "lum": "0.522",
            "note": "Brighter/greener than Sky Blue. Crisp ice-cyan contrast."
        },
        {
            "name": "2. Electric Lime",
            "hex": "#BBDD44",
            "tier": "HIGH",
            "lum": "0.605",
            "note": "High-vis acid green-yellow. Extremely bright on #7c7c7c."
        },
        {
            "name": "3. Pale Lavender",
            "hex": "#B8A0E8",
            "tier": "HIGH",
            "lum": "0.402",
            "note": "Light periwinkle. Stays a distinct cool violet-steel tone."
        },
        {
            "name": "4. Deep Wine",
            "hex": "#882255",
            "tier": "LOW",
            "lum": "0.070",
            "note": "Rich dark burgundy. Far deeper/cooler than Vermilion."
        },
        {
            "name": "5. Forest Pine",
            "hex": "#117733",
            "tier": "LOW",
            "lum": "0.136",
            "note": "Deep pine evergreen. Distinct dark step below Mint."
        },
        {
            "name": "6. Midnight Navy",
            "hex": "#114477",
            "tier": "LOW",
            "lum": "0.056",
            "note": "Deep inky indigo. Chromatic dark alternative to Charcoal."
        }
    ]

    row_y = 180
    row_h = 76

    for i, c in enumerate(candidates):
        # alternating row background
        if i % 2 == 0:
            draw.rounded_rectangle([50, row_y - 4, W - 50, row_y + row_h - 10], radius=6, fill=(26, 28, 34))

        rgb = hex_to_rgb(c["hex"])
        deutan_rgb = simulate_deutan(*rgb)
        protan_rgb = simulate_protan(*rgb)

        # 1. Color Name & Hex
        draw.text((58, row_y + 6), c["name"], fill=(245, 245, 250), font=font_name)
        draw.text((58, row_y + 32), c["hex"], fill=(255, 255, 255), font=font_mono)

        # 2. Tier / Lum
        tier_col = (240, 228, 66) if c["tier"] == "HIGH" else (86, 180, 233)
        draw.text((288, row_y + 8), c["tier"], fill=tier_col, font=font_hdr)
        draw.text((288, row_y + 32), f"L = {c['lum']}", fill=(160, 165, 180), font=font_mono_s)

        # 3. Panel Preview (#7c7c7c background mount)
        draw.rounded_rectangle([428, row_y + 2, 598, row_y + 58], radius=4, fill=(124, 124, 124))
        # Flush color stripe on left edge (representing the Eurorack badge)
        draw.rectangle([428, row_y + 2, 458, row_y + 58], fill=rgb)
        draw.text((470, row_y + 20), "On #7c7c7c", fill=(240, 240, 240), font=font_desc)

        # 4. Normal Swatch
        draw.rounded_rectangle([648, row_y + 4, 738, row_y + 54], radius=4, fill=rgb)

        # 5. Deutan Swatch
        draw.rounded_rectangle([788, row_y + 4, 878, row_y + 54], radius=4, fill=deutan_rgb)

        # 6. Protan Swatch
        draw.rounded_rectangle([988, row_y + 4, 1078, row_y + 54], radius=4, fill=protan_rgb)

        # 7. Note
        draw.text((1188, row_y + 18), c["note"], fill=(190, 195, 210), font=font_desc)

        row_y += row_h

    # Bottom comparison bar against existing 7 Okabe-Ito colors + anchors
    draw.line([(50, row_y + 20), (W - 50, row_y + 20)], fill=(45, 48, 58), width=1)
    
    cy = row_y + 35
    draw.text((58, cy), "REFERENCE: EXISTING 7 COLORS + 2 ANCHORS", fill=(130, 138, 155), font=font_hdr)
    
    existing_swatches = [
        ("White", "#FFFFFF"), ("Yellow", "#F0E442"), ("Orange", "#E69F00"), ("Sky Blue", "#56B4E9"),
        ("Purple", "#CC79A7"), ("Mint", "#009E73"), ("Vermilion", "#D55E00"), ("Cobalt", "#0072B2"), ("Charcoal", "#1A1A1A")
    ]
    
    ex_x = 58
    ex_y = cy + 26
    for name, hex_c in existing_swatches:
        rgb = hex_to_rgb(hex_c)
        draw.rounded_rectangle([ex_x, ex_y, ex_x + 36, ex_y + 26], radius=3, fill=rgb, outline=(60, 65, 75) if hex_c == "#1A1A1A" else None)
        draw.text((ex_x + 44, ex_y + 6), name, fill=(180, 185, 195), font=font_desc)
        ex_x += 165

    # Save
    out_dir = r"C:\Users\Pat\.gemini\antigravity\brain\1810fd88-4c00-4461-8eb4-ad0fdb55eb44"
    os.makedirs(out_dir, exist_ok=True)
    out_png = os.path.join(out_dir, "cue_sheet.png")
    img.save(out_png)

    scratch_png = os.path.join("scratch", "palette", "cue_sheet.png")
    img.save(scratch_png)
    print(f"Cue sheet saved to {out_png} and {scratch_png}")

if __name__ == "__main__":
    main()
