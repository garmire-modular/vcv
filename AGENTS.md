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
- [ ] **Custom Hardware Widgets:** ModuleWidget uses custom Garmire widgets (`res/knob.svg`, `res/tinyknob.svg`, `res/3_5mm_jacks.svg`, `res/button_gray_0.svg`).
- [ ] **Triple Skin Architecture:** Panel script generates Medium, Light, and Dark SVG skins (`res/<Module>.svg`, `res/<Module>_Light.svg`, `res/<Module>_Dark.svg`) with context menu switching and JSON persistence.
- [ ] **No Virtual Screws:** No `ScrewSilver` or `ScrewBlack` widgets added to `ModuleWidget`.
- [ ] **Version Tracking:** Initial module version starts strictly at `v1.0.0` (independent of the overall plugin suite version `v2.X.X`), with per-module version recorded and autoincremented.
- [ ] **Panel Edit Method:** Vector modifications target `scratch/generate_<module>_panel.py`, NEVER direct edits to `res/<Module>.svg`.
- [ ] **Font Handling:** No `<text>` tags used. D-DIN/Quicksand typography converted to cubic paths (`C`) via `fontTools` + `pathops` per Section 7.5 hierarchy.
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

## 4. Mandatory Pre-Commit Compliance Verification & Plugin Deployment (`make install`)

### 4.1. Automated Compliance Verification
Before committing code, vector panels, or metadata changes, you MUST run the automated compliance linter:
```powershell
python scratch/verify_compliance.py
```
This script validates that:
- No virtual screws (`ScrewSilver` or `ScrewBlack`) exist.
- No redundant `"input"` / `"output"` (or `"IN"` / `"OUT"`) words exist in `configInput()` or `configOutput()`.
- Attenuverter tooltips follow strict `<Parameter> CV depth` sentence case.
- SVGs contain zero `<text>` elements.
- All modules define a valid functional taxonomy class in `plugin.json`.

If any `[ERROR]` is flagged, it must be resolved prior to committing or installing.

### 4.2. Plugin Directory Deployment (`make install`)
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

This section defines the mandatory, unified panel grid, spatial architecture, component clearance tolerances, custom hardware widget standards, and typography hierarchy for all **Garmire** hardware-compatible modules.

---

### 7.1. Horizontal Column Grid & Alignment Rules

- **Panel Height**: Standard Eurorack 3U ($128.50\text{ mm}$ / $240\text{ px}$ MetaModule bitmap). Background fill: `#6e6e6e`.
- **Panel Width**: $W = \text{HP} \times 5.08\text{ mm}$.
- **Horizontal Outer Margins**:
  - For modules with 2 or more columns, the first (leftmost) and last (rightmost) component columns are placed **strictly at $7.00\text{ mm}$ from either panel edge**:
    - Left column: $x_0 = 7.00\text{ mm}$
    - Right column: $x_{N-1} = W - 7.00\text{ mm}$
- **Column Pitch & Offset**:
  - Preferred horizontal grid pitch / column spacing: **$13.32\text{ mm}$**.
- **Odd Number of Columns Rule**:
  - When a control row has an odd number of columns (e.g. 3 or 5), the center component column **MUST always sit exactly at the $50\%$ midline ($X = W / 2.0$)**.

---

### 7.2. Vertical Coordinates & Spacing Architecture

All Garmire panels adhere strictly to universal vertical coordinates, anchors, and bottom-up stacking rules:

```
+-------------------------------------------------------------------+ Y = 0.00 mm
| ZONE 1: HEADER & TITLE                                            |
| - Module Title: Fixed Y = 8.000 mm (D-DIN-Bold, centered)          |
| - Version Tag:  Fixed Y = 8.000 mm (Quicksand-Regular, X = W-2.15)|
+-------------------------------------------------------------------+
| ZONE 2: PRIMARY KNOBS (Top-Down Spacing)                           |
| - Knob Row 1 Center: Fixed Y = 18.00 mm (Label Y = 12.25 mm)       |
| - Knob Row 2 Center: Fixed Y = 34.00 mm (Label Y = 28.25 mm)       |
|   (Universal Knob Pitch: ΔY = 16.00 mm, Label Offset: -5.75 mm)   |
+================ Delineator Line 1 ================================+
| ZONE 3: CV ATTENUVERTERS (Trimpots)                               |
| - Trimpot Pitch: ΔY = 11.00 mm                                    |
| - Trimpot Label Offset: -4.00 mm above trimpot center             |
+================ Delineator Line 2 ================================+
| ZONE 4: I/O PORTS & JACKS (Bottom-Up from Y = 117.00 mm)          |
| - Jack Pitch: ΔY = 13.00 mm universal                             |
| - Jack Label Offset: -5.00 mm above jack center                   |
| - Bottom Output Jack Row Center: Fixed Y = 117.00 mm               |
+-------------------------------------------------------------------+ Y = 128.50 mm
```

#### **1. Fixed Vertical Coordinates & Pitches**
- **Bottom Output Jack Anchor**: Fixed at **$Y = 117.00\text{ mm}$**.
- **Universal Jack Pitch ($\Delta Y_{\text{jack}}$)**: **$13.00\text{ mm}$** between center positions. Jack rows stack bottom-up:
  - Row $N$ (Lowest / Outputs): $Y = 117.00\text{ mm}$ (Label baseline $Y = 112.00\text{ mm}$)
  - Row $N-1$: $Y = 104.00\text{ mm}$ (Label baseline $Y = 99.00\text{ mm}$)
  - Row $N-2$: $Y = 91.00\text{ mm}$ (Label baseline $Y = 86.00\text{ mm}$)
- **Jack Label Baseline**: Strictly **$-5.00\text{ mm}$ above jack center**.
- **Universal Attenuverter Trimpot Pitch ($\Delta Y_{\text{trim}}$)**: **$11.00\text{ mm}$** between center positions.
- **Trimpot Label Baseline**: Strictly **$-4.00\text{ mm}$ above trimpot center**.
- **Knob Row 1 Center**: Fixed at **$Y = 18.00\text{ mm}$**.
- **Universal Knob Vertical Pitch ($\Delta Y_{\text{knob}}$)**: **$16.00\text{ mm}$** universal (Row 1 at $18.00\text{ mm}$, Row 2 at $34.00\text{ mm}$, Row 3 at $50.00\text{ mm}$).
- **Knob Label Baseline**: Strictly **$-5.75\text{ mm}$ above knob center** (Row 1 label baseline at $Y = 12.250\text{ mm}$).

---

### 7.3. Delineator Lines Specification

Section delineator lines visually separate Zones 2, 3, and 4:
- **Vertical Positioning**:
  - Delineator lines are **always located exactly $10.00\text{ mm}$ above the center position** of the control/jack row immediately below them:
    - **Delineator Line 2** (separating Attenuverters and Jacks): Center $Y = Y_{\text{jack\_top\_center}} - 10.00\text{ mm}$.
    - **Delineator Line 1** (separating Knobs and Attenuverters): Center $Y = Y_{\text{trim\_top\_center}} - 10.00\text{ mm}$.
- **Horizontal Alignment & Span**:
  - Right-aligned to **$80\%$ of panel width** ($x_{\text{start}} = 0.20 \times W$, $x_{\text{end}} = W$).
- **Line Height & Structure**:
  - Total height: **$0.300\text{ mm}$**.
  - Composed of two stacked lines:
    - Top highlight line: $0.150\text{ mm}$ height, fill/stroke `#999999`.
    - Bottom shadow line: $0.150\text{ mm}$ height, fill/stroke `#5e5e5e`.

---

### 7.4. Custom Hardware Widget SVG Standards

All modules must instantiate Garmire's custom hardware SVG components instead of default VCV Rack widgets:

| Hardware Component | C++ Widget Type | Top SVG Asset (`res/`) | Background SVG Asset (`res/`) |
| :--- | :--- | :--- | :--- |
| **Main Knob** | `RoundKnob` | `res/knob.svg` | `res/knob_bg.svg` |
| **Attenuverter Trimpot** | `RoundKnob` | `res/tinyknob.svg` | `res/tinyknob_bg.svg` |
| **3.5mm Eurorack Jack** | `app::SvgPort` | `res/3_5mm_jacks.svg` | N/A |
| **Tactile Momentary Button**| `app::SvgSwitch` | Frame 0: `res/button_gray_0.svg`<br>Frame 1: `res/button_gray_1.svg` | N/A |

*Widget Implementation Example*:
```cpp
struct GarmirePort : app::SvgPort {
    GarmirePort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};
struct GarmireKnob : RoundKnob {
    GarmireKnob() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
    }
};
struct GarmireTrimpot : RoundKnob {
    GarmireTrimpot() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
    }
};
struct GarmireButton : app::SvgSwitch {
    GarmireButton() {
        momentary = true;
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
    }
};
```

---

### 7.5. Typography Hierarchy & Sizing

| Feature / Label Role | Font File | Size (pt) | Scale Factor (`scale`) | Case | Vertical Anchor / Baseline Offset | Color (Medium Skin) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Module Title** | `res/D-DIN-Bold.otf` | **16 pt** | `0.005644` | UPPERCASE | Fixed $Y = 8.000\text{ mm}$ (horizontally centered) | `#ffffff` |
| **Version Tag** | `res/Quicksand-Regular.ttf` | **4.5 pt** | `0.001600` | lowercase | Fixed $Y = 8.000\text{ mm}$ (right margin $X = W - 2.15\text{ mm}$) | `#aaaaaa` |
| **Main Knobs (Zone 1 & 2)** | `res/D-DINCondensed-Bold.otf` | **10 pt** | `0.003528` | UPPERCASE | Offset **$-5.75\text{ mm}$** above knob center | `#222222` |
| **CV Trimpots (Zone 3)** | `res/D-DINCondensed.otf` | **8 pt** | `0.002822` | UPPERCASE | Offset **$-4.00\text{ mm}$** above trimpot center | `#222222` |
| **I/O Jacks (Zone 4)** | `res/D-DINCondensed.otf` | **8 pt** | `0.002822` | UPPERCASE | Offset **$-5.00\text{ mm}$** above jack center | `#222222` |

#### **Scale Factor Formula**:
$$\text{scale} = \frac{\text{size}_{\text{pt}} \times 25.4\text{ mm/in}}{72.0\text{ pt/in} \times \text{UPM}}$$
For $\text{UPM} = 1000$ (OTF standard):
- **16 pt**: $\frac{16.0 \times 25.4}{72.0 \times 1000} \approx \mathbf{0.005644}$
- **10 pt**: $\frac{10.0 \times 25.4}{72.0 \times 1000} \approx \mathbf{0.003528}$
- **8 pt**: $\frac{8.0 \times 25.4}{72.0 \times 1000} \approx \mathbf{0.002822}$
- **4.5 pt**: $\frac{4.535 \times 25.4}{72.0 \times 1000} \approx \mathbf{0.001600}$

#### **Mandatory Path Vector Pipeline Rules**:
1. **Zero `<text>` Elements**: Raw `<text>` tags are strictly forbidden in SVG faceplates. All typography must be converted to cubic Beziers (`C`) via `fontTools` and simplified with Skia `pathops.simplify()`.
2. **NanoSVG Number Formatting**: Coordinates must be formatted with `clean_svg_path_numbers()` (1 decimal place rounding `.1f` and space padding around command letters `M C L Z`).

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

---

### 7.7. Triple Skin Theme System (Medium, Light, Dark)

Every Garmire module faceplate supports three distinct skin themes: **Medium** (default Eurorack studio gray), **Light** (clean studio aluminum), and **Dark** (matte black stealth).

#### **1. Skin Palette Specifications**

| Theme | Panel Filename (`res/`) | Background Fill | Title Color | Label & Line Color | Delineator Top Highlight | Delineator Bottom Shadow | Button Light Ring Trim |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Medium (Default)** | `<Module>.svg` | `#6e6e6e` | `#ffffff` | `#222222` | `#999999` | `#5e5e5e` | `#484848` |
| **Light** | `<Module>_Light.svg` | `#c6c6c6` | `#333333` | `#222222` | `#e4e4e4` | `#a8a8a8` | `#9e9e9e` |
| **Dark** | `<Module>_Dark.svg` | `#333333` | `#c6c6c6` | `#ffffff` | `#585858` | `#202020` | `#222222` |

#### **2. Panel Generator Multi-Skin Output**
In `scratch/generate_<module>_panel.py`, the generator must iterate through the three skins, rendering all three SVG faceplates and their verification bitmaps:
```python
skins = [
    {
        'name': 'Medium',
        'filename': f'res/{module_name}.svg',
        'bg_color': '#6e6e6e',
        'title_color': '#ffffff',
        'label_color': '#222222',
        'line_color': '#222222',
        'hi_color': '#999999',
        'sh_color': '#5e5e5e',
        'ring_color': '#484848'
    },
    {
        'name': 'Light',
        'filename': f'res/{module_name}_Light.svg',
        'bg_color': '#c6c6c6',
        'title_color': '#333333',
        'label_color': '#222222',
        'line_color': '#222222',
        'hi_color': '#e4e4e4',
        'sh_color': '#a8a8a8',
        'ring_color': '#9e9e9e'
    },
    {
        'name': 'Dark',
        'filename': f'res/{module_name}_Dark.svg',
        'bg_color': '#333333',
        'title_color': '#c6c6c6',
        'label_color': '#ffffff',
        'line_color': '#ffffff',
        'hi_color': '#585858',
        'sh_color': '#202020',
        'ring_color': '#222222'
    }
]
```

#### **3. C++ Module & Widget Theme Switching Protocol**
Every module C++ file implements persistent theme state and dynamic SVG swapping:

```cpp
// 1. In Module struct:
enum SkinTheme {
    THEME_MEDIUM = 0,
    THEME_LIGHT = 1,
    THEME_DARK = 2
};
SkinTheme skinTheme = THEME_MEDIUM;

json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
    return rootJ;
}

void dataFromJson(json_t* rootJ) override {
    json_t* stJ = json_object_get(rootJ, "skinTheme");
    if (stJ) {
        skinTheme = (SkinTheme)json_integer_value(stJ);
    }
}

// 2. In ModuleWidget struct:
SvgPanel* panel = nullptr;
SkinTheme currentTheme = THEME_MEDIUM;

void setTheme(SkinTheme theme) {
    currentTheme = theme;
    if (panel) {
        std::string path;
        switch (theme) {
            case THEME_LIGHT:
                path = asset::plugin(pluginInstance, "res/<Module>_Light.svg");
                break;
            case THEME_DARK:
                path = asset::plugin(pluginInstance, "res/<Module>_Dark.svg");
                break;
            case THEME_MEDIUM:
            default:
                path = asset::plugin(pluginInstance, "res/<Module>.svg");
                break;
        }
        panel->setBackground(Svg::load(path));
    }
}

void step() override {
    ModuleWidget::step();
    auto* mod = dynamic_cast<MyModule*>(module);
    if (mod && mod->skinTheme != currentTheme) {
        setTheme(mod->skinTheme);
    }
}

// 3. In appendContextMenu:
void appendContextMenu(Menu* menu) override {
    auto* mod = dynamic_cast<MyModule*>(module);
    if (!mod) return;

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Skin"));

    const char* skinLabels[] = {"Medium (Default)", "Light", "Dark"};
    for (int i = 0; i < 3; i++) {
        SkinTheme theme = (SkinTheme)i;
        menu->addChild(createCheckMenuItem(skinLabels[i], "",
            [=]() { return mod->skinTheme == theme; },
            [=]() {
                mod->skinTheme = theme;
                setTheme(theme);
            }
        ));
    }
}
```


