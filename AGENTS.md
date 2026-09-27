---
title: 'Agent Directives: VCV Rack & 4ms MetaModule Development'
created: '2026-09-26T19:47:48.812Z'
modified: '2026-09-26T19:47:50.536Z'
---

# Agent Directives: VCV Rack & 4ms MetaModule Development

You are an expert audio DSP engineer and systems programmer working in this repository. All code, vector artwork, and configuration files must comply strictly with VCV Rack v2 and 4ms MetaModule hardware constraints.

---

## 1. Mandatory Pre-Flight Checklist

Before writing or modifying any code, SVGs, or build configurations, you MUST output a planning block containing this verified checklist:

```markdown
### Pre-Flight Invariant Verification
- [ ] **Architecture Check:** DSP changes preserve real-time safety (no malloc, free, locks, or file I/O in `process()`).
- [ ] **Panel Edit Method:** Vector modifications target `scratch/generate_<module>_panel.py`, NEVER direct edits to `res/<Module>.svg`.
- [ ] **Font Handling:** No `<text>` tags used. All typography converted to cubic paths (`C`) via `fontTools` + `pathops`.
- [ ] **Sync Verification:** Any version bump is synchronously reflected across `plugin.json`, faceplate SVG, and `metamodule/assets/<Module>.png`.
- [ ] **Build Targets:** Commands to validate both VCV Rack and MetaModule cross-compilation are identified.
```

---

## 2. Git Version Control & Remote Sync

- **Local Commits on Every Completed Task/Change**:
  - Whenever code, panel scripts, or asset changes are verified (compilation succeeds and pre-flight invariants are satisfied), stage the relevant source files and create a clean, descriptive local commit (e.g., `feat(...)`, `fix(...)`, `panel(...)`, `docs(...)`).
  - Never commit build artifacts (`*.dll`, `*.vcvplugin`, `build/`, `dist/`).
- **Prompt to Push to GitHub on Milestones**:
  - After creating a new module or completing a substantial revision (e.g., major DSP rewrite, UI overhaul, version bump), explicitly ask the user:
    > *"Would you like to push to GitHub?"*
  - Do NOT push to GitHub automatically without user confirmation. Once confirmed by the user, push to the designated GitHub remote (`git push origin <branch>`).

---

## 3. Build & Compilation Concurrency Control

To prevent race conditions, file locks on `plugin.dll` or `build/` objects, and compiler conflicts:
- **Pre-Compilation Concurrency Check**:
  - Before running any `make` or build command, the system MUST inspect running processes to check if another compilation is currently in progress (`make`, `g++`, `gcc`, or `ninja`).
  - Check command:
    ```powershell
    Get-Process make, g++, gcc, ninja -ErrorAction SilentlyContinue
    ```
- **Concurrency Wait Protocol**:
  - If any compilation process is currently running:
    - Do **NOT** start a new compilation.
    - Wait **25 seconds** before checking process status again.
    - Repeat the 25-second wait loop until all conflicting compilation processes have exited.
  - Proceed with the `make` invocation **only** when the system confirms no other compilation is in progress.

---

## 6. Panel Layout & UI Design System

This section defines the mandatory, unified panel grid, spatial architecture, component clearance tolerances, and typography hierarchy for all **Garmire** hardware-compatible modules.

---

### 6.1. Grid & Dimensions

- **Panel Height**: Standard Eurorack 3U ($128.50\text{ mm}$ / $240\text{ px}$ MetaModule bitmap height).
- **Slim Module Width (6 HP)**: $30.48\text{ mm}$ ($1.20\text{ in}$) width ($57\text{ px}$ MetaModule bitmap width).
- **Panel Background Fill**: Dark gray `#7c7c7c`.
- **Horizontal Centering Columns**:
  - **Left Channel ($X$)**: $x = 7.62\text{ mm}$ ($0.30\text{ in}$)
  - **Right Channel ($Y$)**: $x = 22.86\text{ mm}$ ($0.90\text{ in}$)
  - **Center Column**: $x = 15.24\text{ mm}$ ($0.60\text{ in}$)

---

### 6.2. Layout Architecture: Fixed Anchors vs. Bottom-Aligned Dynamic Spacing

Garmire panels utilize a **Fixed-Anchor & Bottom-Aligned** spatial system. Absolute Y-coordinates apply **only** to the top title/first knob row and bottom signal outputs. All intermediate parameter rows, attenuverters, and CV inputs are positioned by **relative spacing rules bottom-aligned upward from the fixed output jacks**.

```
+-------------------------------------------------------+ Y = 0.00 mm
| ZONE 1: TITLE & FIXED TOP ANCHORS                     |
| - Title "module" in Node.otf (Fixed Y = 7.62 mm)      |
| - Version Tag "v1.X.Y" (Fixed Y = 10.41 mm)           |
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
+-------------------------------------------------------+ Y = 128.50 mm
```

#### **1. Fixed Top Anchors**
- **Module Title**: Baseline $Y = 7.620\text{ mm}$ ($0.30\text{ in}$), centered horizontally ($x = 15.24\text{ mm}$). Rendered in lowercase `Node.otf` at scale $0.004800$, fill `#ffffff`.
- **Version Tag**: Baseline $Y = 10.414\text{ mm}$, start $x \approx 13.25\text{ mm}$, scale $0.001600$, fill `#aaaaaa` (e.g. `v1.0.3`).
- **Row 1 Main Knobs**: Center $Y = 21.59\text{ mm}$ ($X = 7.62\text{ mm}$, $Y = 22.86\text{ mm}$).
  - *Row 1 Label*: Baseline $Y = 13.070\text{ mm}$, scale $0.003000$, fill `#1c1c1c` (e.g. `X`, `Y`).

#### **2. Fixed Bottom Anchor**
- **Bottom Output Jack Row**: Center **$Y = 118.00\text{ mm}$** ($10.50\text{ mm}$ clearance above the bottom edge $Y = 128.50\text{ mm}$).
  - *Shared Output Label*: Baseline $Y = 116.500\text{ mm}$, scale $0.002400$ (`OUT`).

#### **3. Bottom-Aligned I/O Jack Stacking (Zone 4)**
All jack rows stack **upward from the fixed output row** at $Y = 118.00\text{ mm}$ with a fixed vertical pitch of **$\Delta Y = 9.50\text{ mm}$**:
- **Output Jacks (Row $N$)**: $Y = 118.00\text{ mm}$
- **CV Input Row 2 (Row $N-1$)**: $Y = 118.00 - 9.50 = 108.50\text{ mm}$
- **CV Input Row 1 (Row $N-2$)**: $Y = 118.00 - 2(9.50) = 99.00\text{ mm}$
- **Signal Input Row (Row $N-3$)**: $Y = 118.00 - 3(9.50) = 89.50\text{ mm}$

#### **4. Dynamic Attenuverter & Delineator Placement (Zone 3)**
- **Delineator Line 2**: Placed $9.00\text{ mm}$ above the top-most jack center line.
- **Trimpot Rows**: Stacked above Line 2 at a vertical pitch of **$\Delta Y = 12.00\text{ mm}$**.
- **Delineator Line 1**: Placed $9.50\text{ mm}$ above the highest trimpot center line, separating Zone 3 from Zone 2.

---

### 6.3. Relative Spacing & Clearance Tolerances

| Relative Interface | Vertical Pitch ($\Delta Y$) | Horizontal Separation ($\Delta X$) | Minimum Physical Clearance |
| :--- | :--- | :--- | :--- |
| **Knob Row $\rightarrow$ Knob Row** | $21.41\text{ mm}$ | $0.00\text{ mm}$ | $11.41\text{ mm}$ (outer edge to edge) |
| **Trimpot Row $\rightarrow$ Trimpot Row** | $12.00\text{ mm}$ | $0.00\text{ mm}$ | $5.50\text{ mm}$ |
| **Jack Row $\rightarrow$ Jack Row** | $9.50\text{ mm}$ | $0.00\text{ mm}$ | $3.00\text{ mm}$ (connector collar clearance) |
| **Left Column ($X$) $\rightarrow$ Right Column ($Y$)** | $0.00\text{ mm}$ | $15.24\text{ mm}$ | $5.24\text{ mm}$ (horizontal jack separation) |

---

### 6.4. Typography & Hierarchy

| Typography Role | Font File | Case | Scale Factor | Relative Baseline Offset | Color |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Module Title** | `res/Node.otf` | Lowercase | `0.004800` | Fixed $Y = 7.620\text{ mm}$ | `#ffffff` |
| **Version Tag** | `res/Quicksand.ttf` | Lowercase | `0.001600` | Fixed $Y = 10.414\text{ mm}$ | `#aaaaaa` |
| **Channel Headers (Knobs/Jacks)** | `res/Quicksand-Medium.ttf` | Uppercase | `0.003000` | $-8.52\text{ mm}$ above Knob 1 / $-5.50\text{ mm}$ above Jack 1 | `#1c1c1c` |
| **Parameter Subheaders** | `res/Quicksand-Medium.ttf` | Uppercase | `0.002400` | $-8.50\text{ mm}$ above Knob $N$ | `#1c1c1c` |
| **Trimpot CV Labels** | `res/Quicksand-Medium.ttf` | Uppercase | `0.002000` | $-4.50\text{ mm}$ above Trimpot Center | `#2c2c2c` |
| **Jack Function Labels** | `res/Quicksand-Medium.ttf` | Uppercase | `0.002400` | $-1.50\text{ mm}$ above Jack Center | `#2c2c2c` |

#### **Mandatory Path Vector Pipeline Rules**:
1. **No `<text>` Elements**: Raw `<text>` tags are strictly forbidden in SVG faceplates.
2. **Cubic Bezier Conversion (`QuadToCubicPathPen`)**: TrueType quadratic curves (`Q`) must be converted to explicit cubic Beziers (`C`).
3. **Skia PathOps Simplification (`pathops.simplify`)**: All glyph contours must be passed through Skia `pathops.simplify()` to resolve winding directions and eliminate self-intersections.
4. **NanoSVG Number Formatting**: Coordinates must be formatted with `clean_svg_path_numbers()` (1 decimal place rounding `.1f` and space padding around command letters `M C L Z`).

---

### 6.5. Panel Labeling Directives & Clean Design Standards

To maintain a minimal, professional visual aesthetic and avoid visual clutter, all Garmire faceplates must comply strictly with the following labeling conventions:

1. **No Redundant "Input" / "Output" Words on Port Labels**:
   - VCV Rack automatically appends `"input"` or `"output"` to jack tooltips in the software interface.
   - Do NOT include `"INPUT"` or `"OUTPUT"` in SVG faceplate jack labels. Use concise labels such as `IN`, `OUT`, or centered parameter headers (e.g., `DEPTH`, `COUNT`).

2. **No Redundant "CV" Words in Attenuverter Section**:
   - Trimpots located in Zone 3 are inherently CV modulation attenuverters.
   - Do NOT append `"CV"` to trimpot row labels on the faceplate. Label trimpot rows with the parameter name alone (e.g. `DEPTH`, `COUNT`, `RATE`, `SLOPE`) without the word `"CV"`.

3. **Strict Ban on Duplicate Identical Labels (Single Centered Label Across Controls)**:
   - Duplication of labels for adjacent controls (knobs, trimpot attenuverters, or I/O jacks) that share the same function or channel is a **strict no-no**.
   - NEVER repeat identical labels horizontally across multiple columns (e.g., NEVER render `"X X X X"`, `"Y Y Y Y"`, `"R R R R"`, `"DEPTH DEPTH"`).
   - Place a **single, horizontally centered label** centered across the entire row/span of controls.
   - Channel / column identity is established solely by the top column headers (e.g., `IN 1`, `IN 2`, `SW`, `OUT` or `X`, `Y`), while row / parameter function is established by the single centered label.

4. **Attenuverter Parameter Labels & Tooltips Standard**:
   - All attenuverter parameters in source code must be configured with tooltips in the standard format:
     - For channel-specific attenuverters: `"X <parameter> CV depth"` or `"Y <parameter> CV depth"` (e.g., `"X smooth CV depth"`, `"Y slope CV depth"`, `"X crest CV depth"`). The channel prefix `X`/`Y` and `CV` are uppercase; the parameter name and `depth` are lowercase.
     - For non-channel or single-parameter attenuverters: `"<Parameter> CV depth"` (e.g., `"Count CV depth"`, `"Length CV depth"`, `"Rotation CV depth"`). Sentence case with `CV` uppercase and `depth` lowercase.
   - Do NOT use `"Attenuverter"`, `"CV Attenuverter"`, `"Depth"`, or title-cased `"CV Depth"`.

