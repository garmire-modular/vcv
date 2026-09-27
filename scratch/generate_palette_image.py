import os
from PIL import Image, ImageDraw, ImageFont

def hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip('#')
    return tuple(int(hex_str[i:i+2], 16) for i in (0, 2, 4))

def main():
    W, H = 2300, 1160
    img = Image.new("RGBA", (W, H), (26, 28, 32, 255))
    draw = ImageDraw.Draw(img)

    # Fonts
    font_dir = "res"
    try:
        font_title = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 38)
        font_subtitle = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Medium.ttf"), 19)
        font_section = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 22)
        font_th = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 16)
        font_td_name = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 18)
        font_td_sub = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Regular.ttf"), 15)
        font_td_desc = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Medium.ttf"), 15)
        font_mono = ImageFont.truetype("consola.ttf", 17)
        font_mono_small = ImageFont.truetype("consola.ttf", 14)
        font_badge_lbl = ImageFont.truetype(os.path.join(font_dir, "Quicksand-Bold.ttf"), 15)
    except Exception as e:
        print(f"Font load fallback: {e}")
        font_title = ImageFont.load_default()
        font_subtitle = font_section = font_th = font_td_name = font_td_sub = font_td_desc = font_mono = font_mono_small = font_badge_lbl = ImageFont.load_default()

    # Title & Subtitle
    draw.text((60, 42), "UNIVERSAL ACCESSIBLE COLOR PALETTE (OKABE–ITO)", fill=(255, 255, 255, 255), font=font_title)
    draw.text((60, 92), "Certified distinguishability for Deuteranopia, Protanopia, Tritanopia & Monochromacy | Garmire Module Design System", fill=(170, 175, 185, 255), font=font_subtitle)

    # Dividing rule
    draw.line([(60, 132), (W - 60, 132)], fill=(55, 60, 70, 255), width=2)

    # Color Data
    palette = [
        {
            "name": "Sky Blue",
            "hex": "#56B4E9",
            "role": "Light Cyan-Blue (High Luminance)",
            "cvd": "Clean separation from Orange, Vermilion & Yellow across all CVD axes."
        },
        {
            "name": "Safety Orange",
            "hex": "#E69F00",
            "role": "Golden Amber (Mid-High Luminance)",
            "cvd": "Distinct from Blues; does not shift to dull brown/green under protan/deutan."
        },
        {
            "name": "Cobalt Blue",
            "hex": "#0072B2",
            "role": "Deep Primary Blue (Low Luminance)",
            "cvd": "Dark anchor; maximum contrast against Sky Blue, Orange, and Yellow."
        },
        {
            "name": "Vermilion",
            "hex": "#D55E00",
            "role": "Punchy Red-Orange (Mid-Low Luminance)",
            "cvd": "Perceived noticeably darker than Orange/Yellow; no red-green confusion."
        },
        {
            "name": "Bluish Green",
            "hex": "#009E73",
            "role": "Saturated Teal / Mint (Mid Luminance)",
            "cvd": "High blue component prevents confusion with Yellow/Orange in red-green blindness."
        },
        {
            "name": "Electric Yellow",
            "hex": "#F0E442",
            "role": "Bright Lemon (Peak Luminance)",
            "cvd": "Highest luminance in set; instantly identified even under total monochromacy."
        },
        {
            "name": "Reddish Purple",
            "hex": "#CC79A7",
            "role": "Warm Orchid / Magenta (Mid Luminance)",
            "cvd": "Easily identified against blues and greens; clear identity under protanopia."
        }
    ]

    # Table layout
    tbl_x = 60
    tbl_y = 155
    tbl_w = 1630
    row_h = 72

    # Column X coordinates
    col_swatch = tbl_x + 12
    col_name = tbl_x + 180
    col_hex = tbl_x + 380
    col_rgb = tbl_x + 520
    col_role = tbl_x + 670
    col_cvd = tbl_x + 1010

    # Header Row
    draw.rounded_rectangle([tbl_x, tbl_y, tbl_x + tbl_w, tbl_y + 36], radius=6, fill=(36, 40, 46, 255))
    draw.text((col_swatch, tbl_y + 8), "SWATCH", fill=(140, 150, 165, 255), font=font_th)
    draw.text((col_name, tbl_y + 8), "NAME", fill=(140, 150, 165, 255), font=font_th)
    draw.text((col_hex, tbl_y + 8), "HEX CODE", fill=(140, 150, 165, 255), font=font_th)
    draw.text((col_rgb, tbl_y + 8), "RGB (0-255)", fill=(140, 150, 165, 255), font=font_th)
    draw.text((col_role, tbl_y + 8), "ROLE / LUMINANCE", fill=(140, 150, 165, 255), font=font_th)
    draw.text((col_cvd, tbl_y + 8), "CVD ACCESSIBILITY BENEFIT", fill=(140, 150, 165, 255), font=font_th)

    cur_y = tbl_y + 44
    for i, item in enumerate(palette):
        if i % 2 == 0:
            draw.rounded_rectangle([tbl_x, cur_y - 3, tbl_x + tbl_w, cur_y + row_h - 10], radius=8, fill=(32, 35, 41, 255))
        
        rgb = hex_to_rgb(item["hex"])
        # Swatch with subtle border
        draw.rounded_rectangle([col_swatch, cur_y + 2, col_swatch + 140, cur_y + row_h - 16], radius=6, fill=rgb, outline=(255, 255, 255, 60), width=1)
        
        # Name
        draw.text((col_name, cur_y + 12), item["name"], fill=(245, 245, 250, 255), font=font_td_name)
        
        # Hex Code
        draw.text((col_hex, cur_y + 14), item["hex"], fill=(255, 255, 255, 255), font=font_mono)
        
        # RGB
        rgb_str = f"{rgb[0]}, {rgb[1]}, {rgb[2]}"
        draw.text((col_rgb, cur_y + 14), rgb_str, fill=(170, 180, 195, 255), font=font_mono_small)
        
        # Role
        draw.text((col_role, cur_y + 14), item["role"], fill=(210, 215, 225, 255), font=font_td_desc)

        # CVD Benefit
        draw.text((col_cvd, cur_y + 14), item["cvd"], fill=(185, 200, 220, 255), font=font_td_desc)

        cur_y += row_h

    # Anchors row at bottom of table
    draw.rounded_rectangle([tbl_x, cur_y - 3, tbl_x + tbl_w, cur_y + row_h - 10], radius=8, fill=(32, 35, 41, 255))
    # Double swatch for White & Charcoal
    draw.rounded_rectangle([col_swatch, cur_y + 2, col_swatch + 66, cur_y + row_h - 16], radius=6, fill=(255, 255, 255, 255), outline=(100, 100, 100, 255), width=1)
    draw.rounded_rectangle([col_swatch + 74, cur_y + 2, col_swatch + 140, cur_y + row_h - 16], radius=6, fill=(26, 26, 26, 255), outline=(80, 80, 80, 255), width=1)
    draw.text((col_name, cur_y + 12), "White & Charcoal", fill=(245, 245, 250, 255), font=font_td_name)
    draw.text((col_hex, cur_y + 14), "#FFFFFF / #1A1A1A", fill=(255, 255, 255, 255), font=font_mono_small)
    draw.text((col_rgb, cur_y + 14), "Anchors", fill=(170, 180, 195, 255), font=font_mono_small)
    draw.text((col_role, cur_y + 14), "Max / Min Luminance Reference", fill=(210, 215, 225, 255), font=font_td_desc)
    draw.text((col_cvd, cur_y + 14), "Immediate structural luminance anchor points regardless of color deficiency.", fill=(185, 200, 220, 255), font=font_td_desc)

    # ----------------------------------------------------
    # RIGHT SIDE PANEL: FAC 73 MODULE STRIPE SIMULATION
    # ----------------------------------------------------
    rp_x = tbl_x + tbl_w + 35
    rp_y = 155
    rp_w = W - rp_x - 60
    rp_h = 920

    # Panel Container (Represents Eurorack panel section in #7c7c7c)
    draw.rounded_rectangle([rp_x, rp_y, rp_x + rp_w, rp_y + rp_h], radius=12, fill=(36, 40, 46, 255), outline=(55, 60, 70, 255), width=2)
    
    draw.text((rp_x + 22, rp_y + 20), "FAC 73 BADGE EXAMPLES", fill=(255, 255, 255, 255), font=font_section)
    draw.text((rp_x + 22, rp_y + 54), "Simulated on Eurorack #7c7c7c faceplate\nSize: 1.0\" tall x 0.2\" wide (5:1 aspect ratio)", fill=(170, 175, 185, 255), font=font_td_sub)

    # Badge mockups
    mockups = [
        {
            "title": "2-Band High Contrast",
            "desc": "Cobalt Blue (#0072B2) + Electric Yellow (#F0E442)",
            "bands": [("#0072B2", 1), ("#F0E442", 1)]
        },
        {
            "title": "3-Band Tricolor (Saville Style)",
            "desc": "Vermilion (#D55E00) + White (#FFFFFF) + Sky Blue (#56B4E9)",
            "bands": [("#D55E00", 1), ("#FFFFFF", 1), ("#56B4E9", 1)]
        },
        {
            "title": "3-Band Weighted Ratios (2 : 1 : 1)",
            "desc": "Bluish Green (wide) + Orange + Charcoal",
            "bands": [("#009E73", 2), ("#E69F00", 1), ("#1A1A1A", 1)]
        },
        {
            "title": "4-Band Full Code",
            "desc": "Safety Orange + Magenta + Sky Blue + Yellow",
            "bands": [("#E69F00", 1), ("#CC79A7", 1), ("#56B4E9", 1), ("#F0E442", 1)]
        }
    ]

    bm_y = rp_y + 115
    for mock in mockups:
        # Mockup background card representing #7c7c7c panel
        draw.rounded_rectangle([rp_x + 20, bm_y, rp_x + rp_w - 20, bm_y + 175], radius=8, fill=(124, 124, 124, 255), outline=(100, 100, 100, 255), width=1)
        
        # Badge dimensions: 1" tall x 0.2" wide -> in pixels: 150px tall x 30px wide
        bx = rp_x + 45
        by = bm_y + 12
        bw = 32
        bh = 150

        # Draw panel left edge indicator
        draw.line([(bx - 12, by - 5), (bx - 12, by + bh + 5)], fill=(50, 50, 50, 255), width=2)

        # Draw badge bands
        total_weight = sum(w for _, w in mock["bands"])
        cur_band_y = by
        for hex_col, weight in mock["bands"]:
            band_h = int((weight / total_weight) * bh)
            rgb = hex_to_rgb(hex_col)
            draw.rectangle([bx, cur_band_y, bx + bw, cur_band_y + band_h], fill=rgb)
            cur_band_y += band_h
        
        # Fill remainder to avoid rounding gap
        if cur_band_y < by + bh:
            last_col = hex_to_rgb(mock["bands"][-1][0])
            draw.rectangle([bx, cur_band_y, bx + bw, by + bh], fill=last_col)

        # Crisp badge outline
        draw.rectangle([bx, by, bx + bw, by + bh], outline=(30, 30, 30, 255), width=1)

        # Text labels on mock card
        tx = bx + bw + 20
        draw.text((tx, by + 10), mock["title"], fill=(255, 255, 255, 255), font=font_badge_lbl)
        
        # Wrap desc
        words = mock["desc"].split(" + ")
        d_y = by + 34
        for w in words:
            draw.text((tx, d_y), "+ " + w if d_y > by + 34 else w, fill=(240, 240, 240, 255), font=font_mono_small)
            d_y += 18
            
        draw.text((tx, by + 118), "1.0\" x 0.2\" edge graphic", fill=(215, 215, 215, 255), font=font_td_sub)

        bm_y += 195

    # Footer banner
    draw.line([(60, H - 70), (W - 60, H - 70)], fill=(55, 60, 70, 255), width=1)
    draw.text((60, H - 50), "Note: Alternating high/low luminance across adjacent bands ensures pattern recognition even under monochromatic stage lighting.", fill=(150, 160, 175, 255), font=font_td_sub)
    draw.text((W - 360, H - 50), "Exact Hex Codes Rendered in sRGB", fill=(100, 180, 255, 255), font=font_mono_small)

    # Save image
    out_dir = r"C:\Users\Pat\.gemini\antigravity\brain\1810fd88-4c00-4461-8eb4-ad0fdb55eb44"
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "accessible_palette_exact.png")
    img.save(out_path)
    print(f"Saved exact image to: {out_path}")

    scratch_path = os.path.join("scratch", "accessible_palette_exact.png")
    img.save(scratch_path)
    print(f"Saved copy to: {scratch_path}")

if __name__ == "__main__":
    main()
