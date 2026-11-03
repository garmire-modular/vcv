# Stage 1 UI Audit Handoff: Fonts & Triple Skin Integration

> **Target Scope:** All 72 modules in `plugin.json`  
> **Primary Objectives:**  
> 1. Verify typography hierarchy & vectorization (zero raw `<text>` elements).  
> 2. Ensure all three skins (**Medium**, **Light**, **Dark**) are generated in `res/` and wired into C++ with context menu switching and JSON persistence.

---

## 1. Executive Status & Baseline Inventory

A pre-audit inventory scan (`python scratch/audit_ui_stage1.py`) across all 72 modules defined in `plugin.json` shows:

| Audit Criterion | Current Count | Target | Status |
| :--- | :---: | :---: | :--- |
| **Panel Generator Scripts (`scratch/`)** | **72 / 72** | 72 | 100% complete — every module has a dedicated generator |
| **Medium Faceplates (`res/<Module>.svg`)** | **72 / 72** | 72 | 100% complete |
| **Zero `<text>` Vectorization** | **72 / 72** | 72 | 100% clean across all current SVG faceplates |
| **Light Faceplates (`res/<Module>_Light.svg`)**| **11 / 72** | 72 | **61 modules missing** |
| **Dark Faceplates (`res/<Module>_Dark.svg`)** | **11 / 72** | 72 | **61 modules missing** |
| **C++ Theme Switching Active** | **10 / 72** | 72 | **62 modules missing** |
| **Fully Compliant Modules** | **10 / 72** | 72 | `Polly`, `Panopia`, `Panopticon`, `Beatrice`, `Octavia`, `Randi`, `Grainne`, `Lain`, `Salus`, `Serqet` *(Note: `Reverie` has all 3 SVGs generated, but lacks C++ wiring)* |

---

## 2. Mandatory Stage 1 Invariants

### 2.1. Typography Directives
- **Zero `<text>` Tags**: Every faceplate SVG must render text strictly as cubic Bezier paths (`<path d="..."/>`). No raw `<text>` or CSS `<style>` tags.
- **Font Hierarchy**:
  - **Module Title**: `res/D-DIN-Bold.otf` at 16 pt (`scale = 0.005644`), UPPERCASE, centered at $Y = 8.000\text{ mm}$.
  - **Version Tag**: Any clean sans-serif (e.g. Quicksand, D-DIN, Segoe UI) at 4.5 pt (`scale = 0.001600`), lowercase, fixed $Y = 8.000\text{ mm}$, right margin $X = W - 2.15\text{ mm}$.
  - **Main Knobs (Zone 1 & 2)**: `res/D-DINCondensed-Bold.otf` at 10 pt (`scale = 0.003528`), UPPERCASE, $-5.75\text{ mm}$ above knob center.
  - **CV Trimpots (Zone 3)**: `res/D-DINCondensed.otf` at 8 pt (`scale = 0.002822`), UPPERCASE, $-4.00\text{ mm}$ above trimpot center.
  - **I/O Jacks (Zone 4)**: `res/D-DINCondensed.otf` at 8 pt (`scale = 0.002822`), UPPERCASE, $-5.00\text{ mm}$ above jack center.
- **Panel Labeling**:
  - No duplicate identical labels across adjacent columns (use a single centered label across the span).
  - Trimpots in Zone 3 are inherently CV depth controls; label with the parameter name alone (no `"CV"` suffix).

### 2.2. Triple Skin Theme Specifications
Every module faceplate must be generated in three variants:

| Theme | SVG Filename (`res/`) | Background Fill | Title Color | Label & Line Color | Delineator Top Highlight | Delineator Bottom Shadow | Button Light Ring Trim |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Medium (Default)** | `<Module>.svg` | `#6e6e6e` | `#ffffff` | `#222222` | `#999999` | `#5e5e5e` | `#484848` |
| **Light** | `<Module>_Light.svg` | `#c6c6c6` | `#333333` | `#222222` | `#e4e4e4` | `#a8a8a8` | `#9e9e9e` |
| **Dark** | `<Module>_Dark.svg` | `#333333` | `#c6c6c6` | `#ffffff` | `#585858` | `#202020` | `#222222` |

---

## 3. Step-by-Step Module Audit & Remediation Workflow

When auditing and updating each module, execute the following 5-step cycle:

### Step 1: Update the Panel Generator Script
Open `scratch/generate_<module>_panel.py`. Ensure it defines the multi-skin loop:
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

for skin in skins:
    # render SVG with skin['bg_color'], skin['title_color'], etc.
    with open(skin['filename'], 'w', encoding='utf-8') as f:
        f.write(svg_content)
```
Run the generator:
```powershell
python scratch/generate_<module>_panel.py
```

### Step 2: Implement C++ Theme Switching in `src/<Module>.cpp`
In the `Module` struct:
```cpp
enum SkinTheme {
    THEME_MEDIUM = 0,
    THEME_LIGHT = 1,
    THEME_DARK = 2
};
SkinTheme skinTheme = THEME_MEDIUM;

json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
    // ... other module state ...
    return rootJ;
}

void dataFromJson(json_t* rootJ) override {
    json_t* stJ = json_object_get(rootJ, "skinTheme");
    if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);
    // ... other module state ...
}
```

In the `ModuleWidget` struct:
```cpp
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

### Step 3: Verify Single-Module Compliance
Run:
```powershell
python scratch/verify_compliance.py src/<Module>.cpp res/<Module>.svg
```

### Step 4: Run the Stage 1 Audit Script
Verify that the module has flipped to clean status:
```powershell
python scratch/audit_ui_stage1.py
```

### Step 5: Version Bump, Compile & Deploy
- Bump individual module version in `scratch/generate_<module>_panel.py` and `metamodule/assets/<Module>.png` (e.g. `v1.0.0` $\rightarrow$ `v1.0.1`).
- Check compiler concurrency:
  ```powershell
  Get-Process make, g++, gcc, ninja -ErrorAction SilentlyContinue
  ```
- Deploy to VCV Rack:
  ```powershell
  make install
  ```

---

## 4. Key Reference Files & Tools

- **Audit Linter**: `python scratch/audit_ui_stage1.py` (prints full status table and completion count).
- **Compliance Script**: `python scratch/verify_compliance.py` (checks virtual screws, tooltips, SVGs).
- **Typography Standards**: [docs/TYPOGRAPHY.md](file:///g:/dupe/work/garmire/docs/TYPOGRAPHY.md)
- **Panel Design System**: [docs/module_design.md](file:///g:/dupe/work/garmire/docs/module_design.md)
- **Palette Builder Sync**: `python scratch/palette/build_standalone.py` (run whenever panel graphics change).
- **Canonical Compliant Modules for Reference**:
  - `src/Polly.cpp` & `scratch/generate_polly_panel.py`
  - `src/Beatrice.cpp` & `scratch/generate_beatrice_panel.py`
  - `src/Grainne.cpp` & `scratch/generate_grainne_panel.py`
  - `src/Salus.cpp` & `scratch/generate_salus_panel.py`
