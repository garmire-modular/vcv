# Garmire UI Design & MetaModule Graphics Harness

This document serves as the authoritative specification and operating manual for all UI design refreshes across the Garmire module library (VCV Rack v2 & 4ms MetaModule hardware).

---

## 1. Memorized MetaModule SDK & Hardware Architecture

All requirements from `C:\msys64\home\Pat\VCVRackModules\metamodule-plugin-sdk\docs` have been internalized:

### 1.1. Display & Screen Specifications
- **Hardware Display**: $240 \times 320\text{ pixels}$, ~144 ppi physical screen, **16-bit color (RGB565)**.
- **Module Faceplate Dimensions**:
  - Full-screen view: **Exactly 240 pixels high** ($128.50\text{ mm}$ standard Eurorack 3U $\rightarrow$ effective DPI of $47.44\text{ px/in}$).
  - Zoomed-out view: 180 pixels high (down to 120 pixels in future firmware).
  - Module Width: Directly proportional to Eurorack HP:
    - **3 HP** ($15.24\text{ mm}$): $28\text{ px}$ width
    - **6 HP** ($30.48\text{ mm}$): $57\text{ px}$ width
    - **10 HP** ($50.80\text{ mm}$): $95\text{ px}$ width
    - **15 HP** ($76.20\text{ mm}$): $143\text{ px}$ width
    - **22.5 HP** ($114.30\text{ mm}$): $214\text{ px}$ width

### 1.2. Color & Visual Rendering Constraints (RGB565)
- **16-Bit Color Depth (5-6-5)**:
  - Subtle gradients exhibit visible stepping and color banding. Faceplate art must use solid, crisp fills or deliberately stepped contrast.
  - Drop shadows and soft Gaussian blurs look muddy and should be avoided.
  - Hairlines and thin strokes under $0.15\text{ mm}$ (<1px) will shimmer or vanish. Strokes must be $\ge 0.176\text{ mm}$ (VCV) and $\ge 1\text{ px}$ (raster PNG).
- **Text Opacity Rule**: Opacity for text is ignored on hardware—all text is rendered at **100% opacity**. Colors must not rely on background alpha blending.

### 1.3. Asset Packaging & Runtime Mapping
- VCV Rack loads SVG faceplates from `res/<Module>.svg`.
- MetaModule packages PNG faceplates and assets from `metamodule/assets/<Module>.png` directly into the `.mmplugin` bundle via `tar`.
- The MetaModule VCV Rack adaptor intercepts `APP->window->loadSvg("res/foo.svg")` and dynamically maps it to `foo.png`. Filenames and relative paths must match identically.
- SVGs are **never rendered at runtime** on MetaModule (NanoSVG is not supported on hardware).

### 1.4. Displays (Screens) on MetaModule
- **Text Displays**:
  - High efficiency, rendered using `VCVTextDisplay` (VCV adaptor) or `DynamicTextDisplay` (native).
  - Uses LVGL binary font files (`FontName_SZ.bin`, 4-bpp) or built-in system fonts (`Segment7_14`, `DefaultMedium`, etc.).
  - Content supplied via non-audio thread `get_display_text(int display_id, std::span<char> text)`.
  - Display IDs must never collide with Light IDs.
- **Graphics Displays (Firmware v2.0+)**:
  - NanoVG calls inside widget `draw()` / `drawLayer()` (layer 1 only).
  - Drawing is strictly clipped to widget `box` bounds (except text for center alignment).
  - Target frame rate is ~20 FPS. No NanoSVG or texture drawing in dynamic screens.

---

## 2. VCV Rack Graphics Architecture & Vector Invariants

- **NanoSVG Invariants**:
  - **Zero `<text>` Tags**: NanoSVG does not reliably render SVG `<text>` elements. All typography must be converted to cubic Bezier paths (`C`) via `fontTools` and `pathops`.
  - **No Quadratic Beziers (`Q`)**: TrueType quadratic curves must be converted to explicit cubic Beziers (`C`) via `QuadToCubicPathPen`.
  - **Winding & Simplification**: All glyph contours must pass through Skia `pathops.simplify()` to eliminate self-intersections and ensure consistent `NonZero` fill rule.
  - **NanoSVG Number Formatting**: Coordinates must be formatted to 1 decimal place (`.1f`) with space padding around command tokens (`M C L Z`).

---

## 3. Garmire Unified Layout & Clearance Specification

Garmire panels adhere to the **Fixed-Anchor & Bottom-Aligned** spatial architecture:

```
+-------------------------------------------------------+ Y = 0.00 mm
| ZONE 1: TITLE & FIXED TOP ANCHORS                     |
| - Title in Node.otf (Fixed Y = 7.62 mm, #ffffff)      |
| - Version Tag "v1.X.Y" (Fixed Y = 10.41 mm, #aaaaaa)  |
| - Row 1 Parameter Labels (Fixed Y = 13.07 mm)         |
| - Row 1 Main Knobs (Fixed Y = 21.59 mm)               |
+-------------------------------------------------------+
| ZONE 2: PRIMARY PARAMETERS (Top-Down Spacing)         |
| - Additional Parameter Rows (ΔY = 21.41 mm pitch)     |
+================ Delineator Line 1 =====================+
| ZONE 3: CV ATTENUVERTERS / MODIFIERS (Dynamic)        |
| - Trimpot Rows (ΔY = 12.00 mm pitch stacked above L2) |
+================ Delineator Line 2 =====================+
| ZONE 4: I/O PORTS & ROUTING (Bottom-Aligned Upward)   |
| - Jack Row 1 (Inputs)    [Y = Y_out - 3 * 9.50 mm]    |
| - Jack Row 2 (Primary CV)[Y = Y_out - 2 * 9.50 mm]    |
| - Jack Row 3 (Sec. CV)   [Y = Y_out - 1 * 9.50 mm]    |
| - Jack Row 4 (Outputs)   [Fixed Y = 118.00 mm]        |
| - Garmire Brand Insignia [Y = 123.50 mm]              |
+-------------------------------------------------------+ Y = 128.50 mm
```

### Physical Clearance Tolerances:
- **Knob $\rightarrow$ Knob**: $\Delta Y \ge 11.41\text{ mm}$
- **Trimpot $\rightarrow$ Trimpot**: $\Delta Y \ge 5.50\text{ mm}$
- **Jack $\rightarrow$ Jack**: $\Delta Y \ge 9.50\text{ mm}$ pitch ($3.00\text{ mm}$ collar clearance)
- **Left Column ($X$) $\rightarrow$ Right Column ($Y$)**: $\Delta X \ge 15.24\text{ mm}$ ($5.24\text{ mm}$ collar clearance)

### Labeling Directives:
1. No redundant `"INPUT"` or `"OUTPUT"` text.
2. No redundant `"CV"` on trimpots.
3. No duplicate identical labels horizontally across columns. Single centered label only.
4. Attenuverter tooltip convention: `"X <parameter> CV depth"` / `"<Parameter> CV depth"`.

---

## 4. UI Design Harness Tooling

The harness provides two primary Python modules in `scratch/`:

### 4.1. `scratch/design_harness.py`
Core object-oriented library providing:
- `UITheme`: Preset and custom visual themes (`slate`, `dark_carbon`, `anodized_black`, `aluminum`).
- `TypographyEngine`: High-performance glyph caching, quadratic-to-cubic Bezier conversion, and Skia path simplification.
- `HardwareAssetEngine`: Vector generators for jacks, screws, trimpots, knobs, and the Garmire brand logo.
- `ClearanceValidator`: Validates physical spacing and SVG NanoSVG invariants.
- `DualTargetExporter`: Synchronously writes `res/<Module>.svg` and renders `metamodule/assets/<Module>.png` (240px) via CairoSVG.

### 4.2. `scratch/refresh_all_panels.py`
Batch CLI tool for applying global refreshes:
```bash
# Refresh all modules with a dark carbon theme and add brand logo:
python scratch/refresh_all_panels.py --theme dark_carbon --logo

# Refresh all modules with custom background color:
python scratch/refresh_all_panels.py --bg "#1e1e24"

# Refresh a single module:
python scratch/refresh_all_panels.py --module Smooth --theme anodized_black
```

### 4.3. Registered Subagent: `ui_designer`
A dedicated subagent (`ui_designer`) is registered and available via `invoke_subagent` to perform deep vector refactors, layout audits, custom hardware asset designs, and pre-flight validation.
