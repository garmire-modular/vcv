# Bitterroot: Formal Engineering Specification

**Document Version:** 1.3.0  
**Module Version:** 1.0.0  
**Slug:** `Bitterroot`  
**Panel Title:** `bitterroot` (Font: `res/Node.otf`, lowercase, baseline $Y = 7.620\text{ mm}$, centered $X = 71.120\text{ mm}$)  
**Format:** 28 HP Eurorack ($142.240\text{ mm}$ width, $128.500\text{ mm}$ height, $266 \times 240\text{ px}$ MetaModule bitmap)  

---

## 1. Module Overview & Functional Concept

**Bitterroot** is a 28 HP 9-block modular vector synthesis workstation, 10-bit digital topological processor, and volumetric $Z$-axis laser beam intensity engine. Designed for high-speed laser galvo projection, oscilloscope art, and wide-bandwidth vector graphics, it transforms incoming continuous $X$ and $Y$ coordinate signals into a discrete 10-bit integer grid $[0, 1023] \times [0, 1023]$. 

Within this discrete coordinate space, bitwise, dyadic, and algebraic transformations fracture continuous Euclidean trajectories into self-similar fractal dust, Walsh reflections, crystalline lattice planes, and 4D hypercube projections while strictly preserving display boundaries.

Crucially, Bitterroot includes a dedicated **$Z$-Axis Beam Intensity & Topological Blanking Engine**. Because discrete bitwise operations cause instantaneous coordinate jumps across the screen, physical galvo mirrors would ordinarily produce harsh retrace streaks and scanner heat. Bitterroot computes instantaneous velocity, recursion depth, and binary transition energy to dynamically modulate and output a dedicated **`OUT Z`** signal ($0\text{--}5\text{V}$ or $0\text{--}10\text{V}$). This turns chaotic scribbles into crystalline laser constellations, multi-planar diffraction gratings, and holographic depth sculptures.

The module features a **$3 \times 3$ grid of nine simultaneous topological transformation blocks**:
- **Row 1:**
  - **Block A (Morton):** Z-Order Space-Filling Curves & Hilbert Fractal Morph (`SHIFT`, `STRIDE`, `HILBERT`)
  - **Block B (Reverse):** Bitwise Dyadic Reflection & Asymmetric Axis Skew (`WIDTH`, `OFFSET`, `SKEW`)
  - **Block C (Transpose):** Bit-Plane Transposition & 3-Way Bit Permutation Cycle (`PLANE A`, `PLANE B`, `CYCLE C`)
- **Row 2:**
  - **Block D (Avalanche):** Carry-Propagated Cross-Modulation & Dual-Direction Borrow/Carry (`MASK`, `SHIFT`, `BORROW`)
  - **Block E (Permute):** 20-bit Circular Permutation Matrix & Coprime Stride Hopping (`MODE`, `ROTATE`, `STRIDE`)
  - **Block F (Gray):** Reflected Binary Gray Code Dyadic Folding & Multi-Order Tap Distance (`DEPTH`, `MODE`, `TAP`)
- **Row 3:**
  - **Block G (Galois):** Galois Field $\text{GF}(2^{10})$ Polynomial Scramble & Non-Linear Inversion (`POLY`, `ALPHA`, `POWER`)
  - **Block H (Automata):** 1D Elementary Cellular Automata Mesh & Cross-Axis Seed Injection (`RULE`, `STEPS`, `INJECT`)
  - **Block I (Hamming):** Popcount & Mutual Hamming Distance Cross-Coupling (`GAIN`, `MODE`, `MUTUAL`)

All 27 effect controls use **full-sized knobs (`RoundBlackKnob`)** organized into three distinct, cleanly separated columns with dedicated parameter headers and subheaders. The patch bay accommodates **35 jacks**: 27 per-block CV inputs (each with explicit parameter tooltips and faceplate engraving) and 8 master I/O jacks (`IN X`, `IN Y`, `SCAN X CV`, `SCAN Y CV`, `ROUTE CV`, `OUT X`, `OUT Y`, `OUT Z`).

The transformation blocks can be processed as an end-to-end **Cascaded Serial Pipeline** ($A \to B \to \dots \to I$) or modulated via a **2D Matrix Crossfade Scanner** (where master Scan X and Scan Y coordinates focus and interpolate the beam across the $3 \times 3$ cell plane).

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

- **`OUT Z` Intensity Reconstruction:**
  - Unipolar $0.0\text{V} \dots +5.0\text{V}$ (or $0.0\text{V} \dots +10.0\text{V}$ when in Unipolar mode).
  - $0.0\text{V} = \text{Full Beam Blanking (Laser OFF)}$, Maximum Voltage $= \text{Full Intensity (Laser ON)}$.

- **Boundary Invariant:** Inputs exceeding the specified electrical limits are hard-clamped to $[0, 1023]$ prior to entering the transformation pipeline, ensuring signals never wrap out of display bounds unless explicitly commanded by an active transformation.

### 2.2. Control Voltages (CV)
- **Parameter Modulation CVs:** $0\text{V} \dots +5\text{V}$ (or $\pm 5\text{V}$ bipolar modulation). $1\text{V}$ equals $20\%$ full-scale sweep ($0.2 \times \text{range}$).
- **Scan X / Scan Y CVs:** $\pm 5.0\text{V}$ normalized across the $3 \times 3$ grid coordinates.
- **Route CV:** Schmitt trigger input ($V_{\text{high}} = +1.7\text{V}$, $V_{\text{low}} = +0.8\text{V}$) switching between Cascade Serial and Matrix Scan modes.

---

## 3. Mathematical Specifications: The 9 Topological Transformations

All operations receive integer coordinates $(X, Y) \in [0, 1023]^2$, compute transformed coordinates $(X_{\text{out}}, Y_{\text{out}}) \in [0, 1023]^2$, and generate a normalized block $Z$-intensity factor $Z_k \in [0.0, 1.0]$.

### 3.1. Block A — Morton Order & Space-Filling Curves
- **Param 1 (Shift / Rotation $k$):** Integer bit rotation $k \in [0, 19]$ applied to the 20-bit Morton key.
- **Param 2 (Interleave Stride / Mode $m$):** Interleaving stride mode ($0 = \text{Standard Morton}$, $1 = \text{Inverted Axis Priority}$, $2 = \text{2-Bit Block Interleave}$).
- **Param 3 (Hilbert Quadrant Inversion / Morph $H$):** Continuous morph factor $H \in [0.0, 1.0]$ applying Gray-coded quadrant reflection to convert discontinuous Z-order curves into continuous Hilbert/Peano trajectories.
- **$Z$-Intensity Output:** Proportional to instantaneous quadrant leap distance:
  $$Z_A = 1.0 - \text{clamp}\left(\frac{|\Delta X| + |\Delta Y|}{512.0}, 0.0, 1.0\right)$$
  Blanks retrace streaks during inter-quadrant hops, rendering sharp fractal dust.

### 3.2. Block B — Bitwise Reversal (Dyadic Reflection)
- **Param 1 (Reversal Width $W$):** Number of bits reversed $W \in [1, 10]$ (from LSB upward).
- **Param 2 (Dyadic Offset / Mask $S$):** Pre-reversal bitwise XOR mask $S \in [0, 1023]$ (phase translation in Walsh space).
- **Param 3 (Asymmetric Axis Skew $\Delta W$):** Differential reversal width skew $\Delta W \in [-5, +5]$ between $X$ and $Y$.
- **$Z$-Intensity Output:** Slew-rate blanking: dims when low-order bits trigger macro jumps across the display.

### 3.3. Block C — Bit-Plane Transposition
- **Param 1 (Plane Index A $p_a$):** Bit index $p_a \in [0, 9]$ exchanged between $X$ and $Y$.
- **Param 2 (Plane Index B $p_b$):** Second bit index $p_b \in [0, 9]$ exchanged between $X$ and $Y$.
- **Param 3 (3-Way Permutation Cycle $p_c$):** Third plane index $p_c \in [0, 9]$ creating a cyclic 3-way permutation $X[p_a] \to Y[p_b] \to X[p_c] \to X[p_a]$.
- **$Z$-Intensity Output:** Luminance indexed by the state of swapped planes, yielding layered 3D diffraction planes.

### 3.4. Block D — Carry-Propagated Cross-Modulation (Shear Wrapping)
- **Param 1 (Carry Mask $M$):** 10-bit bitmask $M \in [0, 1023]$ gating carry propagation.
- **Param 2 (Shear Cascade Shift $k$):** Shift amount $k \in [0, 4]$ of avalanche injection.
- **Param 3 (Borrow / Dual-Direction Polarity $B$):** Interpolates between additive carry ($+C$) and borrow subtraction ($-C \pmod{1024}$).
- **$Z$-Intensity Output:** Illuminates tear boundaries proportionally to instantaneous carry magnitude $C = (X \;\&\; Y \;\&\; M) \ll 1$.

### 3.5. Block E — Circular Permutation Matrix
- **Param 1 (Permutation Mode $P$):** Mode selector ($0 = \text{Odd/Even Split}$, $1 = \text{Perfect Shuffle}$, $2 = \text{Bit Inversion Shuffling}$, $3 = \text{Bit Quadrant Rotation}$).
- **Param 2 (Register Rotation $R$):** 20-bit circular shift $R \in [0, 19]$ applied to the joint register $[X_{9..0}, Y_{9..0}]$.
- **Param 3 (Coprime Stride Hopping $S$):** Stride step $S \in \{1, 3, 5, 7, 9, 11\}$ modulo 20.
- **$Z$-Intensity Output:** Uses middle bits $U[14..5]$ as virtual 3D/4D depth for perspective shading.

### 3.6. Block F — Gray Code Dyadic Folding
- **Param 1 (Bit Depth $N$):** Active Gray code depth $N \in [1, 10]$ bits.
- **Param 2 (Direction / Mode $M$):** $0 = \text{Binary to Gray}$, $1 = \text{Gray to Binary}$, $2 = \text{Dual Reflected Cascade}$.
- **Param 3 (Tap Distance Order $k$):** XOR tap distance $k \in [1, 9]$ ($G_k = X \oplus (X \gg k)$).
- **$Z$-Intensity Output:** Zebra parity grading across symmetry fold axes.

### 3.7. Block G — Galois Field $\text{GF}(2^{10})$ Polynomial Scramble
- **Param 1 (Irreducible Polynomial Tap $P$):** Selects irreducible polynomials over $\text{GF}(2^{10})$ (e.g., $x^{10} + x^3 + 1 \implies \text{0x409}$, $x^{10} + x^7 + 1 \implies \text{0x481}$, $x^{10} + x^9 + x^4 + 1$).
- **Param 2 (Multiplier Element $\alpha$):** Field element multiplier $\alpha \in [1, 1023]$.
- **Param 3 (Non-Linear Inversion / Power $\beta$):** Exponent mode ($0 = \text{Linear Multiplication}$, $1 = \text{Multiplicative Inversion } X^{-1}$, $2 = \text{Cube } X^3$, $3 = \text{S-Box Quintic } X^5$).
- **$Z$-Intensity Output:** Inter-point hop blanking, rendering starry point clouds.

### 3.8. Block H — 1D Elementary Cellular Automata Mesh
- **Param 1 (Wolfram Rule $R$):** Rule number $R \in [0, 255]$ (e.g. Rule 90, Rule 30, Rule 110, Rule 150).
- **Param 2 (Iteration Steps $S$):** Number of CA steps $S \in [1, 4]$.
- **Param 3 (Cross-Axis Seed Injection $J$):** Coupling mode ($0 = \text{Edge Seeding}$, $1 = \text{Center-Cell Seeding}$, $2 = \text{Full XOR Superposition}$).
- **$Z$-Intensity Output:** Directly mapped to binary state of active CA cells, blanking dead cells.

### 3.9. Block I — Popcount / Hamming Dispersion
- **Param 1 (Weight Gain / Displacement $K$):** Displacement factor $K \in [0, 32]$.
- **Param 2 (Parity Mode $M$):** Parity check routing ($0 = \text{Even/Odd Sign Flip}$, $1 = \text{Hamming Shear}$, $2 = \text{Orthogonal Jump}$).
- **Param 3 (Mutual Hamming Distance Coupling $D$):** Modulates scale and angle using mutual distance $d_H(X, Y) = \text{popcount}(X \oplus Y)$.
- **$Z$-Intensity Output:** Topographic energy map indexed by joint bit population.

---

## 4. Signal Routing Architecture & Master Section

```
                       +-----------------------------------+
                       | IN X / IN Y (10-bit ADC [0,1023]) |
                       +-----------------+-----------------+
                                         |
                       +-----------------v-----------------+
                       |       ROUTE TOPOLOGY SELECT       |
                       +--------+-----------------+--------+
                                |                 |
            [MODE 0: CASCADED SERIAL]         [MODE 1: MATRIX SCAN]
                                |                 |
               +----------------v-----+      +----v--------------------+
               | Row 1: Block A->B->C |      | 2D Spatial Crossfade    |
               +----------------+-----+      | Interpolation           |
               | Row 2: Block D->E->F |      | Scan X, Scan Y (2D)     |
               +----------------+-----+      | Weights W_A ... W_I     |
               | Row 3: Block G->H->I |      | Sum(W_k * Wet_k)        |
               +----------------+-----+      +----+--------------------+
                                |                 |
                                +--------+--------+
                                         |
                               +---------v----------+
                               | Master Slew Filter |
                               +---------+----------+
                               | 10-bit DAC Reconst |
                               +---------+----------+
                                         |
                               +---------v-------------------------+
                               | OUT X / OUT Y / OUT Z (Intensity) |
                               +-----------------------------------+
```

---

## 5. Front Panel Mechanical Layout & Coordinates (28 HP / $142.240\text{ mm}$)

### 5.1. Coordinate Constants & Margins
- **Panel Width:** $142.240\text{ mm}$ (28 HP)
- **Panel Height:** $128.500\text{ mm}$ (3U)
- **Background Fill:** `#6e6e6e`
- **Palette Badge:** $X = [0.000, 2.540]\text{ mm}$, $Y = [19.800, 108.700]\text{ mm}$, fill `#5d5d5d`.

### 5.2. Horizontal Column Grid
- **Block Columns (Center of Each 3-Knob Cell, Full-Size Knobs):**
  - Column 1 (Blocks A, D, G): Center $X = 26.000\text{ mm}$
    - Knobs: $X_{\text{P1}} = 13.000\text{ mm}$, $X_{\text{P2}} = 26.000\text{ mm}$, $X_{\text{P3}} = 39.000\text{ mm}$ (pitch $13.0\text{ mm}$)
  - Column 2 (Blocks B, E, H): Center $X = 71.120\text{ mm}$
    - Knobs: $X_{\text{P1}} = 58.120\text{ mm}$, $X_{\text{P2}} = 71.120\text{ mm}$, $X_{\text{P3}} = 84.120\text{ mm}$ (pitch $13.0\text{ mm}$)
  - Column 3 (Blocks C, F, I): Center $X = 116.240\text{ mm}$
    - Knobs: $X_{\text{P1}} = 103.240\text{ mm}$, $X_{\text{P2}} = 116.240\text{ mm}$, $X_{\text{P3}} = 129.240\text{ mm}$ (pitch $13.0\text{ mm}$)
  - Inter-Block Column Clearance Gap: $19.120\text{ mm}$ (clean separation between blocks).

### 5.3. Vertical Stacking Architecture
- **Zone 1: Title & Version**
  - Title `bitterroot`: Baseline $Y = 7.620\text{ mm}$, centered $X = 71.120\text{ mm}$, `Node.otf`, scale $0.0048$, fill `#ffffff`.
  - Version `v1.0.0`: Baseline $Y = 10.414\text{ mm}$, centered $X = 71.120\text{ mm}$, `Quicksand.ttf`, scale $0.0016$, fill `#aaaaaa`.
- **Zone 2: Transformation Blocks (Rows 1, 2, 3 - Full Size Knobs)**
  - **Row 1 (Blocks A, B, C):**
    - Block Title Baseline: $Y = 13.800\text{ mm}$ (`MORTON`, `REVERSE`, `TRANSPOSE`)
    - Knob Labels Baseline: $Y = 16.500\text{ mm}$
    - Knob Center: $Y = 23.500\text{ mm}$
  - **Row 2 (Blocks D, E, F):**
    - Block Title Baseline: $Y = 32.800\text{ mm}$ (`AVALANCHE`, `PERMUTE`, `GRAY`)
    - Knob Labels Baseline: $Y = 35.500\text{ mm}$
    - Knob Center: $Y = 42.500\text{ mm}$
  - **Row 3 (Blocks G, H, I):**
    - Block Title Baseline: $Y = 51.800\text{ mm}$ (`GALOIS`, `AUTOMATA`, `HAMMING`)
    - Knob Labels Baseline: $Y = 54.500\text{ mm}$
    - Knob Center: $Y = 61.500\text{ mm}$
- **Zone 3: Central Master Section & 3x3 Miniature Vector LED Matrix**
  - Delineator Line 1: $Y = 69.500\text{ mm}$
  - Master Labels: Baseline $Y = 72.000\text{ mm}$ (`SCAN X`, `SCAN Y`, `ROUTE`, `GRID`, `Z BLANK`)
  - Master Knobs Center: $Y = 77.500\text{ mm}$ (`SCAN X`: $20.0\text{ mm}$, `SCAN Y`: $36.0\text{ mm}$, `ROUTE`: $52.0\text{ mm}$, `Z BLANK`: $122.24\text{ mm}$)
  - **Central 3x3 Miniature Vector Activity LED Matrix:**
    - Centered at $X = 71.120\text{ mm}, Y = 77.500\text{ mm}$
    - Pitch $3.5\text{ mm}$ ($X \in \{67.62, 71.12, 74.62\}$, $Y \in \{74.5, 77.5, 80.5\}$).
- **Zone 4: Bottom I/O Patch Bay (35 Jacks, Bottom-Aligned Upward)**
  - Delineator Line 2: $Y = 83.500\text{ mm}$
  - **Jack Row 1 ($Y = 89.500\text{ mm}$):** 9 Jacks (Row 1 CVs, Labels at $Y = 85.30\text{ mm}$)
    - Block A ($X = 13.0, 26.0, 39.0\text{ mm}$): `SHFT`, `STRD`, `HLBT`
    - Block B ($X = 58.12, 71.12, 84.12\text{ mm}$): `WDTH`, `OFST`, `SKEW`
    - Block C ($X = 103.24, 116.24, 129.24\text{ mm}$): `PL A`, `PL B`, `CYCL`
  - **Jack Row 2 ($Y = 99.000\text{ mm}$):** 9 Jacks (Row 2 CVs, Labels at $Y = 94.80\text{ mm}$)
    - Block D ($X = 13.0, 26.0, 39.0\text{ mm}$): `MASK`, `SHFT`, `BORW`
    - Block E ($X = 58.12, 71.12, 84.12\text{ mm}$): `MODE`, `ROT`, `STRD`
    - Block F ($X = 103.24, 116.24, 129.24\text{ mm}$): `DPTH`, `MODE`, `TAP`
  - **Jack Row 3 ($Y = 108.500\text{ mm}$):** 9 Jacks (Row 3 CVs, Labels at $Y = 104.30\text{ mm}$)
    - Block G ($X = 13.0, 26.0, 39.0\text{ mm}$): `POLY`, `ALPH`, `POWR`
    - Block H ($X = 58.12, 71.12, 84.12\text{ mm}$): `RULE`, `STEP`, `INJ`
    - Block I ($X = 103.24, 116.24, 129.24\text{ mm}$): `GAIN`, `MODE`, `MUTL`
  - **Jack Row 4 ($Y = 118.000\text{ mm}$, Fixed Invariant):** 8 Master Jacks (Labels at $Y = 113.80\text{ mm}$)
    - `IN X` ($X = 13.000\text{ mm}$), `IN Y` ($X = 27.500\text{ mm}$)
    - `SCAN X CV` ($X = 47.000\text{ mm}$), `SCAN Y CV` ($X = 63.000\text{ mm}$), `ROUTE CV` ($X = 79.000\text{ mm}$)
    - `OUT X` ($X = 99.000\text{ mm}$), `OUT Y` ($X = 114.500\text{ mm}$), `OUT Z` ($X = 130.000\text{ mm}$)

---

## 6. Complete Parameter & Port Tooltip Specifications

All parameter controls and CV inputs deliver custom tooltip descriptions in VCV Rack.

### 6.1. Parameter Knobs
| Parameter ID | Physical Label | Range | Default | Unit / Tooltip Format |
| :--- | :--- | :--- | :--- | :--- |
| `BLOCK_A_P1` | `SHIFT` | $0 \dots 19$ | 0 | `Shift %d bits` |
| `BLOCK_A_P2` | `STRIDE` | $0 \dots 2$ | 0 | `Standard`, `Inverted`, `2-Bit Block` |
| `BLOCK_A_P3` | `HILBERT` | $0.0 \dots 1.0$ | 0.0 | `Hilbert Morph: %.1f%%` |
| `BLOCK_B_P1` | `WIDTH` | $1 \dots 10$ | 10 | `Reverse %d bits` |
| `BLOCK_B_P2` | `OFFSET` | $0 \dots 1023$ | 0 | `Mask: 0x%03X (%d)` |
| `BLOCK_B_P3` | `SKEW` | $-5 \dots +5$ | 0 | `Axis Skew: %+d bits` |
| `BLOCK_C_P1` | `PLANE A` | $0 \dots 9$ | 7 | `Swap Bit %d` |
| `BLOCK_C_P2` | `PLANE B` | $0 \dots 9$ | 3 | `Swap Bit %d` |
| `BLOCK_C_P3` | `CYCLE C` | $0 \dots 9$ | 5 | `Cycle Bit %d` |
| `BLOCK_D_P1` | `MASK` | $0 \dots 1023$ | 511 | `Carry Mask: 0x%03X` |
| `BLOCK_D_P2` | `SHIFT` | $0 \dots 4$ | 1 | `Cascade Shift %d` |
| `BLOCK_D_P3` | `BORROW` | $0.0 \dots 1.0$ | 0.0 | `Carry/Borrow: %.1f%%` |
| `BLOCK_E_P1` | `MODE` | $0 \dots 3$ | 0 | `Odd/Even`, `Shuffle`, `Inversion`, `Quadrant` |
| `BLOCK_E_P2` | `ROTATE` | $0 \dots 19$ | 0 | `Rotate %d steps` |
| `BLOCK_E_P3` | `STRIDE` | $0 \dots 5$ | 0 | `Stride: %d` (1, 3, 5, 7, 9, 11) |
| `BLOCK_F_P1` | `DEPTH` | $1 \dots 10$ | 10 | `Gray Depth %d bits` |
| `BLOCK_F_P2` | `MODE` | $0 \dots 2$ | 0 | `Binary to Gray`, `Gray to Binary`, `Dual Reflected` |
| `BLOCK_F_P3` | `TAP` | $1 \dots 9$ | 1 | `XOR Tap Dist %d` |
| `BLOCK_G_P1` | `POLY` | $0 \dots 7$ | 0 | `Poly 0x%03X` (Irreducible Table) |
| `BLOCK_G_P2` | `ALPHA` | $1 \dots 1023$ | 3 | `Alpha: 0x%03X (%d)` |
| `BLOCK_G_P3` | `POWER` | $0 \dots 3$ | 0 | `Linear`, `Inversion`, `Cube`, `S-Box Quintic` |
| `BLOCK_H_P1` | `RULE` | $0 \dots 255$ | 90 | `Rule %d (0x%02X)` |
| `BLOCK_H_P2` | `STEPS` | $1 \dots 4$ | 1 | `%d CA Steps` |
| `BLOCK_H_P3` | `INJECT` | $0 \dots 2$ | 0 | `Edge Seed`, `Center Seed`, `Full XOR` |
| `BLOCK_I_P1` | `GAIN` | $0 \dots 32$ | 8 | `Weight Gain: %d` |
| `BLOCK_I_P2` | `MODE` | $0 \dots 2$ | 0 | `Sign Flip`, `Shear`, `Jump` |
| `BLOCK_I_P3` | `MUTUAL` | $0.0 \dots 1.0$ | 0.0 | `Mutual Dist: %.1f%%` |
| `SCAN_X_PARAM` | `SCAN X` | $-1.0 \dots +1.0$ | 0.0 | `X: %+.2f` |
| `SCAN_Y_PARAM` | `SCAN Y` | $-1.0 \dots +1.0$ | 0.0 | `Y: %+.2f` |
| `ROUTE_PARAM` | `ROUTE` | $0 \dots 1$ | 0 | `Cascade Serial` / `Matrix Scan` |
| `Z_BLANK_PARAM` | `Z BLANK` | $0.0 \dots 1.0$ | 0.5 | `Blanking: %.1f%%` |

### 6.2. CV Inputs
- `BLOCK_A_P1_CV` .. `BLOCK_A_P3_CV`: `"Block A (Morton) Shift CV"`, `"Stride CV"`, `"Hilbert Morph CV"`
- `BLOCK_B_P1_CV` .. `BLOCK_B_P3_CV`: `"Block B (Reverse) Width CV"`, `"Offset Mask CV"`, `"Skew CV"`
- `BLOCK_C_P1_CV` .. `BLOCK_C_P3_CV`: `"Block C (Transpose) Plane A CV"`, `"Plane B CV"`, `"Cycle C CV"`
- `BLOCK_D_P1_CV` .. `BLOCK_D_P3_CV`: `"Block D (Avalanche) Carry Mask CV"`, `"Shift CV"`, `"Borrow CV"`
- `BLOCK_E_P1_CV` .. `BLOCK_E_P3_CV`: `"Block E (Permute) Mode CV"`, `"Rotate CV"`, `"Stride CV"`
- `BLOCK_F_P1_CV` .. `BLOCK_F_P3_CV`: `"Block F (Gray) Depth CV"`, `"Mode CV"`, `"Tap Distance CV"`
- `BLOCK_G_P1_CV` .. `BLOCK_G_P3_CV`: `"Block G (Galois) Poly CV"`, `"Alpha CV"`, `"Power Mode CV"`
- `BLOCK_H_P1_CV` .. `BLOCK_H_P3_CV`: `"Block H (Automata) Rule CV"`, `"Steps CV"`, `"Injection CV"`
- `BLOCK_I_P1_CV` .. `BLOCK_I_P3_CV`: `"Block I (Hamming) Gain CV"`, `"Mode CV"`, `"Mutual Dist CV"`
- `SCAN_X_CV_INPUT`: `"Scan X Coordinate CV"`
- `SCAN_Y_CV_INPUT`: `"Scan Y Coordinate CV"`
- `ROUTE_CV_INPUT`: `"Route Mode CV"`
