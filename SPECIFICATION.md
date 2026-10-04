# Technical Specification: Eunice
**Module Name:** Eunice  
**Brand / Author:** Garmire  
**Version:** v2.26.0  
**Tags:** Sample and hold, Random, Polyphonic, Utility  
**Physical Form Factor:** Eurorack 3U, 8 HP ($40.64\text{ mm} \times 128.50\text{ mm}$), 3-Column Layout  
**Target Environments:** VCV Rack v2, 4ms MetaModule Hardware Plugin SDK  

---

## 1. Overview & Architectural Philosophy

**Eunice** is an organic, dual-channel Sample & Hold (S&H) and Track & Hold (T&H) voltage processor and complex generative modulation source. It pairs an internal noise-jittered triangle generator with clock-tracking filtering, non-linear voltage distribution shaping (tilt), Buchla 266 / Doepfer A-149-3 inspired correlation feedback, and an assignable asymmetrical slew limiter (independent Rise and Fall).

Eunice is fully polyphonic (up to 16 channels) with independent generative noise seeds across voices, level-gated tracking, edge-triggered sampling, and comprehensive visual monitoring through discrete 2mm LEDs.

---

## 2. Front Panel Layout & Mechanical Coordinates

### 2.1 Dimensions & Columns
- **Width:** 8 HP = $40.64\text{ mm}$ ($1.60\text{ in}$) / 160 px (VCV Rack 15px/HP: 120 px; standard 20px/HP: 160 px; standard 4ms MetaModule bitmap width: $76\text{ px}$).
- **Height:** 3U = $128.50\text{ mm}$ ($5.059\text{ in}$) / $240\text{ px}$ MetaModule bitmap height.
- **Horizontal Columns:**
  - **Column 1 (Left / A):** $x = 8.13\text{ mm}$ ($0.320\text{ in}$)
  - **Column 2 (Center / B):** $x = 20.32\text{ mm}$ ($0.800\text{ in}$)
  - **Column 3 (Right / C):** $x = 32.51\text{ mm}$ ($1.280\text{ in}$)
- **Left Color Badge:**
  - Standard Eurorack unassigned placeholder: $x = 0.00\text{ mm}$, $w = 2.54\text{ mm}$ (0.10 in), $y = 19.80\text{ mm}$ to $108.70\text{ mm}$ ($h = 88.90\text{ mm}$), fill `#5d5d5d`.

### 2.2 Component Positioning & Row Map
| Row | Y Center (mm) | Type | Col 1 ($x=8.13$) | Col 2 ($x=20.32$) | Col 3 ($x=32.51$) | Notes |
|:---|:---|:---|:---|:---|:---|:---|
| **Top** | $7.62$ / $10.41$ | Text | Title `eunice` | Centered at $x=20.32$ | Version tag `v2.26.0` |
| **Row 1** | $21.59$ | Knob | `Rate` | `Distribution` | `Correlation` | Large Knobs ($10.0\text{ mm}$ dia) |
| **Row 2** | $40.00$ | Switch/Knob | `Slew Dest` (3-Pos Switch) | `Slew Rise` | `Slew Fall` | Large Knobs / Toggle |
| **Row 3** | $56.00$ | Trimpot | `Rate CV Atten` | `Dist CV Atten` | `Corr CV Atten` | 6.5mm Trimpots |
| **Row 4** | $68.00$ | Trimpot | `Rise CV Atten` | `Fall CV Atten` | *(Reserved/Blank)* | 6.5mm Trimpots |
| **Row 5** | $89.50$ | Jack | `Signal In` | `Distribution CV` | `Correlation CV` | Input Ports (3.5mm PJ301M) |
| **Row 6** | $99.00$ | Jack | `Rate CV` | `External Gate/Clock` | `S&H Out` | Input & S&H Output |
| **Row 7** | $108.50$ | Jack | `Slew Rise CV` | `Slew Fall CV` | `T&H Out` | Input & T&H Output |

### 2.3 LED Indicator Positions (2mm)
1. **Clock LED (Red 2mm):** Located between Row 1 & Row 3 near Rate control ($x = 8.13\text{ mm}$, $y = 31.00\text{ mm}$).
2. **Gate Input LED (Red 2mm):** Adjacent to External Gate/Clock jack ($x = 20.32\text{ mm}$, $y = 94.00\text{ mm}$).
3. **Signal LED (Bipolar Green/Red 2mm):** Adjacent to Signal In jack ($x = 8.13\text{ mm}$, $y = 84.50\text{ mm}$).
4. **S&H Out LED (Bipolar Green/Red 2mm):** Adjacent to S&H Out jack ($x = 32.51\text{ mm}$, $y = 94.00\text{ mm}$).
5. **T&H Out LED (Bipolar Green/Red 2mm):** Adjacent to T&H Out jack ($x = 32.51\text{ mm}$, $y = 103.50\text{ mm}$).

---

## 3. Signal Flow & Functional Architecture

```
                       [Polyphonic Channel c: 0..N-1]
                                     │
      ┌──────────────────────────────┴──────────────────────────────┐
      │                                                             │
      ▼ (Unpatched Normalization)                                   │
[Internal Generator Core]                                           │
- Base ~100Hz Triangle                                              │
- Pink/White Noise Jitter                                           │
- Clock-Tracking Lowpass Filter (fc = 2 * f_clock)                  │
      │                                                             │
      ▼                                                             │
[Signal In Jack (c)] ◄──────────────────────────────────────────────┘
      │
      ▼
[Stage 1: Distribution Tilt Engine]
  - Parameter: D = clamp(Dist_Knob + Atten * Dist_CV, 0, 1)
  - Right-Click Modes:
    1. Power-Law / Gamma Skew (Default)
    2. Sigmoidal / Tanh DC Bias
    3. Diode / Half-Wave Saturation Tilt
      │
      ▼ V_dist
[Stage 2: Buchla 266 / Doepfer A-149-3 Correlation Crossfader]
  - Parameter: C = clamp(Corr_Knob + Atten * Corr_CV, 0, 1)
  - V_in_eff = (1 - C) * V_dist + C * V_held_S&H[c]
      │
      ├───────────────────────────────────────────┐
      │                                           │
      ▼                                           ▼
[Stage 3A: Sample & Hold]                   [Stage 3B: Track & Hold]
- Edge-triggered on Clock Rising Edge       - Level-gated by Clock HIGH/LOW
- Updates V_held_S&H[c] = V_in_eff          - If HIGH: tracks V_in_eff
- Holds constant between triggers           - If LOW: holds frozen level
      │                                           │
      ▼ V_raw_SH                                  ▼ V_raw_TH
      └─────────────────────┬─────────────────────┘
                            │
               [Stage 4: Asymmetrical Slew Engine]
               - Parameters: Rise Time (0.5ms - 10s), Fall Time (0.5ms - 10s)
               - CV Modulated with Exponential Response
               - Modes: Linear Ramp (Default) vs Exponential RC (Context Menu)
               - Destination Switch:
                 * UP:   S&H slewed, T&H raw
                 * MID:  Both slewed
                 * DOWN: T&H slewed, S&H raw
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
        [S&H Out (c)]               [T&H Out (c)]
        (Bipolar LED)               (Bipolar LED)
```

---

## 4. Detailed Mathematical & Algorithmic Specifications

### 4.1 Internal Clock & External Gate Logic
- **Rate Range:** $f_{\text{clock}} \in [0.05\text{ Hz}, 2000.0\text{ Hz}]$, mapped exponentially from Rate Knob ($k \in [0, 1]$) and Rate CV ($V_{\text{cv}} \in [-5\text{V}, +5\text{V}]$):
  $$f_{\text{clock}} = 0.05 \times 2^{k \cdot 15.2877 + V_{\text{cv}} \cdot \text{atten}}$$
- **Duty Cycle:** Internal clock runs at a fixed $50\%$ duty cycle square wave.
- **Clock Normalization:** If `External Gate/Clock` input is unpatched, internal clock drives the sampling/tracking engine. If patched, the external signal overrides the clock source.
- **Detection:**
  - S&H acquisition triggers when clock state transitions from LOW to HIGH ($V_{\text{clock}} > 1.7\text{V}$ with Schmitt trigger hysteresis: low threshold $0.8\text{V}$, high threshold $2.0\text{V}$).
  - T&H tracks when $V_{\text{clock}} \ge 1.7\text{V}$ and holds when $V_{\text{clock}} < 0.8\text{V}$.

### 4.2 Internal Generator & Clock-Tracking Lowpass
- **Base Triangle:** $f_{\text{tri}} = 100.0\text{ Hz}$.
- **Noise Jitter:** Continuous uniformly distributed pseudo-random noise perturbing phase/frequency per polyphonic channel:
  $$\phi_{n+1} = \left(\phi_n + \frac{f_{\text{tri}}}{f_s} + \eta_n \cdot 0.02\right) \pmod{1.0}$$
  where $\eta_n \in [-1.0, 1.0]$ is independent white noise.
- **Triangle Signal:** $V_{\text{tri}} = 10.0 \cdot (2.0 \cdot |\phi - 0.5| - 0.5) \in [-5\text{V}, +5\text{V}]$.
- **Clock-Tracking Lowpass Filter:** 1-pole lowpass ($6\text{ dB/oct}$) with cutoff:
  $$f_c = \text{clamp}(2.0 \cdot f_{\text{clock}}, 5.0\text{ Hz}, 16000.0\text{ Hz})$$
  $$\alpha = 1.0 - \exp\left(-\frac{2\pi f_c}{f_s}\right)$$
  $$y_n = y_{n-1} + \alpha (V_{\text{tri}} - y_{n-1})$$

### 4.3 Distribution Tilt Modes ($D \in [0, 1]$)
Let $u = \text{clamp}(V_{\text{in}} / 5.0, -1.0, 1.0)$:
1. **Mode 0: Power-Law / Gamma Skew (Default):**
   - Let unipolar $p = 0.5 \cdot (u + 1.0) \in [0, 1]$.
   - Let $\gamma = 2^{4.0 \cdot (0.5 - D)} \in [0.25, 4.0]$.
   - $p_{\text{skew}} = p^\gamma$.
   - $V_{\text{dist}} = 5.0 \cdot (2.0 \cdot p_{\text{skew}} - 1.0)$.
2. **Mode 1: Sigmoidal / Tanh DC Bias:**
   - Offset: $\Delta = 3.0 \cdot (2.0 \cdot D - 1.0)$.
   - $V_{\text{dist}} = 5.0 \cdot \tanh\left(\frac{V_{\text{in}} + \Delta}{5.0}\right) \cdot \frac{1}{\tanh(1.0 + |\Delta|/5.0)}$.
3. **Mode 2: Diode / Half-Wave Saturation:**
   - When $D > 0.5$: Asymmetrically compresses negative swings while keeping positive swings open:
     $$k_{\text{neg}} = 1.0 - 1.8 \cdot (D - 0.5)$$
     $$V_{\text{dist}} = \begin{cases} V_{\text{in}} & \text{if } V_{\text{in}} \ge 0 \\ V_{\text{in}} \cdot k_{\text{neg}} & \text{if } V_{\text{in}} < 0 \end{cases}$$
   - When $D < 0.5$: Asymmetrically compresses positive swings while keeping negative swings open.

### 4.4 Correlation Feedback Engine ($C \in [0, 1]$)
$$V_{\text{in\_effective}}[c] = (1.0 - C) \cdot V_{\text{dist}}[c] + C \cdot V_{\text{held\_S\&H}}[c]$$
- At $C = 0.0$: Independent random / incoming signal steps.
- At $C = 1.0$: Output freezes on current held voltage ($0\text{V}$ delta).
- At $0.0 < C < 1.0$: Bounded Brownian drift / random walk.

### 4.5 Asymmetrical Slew Limiter
- **Time Range:** $T \in [0.0005\text{ s}, 10.0\text{ s}]$, exponential mapping:
  $$T_{\text{rise}} = 0.0005 \cdot \left(\frac{10.0}{0.0005}\right)^{\text{clamp}(k_{\text{rise}} + \text{atten} \cdot V_{\text{cv\_rise}} / 5.0, 0, 1)}$$
  $$T_{\text{fall}} = 0.0005 \cdot \left(\frac{10.0}{0.0005}\right)^{\text{clamp}(k_{\text{fall}} + \text{atten} \cdot V_{\text{cv\_fall}} / 5.0, 0, 1)}$$
- **Linear Slew (Default):**
  - Maximum delta per sample: $\Delta_{\text{max\_rise}} = \frac{10.0\text{V}}{T_{\text{rise}} \cdot f_s}$, $\Delta_{\text{max\_fall}} = \frac{10.0\text{V}}{T_{\text{fall}} \cdot f_s}$.
  - Target delta: $\Delta = V_{\text{target}} - V_{\text{current}}$.
  - If $\Delta > 0$: $V_{\text{current}} += \min(\Delta, \Delta_{\text{max\_rise}})$.
  - If $\Delta < 0$: $V_{\text{current}} -= \min(-\Delta, \Delta_{\text{max\_fall}})$.
- **Exponential RC Slew (Context Menu):**
  - Filter coefficient: $\alpha = 1.0 - \exp\left(-\frac{1.0}{T \cdot f_s}\right)$.
  - Applied with $\alpha_{\text{rise}}$ when $V_{\text{target}} > V_{\text{current}}$, and $\alpha_{\text{fall}}$ when $V_{\text{target}} < V_{\text{current}}$.

---

## 5. Right-Click Context Menu & JSON Persistence

1. **Distribution Tilt Mode:**
   - `Power-Law Skew (Default)`
   - `Sigmoid Bias`
   - `Diode Saturation`
2. **Slew Profile:**
   - `Linear (Default)`
   - `Exponential RC`
3. **JSON Keys:**
   - `"distributionMode"`: `0`, `1`, or `2`
   - `"slewProfile"`: `0` (Linear) or `1` (Exponential)

---

## 6. Verification & Test Criteria

1. **DSP Unit Tests:**
   - Real-time safety: zero heap allocation (`malloc`/`new`), zero mutexes, zero file access.
   - Polyphonic channel count tracking up to 16 voices.
   - Slew limiting step response validation.
   - Correlation crossfade linearity & boundary conditions ($C=0, C=1$).
   - Distribution warping monotonicity and $[-5\text{V}, +5\text{V}]$ range bounding.
2. **VCV Rack & 4ms MetaModule Compilation:**
   - Cross-compilation with `arm-none-eabi-gcc` against MetaModule SDK.
   - Zero `<text>` tags in SVG panel.
