---
title: 'Agent Directives: VCV Rack & 4ms MetaModule Development'
created: '2026-09-26T19:47:48.812Z'
modified: '2026-09-26T19:47:50.536Z'
---

# Agent Directives: VCV Rack & 4ms MetaModule Development

You are an expert audio DSP engineer and systems programmer working in this repository. All code, vector artwork, and configuration files must comply strictly with VCV Rack v2 and 4ms MetaModule hardware constraints.

> [!IMPORTANT]
> **Workspace Path Directive**: The repository path has officially moved from `G:\dupe\work\antigravity\Concordia\26-27\laser` to `G:\dupe\work\garmire`.
> Although a filesystem junction exists for compatibility, all agents MUST target `G:\dupe\work\garmire` for all file paths, tool arguments, scripts, and terminal commands.

---

## 1. Mandatory Pre-Flight Checklist

Before writing or modifying any code, SVGs, or build configurations, you MUST output a planning block containing this verified checklist:

```markdown
### Pre-Flight Invariant Verification
- [ ] **Architecture Check:** DSP changes preserve real-time safety (no malloc, free, locks, or file I/O in `process()`).
- [ ] **Functional Taxonomy:** Module class (Generate, Transform, Utility, or Control) confirmed via user prompt and tagged in `plugin.json`.
- [ ] **HP Sizing & Crowding Check:** Pre-flight component spacing verified against clearance tolerances; HP increase proposed to user if crowded.
- [ ] **No Virtual Screws:** No `ScrewSilver` or `ScrewBlack` widgets added to `ModuleWidget`.
- [ ] **Version Tracking:** Initial module version starts strictly at `v1.0.0` (independent of the overall plugin suite version `v2.X.X`), with per-module version recorded and autoincremented.
- [ ] **Panel Edit Method:** Vector modifications target `scratch/generate_<module>_panel.py`, NEVER direct edits to `res/<Module>.svg`.
- [ ] **Font Handling:** No `<text>` tags used. All typography converted to cubic paths (`C`) via `fontTools` + `pathops`.
- [ ] **Port & Parameter Tooltips:** Tooltips use strict sentence case; no redundant "input"/"output" words in jack tooltips; attenuverters named `<Parameter> CV depth`.
- [ ] **Sync Verification:** Mandatory per-module version bumps are synchronously reflected across `plugin.json`, faceplate SVG, and `metamodule/assets/<Module>.png`.
- [ ] **Build Targets:** Commands to validate both VCV Rack and MetaModule cross-compilation are identified.
```

---

## 2. Git Version Control & Remote Sync

- **Local Commits on Every Completed Task/Change**:
  - Whenever code, panel scripts, or asset changes are verified (compilation succeeds and pre-flight invariants are satisfied), stage the relevant source files and create a clean, descriptive local commit (e.g., `feat(...)`, `fix(...)`, `panel(...)`, `docs(...)`).
  - Never commit build artifacts (`*.dll`, `*.vcvplugin`, `build/`, `dist/`).
- **Per-Module Versioning & Autoincrementation Protocol**:
  - **Individual Module Version**: Each module tracks its own independent semantic version starting at **`v1.0.0`** upon initial creation.
  - **Module Version Autoincrementation**: Every time a module undergoes changes (DSP fixes, UI adjustments, feature updates), its individual version MUST be bumped (e.g. `v1.0.0` $\rightarrow$ `v1.0.1` for fixes/refinements, `v1.1.0` for feature additions).
  - **Panel & Asset Synchrony**: The per-module version is stamped on the faceplate SVG (`scratch/generate_<module>_panel.py`), rendered to `res/<Module>.svg`, and mirrored in `metamodule/assets/<Module>.png`.
  - **Plugin Suite Version Separation**: The overall plugin suite version in `plugin.json` (`"version": "2.26.0"`) is managed separately at the release/package level. Never jump an initial or individual module version to match the suite's global `2.26.0`.
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

## 4. Mandatory Plugin Directory Deployment (`make install`)

Whenever compiling or verifying changes to the VCV Rack plugin:
- The system MUST ALWAYS copy the compiled Rack files to the active user plugin directories so the user can immediately test them in VCV Rack.
- Run `make install` (or deploy `plugin.dll`, `plugin.json`, `res/`, and `dist/*.vcvplugin` to `C:/Users/Pat/AppData/Local/Rack2/plugins-win-x64/Garmire` and `C:/Users/Pat/AppData/Local/Rack2/plugins-win-x64`).

---

## 5. Module Taxonomy & Classification Prompt

Every Garmire module belongs to one of four functional classes:
- **Generate**: Oscillators, noise/chaos generators, pattern/function generators, sources.
- **Transform**: Waveshapers, slew, filters, spatial/geometric warpers, math operators.
- **Utility**: Mixers, attenuverters, switches, mutes, routing, splitters.
- **Control**: Envelopes, CV sources, sequencers, macro controllers.

> [!IMPORTANT]
> **Mandatory User Classification Prompt**:
> Whenever designing, scaffolding, or adding a new module, you MUST explicitly prompt the user to confirm/assign which of the 4 classes the module belongs to:
> *"Which functional class does this module belong to: Generate, Transform, Utility, or Control?"*
> Once confirmed, record the classification in the pre-flight checklist and add the corresponding tag to the module's entry in `plugin.json`.

---

## 6. Module Sizing, Component Spacing & Crowding Calculation

Before generating a panel or writing widget layouts, conduct a pre-flight panel component calculation:
- **Minimum Horizontal Clearances**:
  - Minimum horizontal jack pitch: $\ge 8.50\text{ mm}$ center-to-center (PJ301M knurled collar $\varnothing \approx 8.3\text{ mm}$).
  - Minimum horizontal knob pitch: $\ge 12.00\text{ mm}$ for small knobs / trimpots; $\ge 15.00\text{ mm}$ for medium knobs.
  - Minimum side margin from panel edge to component center: $\ge 6.00\text{ mm}$.
- **Pre-Flight Width Calculation**:
  $$\text{Width}_{\text{req}} = 2 \times \text{Margin} + (N_{\text{cols}} - 1) \times \Delta X_{\text{min}}$$
- **Proactive HP Increase Suggestion**:
  - If placing the required columns or controls in a targeted HP results in clearance violations or visual crowding, you MUST proactively suggest increasing the module's width to the next standard Eurorack HP (e.g. 6 HP $\rightarrow$ 8 HP / 10 HP / 12 HP / 20 HP) before proceeding.

---

## 7. Panel Layout & UI Design System

This section defines the mandatory, unified panel grid, spatial architecture, component clearance tolerances, and typography hierarchy for all **Garmire** hardware-compatible modules.

---

### 7.1. Standard Footprints & Column Grids

- **Height**: Standard Eurorack 3U ($128.50\text{ mm}$ / $240\text{ px}$ MetaModule bitmap). Fill: `#6e6e6e`.
- **Footprint Tiers**:
  - **8 HP** ($40.64\text{ mm}$): 2, 3, 4 params. 2 columns: $x = [12.00, 28.64]\text{ mm}$.
  - **12 HP** ($60.96\text{ mm}$): 5, 6 params. 3 columns: $x = [12.00, 30.48, 48.96]\text{ mm}$.
  - **14 HP** ($71.12\text{ mm}$): 7, 8 params. 4 columns: $x = [8.56, 26.23, 44.89, 62.56]\text{ mm}$.
- **Odd-Parameter Rule**: On partial rows, controls right-align; column 0 ($x[0]$) remains empty (e.g. 3-param module places 3rd control in column 1 on row 2).

---

### 7.2. Vertical Coordinates & Spacing Architecture

```
+-------------------------------------------------------+ Y = 0.00 mm
| ZONE 1 & 2: MAIN KNOBS (Top-Down Spacing)             |
| - Title Baseline: Y = 7.00 mm (Node.otf or D-DIN)     |
| - Knob Row 1: Center Y = 20.00 mm (Label Y = 14.50 mm)|
| - Knob Row 2: Center Y = 37.00 mm (Label Y = 31.50 mm)|
+================ Section Divider 1 ====================+
| ZONE 3: ATTENUVERTERS (Stacked from Divider 2)        |
| - Trimpot Row 1: Center Y = 66.00 mm (Label 62.00 mm) |
| - Trimpot Row 2: Center Y = 77.00 mm (Label 73.00 mm) |
+================ Section Divider 2 ====================+
| ZONE 4: I/O JACKS (Bottom-Up from Y = 120.00 mm)       |
| - Jack Row 1 (CV 1): Center Y = 93.00 mm (Label 87.00)|
| - Jack Row 2 (CV 2): Center Y = 106.50 mm (Lbl 100.50)|
| - Jack Row 3 (Outs): Center Y = 120.00 mm (Lbl 114.00)|
+-------------------------------------------------------+ Y = 128.50 mm
```

#### **Standard Multi-Row Modules (3 to 8 Parameters)**
All modules with 2 knob rows share identical vertical coordinates:
- **Knob Row 1**: Center $Y = 20.00\text{ mm}$ (Label baseline $14.50\text{ mm}$, offset $-5.50\text{ mm}$).
- **Knob Row 2**: Center $Y = 37.00\text{ mm}$ (Label baseline $31.50\text{ mm}$, pitch $\Delta Y = 17.00\text{ mm}$).
- **Section Divider 1**: Center $Y = 57.00\text{ mm}$ ($5.00\text{ mm}$ above Trimpot Row 1 label).
- **Trimpot Row 1**: Center $Y = 66.00\text{ mm}$ (Label baseline $62.00\text{ mm}$, offset $-4.00\text{ mm}$).
- **Trimpot Row 2**: Center $Y = 77.00\text{ mm}$ (Label baseline $73.00\text{ mm}$, pitch $\Delta Y = 11.00\text{ mm}$).
- **Section Divider 2**: Center $Y = 82.00\text{ mm}$ ($5.00\text{ mm}$ below Trimpot Row 2 center).
- **Jack Row 1 (CV 1)**: Center $Y = 93.00\text{ mm}$ (Label baseline $87.00\text{ mm}$, $5.00\text{ mm}$ below Divider 2, $-6.00\text{ mm}$ above jack center).
- **Jack Row 2 (CV 2)**: Center $Y = 106.50\text{ mm}$ (Label baseline $100.50\text{ mm}$, pitch $\Delta Y = 13.50\text{ mm}$).
- **Jack Row 3 (Outputs)**: Center $Y = 120.00\text{ mm}$ (Label baseline $114.00\text{ mm}$, pitch $\Delta Y = 13.50\text{ mm}$).

#### **Single-Row Modules (2 Parameters)**
- **Knob Row 1**: Center $Y = 20.00\text{ mm}$ (Label $14.50\text{ mm}$).
- **Section Divider 1**: Center $Y = 81.50\text{ mm}$ ($5.00\text{ mm}$ above Trimpot label).
- **Trimpot Row 1**: Center $Y = 90.50\text{ mm}$ (Label $86.50\text{ mm}$).
- **Section Divider 2**: Center $Y = 95.50\text{ mm}$ ($5.00\text{ mm}$ below Trimpot center).
- **Jack Row 1 (CV)**: Center $Y = 106.50\text{ mm}$ (Label $100.50\text{ mm}$, $5.00\text{ mm}$ below Divider 2).
- **Jack Row 2 (Outputs)**: Center $Y = 120.00\text{ mm}$ (Label $114.00\text{ mm}$, pitch $\Delta Y = 13.50\text{ mm}$).

---

### 7.3. Universal Relative Clearance & Offsets

- **Dividers**: Stretched to $80-82\%$ panel width, right-aligned. Height $0.30\text{ mm}$ ($0.15\text{ mm}$ highlight `#999999` + $0.15\text{ mm}$ shadow `#505050`).
- **Divider-to-Control Tolerances**:
  - Divider 1 $\rightarrow$ Trimpot 1 label: $5.00\text{ mm}$.
  - Trimpot lowest center $\rightarrow$ Divider 2: $5.00\text{ mm}$.
  - Divider 2 $\rightarrow$ Jack 1 label: $5.00\text{ mm}$.
- **Vertical Pitches & Offsets**:
  - Knobs: $\Delta Y = 17.00\text{ mm}$ (Label baseline $-5.50\text{ mm}$ above center).
  - Trimpots: $\Delta Y = 11.00\text{ mm}$ (Label baseline $-4.00\text{ mm}$ above center).
  - Jacks: $\Delta Y = 13.50\text{ mm}$ (Label baseline $-6.00\text{ mm}$ above center).
- **Bottom Anchor**: Lowest jack row fixed at $Y = 120.00\text{ mm}$ ($4.35\text{ mm}$ hardware lip margin to bottom edge).

---

### 7.4. Typography & Hierarchy

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

### 7.5. Parameter Tooltip & Panel Labeling Directives

To maintain a minimal, professional aesthetic and avoid tooltip clutter in VCV Rack:

1. **No Redundant "Input" / "Output" Words in Port Tooltips**:
   - VCV Rack automatically appends `"input"` to `configInput()` and `"output"` to `configOutput()` tooltips in the software interface.
   - Do NOT include `"input"` or `"output"` (or `"IN"` / `"OUT"`) in `configInput()` or `configOutput()` port names. Use concise names such as `configInput(X_INPUT, "X")` or `configInput(FREQ_CV_INPUT, "Frequency CV")`.

2. **No Redundant "CV" Words in Attenuverter Section on Faceplates**:
   - Trimpots located in Zone 3 are inherently CV modulation attenuverters.
   - Do NOT append `"CV"` to trimpot row labels on the faceplate SVG. Label trimpot rows with the parameter name alone (e.g. `DEPTH`, `COUNT`, `RATE`, `SLOPE`).

3. **Strict Ban on Duplicate Identical Labels (Single Centered Label Across Controls)**:
   - Duplication of labels for adjacent controls (knobs, trimpot attenuverters, or I/O jacks) that share the same function or channel is a **strict no-no**.
   - NEVER repeat identical labels horizontally across multiple columns (e.g., NEVER render `"X X X X"`, `"Y Y Y Y"`, `"R R R R"`, `"DEPTH DEPTH"`).
   - Place a **single, horizontally centered label** centered across the entire row/span of controls.
   - Channel / column identity is established solely by the top column headers (e.g., `IN 1`, `IN 2`, `SW`, `OUT` or `X`, `Y`), while row / parameter function is established by the single centered label.

4. **Attenuverter Parameter Tooltip Standard**:
   - All attenuverter parameters in source code must be configured with tooltips in the standard format:
     - For channel-specific attenuverters: `"X <parameter> CV depth"` or `"Y <parameter> CV depth"` (e.g., `"X smooth CV depth"`, `"Y slope CV depth"`, `"X crest CV depth"`). The channel prefix `X`/`Y` and `CV` are uppercase; the parameter name and `depth` are lowercase.
     - For non-channel or single-parameter attenuverters: `"<Parameter> CV depth"` (e.g., `"Count CV depth"`, `"Length CV depth"`, `"Rotation CV depth"`). Sentence case with `CV` uppercase and `depth` lowercase.
   - Do NOT use `"Attenuverter"`, `"CV Attenuverter"`, `"Depth"`, or title-cased `"CV Depth"`.

5. **Strict Sentence Case for All Parameter & Port Tooltips**:
   - All `configParam()`, `configInput()`, and `configOutput()` display strings must use sentence case (e.g., `"Frequency modulation"`, `"Rise time"`, `"X position offset"`), keeping standard capitalized acronyms/abbreviations (`CV`, `VCA`, `LFO`, `RGB`, `OKLCH`, `X`, `Y`).
   - Do NOT use Title Case for tooltip names (e.g., use `"Rise time"`, NOT `"Rise Time"`).

6. **Strict Ban on Virtual Screws**:
   - Garmire modules do not include virtual mounting screws in software.
   - NEVER instantiate `ScrewSilver`, `ScrewBlack`, or any screw widget in `ModuleWidget` constructors.

---

### 7.6. Module Palette Badge Protocol & Palette Database Synchronization

All Garmire hardware modules include a vertical color badge oriented along the far left edge of the faceplate to establish visual identity across the rack.

1. **Badge Dimensions & Geometry**:
   - **Horizontal Position**: Flush left at $X = 0.00\text{ mm}$ ($x = 0$).
   - **Width**: Standard Eurorack $2.54\text{ mm}$ ($0.10\text{ in}$ / $0.5\text{ HP}$).
   - **Height**: Standard $88.90\text{ mm}$ ($3.50\text{ in}$).
   - **Vertical Centering**: Centered vertically at $Y = 64.25\text{ mm}$ ($y = 19.80\text{ mm}$ to $y = 108.70\text{ mm}$).

2. **New Module Default Placeholder Badge (`#5d5d5d`)**:
   - Anytime a new module is created, its faceplate SVG generator (`scratch/generate_<module>_panel.py`) MUST assign a placeholder badge consisting of a pure `#5d5d5d` neutral gray rectangle:
     ```xml
     <g id="palette-badge">
       <rect x="0" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />
     </g>
     ```
   - This `#5d5d5d` badge serves as the unassigned placeholder until an official chromatic palette has been generated and saved for the module.

3. **Palette Builder Database Synchronization**:
   - Whenever a new module is created or its panel SVG is generated/updated, the palette builder script MUST be executed immediately:
     ```powershell
     python scratch/palette/build_standalone.py
     ```
   - This script updates the embedded SVG and module definitions in `scratch/palette/index.html`. As a result, when the Palette Manager website is refreshed, the new module will immediately appear in the module selection dropdown and stage for preview and palette assignment.

