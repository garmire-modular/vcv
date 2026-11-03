# Garmire Module Panel Design & Hardware UI System

This guide contains the complete visual and physical interface specifications for Garmire modules. 
**When tasked with creating, sizing, or modifying a module faceplate, panel generator script, custom hardware widgets, or theme skins, follow this document.**

---

## 1. Module Sizing, Component Spacing & Crowding Calculation

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

## 2. Horizontal Column Grid & Alignment Rules

- **Panel Height**: Standard Eurorack 3U ($128.50\text{ mm}$ / $240\text{ px}$ MetaModule bitmap). Background fill: `#6e6e6e` (Medium).
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

## 3. Vertical Coordinates & Spacing Architecture

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

### Fixed Vertical Coordinates & Pitches
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

## 4. Delineator Lines Specification

Section delineator lines visually separate Zones 2, 3, and 4:
- **Vertical Positioning**:
  - Located exactly **$10.00\text{ mm}$ above the center position** of the control/jack row immediately below them:
    - **Delineator Line 2** (separating Attenuverters and Jacks): Center $Y = Y_{\text{jack\_top\_center}} - 10.00\text{ mm}$.
    - **Delineator Line 1** (separating Knobs and Attenuverters): Center $Y = Y_{\text{trim\_top\_center}} - 10.00\text{ mm}$.
- **Horizontal Alignment & Span**:
  - Right-aligned to **$80\%$ of panel width** ($x_{\text{start}} = 0.20 \times W$, $x_{\text{end}} = W$).
- **Line Height & Structure**:
  - Total height: **$0.300\text{ mm}$**.
  - Composed of two stacked lines:
    - Top highlight line: $0.150\text{ mm}$ height, fill/stroke `#999999` (Medium skin).
    - Bottom shadow line: $0.150\text{ mm}$ height, fill/stroke `#5e5e5e` (Medium skin).

---

## 5. Custom Hardware Widget SVG Standards

All modules must instantiate Garmire's custom hardware SVG components instead of default VCV Rack widgets:

| Hardware Component | C++ Widget Type | Top SVG Asset (`res/`) | Background SVG Asset (`res/`) |
| :--- | :--- | :--- | :--- |
| **Main Knob** | `RoundKnob` | `res/knob.svg` | `res/knob_bg.svg` |
| **Attenuverter Trimpot** | `RoundKnob` | `res/tinyknob.svg` | `res/tinyknob_bg.svg` |
| **3.5mm Eurorack Jack** | `app::SvgPort` | `res/3_5mm_jacks.svg` | N/A |
| **Tactile Momentary Button**| `app::SvgSwitch` | Frame 0: `res/button_gray_0.svg`<br>Frame 1: `res/button_gray_1.svg` | N/A |

### C++ Widget Implementation Example:
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

## 6. Illustrator SVG Normalization (`illustrator_svg_export_fix.py`)

All user-provided SVG files (custom buttons, knobs, sliders, icons) exported from Adobe Illustrator MUST be normalized before placement in `res/`:
```powershell
python scratch/illustrator_svg_export_fix.py <path_to_svg> --in-place
```
- **Why**: Illustrator default exports embed CSS `<style>` elements and `.cls-X` classes. NanoSVG cannot parse CSS and falls back to rendering unstyled shapes as solid black boxes.
- **Action**: Inlines presentation attributes (`fill`, `stroke`), removes `<style>` tags, moves `<clipPath>` inside `<defs>`, and formats coordinates for NanoSVG.

---

## 7. Typography Hierarchy & Vector Conversion

See also [TYPOGRAPHY.md](file:///g:/dupe/work/garmire/docs/TYPOGRAPHY.md) for full font specifications.

| Feature / Label Role | Font File | Size (pt) | Scale Factor (`scale`) | Case | Vertical Anchor / Baseline Offset | Color (Medium Skin) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Module Title** | `res/D-DIN-Bold.otf` | **16 pt** | `0.005644` | UPPERCASE | Fixed $Y = 8.000\text{ mm}$ (horizontally centered) | `#ffffff` |
| **Version Tag** | `res/Quicksand-Regular.ttf` | **4.5 pt** | `0.001600` | lowercase | Fixed $Y = 8.000\text{ mm}$ (right margin $X = W - 2.15\text{ mm}$) | `#aaaaaa` |
| **Main Knobs (Zone 1 & 2)** | `res/D-DINCondensed-Bold.otf` | **10 pt** | `0.003528` | UPPERCASE | Offset **$-5.75\text{ mm}$** above knob center | `#222222` |
| **CV Trimpots (Zone 3)** | `res/D-DINCondensed.otf` | **8 pt** | `0.002822` | UPPERCASE | Offset **$-4.00\text{ mm}$** above trimpot center | `#222222` |
| **I/O Jacks (Zone 4)** | `res/D-DINCondensed.otf` | **8 pt** | `0.002822` | UPPERCASE | Offset **$-5.00\text{ mm}$** above jack center | `#222222` |

### Path Vector Pipeline Rules:
1. **Zero `<text>` Elements**: Raw `<text>` tags are strictly forbidden in SVG faceplates. All typography must be converted to cubic Beziers (`C`) via `fontTools` and simplified with Skia `pathops.simplify()`.
2. **NanoSVG Number Formatting**: Coordinates must be formatted with `clean_svg_path_numbers()` (1 decimal place rounding `.1f` and space padding around command letters `M C L Z`).
3. **No Duplicate Identical Labels**: Never repeat identical labels across columns (e.g. no `"X X X"`). Place a single centered label across the span.
4. **No Redundant "CV" in Trimpot Zone**: Trimpots in Zone 3 are inherently CV depth controls; label with the parameter name alone.

---

## 8. Module Palette Badge Protocol & Database Synchronization

All Garmire hardware modules include a vertical color badge oriented along the far left edge of the faceplate:
1. **Badge Dimensions & Geometry**:
   - Flush left at $X = 0.00\text{ mm}$ ($x = 0$).
   - Width: Eurorack $2.54\text{ mm}$ ($0.10\text{ in}$ / $0.5\text{ HP}$).
   - Height: $88.90\text{ mm}$ ($3.50\text{ in}$).
   - Vertically centered at $Y = 64.25\text{ mm}$ ($y = 19.80\text{ mm}$ to $y = 108.70\text{ mm}$).
2. **Placeholder Badge (`#5d5d5d`)**:
   - New modules start with `#5d5d5d` neutral gray:
     ```xml
     <g id="palette-badge">
       <rect x="0" y="19.800" width="2.540" height="88.900" fill="#5d5d5d" stroke="none" />
     </g>
     ```
3. **Palette Builder Database Synchronization**:
   - After generating or updating a module's SVG, run:
     ```powershell
     python scratch/palette/build_standalone.py
     ```
   - This registers the module in `scratch/palette/index.html` for interactive color palette assignment.

---

## 9. Triple Skin Theme System (Medium, Light, Dark)

Every Garmire module faceplate supports three skins: **Medium** (default gray), **Light** (aluminum), and **Dark** (matte black).

### Skin Palette Specifications

| Theme | Panel Filename (`res/`) | Background Fill | Title Color | Label & Line Color | Delineator Top Highlight | Delineator Bottom Shadow | Button Light Ring Trim |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Medium (Default)** | `<Module>.svg` | `#6e6e6e` | `#ffffff` | `#222222` | `#999999` | `#5e5e5e` | `#484848` |
| **Light** | `<Module>_Light.svg` | `#c6c6c6` | `#333333` | `#222222` | `#e4e4e4` | `#a8a8a8` | `#9e9e9e` |
| **Dark** | `<Module>_Dark.svg` | `#333333` | `#c6c6c6` | `#ffffff` | `#585858` | `#202020` | `#222222` |

### Panel Generator Multi-Skin Output Pattern
In `scratch/generate_<module>_panel.py`:
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

### C++ Module & Widget Theme Switching Protocol
```cpp
// In Module struct:
enum SkinTheme { THEME_MEDIUM = 0, THEME_LIGHT = 1, THEME_DARK = 2 };
SkinTheme skinTheme = THEME_MEDIUM;

json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
    return rootJ;
}

void dataFromJson(json_t* rootJ) override {
    json_t* stJ = json_object_get(rootJ, "skinTheme");
    if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);
}

// In ModuleWidget struct:
SvgPanel* panel = nullptr;
SkinTheme currentTheme = THEME_MEDIUM;

void setTheme(SkinTheme theme) {
    currentTheme = theme;
    if (panel) {
        std::string path;
        switch (theme) {
            case THEME_LIGHT:  path = asset::plugin(pluginInstance, "res/<Module>_Light.svg"); break;
            case THEME_DARK:   path = asset::plugin(pluginInstance, "res/<Module>_Dark.svg");  break;
            case THEME_MEDIUM:
            default:           path = asset::plugin(pluginInstance, "res/<Module>.svg");        break;
        }
        panel->setBackground(Svg::load(path));
    }
}

void step() override {
    ModuleWidget::step();
    auto* mod = dynamic_cast<MyModule*>(module);
    if (mod && mod->skinTheme != currentTheme) setTheme(mod->skinTheme);
}

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
