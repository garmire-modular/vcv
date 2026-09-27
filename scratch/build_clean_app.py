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
  <title>FAC 73 Badge Generator</title>
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
      justify-content: center;
      padding: 24px;
      -webkit-font-smoothing: antialiased;
    }}

    .app {{
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 28px;
    }}

    /* Control Bar */
    .controls {{
      display: flex;
      align-items: center;
      gap: 16px;
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

    /* Stage Area */
    .stage {{
      display: flex;
      align-items: center;
      gap: 56px;
    }}

    /* 3HP Eurorack: 15.24mm x 128.5mm */
    /* Exact scale: 65px wide x 548px tall (ratio 0.1186) */
    .module-3hp {{
      position: relative;
      width: 65px;
      height: 548px;
      background: #7c7c7c;
      border-radius: 1px;
      box-shadow: 0 24px 48px rgba(0, 0, 0, 0.65), 0 2px 4px rgba(0, 0, 0, 0.4);
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: space-between;
      padding: 8px 0;
      user-select: none;
      overflow: hidden;
    }}

    /* Eurorack 3HP Oval Mounting Slots */
    .screw {{
      position: absolute;
      width: 14px;
      height: 6px;
      background: #484848;
      border-radius: 3px;
      border: 1px solid #303030;
      box-shadow: inset 0 1px 2px rgba(0,0,0,0.6);
      left: 50%;
      transform: translateX(-50%);
    }}
    .screw.top {{ top: 5px; }}
    .screw.bottom {{ bottom: 5px; }}

    /* Title Block */
    .mod-header {{
      display: flex;
      flex-direction: column;
      align-items: center;
      margin-top: 22px;
      z-index: 2;
    }}

    .mod-title {{
      font-family: 'Node', sans-serif;
      font-size: 13px;
      color: #ffffff;
      line-height: 1;
      letter-spacing: 0.01em;
    }}

    .mod-ver {{
      font-family: 'Quicksand', sans-serif;
      font-size: 6.5px;
      color: #aaaaaa;
      margin-top: 4px;
      letter-spacing: 0.04em;
    }}

    /* 1.0" x 0.2" Edge Badge (25.4mm x 5.08mm) */
    /* Exactly 21.7px wide x 108.3px tall (left edge, vertically centered) */
    .badge {{
      position: absolute;
      left: 0;
      top: 50%;
      transform: translateY(-50%);
      width: 21.7px;
      height: 108.3px;
      display: flex;
      flex-direction: column;
      z-index: 3;
    }}

    .badge-band {{
      width: 100%;
      /* NO borders */
    }}

    /* 3HP Jacks */
    .jacks-zone {{
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 10px;
      margin-bottom: 22px;
      z-index: 2;
    }}

    .jack {{
      width: 19px;
      height: 19px;
      border-radius: 50%;
      background: #1c1c1c;
      border: 1.5px solid #a4a4a4;
      box-shadow: inset 0 1px 2px rgba(0,0,0,0.8), 0 1px 1px rgba(255,255,255,0.15);
      display: flex;
      align-items: center;
      justify-content: center;
    }}

    .jack-hole {{
      width: 7px;
      height: 7px;
      border-radius: 50%;
      background: #000000;
      box-shadow: inset 0 1px 2px rgba(0,0,0,0.9);
    }}

    .jack-label {{
      font-size: 5.5px;
      font-weight: 600;
      color: #1c1c1c;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      margin-top: -6px;
    }}

    /* Palette Swatches */
    .swatches {{
      display: flex;
      flex-direction: column;
      gap: 12px;
    }}

    .swatch-row {{
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 5px 8px;
      border-radius: 5px;
      cursor: pointer;
      transition: background 0.12s ease;
    }}

    .swatch-row:hover {{
      background: #191b22;
    }}

    .swatch-block {{
      width: 26px;
      height: 26px;
      border-radius: 3px;
      /* NO border */
    }}

    .swatch-hex {{
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      font-size: 13px;
      font-weight: 500;
      color: #c7cbd6;
      letter-spacing: 0.04em;
    }}

    .copied-toast {{
      font-size: 11px;
      color: #56B4E9;
      opacity: 0;
      transition: opacity 0.18s;
    }}

    .copied-toast.show {{
      opacity: 1;
    }}
  </style>
</head>
<body>

  <div class="app">

    <!-- Minimal Controls -->
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

      <button class="btn" onclick="generate()" title="Re-roll">&#x21bb;</button>
    </div>

    <!-- Stage: 3HP Module + Swatches -->
    <div class="stage">
      
      <div class="module-3hp">
        <div class="screw top"></div>
        <div class="screw bottom"></div>

        <div class="mod-header">
          <div class="mod-title">switch</div>
          <div class="mod-ver">v1.0.0</div>
        </div>

        <!-- 1.0" x 0.2" Edge Badge (Zero Borders) -->
        <div class="badge" id="badge"></div>

        <!-- 3HP Jacks -->
        <div class="jacks-zone">
          <div class="jack"><div class="jack-hole"></div></div>
          <span class="jack-label">IN 1</span>
          <div class="jack"><div class="jack-hole"></div></div>
          <span class="jack-label">IN 2</span>
          <div class="jack"><div class="jack-hole"></div></div>
          <span class="jack-label">OUT</span>
        </div>
      </div>

      <!-- Swatches List -->
      <div class="swatches" id="swatches"></div>

    </div>

  </div>

  <script>
    const CHROMATIC_HIGH = ['#F0E442', '#56B4E9', '#E69F00'];
    const CHROMATIC_LOW  = ['#0072B2', '#D55E00', '#009E73', '#CC79A7'];
    const ANCHORS = ['#FFFFFF', '#1A1A1A'];

    // Strictly unequal height rhythms
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

    let bandCount = 3;
    let anchorPos = 'top';

    function pickRandom(arr, exclude = []) {{
      const valid = arr.filter(x => !exclude.includes(x));
      return valid[Math.floor(Math.random() * valid.length)];
    }}

    function generate() {{
      // 1. Calculate Anchor First (White or Charcoal)
      const anchorColor = ANCHORS[Math.floor(Math.random() * ANCHORS.length)];
      const anchorTier = (anchorColor === '#FFFFFF') ? 'HIGH' : 'LOW';

      // 2. Luminance Alternation
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

      // 3. Pick unequal height ratio
      const ratioPool = RATIOS[bandCount];
      const selectedRatio = ratioPool[Math.floor(Math.random() * ratioPool.length)];

      render(colors, selectedRatio);
    }}

    function render(colors, weights) {{
      const badgeEl = document.getElementById('badge');
      const swatchesEl = document.getElementById('swatches');
      badgeEl.innerHTML = '';
      swatchesEl.innerHTML = '';

      const total = weights.reduce((a, b) => a + b, 0);

      colors.forEach((hex, i) => {{
        const pct = (weights[i] / total) * 100;

        // Badge band on 3HP panel (ZERO borders)
        const band = document.createElement('div');
        band.className = 'badge-band';
        band.style.backgroundColor = hex;
        band.style.height = pct + '%';
        badgeEl.appendChild(band);

        // Clean Swatch
        const row = document.createElement('div');
        row.className = 'swatch-row';
        row.title = 'Copy ' + hex;
        row.onclick = () => copyHex(hex, row);

        const block = document.createElement('div');
        block.className = 'swatch-block';
        block.style.backgroundColor = hex;

        const hexLabel = document.createElement('div');
        hexLabel.className = 'swatch-hex';
        hexLabel.textContent = hex;

        const toast = document.createElement('span');
        toast.className = 'copied-toast';
        toast.textContent = 'copied';

        row.appendChild(block);
        row.appendChild(hexLabel);
        row.appendChild(toast);
        swatchesEl.appendChild(row);
      }});
    }}

    function copyHex(hex, row) {{
      navigator.clipboard.writeText(hex);
      const toast = row.querySelector('.copied-toast');
      toast.classList.add('show');
      setTimeout(() => toast.classList.remove('show'), 900);
    }}

    function setCount(n) {{
      bandCount = n;
      [2, 3, 4].forEach(x => {{
        document.getElementById('b-' + x).classList.toggle('active', x === n);
      }});
      generate();
    }}

    function setAnchor(pos) {{
      anchorPos = pos;
      document.getElementById('b-top').classList.toggle('active', pos === 'top');
      document.getElementById('b-bottom').classList.toggle('active', pos === 'bottom');
      generate();
    }}

    // Keyboard shortcuts
    window.addEventListener('keydown', (e) => {{
      if (e.key === '2') setCount(2);
      else if (e.key === '3') setCount(3);
      else if (e.key === '4') setCount(4);
      else if (e.key === 't' || e.key === 'T') setAnchor('top');
      else if (e.key === 'b' || e.key === 'B') setAnchor('bottom');
      else if (e.key === ' ') {{
        e.preventDefault();
        generate();
      }}
    }});

    // Init
    generate();
  </script>
</body>
</html>
'''

out_path = r'C:\Users\Pat\.gemini\antigravity\brain\1810fd88-4c00-4461-8eb4-ad0fdb55eb44\palette_generator.html'
with open(out_path, 'w', encoding='utf-8') as f:
    f.write(html_content)

scratch_path = 'scratch/palette_generator.html'
with open(scratch_path, 'w', encoding='utf-8') as f:
    f.write(html_content)

print("Updated clean app written successfully.")
