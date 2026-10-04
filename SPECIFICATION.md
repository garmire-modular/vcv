# Bitterroot: Formal Engineering Specification

**Document Version:** 1.4.0  
**Module Version:** 1.1.0  
**Slug:** `Bitterroot`  
**Panel Title:** `bitterroot` (Font: `res/Node.otf`, lowercase, baseline $Y = 7.620\text{ mm}$, centered $X = 81.280\text{ mm}$)  
**Format:** 32 HP Eurorack ($162.560\text{ mm}$ width, $128.500\text{ mm}$ height, $304 \times 240\text{ px}$ MetaModule bitmap)  

---

## 1. Module Overview & Functional Concept

**Bitterroot** is a 32 HP 9-effect modular vector synthesis workstation, 10-bit digital topological processor, and volumetric $Z$-axis laser beam intensity engine. Designed for high-speed laser galvo projection, oscilloscope art, and wide-bandwidth vector graphics, it transforms incoming continuous $X$ and $Y$ coordinate signals into a discrete 10-bit integer grid $[0, 1023] \times [0, 1023]$. 

Within this discrete coordinate space, bitwise, dyadic, and algebraic transformations fracture continuous Euclidean trajectories into self-similar fractal dust, Walsh reflections, crystalline lattice planes, and 4D hypercube projections while strictly preserving display boundaries.

Crucially, Bitterroot includes a dedicated **$Z$-Axis Beam Intensity & Retrace Blanking Engine**. Because discrete bitwise operations cause instantaneous coordinate jumps across the screen, physical galvo mirrors would ordinarily produce harsh retrace streaks and scanner heat. Bitterroot computes instantaneous velocity, recursion depth, and binary transition energy to dynamically modulate and output a dedicated **`OUT Z`** signal ($0\text{--}1\text{V}$, $0\text{--}5\text{V}$, or $0\text{--}10\text{V}$). This turns chaotic scribbles into crystalline laser constellations, multi-planar diffraction gratings, and holographic depth sculptures.

The module features a **$3 \times 3$ grid of nine simultaneous topological effects**:
- **Row 1:**
  - **Morton:** Z-Order Space-Filling Curves & Hilbert Fractal Morph (`SHIFT`, `STRIDE`, `HILBERT`, `MIX`)
  - **Reverse:** Bitwise Dyadic Reflection & Asymmetric Axis Skew (`WIDTH`, `OFFSET`, `SKEW`, `MIX`)
  - **Transpose:** Bit-Plane Transposition & 3-Way Bit Permutation Cycle (`PLANE A`, `PLANE B`, `CYCLE C`, `MIX`)
- **Row 2:**
  - **Avalanche:** Carry-Propagated Cross-Modulation & Dual-Direction Borrow/Carry (`MASK`, `SHIFT`, `BORROW`, `MIX`)
  - **Permute:** 20-bit Circular Permutation Matrix & Coprime Stride Hopping (`MODE`, `ROTATE`, `STRIDE`, `MIX`)
  - **Gray:** Reflected Binary Gray Code Dyadic Folding & Multi-Order Tap Distance (`DEPTH`, `MODE`, `TAP`, `MIX`)
- **Row 3:**
  - **Galois:** Galois Field $\text{GF}(2^{10})$ Polynomial Scramble & Non-Linear Inversion (`POLY`, `ALPHA`, `POWER`, `MIX`)
  - **Automata:** 1D Elementary Cellular Automata Mesh & Cross-Axis Seed Injection (`RULE`, `STEPS`, `INJECT`, `MIX`)
  - **Hamming:** Popcount & Mutual Hamming Distance Cross-Coupling (`GAIN`, `MODE`, `MUTUAL`, `MIX`)

All 36 effect controls use **full-sized knobs (`RoundBlackKnob`)** organized into three distinct, cleanly separated columns with dedicated parameter headers and subheaders. 

Every effect includes a dedicated **Mix** knob:
- $0\%$ mix is **complete code bypass** (zero DSP calculation overhead, pure unmodified pass-through).
- The default position for each of the 9 Mix knobs is $0\%$ (full dry bypass).

Below the effect controls sits the **Master Engine Section**:
- Left side: `Scan X focus` (knob, $0\text{--}100\%$, default $50\%$), `Scan Y focus` (knob, $0\text{--}100\%$, default $50\%$), `Route mode` switch (`Serial` default vs `Matrix scan`), 3x3 Vector Activity LED matrix preview, and `Z blanking threshold` (knob).
- Right side: Master jacks (`IN X`, `IN Y`, `SCAN X`, `SCAN Y`, `Z BLK` CV depth, `OUT X`, `OUT Y`, `OUT Z`).

Directly below the master row sits the **$12 \times 3$ CV Depth Jack Matrix** accommodating 36 jacks:
- Columns 1–4: Morton (`SHFT`, `STRD`, `HLBT`, `MIX`)
- Columns 5–8: Reverse (`WDTH`, `OFST`, `SKEW`, `MIX`)
- Columns 9–12: Transpose (`PL A`, `PL B`, `CYCL`, `MIX`)
- Row 2 CV: Avalanche, Permute, Gray
- Row 3 CV: Galois, Automata, Hamming (aligned to fixed bottom invariant at $Y = 118.00\text{ mm}$).

---

## 2. Electrical Specifications & Coordinate Conversions

### 2.1. Voltage Standard & ADC/DAC Mapping
By default, the module operates in **Bipolar $\pm 5.0\text{V}$ Mode** (the Eurorack laser/audio synthesizer standard), with an optional **Unipolar $0\text{--}10.0\text{V}$ Mode** selectable via right-click context menu (persisted in JSON patch data).

- **Bipolar $\pm 5.0\text{V}$ Standard (Default):**
  - Voltage Range: $-5.000\text{V} \le V_{\text{in}} \le +5.000\text{V}$
  - Midpoint Center: $0.000\text{V} \implies 512$
  - ADC Quantization (10-bit integer $[0, 1023]$):
    $$D = \text{clamp}\left(\left\lfloor \frac{V_{\text{in}} + 5.0}{10.0} \times 1023.0 + 0.5 \right\rfloor, 0, 1023\right)$$
  - DAC Reconstruction ($\pm 5.0\text{V}$ analog output):
    $$V_{\text{out}} = \left(\frac{D}{1023.0}\right) \times 10.0\text{V} - 5.0\text{V}$$

- **Unipolar $0\text{--}10.0\text{V}$ Standard:**
  - Voltage Range: $0.000\text{V} \le V_{\text{in}} \le +10.000\text{V}$
  - Midpoint Center: $+5.000\text{V} \implies 512$
  - ADC Quantization:
    $$D = \text{clamp}\left(\left\lfloor \frac{V_{\text{in}}}{10.0} \times 1023.0 + 0.5 \right\rfloor, 0, 1023\right)$$
  - DAC Reconstruction:
    $$V_{\text{out}} = \left(\frac{D}{1023.0}\right) \times 10.0\text{V}$$

- **`OUT Z` Intensity Scaling:**
  - Selectable via context menu: **1.0V** (Laser diode / logic), **5.0V** (Eurorack nominal - default), **10.0V** (Full video unipolar).
  - $0.0\text{V} = \text{Full Beam Blanking (Laser OFF)}$, Maximum Voltage $= \text{Full Intensity (Laser ON)}$.

---

## 3. Mathematical Specifications: The 9 Topological Transformations

All operations receive integer coordinates $(X, Y) \in [0, 1023]^2$, compute transformed coordinates $(X_{\text{fx}}, Y_{\text{fx}})$, and blend dry/wet using parameter `mix`.

### 3.1. Morton — Space-Filling Curves & Hilbert Fractal Morph
- **Param 1 (`Morton shift`):** Integer bit rotation $k \in [0, 19]$ applied to the 20-bit Morton key.
- **Param 2 (`Morton stride`):** Interleaving stride mode ($0 = \text{Standard}$, $1 = \text{Inverted}$, $2 = \text{2-bit block}$).
- **Param 3 (`Morton morph`):** Continuous morph factor $H \in [0.0, 1.0]$ applying Gray-coded quadrant reflection to convert discontinuous Z-order curves into continuous Hilbert/Peano trajectories.
- **Param 4 (`Morton mix`):** Dry/Wet mix ($0.0 = \text{complete code bypass}$, $1.0 = \text{100\% wet}$).

### 3.2. Reverse — Bitwise Dyadic Reflection & Asymmetric Axis Skew
- **Param 1 (`Reverse width`):** Number of bits reversed $W \in [1, 10]$ (from LSB upward).
- **Param 2 (`Reverse mask`):** Pre-reversal bitwise XOR mask $S \in [0, 1023]$.
- **Param 3 (`Reverse skew`):** Differential reversal width skew $\Delta W \in [-5, +5]$ between $X$ and $Y$.
- **Param 4 (`Reverse mix`):** Dry/Wet mix.

### 3.3. Transpose — Bit-Plane Transposition
- **Param 1 (`Transpose plane A`):** Bit index $p_a \in [0, 9]$ exchanged between $X$ and $Y$.
- **Param 2 (`Transpose plane B`):** Second bit index $p_b \in [0, 9]$ exchanged between $X$ and $Y$.
- **Param 3 (`Transpose cycle C`):** Third plane index $p_c \in [0, 9]$ creating a cyclic 3-way permutation $X[p_a] \to Y[p_b] \to X[p_c] \to X[p_a]$.
- **Param 4 (`Transpose mix`):** Dry/Wet mix.

### 3.4. Avalanche — Carry-Propagated Cross-Modulation (Shear Wrapping)
- **Param 1 (`Avalanche mask`):** 10-bit bitmask $M \in [0, 1023]$ gating carry propagation.
- **Param 2 (`Avalanche shift`):** Shift amount $k \in [0, 4]$ of avalanche injection.
- **Param 3 (`Avalanche borrow`):** Interpolates between additive carry ($+C$) and borrow subtraction ($-C \pmod{1024}$).
- **Param 4 (`Avalanche mix`):** Dry/Wet mix.

### 3.5. Permute — Circular Permutation Matrix
- **Param 1 (`Permute mode`):** Mode selector ($0 = \text{Odd/even}$, $1 = \text{Shuffle}$, $2 = \text{Inversion}$, $3 = \text{Quadrant}$).
- **Param 2 (`Permute rotate`):** 20-bit circular shift $R \in [0, 19]$ applied to joint register $[X_{9..0}, Y_{9..0}]$.
- **Param 3 (`Permute stride`):** Stride step $S \in \{1, 3, 5, 7, 9, 11\}$ modulo 20.
- **Param 4 (`Permute mix`):** Dry/Wet mix.

### 3.6. Gray — Gray Code Dyadic Folding
- **Param 1 (`Gray depth`):** Active Gray code depth $N \in [1, 10]$ bits.
- **Param 2 (`Gray mode`):** $0 = \text{Binary to gray}$, $1 = \text{Gray to binary}$, $2 = \text{Dual reflected}$.
- **Param 3 (`Gray tap`):** XOR tap distance $k \in [1, 9]$ ($G_k = X \oplus (X \gg k)$).
- **Param 4 (`Gray mix`):** Dry/Wet mix.

### 3.7. Galois — Galois Field $\text{GF}(2^{10})$ Polynomial Scramble
- **Param 1 (`Galois poly`):** Selects irreducible polynomials over $\text{GF}(2^{10})$ (`0x409`, `0x481`, `0x611`, `0x50D`, `0x46F`, `0x425`, `0x679`, `0x4D5`).
- **Param 2 (`Galois alpha`):** Field element multiplier $\alpha \in [1, 1023]$.
- **Param 3 (`Galois mode`):** Exponent mode ($0 = \text{Linear}$, $1 = \text{Inversion } X^{-1}$, $2 = \text{Cube } X^3$, $3 = \text{S-box quintic } X^5$).
- **Param 4 (`Galois mix`):** Dry/Wet mix.

### 3.8. Automata — 1D Elementary Cellular Automata Mesh
- **Param 1 (`Automata rule`):** Rule number $R \in [0, 255]$ (e.g. Rule 90, Rule 30, Rule 110, Rule 150).
- **Param 2 (`Automata steps`):** Number of CA steps $S \in [1, 4]$.
- **Param 3 (`Automata coupling`):** Coupling mode ($0 = \text{Edge}$, $1 = \text{Center}$, $2 = \text{Full XOR}$).
- **Param 4 (`Automata mix`):** Dry/Wet mix.

### 3.9. Hamming — Popcount / Hamming Dispersion
- **Param 1 (`Hamming gain`):** Displacement factor $K \in [0, 32]$.
- **Param 2 (`Hamming mode`):** Parity check routing ($0 = \text{Sign flip}$, $1 = \text{Shear}$, $2 = \text{Jump}$).
- **Param 3 (`Hamming coupling`):** Modulates scale and angle using mutual distance $d_H(X, Y) = \text{popcount}(X \oplus Y)$.
- **Param 4 (`Hamming mix`):** Dry/Wet mix.

---

## 4. Front Panel Mechanical Layout & Coordinates (32 HP / $162.560\text{ mm}$)

### 4.1. Coordinate Constants & Margins
- **Panel Width:** $162.560\text{ mm}$ (32 HP)
- **Panel Height:** $128.500\text{ mm}$ (3U)
- **Background Fill:** `#6e6e6e`
- **Palette Badge:** $X = [0.000, 2.540]\text{ mm}$, $Y = [19.800, 108.700]\text{ mm}$, fill `#5d5d5d`.

### 4.2. Horizontal Column Grid
- **Columns of Knobs / CV Matrix Jacks (pitch $11.50\text{ mm}$ per effect):**
  - Effect 1 (Morton, Avalanche, Galois): $X = [11.530, 23.030, 34.530, 46.030]\text{ mm}$ (Center $28.780\text{ mm}$)
  - Effect 2 (Reverse, Permute, Automata): $X = [64.030, 75.530, 87.030, 98.530]\text{ mm}$ (Center $81.280\text{ mm}$)
  - Effect 3 (Transpose, Gray, Hamming): $X = [116.530, 128.030, 139.530, 151.030]\text{ mm}$ (Center $133.780\text{ mm}$)
- **Inter-Effect Clearance Gap:** $18.000\text{ mm}$.

### 4.3. Vertical Stacking Architecture
- **Zone 1: Title & Version**
  - Title `bitterroot`: Baseline $Y = 7.620\text{ mm}$, centered $X = 81.280\text{ mm}$, `Node.otf`, scale $0.0048$, fill `#ffffff`.
  - Version `v1.1.0`: Baseline $Y = 10.414\text{ mm}$, centered $X = 81.280\text{ mm}$, `Quicksand.ttf`, scale $0.0016$, fill `#aaaaaa`.
- **Zone 2: Effect Knob Rows (Full Size Knobs)**
  - **Row 1 (Morton, Reverse, Transpose):**
    - Headers: Baseline $Y = 13.800\text{ mm}$
    - Subheaders: Baseline $Y = 16.500\text{ mm}$
    - Knobs Center: $Y = 23.500\text{ mm}$
  - **Row 2 (Avalanche, Permute, Gray):**
    - Headers: Baseline $Y = 32.800\text{ mm}$
    - Subheaders: Baseline $Y = 35.500\text{ mm}$
    - Knobs Center: $Y = 42.500\text{ mm}$
  - **Row 3 (Galois, Automata, Hamming):**
    - Headers: Baseline $Y = 51.800\text{ mm}$
    - Subheaders: Baseline $Y = 54.500\text{ mm}$
    - Knobs Center: $Y = 61.500\text{ mm}$
- **Delineator Line 1:** $Y = 70.000\text{ mm}$
- **Zone 3: Master Controls & Master Jacks (Center $Y = 79.500\text{ mm}$, Labels $Y = 72.800\text{ mm}$)**
  - Controls: `SCAN X` ($12.00\text{ mm}$), `SCAN Y` ($24.50\text{ mm}$), `ROUTE` ($37.00\text{ mm}$), `GRID` 3x3 LEDs ($49.50\text{ mm}$), `Z BLANK` ($64.00\text{ mm}$)
  - Jacks: `IN X` ($80.00\text{ mm}$), `IN Y` ($90.00\text{ mm}$), `SCAN X` CV ($103.00\text{ mm}$), `SCAN Y` CV ($113.00\text{ mm}$), `Z BLK` CV ($123.00\text{ mm}$), `OUT X` ($136.00\text{ mm}$), `OUT Y` ($146.00\text{ mm}$), `OUT Z` ($156.00\text{ mm}$)
- **Delineator Line 2:** $Y = 88.500\text{ mm}$
- **Zone 4: 12 $\times$ 3 CV Depth Jack Matrix**
  - **CV Row 1 (Morton, Reverse, Transpose):** Center $Y = 98.000\text{ mm}$, Labels $Y = 93.800\text{ mm}$
  - **CV Row 2 (Avalanche, Permute, Gray):** Center $Y = 108.000\text{ mm}$, Labels $Y = 103.800\text{ mm}$
  - **CV Row 3 (Galois, Automata, Hamming):** Center $Y = 118.000\text{ mm}$ (Fixed Invariant), Labels $Y = 113.800\text{ mm}$

---

## 5. Parameter & Port Tooltip Standards

- **Sentence Case Standard:** All tooltips capitalize only the first word (e.g. `Scan X focus`, `Wolfram rule`, `Galois multiplier alpha`).
- **CV Depth Standard:** All CV input tooltips end with `CV depth` (e.g. `Morton shift CV depth`, `Morton mix CV depth`, `Z blanking CV depth`).
- **Mix Knobs:** Range $0.0 \dots 1.0$, default $0.0$ ($0\%$), displayed with `%` formatting.
- **Scan X & Scan Y Knobs:** Range $0.0 \dots 1.0$, default $0.5$ ($50.0\%$), displayed with `%` formatting where $50\%$ represents center origin.
