import os
import json
import base64
import xml.etree.ElementTree as ET

REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PALETTE_DIR = os.path.join(REPO_DIR, "scratch", "palette")
PLUGIN_JSON = os.path.join(REPO_DIR, "plugin.json")
RES_DIR = os.path.join(REPO_DIR, "res")

def get_b64(path):
    with open(path, 'rb') as f:
        return base64.b64encode(f.read()).decode('ascii')

node_b64 = get_b64(os.path.join(RES_DIR, 'Node.otf'))
qs_b64 = get_b64(os.path.join(RES_DIR, 'Quicksand-Medium.ttf'))

# Read plugin.json and all 33 SVGs
with open(PLUGIN_JSON, 'r', encoding='utf-8') as f:
    pdata = json.load(f)

modules = []
for m in pdata.get('modules', []):
    slug = m.get('slug')
    name = m.get('name', slug)
    svg_file = os.path.join(RES_DIR, f"{slug}.svg")
    hp = 6
    svg_content = ""
    if os.path.exists(svg_file):
        with open(svg_file, 'r', encoding='utf-8') as sf:
            svg_content = sf.read()
        try:
            tree = ET.fromstring(svg_content)
            w_str = tree.get("width", "")
            if "mm" in w_str:
                hp = round(float(w_str.replace("mm", "").strip()) / 5.08)
            else:
                vb = tree.get("viewBox", "").split()
                if len(vb) == 4:
                    hp = round(float(vb[2]) / 5.08)
        except Exception as e:
            print(f"Error parsing {slug}: {e}")
    
    modules.append({
        "slug": slug,
        "name": name,
        "hp": hp,
        "svg": svg_content
    })

modules_json_str = json.dumps(modules)

html_content = f'''<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Module Palette Manager</title>
  <style>
    @font-face {{
      font-family: 'Node';
      src: url('data:font/opentype;base64,{node_b64}') format('opentype');
    }}
    @font-face {{
      font-family: 'Quicksand';
      src: url('data:font/truetype;base64,{qs_b64}') format('truetype');
      font-weight: 500;
    }}

    *, *::before, *::after {{
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }}

    body {{
      background: #111215;
      color: #e5e5e5;
      font-family: 'Quicksand', -apple-system, sans-serif;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: flex-start;
      padding: 20px 24px 40px;
      -webkit-font-smoothing: antialiased;
    }}

    .app {{
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 16px;
      width: 100%;
      max-width: 1320px;
    }}

    /* Top Command Header */
    .top-bar {{
      display: flex;
      align-items: center;
      justify-content: space-between;
      width: 100%;
      gap: 16px;
      flex-wrap: wrap;
    }}

    /* Module Selector Bar */
    .module-select-bar {{
      display: flex;
      align-items: center;
      gap: 8px;
      background: #18191e;
      padding: 6px 12px;
      border-radius: 8px;
      border: 1px solid #262832;
    }}

    .nav-btn {{
      background: transparent;
      border: none;
      color: #7b808e;
      font-size: 15px;
      cursor: pointer;
      padding: 4px 6px;
      border-radius: 4px;
      transition: color 0.12s;
      display: flex;
      align-items: center;
    }}
    .nav-btn:hover {{ color: #ffffff; background: #22242c; }}

    select.module-dropdown {{
      background: #121316;
      color: #ffffff;
      font-family: 'Quicksand', monospace;
      font-size: 13px;
      font-weight: 600;
      padding: 6px 10px;
      border-radius: 6px;
      border: 1px solid #2e313c;
      outline: none;
      cursor: pointer;
      min-width: 250px;
    }}
    select.module-dropdown:focus {{
      border-color: #56B4E9;
    }}

    .mod-meta-badge {{
      font-size: 12px;
      font-weight: 600;
      color: #8c92a2;
      background: #20222a;
      padding: 4px 10px;
      border-radius: 5px;
      border: 1px solid #2a2d37;
      letter-spacing: 0.03em;
    }}

    .mod-saved-indicator {{
      font-size: 11px;
      font-weight: 600;
      color: #009E73;
      display: none;
      align-items: center;
      gap: 4px;
    }}
    .mod-saved-indicator.active {{
      display: flex;
    }}

    /* Generator Controls */
    .controls {{
      display: flex;
      align-items: center;
      gap: 12px;
      background: #18191e;
      padding: 6px 10px;
      border-radius: 8px;
      border: 1px solid #262832;
    }}

    .group {{
      display: flex;
      align-items: center;
      gap: 3px;
    }}

    .btn {{
      background: transparent;
      border: 1px solid transparent;
      color: #7b808e;
      font-family: inherit;
      font-size: 13px;
      font-weight: 600;
      padding: 6px 13px;
      border-radius: 5px;
      cursor: pointer;
      transition: all 0.12s ease;
      letter-spacing: 0.02em;
    }}
    .btn:hover {{
      color: #ffffff;
      background: #22242c;
    }}
    .btn.active {{
      background: #2a2d37;
      color: #ffffff;
      border-color: #3b3f4d;
    }}

    .divider {{
      width: 1px;
      height: 18px;
      background: #282a32;
    }}

    .panel-color-group {{
      display: flex;
      align-items: center;
      gap: 3px;
    }}

    .panel-btn {{
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 5px 9px;
      font-size: 11px;
      font-family: 'consola', monospace;
    }}

    .color-dot {{
      display: inline-block;
      width: 9px;
      height: 9px;
      border-radius: 50%;
      border: 1px solid rgba(255, 255, 255, 0.25);
      flex-shrink: 0;
    }}


    .btn-save {{
      background: #0072B2;
      color: #ffffff;
      font-family: inherit;
      font-size: 13px;
      font-weight: 600;
      padding: 6px 16px;
      border-radius: 6px;
      border: 1px solid #1a82c4;
      cursor: pointer;
      transition: all 0.12s;
    }}
    .btn-save:hover {{
      background: #0085d0;
    }}
    .btn-save.disabled {{
      background: #252832 !important;
      color: #616777 !important;
      border-color: #353948 !important;
      cursor: not-allowed !important;
    }}

    .btn-disk {{
      background: #20232c;
      color: #9aa1b3;
      font-family: inherit;
      font-size: 12px;
      font-weight: 600;
      padding: 6px 11px;
      border-radius: 6px;
      border: 1px solid #2d3240;
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: 5px;
      transition: all 0.12s;
    }}
    .btn-disk:hover {{
      background: #2b303d;
      color: #ffffff;
    }}
    .btn-disk.linked {{
      border-color: #009E73;
      color: #009E73;
    }}

    /* Collision & Reservation Banner */
    .collision-banner {{
      display: none;
      align-items: center;
      justify-content: center;
      gap: 8px;
      padding: 6px 16px;
      border-radius: 6px;
      font-size: 12px;
      font-weight: 600;
      width: 100%;
      letter-spacing: 0.02em;
      transition: all 0.2s ease;
    }}
    .collision-banner.warn {{
      display: flex;
      background: #2a2012;
      color: #F0E442;
      border: 1px solid #5c4414;
    }}
    .collision-banner.blocked {{
      display: flex;
      background: #2c1414;
      color: #ff6b6b;
      border: 1px solid #731e1e;
    }}

    /* Main Workspace Stage */
    .stage {{
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 60px;
      width: 100%;
      min-height: 580px;
      padding: 6px 0 10px;
    }}

    /* SVG Viewport */
    .faceplate-wrapper {{
      position: relative;
      display: flex;
      align-items: center;
      justify-content: center;
      height: 560px;
      box-shadow: 0 24px 50px rgba(0, 0, 0, 0.7), 0 2px 6px rgba(0, 0, 0, 0.4);
      background: #7c7c7c;
      border-radius: 2px;
      overflow: hidden;
    }}

    .faceplate-wrapper svg {{
      height: 100%;
      width: auto;
      display: block;
    }}

    /* Swatches Column */
    .swatches-panel {{
      display: flex;
      flex-direction: column;
      gap: 12px;
      min-width: 180px;
    }}

    .swatch-row {{
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 6px 10px;
      border-radius: 6px;
      cursor: pointer;
      background: #15161c;
      border: 1px solid #242630;
      transition: all 0.12s ease;
      user-select: none;
    }}
    .swatch-row:hover {{
      background: #1d1f27;
      border-color: #383c4b;
    }}

    .swatch-block {{
      width: 26px;
      height: 26px;
      border-radius: 3px;
      flex-shrink: 0;
      /* ZERO border */
    }}

    .swatch-info {{
      display: flex;
      flex-direction: column;
    }}

    .swatch-hex {{
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      font-size: 13px;
      font-weight: 600;
      color: #e2e5ed;
      letter-spacing: 0.04em;
    }}

    .swatch-dim {{
      font-size: 10px;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      color: #727786;
      margin-top: 1px;
    }}

    .reroll-icon {{
      font-size: 14px;
      color: #555b6d;
      margin-left: auto;
      transition: color 0.12s, transform 0.2s;
    }}
    .swatch-row:hover .reroll-icon {{
      color: #56B4E9;
      transform: rotate(90deg);
    }}

    /* Notification Banner */
    .status-msg {{
      position: fixed;
      bottom: 24px;
      left: 50%;
      transform: translateX(-50%) translateY(20px);
      background: #1a1c22;
      border: 1px solid #333845;
      padding: 10px 18px;
      border-radius: 8px;
      font-size: 13px;
      font-weight: 500;
      color: #ffffff;
      box-shadow: 0 10px 30px rgba(0,0,0,0.6);
      opacity: 0;
      pointer-events: none;
      transition: all 0.2s ease;
      z-index: 100;
      display: flex;
      align-items: center;
      gap: 8px;
    }}
    .status-msg.active {{
      opacity: 1;
      transform: translateX(-50%) translateY(0);
    }}

    /* Custom Confirm & Alert Modal */
    .modal-overlay {{
      display: none;
      position: fixed;
      inset: 0;
      background: rgba(0, 0, 0, 0.75);
      z-index: 999;
      align-items: center;
      justify-content: center;
    }}
    .modal-overlay.open {{
      display: flex;
    }}
    .modal-card {{
      background: #181a20;
      border: 1px solid #323644;
      border-radius: 12px;
      padding: 24px;
      width: 90%;
      max-width: 440px;
      box-shadow: 0 20px 40px rgba(0, 0, 0, 0.8);
    }}
    .modal-title {{
      font-size: 16px;
      font-weight: 700;
      color: #ffffff;
      margin-bottom: 10px;
    }}
    .modal-body {{
      font-size: 13px;
      color: #a2a8b8;
      line-height: 1.5;
      margin-bottom: 20px;
      white-space: pre-line;
    }}
    .modal-actions {{
      display: flex;
      justify-content: flex-end;
      gap: 10px;
    }}
    .btn-modal-cancel {{
      background: #252832;
      border: 1px solid #353948;
      color: #c0c5d2;
      font-family: inherit;
      font-size: 13px;
      font-weight: 600;
      padding: 7px 14px;
      border-radius: 6px;
      cursor: pointer;
    }}
    .btn-modal-cancel:hover {{ background: #2f3340; color: #fff; }}
    .btn-modal-confirm {{
      background: #D55E00;
      border: 1px solid #ea6800;
      color: #ffffff;
      font-family: inherit;
      font-size: 13px;
      font-weight: 600;
      padding: 7px 16px;
      border-radius: 6px;
      cursor: pointer;
    }}
    .btn-modal-confirm:hover {{ background: #e86300; }}
  </style>
</head>
<body>

  <div class="app">

    <!-- Top Command Header -->
    <div class="top-bar">
      
      <!-- Module Selector -->
      <div class="module-select-bar">
        <button class="nav-btn" onclick="navModule(-1)" title="Previous Module (Left Arrow)">&#x25C0;</button>
        <select class="module-dropdown" id="module-select" onchange="onModuleChange(this.value)">
          <!-- Populated from plugin.json -->
        </select>
        <button class="nav-btn" onclick="navModule(1)" title="Next Module (Right Arrow)">&#x25B6;</button>

        <span class="mod-meta-badge" id="module-hp-badge">-- HP</span>
        <span class="mod-saved-indicator" id="module-saved-pill">
          &#x2713; Saved
        </span>
      </div>

      <!-- Generator & File Controls -->
      <div class="controls">
        <!-- Anchor Mode (Transform / Generate / Utility / Control) -->
        <div class="group">
          <button class="btn active" id="b-transform" onclick="setMode('Transform')" title="Transform (White Anchor, Key: T)">Transform</button>
          <button class="btn" id="b-generate" onclick="setMode('Generate')" title="Generate (Charcoal Anchor, Key: G)">Generate</button>
          <button class="btn" id="b-utility" onclick="setMode('Utility')" title="Utility (Warm Bone #E4DAC2, Key: U)">Utility</button>
          <button class="btn" id="b-control" onclick="setMode('Control')" title="Control (Cool Titanium #BCC5CC, Key: C)">Control</button>
        </div>

        <div class="divider"></div>

        <!-- Band Count (2, 3, 4) -->
        <div class="group">
          <button class="btn" id="b-2" onclick="setCount(2)">2</button>
          <button class="btn active" id="b-3" onclick="setCount(3)">3</button>
          <button class="btn" id="b-4" onclick="setCount(4)">4</button>
        </div>

        <div class="divider"></div>

        <button class="btn" onclick="generate()" title="Re-roll Palette (Spacebar)">&#x21bb;</button>


        <div class="divider"></div>

        <!-- Panel Color Selector (#7C7C7C, #6E6E6E, #333333) -->
        <div class="group panel-color-group">
          <button class="btn panel-btn active" id="b-panel-7c7c7c" onclick="setPanelColor('#7C7C7C')" title="Panel Fill: #7C7C7C (Default)">
            <span class="color-dot" style="background:#7C7C7C;"></span>#7C7C7C
          </button>
          <button class="btn panel-btn" id="b-panel-6e6e6e" onclick="setPanelColor('#6E6E6E')" title="Panel Fill: #6E6E6E">
            <span class="color-dot" style="background:#6E6E6E;"></span>#6E6E6E
          </button>
          <button class="btn panel-btn" id="b-panel-333333" onclick="setPanelColor('#333333')" title="Panel Fill: #333333">
            <span class="color-dot" style="background:#333333;"></span>#333333
          </button>
        </div>

        <div class="divider"></div>

        <button class="btn-save" id="btn-save-main" onclick="savePalette()" title="Save Palette (Key: S)">Save Palette</button>

        <div class="divider"></div>

        <button class="btn-disk" id="btn-file-sync" onclick="handleFileSync()" title="Link local palette.json for direct file writing">
          <span id="disk-icon">&#x21e9;</span>
          <span id="disk-label">Export JSON</span>
        </button>
      </div>

    </div>

    <!-- Live Collision / Reservation Banner -->
    <div class="collision-banner" id="collision-banner"></div>

    <!-- Main Workspace Stage -->
    <div class="stage">
      
      <!-- Real Vector SVG Faceplate Mount -->
      <div class="faceplate-wrapper" id="faceplate-container">
        <!-- Injected Module SVG -->
      </div>

      <!-- Clean Swatch Stack -->
      <div class="swatches-panel" id="swatches"></div>

    </div>

  </div>

  <!-- Notification Toast -->
  <div class="status-msg" id="status-toast">
    <span id="toast-icon">&#x2713;</span>
    <span id="toast-text">Saved to palette.json</span>
  </div>

  <!-- Custom Confirm & Alert Modal -->
  <div class="modal-overlay" id="confirm-modal">
    <div class="modal-card">
      <div class="modal-title" id="modal-title">Overwrite Saved Palette?</div>
      <div class="modal-body" id="modal-body">
        This module already has a saved palette in palette.json. Are you sure you want to overwrite it with this new combination?
      </div>
      <div class="modal-actions" id="modal-actions">
        <button class="btn-modal-cancel" id="btn-modal-cancel" onclick="closeConfirmModal(false)">Cancel</button>
        <button class="btn-modal-confirm" id="btn-modal-confirm" onclick="closeConfirmModal(true)">Overwrite</button>
      </div>
    </div>
  </div>

  <script>
    // Embedded 33 Modules Catalog (plugin.json + res/*.svg)
    const MODULES = {modules_json_str};

    const CHROMATIC_HIGH = [
      '#EBEC72', // Lemon Yellow
      '#56B4E9', // Sky Blue
      '#B8A0E8', // Pale Lavender
      '#FF8866'  // Coral Peach
    ];
    const CHROMATIC_LOW = [
      '#0072B2', // Cobalt Blue
      '#D55E00', // Vermilion
      '#CC79A7', // Reddish Purple
      '#882255', // Deep Wine
      '#442288', // Imperial Violet
      '#005566'  // Dark Petrol
    ];

    // Strictly unequal proportional rhythms
    const RATIOS = {{
      2: [
        [62, 38], [38, 62], [68, 32], [32, 68], [57, 43], [43, 57]
      ],
      3: [
        [46, 32, 22], [22, 46, 32], [32, 22, 46],
        [48, 20, 32], [32, 48, 20], [20, 32, 48],
        [50, 32, 18], [18, 50, 32], [32, 18, 50]
      ],
      4: [
        [36, 16, 28, 20], [20, 36, 16, 28], [28, 20, 36, 16],
        [16, 28, 20, 36], [38, 24, 14, 24], [24, 14, 38, 24],
        [40, 18, 26, 16], [16, 26, 18, 40]
      ]
    }};

    const BADGE_WIDTH_MM  = 2.54;  // Standard 2.54 mm (0.10 in / 0.5 HP)
    const BADGE_HEIGHT_MM = 88.90; // 3.50 inch
    const BADGE_Y_OFFSET  = 0.00;  // Center locked at 0

    // State
    const ANCHORS = {{
      'Transform': {{ hex: '#FFFFFF', tier: 'HIGH', label: 'white' }},
      'Generate':  {{ hex: '#1A1A1A', tier: 'LOW',  label: 'charcoal' }},
      'Utility':   {{ hex: '#E4DAC2', tier: 'HIGH', label: 'bone' }},
      'Control':   {{ hex: '#BCC5CC', tier: 'HIGH', label: 'titanium' }}
    }};

    let savedPalettes = {{}};
    let currentModule = null;
    let bandCount = 3;
    let moduleMode = 'Transform'; // 'Transform' | 'Generate' | 'Utility' | 'Control'
    let selectedPanelColor = '#7C7C7C'; // '#7C7C7C' | '#6E6E6E' | '#333333'
    let currentPalette = null; // {{ colors: [], weights: [] }}
    let pendingSaveCallback = null;
    let fileHandle = null; // Native File System Access API handle if linked

    function arraysEqual(a, b) {{
      if (!a || !b || a.length !== b.length) return false;
      for (let i = 0; i < a.length; i++) {{
        if (a[i] !== b[i]) return false;
      }}
      return true;
    }}

    // Check collision against palettes reserved by other modules
    function checkPaletteCollision(colors, weights, currentSlug) {{
      if (!savedPalettes) return {{ reserved: false, sameColors: false, moduleName: '' }};

      for (const [slug, saved] of Object.entries(savedPalettes)) {{
        if (slug === currentSlug) continue; // Skip comparing against itself

        if (arraysEqual(colors, saved.colors)) {{
          const sameWeights = arraysEqual(weights, saved.weights);
          if (sameWeights) {{
            // EXACT MATCH: Both colors and band heights are identical -> RESERVED / TAKEN!
            return {{
              reserved: true,
              sameColors: true,
              slug: slug,
              moduleName: saved.module || slug
            }};
          }} else {{
            // SAME COLORS IN ORDER, BUT DIFFERENT HEIGHTS
            return {{
              reserved: false,
              sameColors: true,
              slug: slug,
              moduleName: saved.module || slug
            }};
          }}
        }}
      }}
      return {{ reserved: false, sameColors: false, moduleName: '' }};
    }}

    function pickRandom(arr, exclude = []) {{
      const valid = arr.filter(x => !exclude.includes(x));
      return valid[Math.floor(Math.random() * valid.length)];
    }}

    function init() {{
      const cached = localStorage.getItem('palette_manager_json');
      if (cached) {{
        try {{
          savedPalettes = JSON.parse(cached);
        }} catch (e) {{}}
      }}

      if ('showSaveFilePicker' in window) {{
        document.getElementById('disk-label').textContent = 'Link File';
        document.getElementById('btn-file-sync').title = 'Link local palette.json so saves write straight to disk';
      }}

      populateDropdown();

      if (MODULES.length > 0) {{
        selectModule(MODULES[0].slug);
      }}
    }}

    function populateDropdown() {{
      const select = document.getElementById('module-select');
      const curSlug = currentModule ? currentModule.slug : (select.value || '');
      select.innerHTML = '';

      MODULES.forEach(m => {{
        const isSaved = !!savedPalettes[m.slug];
        const opt = document.createElement('option');
        opt.value = m.slug;
        const check = isSaved ? '✓ ' : '   ';
        opt.textContent = `${{check}}${{m.name}} (${{m.hp}} HP)`;
        select.appendChild(opt);
      }});

      if (curSlug) select.value = curSlug;
    }}

    function onModuleChange(slug) {{
      selectModule(slug);
    }}

    function navModule(delta) {{
      if (MODULES.length === 0) return;
      const curIdx = MODULES.findIndex(m => m.slug === currentModule.slug);
      let nextIdx = curIdx + delta;
      if (nextIdx < 0) nextIdx = MODULES.length - 1;
      if (nextIdx >= MODULES.length) nextIdx = 0;
      selectModule(MODULES[nextIdx].slug);
    }}

    function selectModule(slug) {{
      const mod = MODULES.find(m => m.slug === slug);
      if (!mod) return;
      currentModule = mod;

      document.getElementById('module-select').value = slug;
      document.getElementById('module-hp-badge').textContent = `${{mod.hp}} HP`;

      const saved = savedPalettes[slug];
      const savedPill = document.getElementById('module-saved-pill');

      if (saved) {{
        savedPill.classList.add('active');
        bandCount = saved.bandCount || saved.colors.length;
        if (saved.mode === 'Effect' || saved.mode === 'Transform') {{
          moduleMode = 'Transform';
        }} else if (saved.mode) {{
          moduleMode = saved.mode;
        }} else {{
          moduleMode = (saved.colors && saved.colors[0] === '#1A1A1A') ? 'Generate' : 'Transform';
        }}
        if (saved.panelColor) {{
          selectedPanelColor = saved.panelColor;
        }}
        updateControlUI();

        currentPalette = {{
          colors: [...saved.colors],
          weights: saved.weights || RATIOS[bandCount][0]
        }};
        renderFaceplateAndSwatches();
      }} else {{
        savedPill.classList.remove('active');
        updateControlUI();
        generate();
      }}
    }}

    function updateControlUI() {{
      [2, 3, 4].forEach(x => {{
        const btn = document.getElementById('b-' + x);
        if (btn) btn.classList.toggle('active', x === bandCount);
      }});
      ['transform', 'generate', 'utility', 'control'].forEach(m => {{
        const btn = document.getElementById('b-' + m);
        if (btn) btn.classList.toggle('active', moduleMode.toLowerCase() === m);
      }});
      ['7c7c7c', '6e6e6e', '333333'].forEach(c => {{
        const btn = document.getElementById('b-panel-' + c);
        if (btn) btn.classList.toggle('active', selectedPanelColor.toLowerCase() === ('#' + c));
      }});
    }}

    function setPanelColor(colorHex) {{
      selectedPanelColor = colorHex;
      updateControlUI();
      renderFaceplateAndSwatches();
    }}

    // Actively avoid rolling already reserved palettes
    function generate() {{
      const anchor = ANCHORS[moduleMode] || ANCHORS['Transform'];
      const anchorColor = anchor.hex;
      const anchorTier = anchor.tier;
      const curSlug = currentModule ? currentModule.slug : null;

      let bestColors = null;
      let bestRatio = null;

      // Try up to 100 random rolls to find a palette that is neither reserved nor shares colors with another module
      for (let attempt = 0; attempt < 100; attempt++) {{
        const colors = new Array(bandCount);
        const used = [anchorColor];
        colors[0] = anchorColor;
        let nextTier = (anchorTier === 'HIGH') ? 'LOW' : 'HIGH';

        for (let i = 1; i < bandCount; i++) {{
          const pool = (nextTier === 'HIGH') ? CHROMATIC_HIGH : CHROMATIC_LOW;
          const c = pickRandom(pool, used);
          colors[i] = c;
          used.push(c);
          nextTier = (nextTier === 'HIGH') ? 'LOW' : 'HIGH';
        }}

        const ratioPool = RATIOS[bandCount];
        const selectedRatio = ratioPool[Math.floor(Math.random() * ratioPool.length)];

        const collision = checkPaletteCollision(colors, selectedRatio, curSlug);

        // Ideal: completely unique (neither reserved nor sharing colors in that order)
        if (!collision.reserved && !collision.sameColors) {{
          bestColors = colors;
          bestRatio = selectedRatio;
          break;
        }}

        // Acceptable fallback: not reserved (even if same colors with different heights)
        if (!collision.reserved && !bestColors) {{
          bestColors = colors;
          bestRatio = selectedRatio;
        }}
      }}

      currentPalette = {{
        colors: bestColors || [anchorColor, CHROMATIC_LOW[0], CHROMATIC_HIGH[0]],
        weights: bestRatio || RATIOS[bandCount][0]
      }};

      renderFaceplateAndSwatches();
    }}

    // Re-roll only the specified band index, actively avoiding reserved palettes
    function rerollBand(index) {{
      if (!currentPalette || !currentPalette.colors) return;
      const curSlug = currentModule ? currentModule.slug : null;

      if (index === 0) {{
        const modes = ['Transform', 'Generate', 'Utility', 'Control'];
        const nextMode = modes[(modes.indexOf(moduleMode) + 1) % modes.length];
        setMode(nextMode);
        return;
      }}

      const anchor = ANCHORS[moduleMode] || ANCHORS['Transform'];
      const anchorTier = anchor.tier;
      const isHigh = (anchorTier === 'HIGH') ? (index % 2 === 0) : (index % 2 === 1);
      const pool = isHigh ? CHROMATIC_HIGH : CHROMATIC_LOW;
      const currentColor = currentPalette.colors[index];
      const otherColors = currentPalette.colors.filter((_, idx) => idx !== index);

      // Candidate colors not already in the palette
      let candidates = pool.filter(c => c !== currentColor && !otherColors.includes(c));
      if (candidates.length === 0) {{
        candidates = pool.filter(c => c !== currentColor);
      }}

      // Shuffle candidates to pick a random one that is not reserved
      candidates.sort(() => Math.random() - 0.5);

      let chosenColor = candidates[0];
      for (const cand of candidates) {{
        const testColors = [...currentPalette.colors];
        testColors[index] = cand;
        const col = checkPaletteCollision(testColors, currentPalette.weights, curSlug);
        if (!col.reserved) {{
          chosenColor = cand;
          break;
        }}
      }}

      currentPalette.colors[index] = chosenColor;
      renderFaceplateAndSwatches();
    }}

    function renderFaceplateAndSwatches() {{
      if (!currentModule || !currentPalette) return;

      const curSlug = currentModule.slug;
      const collision = checkPaletteCollision(currentPalette.colors, currentPalette.weights, curSlug);

      // Update Collision Banner and Save Button State
      const banner = document.getElementById('collision-banner');
      const saveBtn = document.getElementById('btn-save-main');

      if (collision.reserved) {{
        banner.className = 'collision-banner blocked';
        banner.innerHTML = `<span>&#x1F6AB;</span> <span>Cannot Save: Exact palette is already reserved by <b>${{collision.moduleName}}</b></span>`;
        saveBtn.classList.add('disabled');
        saveBtn.title = `Exact palette already reserved by ${{collision.moduleName}}`;
      }} else if (collision.sameColors) {{
        banner.className = 'collision-banner warn';
        banner.innerHTML = `<span>&#x26A0;&#xFE0F;</span> <span>Note: Colors are already used in this order by <b>${{collision.moduleName}}</b> (with different heights)</span>`;
        saveBtn.classList.remove('disabled');
        saveBtn.title = `Save Palette (Key: S)`;
      }} else {{
        banner.className = 'collision-banner';
        banner.innerHTML = '';
        saveBtn.classList.remove('disabled');
        saveBtn.title = `Save Palette (Key: S)`;
      }}

      // Render Faceplate SVG
      const container = document.getElementById('faceplate-container');
      container.innerHTML = currentModule.svg;

      const svgEl = container.querySelector('svg');
      if (svgEl) {{
        // Dynamically update the panel faceplate background rect fill
        const allRects = svgEl.querySelectorAll('rect');
        for (const r of allRects) {{
          const h = parseFloat(r.getAttribute('height'));
          if (h >= 128.0) {{
            r.setAttribute('fill', selectedPanelColor);
            break;
          }}
        }}

        const prior = svgEl.querySelector('#palette-badge');
        if (prior) prior.remove();

        const centerY = 64.25;
        const yStartMm = centerY - (BADGE_HEIGHT_MM / 2) + BADGE_Y_OFFSET;

        const badgeG = document.createElementNS('http://www.w3.org/2000/svg', 'g');
        badgeG.setAttribute('id', 'palette-badge');

        const totalW = currentPalette.weights.reduce((a, b) => a + b, 0);
        let curYMm = yStartMm;

        currentPalette.colors.forEach((hex, i) => {{
          const bandHeightMm = (currentPalette.weights[i] / totalW) * BADGE_HEIGHT_MM;
          const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
          rect.setAttribute('x', '0');
          rect.setAttribute('y', curYMm.toFixed(3));
          rect.setAttribute('width', BADGE_WIDTH_MM.toFixed(3));
          rect.setAttribute('height', bandHeightMm.toFixed(3));
          rect.setAttribute('fill', hex);
          rect.style.cursor = 'pointer';
          rect.setAttribute('title', 'Click to re-roll this band');
          rect.onclick = (e) => {{
            e.stopPropagation();
            rerollBand(i);
          }};
          badgeG.appendChild(rect);
          curYMm += bandHeightMm;
        }});

        svgEl.appendChild(badgeG);
      }}

      // Render Swatches
      const swatchesEl = document.getElementById('swatches');
      swatchesEl.innerHTML = '';
      const totalW = currentPalette.weights.reduce((a, b) => a + b, 0);

      currentPalette.colors.forEach((hex, i) => {{
        const bandH = ((currentPalette.weights[i] / totalW) * BADGE_HEIGHT_MM).toFixed(1);
        const pct = Math.round((currentPalette.weights[i] / totalW) * 100);

        const row = document.createElement('div');
        row.className = 'swatch-row';
        row.title = 'Click to re-roll this color';
        row.onclick = () => rerollBand(i);

        const block = document.createElement('div');
        block.className = 'swatch-block';
        block.style.backgroundColor = hex;

        const info = document.createElement('div');
        info.className = 'swatch-info';

        const hexLabel = document.createElement('div');
        hexLabel.className = 'swatch-hex';
        hexLabel.textContent = hex;

        const dimLabel = document.createElement('div');
        dimLabel.className = 'swatch-dim';
        dimLabel.textContent = `${{bandH}} mm (${{pct}}%)`;

        info.appendChild(hexLabel);
        info.appendChild(dimLabel);

        const rerollIcon = document.createElement('span');
        rerollIcon.className = 'reroll-icon';
        rerollIcon.innerHTML = '&#x21bb;';

        row.appendChild(block);
        row.appendChild(info);
        row.appendChild(rerollIcon);
        swatchesEl.appendChild(row);
      }});
    }}

    function setCount(n) {{
      bandCount = n;
      updateControlUI();
      generate();
    }}

    function setMode(mode) {{
      moduleMode = mode;
      updateControlUI();
      generate();
    }}

    // Saving and Overwrite / Reservation Protection
    function savePalette() {{
      if (!currentModule || !currentPalette) return;

      const slug = currentModule.slug;
      const collision = checkPaletteCollision(currentPalette.colors, currentPalette.weights, slug);

      // 1. HARD BLOCK: Exactly reserved by another module
      if (collision.reserved) {{
        showToast(`🚫 Cannot save: Palette already reserved by ${{collision.moduleName}}`);
        openAlertModal(
          `Palette Already Reserved`,
          `This exact palette (color sequence and band heights) is already reserved by "${{collision.moduleName}}".\n\nEach module must have a distinct palette. Please re-roll or modify a band before saving.`
        );
        return;
      }}

      // 2. WARNING: Same colors in order, but different heights
      if (collision.sameColors) {{
        openConfirmModal(
          `Color Sequence Warning`,
          `Warning: This exact color sequence (${{currentPalette.colors.join(', ')}}) is already used by "${{collision.moduleName}}" (with different band heights).\n\nDo you want to save anyway?`,
          () => proceedSave(slug)
        );
        return;
      }}

      // 3. Normal save / overwrite check
      proceedSave(slug);
    }}

    function proceedSave(slug) {{
      const alreadySaved = savedPalettes[slug];
      if (alreadySaved) {{
        openConfirmModal(
          `Overwrite Saved Palette?`,
          `"${{currentModule.name}}" already has a saved palette in palette.json (${{alreadySaved.colors.join(', ')}}).\n\nAre you sure you want to overwrite it with this new combination?`,
          () => executeSave()
        );
      }} else {{
        executeSave();
      }}
    }}

    async function executeSave() {{
      const slug = currentModule.slug;
      const totalW = currentPalette.weights.reduce((a, b) => a + b, 0);
      const centerY = 64.25;
      const yStartMm = centerY - (BADGE_HEIGHT_MM / 2) + BADGE_Y_OFFSET;

      const anchor = ANCHORS[moduleMode] || ANCHORS['Transform'];
      const anchorColor = anchor.hex;

      const bandsData = currentPalette.colors.map((hex, i) => {{
        const weight = currentPalette.weights[i];
        const hMm = Number(((weight / totalW) * BADGE_HEIGHT_MM).toFixed(3));
        const tier = (hex === '#FFFFFF' || hex === '#E4DAC2' || hex === '#BCC5CC' || CHROMATIC_HIGH.includes(hex)) ? 'HIGH' : 'LOW';
        return {{
          hex: hex,
          tier: tier,
          weight: weight,
          heightMm: hMm
        }};
      }});

      const paletteEntry = {{
        module: currentModule.name,
        slug: slug,
        hp: currentModule.hp,
        mode: moduleMode,
        anchor: anchor.label,
        anchorColor: anchorColor,
        anchorPos: 'top',
        bandCount: bandCount,
        bands: bandsData,
        colors: [...currentPalette.colors],
        weights: currentPalette.weights,
        badgeGeometry: {{
          widthMm: BADGE_WIDTH_MM,
          heightMm: BADGE_HEIGHT_MM,
          yOffsetMm: BADGE_Y_OFFSET,
          x: 0.0,
          y: Number(yStartMm.toFixed(3))
        }},
        panelColor: selectedPanelColor,
        savedAt: new Date().toISOString()
      }};

      savedPalettes[slug] = paletteEntry;

      // 1. Save to localStorage immediately
      localStorage.setItem('palette_manager_json', JSON.stringify(savedPalettes, null, 2));

      // 2. If a local file handle is active (File System Access API), write directly to disk!
      if (fileHandle) {{
        try {{
          const writable = await fileHandle.createWritable();
          await writable.write(JSON.stringify(savedPalettes, null, 2));
          await writable.close();
          showToast(`✓ Written directly to palette.json (${{currentModule.name}})`);
        }} catch (err) {{
          console.warn('File handle write error:', err);
          showToast(`✓ Saved in memory (${{currentModule.name}})`);
        }}
      }} else {{
        showToast(`✓ Saved ${{currentModule.name}} (Use "Export JSON" to save file)`);
      }}

      populateDropdown();
      document.getElementById('module-saved-pill').classList.add('active');
      renderFaceplateAndSwatches();
    }}

    // Direct Disk File Linking / Export (File System Access API & Blob Download)
    async function handleFileSync() {{
      if ('showSaveFilePicker' in window) {{
        try {{
          fileHandle = await window.showSaveFilePicker({{
            suggestedName: 'palette.json',
            types: [{{
              description: 'JSON Files',
              accept: {{ 'application/json': ['.json'] }}
            }}]
          }});
          // Write current state immediately
          const writable = await fileHandle.createWritable();
          await writable.write(JSON.stringify(savedPalettes, null, 2));
          await writable.close();

          const btn = document.getElementById('btn-file-sync');
          btn.classList.add('linked');
          document.getElementById('disk-label').textContent = 'Linked (palette.json)';
          showToast(`✓ Linked to palette.json! All future saves write directly.`);
        }} catch (err) {{
          if (err.name !== 'AbortError') {{
            downloadPaletteJson();
          }}
        }}
      }} else {{
        downloadPaletteJson();
      }}
    }}

    function downloadPaletteJson() {{
      const blob = new Blob([JSON.stringify(savedPalettes, null, 2)], {{ type: 'application/json' }});
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'palette.json';
      a.click();
      URL.revokeObjectURL(url);
      showToast('✓ Downloaded palette.json');
    }}

    function openConfirmModal(title, message, onConfirm) {{
      document.getElementById('modal-title').textContent = title;
      document.getElementById('modal-body').textContent = message;
      document.getElementById('btn-modal-cancel').style.display = 'block';
      document.getElementById('btn-modal-confirm').textContent = 'Proceed';
      pendingSaveCallback = onConfirm;
      document.getElementById('confirm-modal').classList.add('open');
    }}

    function openAlertModal(title, message) {{
      document.getElementById('modal-title').textContent = title;
      document.getElementById('modal-body').textContent = message;
      document.getElementById('btn-modal-cancel').style.display = 'none';
      document.getElementById('btn-modal-confirm').textContent = 'OK';
      pendingSaveCallback = null;
      document.getElementById('confirm-modal').classList.add('open');
    }}

    function closeConfirmModal(confirmed) {{
      document.getElementById('confirm-modal').classList.remove('open');
      if (confirmed && pendingSaveCallback) {{
        pendingSaveCallback();
      }}
      pendingSaveCallback = null;
    }}

    function showToast(text) {{
      const toast = document.getElementById('status-toast');
      document.getElementById('toast-text').textContent = text;
      toast.classList.add('active');
      setTimeout(() => toast.classList.remove('active'), 2500);
    }}

    // Keyboard Shortcuts
    window.addEventListener('keydown', (e) => {{
      if (e.target.tagName === 'SELECT' || e.target.tagName === 'INPUT') return;
      if (e.key === '2') setCount(2);
      else if (e.key === '3') setCount(3);
      else if (e.key === '4') setCount(4);
      else if (e.key === 't' || e.key === 'T' || e.key === 'e' || e.key === 'E') setMode('Transform');
      else if (e.key === 'g' || e.key === 'G') setMode('Generate');
      else if (e.key === 'u' || e.key === 'U') setMode('Utility');
      else if (e.key === 'c' || e.key === 'C') setMode('Control');
      else if (e.key === 's' || e.key === 'S') savePalette();
      else if (e.key === 'ArrowLeft') navModule(-1);
      else if (e.key === 'ArrowRight') navModule(1);
      else if (e.key === ' ') {{
        e.preventDefault();
        generate();
      }}
    }});

    // Boot
    init();
  </script>
</body>
</html>
'''

out_path = os.path.join(PALETTE_DIR, 'index.html')
with open(out_path, 'w', encoding='utf-8') as f:
    f.write(html_content)

print(f"Updated app with collision avoidance and reservation blocking in {out_path}")
