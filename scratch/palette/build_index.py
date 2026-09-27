import os
import base64

def get_b64(path):
    with open(path, 'rb') as f:
        return base64.b64encode(f.read()).decode('ascii')

node_b64 = get_b64('res/Node.otf')
qs_b64 = get_b64('res/Quicksand-Medium.ttf')

html_content = f'''<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>FAC 73 Module Palette Manager</title>
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
      gap: 20px;
      width: 100%;
      max-width: 1200px;
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
      min-width: 240px;
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
      gap: 14px;
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
      padding: 6px 12px;
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
      transition: background 0.12s;
    }}
    .btn-save:hover {{
      background: #0085d0;
    }}

    /* Main Workspace Stage */
    .stage {{
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 60px;
      width: 100%;
      min-height: 600px;
      padding: 20px 0;
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

    .swatch-weight {{
      font-size: 10px;
      font-mono;
      color: #727786;
      margin-top: 1px;
    }}

    .toast {{
      font-size: 11px;
      color: #56B4E9;
      opacity: 0;
      transition: opacity 0.18s;
      margin-left: auto;
    }}
    .toast.show {{
      opacity: 1;
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

    /* Custom Confirm Modal */
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
  <script src="modules_data.js"></script>
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

      <!-- Generator Controls -->
      <div class="controls">
        <div class="group">
          <button class="btn" id="b-2" onclick="setCount(2)">2</button>
          <button class="btn active" id="b-3" onclick="setCount(3)">3</button>
          <button class="btn" id="b-4" onclick="setCount(4)">4</button>
        </div>

        <div class="divider"></div>

        <div class="group">
          <button class="btn active" id="b-top" onclick="setAnchor('top')">Top</button>
          <button class="btn" id="b-bottom" onclick="setAnchor('bottom')">Bottom</button>
        </div>

        <div class="divider"></div>

        <button class="btn" onclick="generate()" title="Re-roll (Spacebar)">&#x21bb;</button>

        <div class="divider"></div>

        <button class="btn-save" onclick="savePalette()">Save Palette</button>
      </div>

    </div>

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

  <!-- Confirm Overwrite Modal -->
  <div class="modal-overlay" id="confirm-modal">
    <div class="modal-card">
      <div class="modal-title" id="modal-title">Overwrite Saved Palette?</div>
      <div class="modal-body" id="modal-body">
        This module already has a saved palette in palette.json. Are you sure you want to overwrite it with this new combination?
      </div>
      <div class="modal-actions">
        <button class="btn-modal-cancel" onclick="closeConfirmModal(false)">Cancel</button>
        <button class="btn-modal-confirm" onclick="closeConfirmModal(true)">Overwrite</button>
      </div>
    </div>
  </div>

  <script>
    const CHROMATIC_HIGH = ['#F0E442', '#56B4E9', '#E69F00'];
    const CHROMATIC_LOW  = ['#0072B2', '#D55E00', '#009E73', '#CC79A7'];
    const ANCHORS = ['#FFFFFF', '#1A1A1A'];

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

    // State
    let modulesList = window.MODULES_CATALOG || [];
    let savedPalettes = {{}};
    let currentModule = null;
    let bandCount = 3;
    let anchorPos = 'top';
    let currentPalette = null; // {{ colors: [], weights: [] }}
    let pendingSaveCallback = null;

    function pickRandom(arr, exclude = []) {{
      const valid = arr.filter(x => !exclude.includes(x));
      return valid[Math.floor(Math.random() * valid.length)];
    }}

    // Fetch initial saved palettes and modules
    async function loadData() {{
      try {{
        const palRes = await fetch('/api/palette');
        if (palRes.ok) {{
          savedPalettes = await palRes.json();
        }}
      }} catch (e) {{
        console.warn('Backend API offline, using cached data.');
      }}

      // Check if modules were returned from server
      try {{
        const modRes = await fetch('/api/modules');
        if (modRes.ok) {{
          modulesList = await modRes.json();
        }}
      }} catch (e) {{}}

      populateDropdown();

      // Default to first module or URL slug
      if (modulesList.length > 0) {{
        selectModule(modulesList[0].slug);
      }}
    }}

    function populateDropdown() {{
      const select = document.getElementById('module-select');
      const curSlug = currentModule ? currentModule.slug : (select.value || '');
      select.innerHTML = '';

      modulesList.forEach(m => {{
        const isSaved = (savedPalettes && savedPalettes[m.slug]) || m.hasPalette;
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
      if (modulesList.length === 0) return;
      const curIdx = modulesList.findIndex(m => m.slug === currentModule.slug);
      let nextIdx = curIdx + delta;
      if (nextIdx < 0) nextIdx = modulesList.length - 1;
      if (nextIdx >= modulesList.length) nextIdx = 0;
      selectModule(modulesList[nextIdx].slug);
    }}

    function selectModule(slug) {{
      const mod = modulesList.find(m => m.slug === slug);
      if (!mod) return;
      currentModule = mod;

      document.getElementById('module-select').value = slug;
      document.getElementById('module-hp-badge').textContent = `${{mod.hp}} HP`;

      const saved = (savedPalettes && savedPalettes[slug]) || mod.palette;
      const savedPill = document.getElementById('module-saved-pill');

      if (saved) {{
        savedPill.classList.add('active');
        // Load the saved palette directly!
        bandCount = saved.bandCount || saved.colors.length;
        anchorPos = saved.anchorPos || 'top';
        updateControlUI();
        currentPalette = {{
          colors: saved.colors,
          weights: saved.weights || RATIOS[bandCount][0]
        }};
        renderFaceplateAndSwatches();
      }} else {{
        savedPill.classList.remove('active');
        // Generate a new one
        generate();
      }}
    }}

    function updateControlUI() {{
      [2, 3, 4].forEach(x => {{
        document.getElementById('b-' + x).classList.toggle('active', x === bandCount);
      }});
      document.getElementById('b-top').classList.toggle('active', anchorPos === 'top');
      document.getElementById('b-bottom').classList.toggle('active', anchorPos === 'bottom');
    }}

    function generate() {{
      // 1. Pick anchor
      const anchorColor = ANCHORS[Math.floor(Math.random() * ANCHORS.length)];
      const anchorTier = (anchorColor === '#FFFFFF') ? 'HIGH' : 'LOW';

      // 2. Strict Luminance Alternation
      const colors = new Array(bandCount);
      const used = [anchorColor];

      if (anchorPos === 'top') {{
        colors[0] = anchorColor;
        let nextTier = (anchorTier === 'HIGH') ? 'LOW' : 'HIGH';
        for (let i = 1; i < bandCount; i++) {{
          const pool = (nextTier === 'HIGH') ? CHROMATIC_HIGH : CHROMATIC_LOW;
          const c = pickRandom(pool, used);
          colors[i] = c;
          used.push(c);
          nextTier = (nextTier === 'HIGH') ? 'LOW' : 'HIGH';
        }}
      }} else {{
        const lastIdx = bandCount - 1;
        colors[lastIdx] = anchorColor;
        let nextTier = (anchorTier === 'HIGH') ? 'LOW' : 'HIGH';
        for (let i = lastIdx - 1; i >= 0; i--) {{
          const pool = (nextTier === 'HIGH') ? CHROMATIC_HIGH : CHROMATIC_LOW;
          const c = pickRandom(pool, used);
          colors[i] = c;
          used.push(c);
          nextTier = (nextTier === 'HIGH') ? 'LOW' : 'HIGH';
        }}
      }}

      // 3. Strictly unequal height weights
      const ratioPool = RATIOS[bandCount];
      const selectedRatio = ratioPool[Math.floor(Math.random() * ratioPool.length)];

      currentPalette = {{
        colors: colors,
        weights: selectedRatio
      }};

      renderFaceplateAndSwatches();
    }}

    function renderFaceplateAndSwatches() {{
      if (!currentModule || !currentPalette) return;

      const container = document.getElementById('faceplate-container');
      container.innerHTML = currentModule.svg;

      const svgEl = container.querySelector('svg');
      if (svgEl) {{
        // Remove any prior badge
        const prior = svgEl.querySelector('#fac73-badge');
        if (prior) prior.remove();

        // Build badge group: 5.08mm wide x 25.4mm tall, Y = 51.55mm
        const badgeG = document.createElementNS('http://www.w3.org/2000/svg', 'g');
        badgeG.setAttribute('id', 'fac73-badge');

        const totalW = currentPalette.weights.reduce((a, b) => a + b, 0);
        const totalHeightMm = 25.4;
        const badgeWidthMm = 5.08;
        let curYMm = 51.55;

        currentPalette.colors.forEach((hex, i) => {{
          const bandHeightMm = (currentPalette.weights[i] / totalW) * totalHeightMm;
          const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
          rect.setAttribute('x', '0');
          rect.setAttribute('y', curYMm.toFixed(3));
          rect.setAttribute('width', badgeWidthMm.toFixed(3));
          rect.setAttribute('height', bandHeightMm.toFixed(3));
          rect.setAttribute('fill', hex);
          // ZERO borders
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
        const pct = Math.round((currentPalette.weights[i] / totalW) * 100);

        const row = document.createElement('div');
        row.className = 'swatch-row';
        row.title = 'Copy ' + hex;
        row.onclick = () => copyHex(hex, row);

        const block = document.createElement('div');
        block.className = 'swatch-block';
        block.style.backgroundColor = hex;

        const info = document.createElement('div');
        info.className = 'swatch-info';

        const hexLabel = document.createElement('div');
        hexLabel.className = 'swatch-hex';
        hexLabel.textContent = hex;

        const weightLabel = document.createElement('div');
        weightLabel.className = 'swatch-weight';
        weightLabel.textContent = pct + '% height';

        info.appendChild(hexLabel);
        info.appendChild(weightLabel);

        const toast = document.createElement('span');
        toast.className = 'toast';
        toast.textContent = 'copied';

        row.appendChild(block);
        row.appendChild(info);
        row.appendChild(toast);
        swatchesEl.appendChild(row);
      }});
    }}

    function copyHex(hex, row) {{
      navigator.clipboard.writeText(hex);
      const toast = row.querySelector('.toast');
      toast.classList.add('show');
      setTimeout(() => toast.classList.remove('show'), 900);
    }}

    function setCount(n) {{
      bandCount = n;
      updateControlUI();
      generate();
    }}

    function setAnchor(pos) {{
      anchorPos = pos;
      updateControlUI();
      generate();
    }}

    // Save handling & Confirm Modal
    async function savePalette() {{
      if (!currentModule || !currentPalette) return;

      const slug = currentModule.slug;
      const alreadySaved = savedPalettes && savedPalettes[slug];

      if (alreadySaved) {{
        // Confirm overwrite
        openConfirmModal(
          `Overwrite Saved Palette?`,
          `"${{currentModule.name}}" already has a saved palette in palette.json (${{alreadySaved.colors.join(', ')}}). Do you want to overwrite it with this new combination?`,
          () => executeSave()
        );
      }} else {{
        executeSave();
      }}
    }}

    async function executeSave() {{
      const slug = currentModule.slug;
      const totalW = currentPalette.weights.reduce((a, b) => a + b, 0);

      const bandsData = currentPalette.colors.map((hex, i) => {{
        const weight = currentPalette.weights[i];
        const hMm = Number(((weight / totalW) * 25.4).toFixed(3));
        const tier = (hex === '#FFFFFF' || CHROMATIC_HIGH.includes(hex)) ? 'HIGH' : 'LOW';
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
        bandCount: bandCount,
        anchorPos: anchorPos,
        bands: bandsData,
        colors: currentPalette.colors,
        weights: currentPalette.weights,
        badgeGeometry: {{
          x: 0.0,
          y: 51.55,
          widthMm: 5.08,
          heightMm: 25.4
        }},
        savedAt: new Date().toISOString()
      }};

      try {{
        const res = await fetch('/api/save', {{
          method: 'POST',
          headers: {{ 'Content-Type': 'application/json' }},
          body: JSON.stringify({{ slug: slug, palette: paletteEntry }})
        }});

        if (res.ok) {{
          savedPalettes[slug] = paletteEntry;
          showToast(`✓ Saved ${{currentModule.name}} to palette.json`);
          populateDropdown();
          document.getElementById('module-saved-pill').classList.add('active');
        }} else {{
          throw new Error('Save failed');
        }}
      }} catch (err) {{
        // Fallback for static viewer
        savedPalettes[slug] = paletteEntry;
        localStorage.setItem('fac73_palettes', JSON.stringify(savedPalettes));
        showToast(`✓ Saved to localStorage (${{currentModule.name}})`);
        populateDropdown();
        document.getElementById('module-saved-pill').classList.add('active');
      }}
    }}

    function openConfirmModal(title, message, onConfirm) {{
      document.getElementById('modal-title').textContent = title;
      document.getElementById('modal-body').textContent = message;
      pendingSaveCallback = onConfirm;
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
      else if (e.key === 't' || e.key === 'T') setAnchor('top');
      else if (e.key === 'b' || e.key === 'B') setAnchor('bottom');
      else if (e.key === 's' || e.key === 'S') savePalette();
      else if (e.key === 'ArrowLeft') navModule(-1);
      else if (e.key === 'ArrowRight') navModule(1);
      else if (e.key === ' ') {{
        e.preventDefault();
        generate();
      }}
    }});

    // Boot
    loadData();
  </script>
</body>
</html>
'''

out_path = os.path.join('scratch', 'palette', 'index.html')
with open(out_path, 'w', encoding='utf-8') as f:
    f.write(html_content)

print(f"Generated clean manager app in {out_path}.")
