# Garmire Faceplate Typography Specification

This document defines the exact font files, point sizes, fontTools vector scale factors, text casing, and vertical offsets for all panel interface features.

---

## 1. Typography Hierarchy & Sizing

| Feature / Label Role | Font File | Size (pt) | Scale Factor (`scale`) | Case | Vertical Anchor / Baseline Offset | Color (Medium Skin) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Module Title** | `res/D-DIN-Bold.otf` | **16 pt** | `0.005644` | UPPERCASE | Fixed $Y = 8.000\text{ mm}$ (horizontally centered) | `#ffffff` / theme title |
| **Version Tag** | `res/Quicksand-Regular.ttf` | **4.5 pt** | `0.001600` | lowercase | Fixed $Y = 8.000\text{ mm}$ (right margin $X = W - 2.15\text{ mm}$) | `#aaaaaa` |
| **Main Knobs (Zone 1 & 2)** | `res/D-DINCondensed-Bold.otf` | **10 pt** | `0.003528` | UPPERCASE | Offset **$-5.75\text{ mm}$** above knob center | `#222222` / theme label |
| **CV Trimpots (Zone 3)** | `res/D-DINCondensed.otf` | **8 pt** | `0.002822` | UPPERCASE | Offset **$-4.00\text{ mm}$** above trimpot center | `#222222` / theme label |
| **I/O Jacks (Zone 4)** | `res/D-DINCondensed.otf` | **8 pt** | `0.002822` | UPPERCASE | Offset **$-5.00\text{ mm}$** above jack center | `#222222` / theme label |

---

## 2. Scale Factor Formula

Vector typography is rendered directly into cubic Bezier path coordinates (`<path d="..."/>`) via `fontTools` and `pathops`. TrueType / OpenType font units per em ($\text{UPM} = 1000$ for OTF) are converted to millimeters using:

$$\text{scale} = \frac{\text{size}_{\text{pt}} \times 25.4\text{ mm/in}}{72.0\text{ pt/in} \times \text{UPM}}$$

- **16 pt**: $\frac{16.0 \times 25.4}{72.0 \times 1000} \approx \mathbf{0.005644}$
- **10 pt**: $\frac{10.0 \times 25.4}{72.0 \times 1000} \approx \mathbf{0.003528}$
- **8 pt**: $\frac{8.0 \times 25.4}{72.0 \times 1000} \approx \mathbf{0.002822}$
- **4.5 pt**: $\frac{4.535 \times 25.4}{72.0 \times 1000} \approx \mathbf{0.001600}$

---

## 3. Mandatory Typography Rules

1. **Zero `<text>` Tags**: Faceplate SVGs must never contain `<text>` elements. All glyphs must be cubic Bezier contours (`C`).
2. **No Redundant "CV" in Trimpot Zone**: Trimpots in Zone 3 are implicitly CV depth controls; omit `"CV"` from trimpot row labels.
3. **No Duplicate Identical Labels**: Never repeat identical labels across adjacent columns. Use a single, horizontally centered label.
4. **Port & Parameter Sentence Case**: Tooltip strings in C++ use strict sentence case (e.g. `"Frequency modulation"`, `"Rise time"`), while panel faceplate typography uses the casing listed in the table above.

---

## 4. Horizontal Section Dividers / Delineators

- **Width & Alignment**: Stretched to exactly **80% of module width** (`0.80 * panel_w`), right-aligned to flush right edge (`x_start = panel_w * 0.20`).
- **Thickness / Height**: Total height **$0.300\text{ mm}$**, composed of a stacked embossed pair:
  - Top highlight line: height **$0.150\text{ mm}$** (`#999999` on Medium, `#e4e4e4` on Light, `#585858` on Dark)
  - Bottom shadow line: height **$0.150\text{ mm}$** (`#5e5e5e` on Medium, `#a8a8a8` on Light, `#202020` on Dark)
- **Vertical Positions ($Y$)**: Placed $5.00\text{ mm}$ clear of adjacent control text (e.g. Standard Multi-Row: Divider 1 at $Y = 57.00\text{ mm}$ / $60.00\text{ mm}$, Divider 2 at $Y = 82.00\text{ mm}$ / $85.00\text{ mm}$).
