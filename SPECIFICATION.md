# Stepped Slew: Formal Engineering Specification

**Document Version:** 1.0.0  
**Target Release:** Garmire v2.26.0  
**Slug:** `SteppedSlew`  
**Panel Title:** `stepped slew` (Font: `res/Node.otf`, lowercase, baseline $Y = 7.620\text{ mm}$, centered $X = 30.480\text{ mm}$)  
**Format:** 12 HP Eurorack ($60.960\text{ mm}$ width, $128.500\text{ mm}$ height, $114 \times 240\text{ px}$ MetaModule bitmap)  

---

## 1. Module Overview & Functional Concept

**Stepped Slew** is an asymmetric dual-slope slew limiter with parameterized trajectory curvature morphing and discrete quantization stepping. Inspired by the classic discrete portamento feel of vintage synthesizers (e.g. CS-80), it extends the concept into arbitrary microtonal and mathematical step domains.

The module processes an incoming continuous or stepped control voltage (or audio signal) and applies independent slew times, curvature responses (logarithmic $\rightarrow$ linear $\rightarrow$ exponential), and step quantizations for rising (`UP`) and falling (`DOWN`) signal transitions.

Simultaneously, it outputs:
1. Continuous Slewed signal.
2. Slewed + Stepped signal (discretized across the trajectory).
3. Movement comparator gates (active while slewing Up, active while slewing Down).
4. End-of-Slew trigger pulses (EOU, EOD).
5. Step-Crossed trigger/gate pulses each time an internal step threshold is traversed.
6. Visual yellow LED activity indicators flashing at each step crossing.

---

## 2. Front Panel Layout & Mechanical Geometry (12 HP / $60.960\text{ mm}$)

### 2.1. Panel Coordinate System & Margins
- **Width**: $60.960\text{ mm}$ (12 HP)
- **Height**: $128.500\text{ mm}$ (3U)
- **Background Color**: `#6e6e6e`
- **Left Margin Badge**: $X = [0.000, 2.540]\text{ mm}$, $Y = [19.800, 108.700]\text{ mm}$, placeholder `#5d5d5d`.

### 2.2. Horizontal Column Grid
- **3-Control Columns (Knobs & Trimpots & CV Jacks)**:
  - Column 1 (`TIME`): $X_1 = 11.430\text{ mm}$
  - Column 2 (`SHAPE`): $X_2 = 30.480\text{ mm}$ (Center)
  - Column 3 (`STEPS`): $X_3 = 49.530\text{ mm}$
  - Horizontal spacing: $\Delta X = 19.050\text{ mm}$ ($0.75\text{ in}$, $> 11.4\text{ mm}$ knob clearance requirement).
- **4-Port Columns (Jack Rows 3 & 4)**:
  - Port Column 1: $X_{P1} = 9.144\text{ mm}$
  - Port Column 2: $X_{P2} = 23.368\text{ mm}$
  - Port Column 3: $X_{P3} = 37.592\text{ mm}$
  - Port Column 4: $X_{P4} = 51.816\text{ mm}$
  - Horizontal spacing: $\Delta X = 14.224\text{ mm}$ ($> 12.0\text{ mm}$ minimum Eurorack jack clearance, connector collars $> 3.0\text{ mm}$).

### 2.3. Vertical Stacking Architecture

#### Zone 1: Title & Version
- **Title**: `stepped slew`, $Y = 7.620\text{ mm}$, centered at $X = 30.480\text{ mm}$, `Node.otf`, scale $0.0048$, fill `#ffffff`.
- **Version**: `v2.26.0`, $Y = 10.414\text{ mm}$, centered at $X = 30.480\text{ mm}$, `Quicksand.ttf`, scale $0.0016$, fill `#aaaaaa`.

#### Zone 2: Primary Parameter Knobs
- **Row 1 Headers**: `TIME`, `SHAPE`, `STEPS` centered over respective columns at $Y = 13.070\text{ mm}$, `Quicksand-Medium.ttf`, scale $0.0024$, fill `#1c1c1c`.
- **Knob Row 1 (UP)**: Center $Y = 21.590\text{ mm}$
  - `TIME UP`: $X = 11.430\text{ mm}, Y = 21.590\text{ mm}$
  - `SHAPE UP`: $X = 30.480\text{ mm}, Y = 21.590\text{ mm}$
  - `STEPS UP`: $X = 49.530\text{ mm}, Y = 21.590\text{ mm}$
  - Section sublabel `UP` at $X = 3.5\text{ mm}, Y = 21.590\text{ mm}$.
- **Knob Row 2 (DOWN)**: Center $Y = 43.000\text{ mm}$ ($\Delta Y = 21.410\text{ mm}$ pitch from Row 1)
  - `TIME DOWN`: $X = 11.430\text{ mm}, Y = 43.000\text{ mm}$
  - `SHAPE DOWN`: $X = 30.480\text{ mm}, Y = 43.000\text{ mm}$
  - `STEPS DOWN`: $X = 49.530\text{ mm}, Y = 43.000\text{ mm}$
  - Section sublabel `DOWN` at $X = 3.5\text{ mm}, Y = 43.000\text{ mm}$.

#### Delineator Line 1
- $Y = 51.500\text{ mm}$, stroke `#3a3a3a`, stroke-width $0.4\text{ mm}$, $X \in [3.5, 57.5]\text{ mm}$.

#### Zone 3: CV Attenuverters (Trimpots)
- **Attenuverter Row 1 (UP CV Depths)**: Center $Y = 58.000\text{ mm}$
  - `TIME UP CV DEPTH`: $X = 11.430\text{ mm}, Y = 58.000\text{ mm}$
  - `SHAPE UP CV DEPTH`: $X = 30.480\text{ mm}, Y = 58.000\text{ mm}$
  - `STEPS UP CV DEPTH`: $X = 49.530\text{ mm}, Y = 58.000\text{ mm}$
  - Row header: `UP CV` at $Y = 53.500\text{ mm}$, scale $0.0020$, fill `#2c2c2c`.
- **Attenuverter Row 2 (DOWN CV Depths)**: Center $Y = 70.000\text{ mm}$ ($\Delta Y = 12.000\text{ mm}$)
  - `TIME DOWN CV DEPTH`: $X = 11.430\text{ mm}, Y = 70.000\text{ mm}$
  - `SHAPE DOWN CV DEPTH`: $X = 30.480\text{ mm}, Y = 70.000\text{ mm}$
  - `STEPS DOWN CV DEPTH`: $X = 49.530\text{ mm}, Y = 70.000\text{ mm}$
  - Row header: `DOWN CV` at $Y = 65.500\text{ mm}$, scale $0.0020$, fill `#2c2c2c`.

#### Delineator Line 2
- $Y = 80.500\text{ mm}$, stroke `#3a3a3a`, stroke-width $0.4\text{ mm}$, $X \in [3.5, 57.5]\text{ mm}$.

#### Zone 4: I/O Jacks (Bottom-Aligned Upward from $Y_{\text{out}} = 118.000\text{ mm}$)
- **Jack Row 1 (UP CV Inputs)**: Center $Y = 89.500\text{ mm}$ ($Y_{\text{out}} - 3 \times 9.500\text{ mm}$)
  - `TIME UP CV IN`: $X = 11.430\text{ mm}, Y = 89.500\text{ mm}$
  - `SHAPE UP CV IN`: $X = 30.480\text{ mm}, Y = 89.500\text{ mm}$
  - `STEPS UP CV IN`: $X = 49.530\text{ mm}, Y = 89.500\text{ mm}$
  - **Yellow Step LED (UP)**: Positioned above and right of Steps Up jack: $X = 54.000\text{ mm}, Y = 85.500\text{ mm}$.
- **Jack Row 2 (DOWN CV Inputs)**: Center $Y = 99.000\text{ mm}$ ($Y_{\text{out}} - 2 \times 9.500\text{ mm}$)
  - `TIME DOWN CV IN`: $X = 11.430\text{ mm}, Y = 99.000\text{ mm}$ (normaled to Time Up CV)
  - `SHAPE DOWN CV IN`: $X = 30.480\text{ mm}, Y = 99.000\text{ mm}$ (normaled to Shape Up CV)
  - `STEPS DOWN CV IN`: $X = 49.530\text{ mm}, Y = 99.000\text{ mm}$ (normaled to Steps Up CV)
  - **Yellow Step LED (DOWN)**: Positioned above and right of Steps Down jack: $X = 54.000\text{ mm}, Y = 95.000\text{ mm}$.
- **Jack Row 3 (Direction & End Event Gates)**: Center $Y = 108.500\text{ mm}$ ($Y_{\text{out}} - 1 \times 9.500\text{ mm}$)
  - Port 1: `UP GATE` ($X = 9.144\text{ mm}, Y = 108.500\text{ mm}$) — High (+10V) while slewing up.
  - Port 2: `EOU TRIG` ($X = 23.368\text{ mm}, Y = 108.500\text{ mm}$) — 1ms pulse (+10V) on end of up.
  - Port 3: `DOWN GATE` ($X = 37.592\text{ mm}, Y = 108.500\text{ mm}$) — High (+10V) while slewing down.
  - Port 4: `EOD TRIG` ($X = 51.816\text{ mm}, Y = 108.500\text{ mm}$) — 1ms pulse (+10V) on end of down.
  - Row header: `GATES / TRIGS` at $Y = 104.000\text{ mm}$, scale $0.0020$, fill `#2c2c2c`.
- **Jack Row 4 (Main Signal I/O)**: Fixed Center $Y = 118.000\text{ mm}$
  - Port 1: `IN` ($X = 9.144\text{ mm}, Y = 118.000\text{ mm}$) — Audio/CV signal input (normaled to 0V).
  - Port 2: `SLEW` ($X = 23.368\text{ mm}, Y = 118.000\text{ mm}$) — Continuous slewed output.
  - Port 3: `STEP` ($X = 37.592\text{ mm}, Y = 118.000\text{ mm}$) — Slewed + stepped output.
  - Port 4: `STEP TRIG` ($X = 51.816\text{ mm}, Y = 118.000\text{ mm}$) — Step-crossed clock/gate output (+10V).
  - Shared labels: Centered above jacks at $Y = 114.500\text{ mm}$, scale $0.0022$, fill `#1c1c1c`.

---

## 3. Mathematical Specifications & DSP Algorithms

### 3.1. Slew Time Calculation
- **Knob Value**: $k_{\text{time}} \in [0.0, 1.0]$.
- **Attenuverter**: $a_{\text{time}} \in [-1.0, 1.0]$.
- **CV Input**: $V_{\text{cv}} \in [-10.0, +10.0]\text{ V}$.
- **Modulated Parameter**:
  $$p = \mathrm{clamp}\left(k_{\text{time}} + a_{\text{time}} \cdot \frac{V_{\text{cv}}}{10.0}, 0.0, 1.0\right)$$
- **Effective Slew Duration $T$**:
  $$T = T_{\min} \cdot \left(\frac{T_{\max}}{T_{\min}}\right)^p = 0.0005 \cdot (20000.0)^p \quad [\text{seconds}]$$
  - Range: $0.5\text{ ms}$ ($500\ \mu\text{s}$) to $10.0\text{ s}$.

### 3.2. Curvature / Shape Response Mathematics
The Shape knob $k_{\text{shape}} \in [-1.0, +1.0]$ controls trajectory curvature without altering the total slew duration $T$:
- $k_{\text{shape}} = -1.0$: Highly Logarithmic / Fast-start (steep initial rise, flattening tail).
- $k_{\text{shape}} = 0.0$: Pure Linear ramp ($dV/dt = \pm \Delta V / T$).
- $k_{\text{shape}} = +1.0$: Highly Exponential / Slow-start (gentle initial rise, accelerating arrival).

#### Trajectory Function:
Let normalized progress be $u \in [0.0, 1.0]$ where $u(t) = t / T$.
Let curvature parameter $\gamma$:
- If $|k_{\text{shape}}| < 10^{-4}$: Linear, $f(u) = u$.
- If $k_{\text{shape}} > 0$ (Exponential):
  $$\alpha = 1.0 + 5.0 \cdot k_{\text{shape}}$$
  $$f(u) = u^\alpha$$
- If $k_{\text{shape}} < 0$ (Logarithmic):
  $$\beta = 1.0 + 5.0 \cdot (-k_{\text{shape}})$$
  $$f(u) = 1.0 - (1.0 - u)^\beta$$

During real-time processing with continuous input changes, the instantaneous phase $u$ advances at rate:
$$du/dt = \frac{1}{T}$$
The continuous output is:
$$V_{\text{slew}}(t) = V_{\text{start}} + (V_{\text{target}} - V_{\text{start}}) \cdot f(u(t))$$

When $V_{\text{in}}$ changes mid-slew, $V_{\text{start}} \leftarrow V_{\text{slew}}(t)$, $V_{\text{target}} \leftarrow V_{\text{in}}$, and $u \leftarrow 0$.

### 3.3. Quantization and Stepping Engine

The **Steps** parameter $N \in [0, 96]$:
$$N = \mathrm{round}\left(\mathrm{clamp}\left(k_{\text{steps}} + a_{\text{steps}} \cdot \frac{V_{\text{cv}}}{10.0} \cdot 96.0, 0.0, 96.0\right)\right)$$

#### Mode A: Equal Transition Sub-division (`Equal`, Default)
- If $N = 0$: Stepping bypassed, $V_{\text{step}} = V_{\text{slew}}$.
- If $N \ge 1$:
  - Transition span $\Delta V = V_{\text{target}} - V_{\text{start}}$.
  - Step size $h = \Delta V / N$.
  - Current step index $m = \lfloor u \cdot N \rfloor$.
  - Discretized output:
    $$V_{\text{step}} = V_{\text{start}} + m \cdot h$$
  - When $u \ge 1.0$ (arrival): $V_{\text{step}} = V_{\text{target}}$.

#### Mode B: Microtonal & V/Oct-Aware Scales
Supported Scales (1V/oct standard, 0V = C root):
1. **Semitone (12-EDO)**: 12 notes/octave ($1/12\text{ V} \approx 83.333\text{ mV}$)
2. **Quarter-tone (24-EDO)**: 24 notes/octave ($1/24\text{ V} \approx 41.667\text{ mV}$)
3. **19-EDO (19-TET)**: 19 notes/octave ($1/19\text{ V} \approx 52.632\text{ mV}$)
4. **22-EDO (22-TET)**: 22 notes/octave ($1/22\text{ V} \approx 45.455\text{ mV}$)
5. **31-EDO (31-TET)**: 31 notes/octave ($1/31\text{ V} \approx 32.258\text{ mV}$)
6. **Just Intonation (5-limit)**: C, C#, D, Eb, E, F, F#, G, Ab, A, Bb, B ratios:
   - $1/1, 16/15, 9/8, 6/5, 5/4, 4/3, 45/32, 3/2, 8/5, 5/3, 9/5, 15/8$
   - Pitch offsets: $\log_2(\text{ratio})\text{ V}$.
7. **Quarter-comma Meantone**: 12-pitch unequal historic temperament with pure major thirds ($\approx 386.31\ \text{cents}$) and flat fifths ($5^{1/4} \approx 696.58\ \text{cents}$).

#### Quantization Strategies (Context Menu Toggle):
1. **Scale Traversal (Cap / Density)**: Traverse chromatic/microtonal scale degrees between $V_{\text{start}}$ and $V_{\text{target}}$, restricted to a maximum of $N$ steps along the path.
2. **Subdivided & Quantized (Nearest Target Grid)**: Divide trajectory into $N$ equal slices, snapping each slice level to the nearest pitch of the active scale.

---

## 4. Gate, Trigger & Indicator State Machines

1. **Up Gate**:
   - $V_{\text{up}} = +10.0\text{ V}$ whenever actively slewing upward ($V_{\text{target}} > V_{\text{slew}} + \epsilon$ and $u < 1.0$), else $0.0\text{ V}$.
2. **Down Gate**:
   - $V_{\text{down}} = +10.0\text{ V}$ whenever actively slewing downward ($V_{\text{target}} < V_{\text{slew}} - \epsilon$ and $u < 1.0$), else $0.0\text{ V}$.
3. **End of Up (EOU) Trig**:
   - Emits a $1.0\text{ ms}$ pulse ($+10.0\text{ V}$) precisely when $u$ reaches $1.0$ from an upward slew.
4. **End of Down (EOD) Trig**:
   - Emits a $1.0\text{ ms}$ pulse ($+10.0\text{ V}$) precisely when $u$ reaches $1.0$ from a downward slew.
5. **Step-Crossed Output**:
   - **Trigger Mode (Default)**: Emits a $1.0\text{ ms}$ pulse ($+10.0\text{ V}$) whenever the step index transitions: $m(t) \ne m(t - \Delta t)$.
   - **Toggle / Flip-Flop Mode (Menu Option)**: Inverts state between $0.0\text{ V}$ and $+10.0\text{ V}$ on every step transition.
6. **Step LEDs**:
   - Flash yellow (decay time $\approx 40\text{ ms}$ for high visual visibility) on Up step crossing and Down step crossing respectively.

---

## 5. Tooltip & ParamQuantity Display Formatting

- **Time Knobs**:
  - Format: `<value> ms` if $T < 1.0\text{ s}$, `<value> s` if $T \ge 1.0\text{ s}$. E.g., `12.5 ms` or `2.45 s`.
- **Shape Knobs**:
  - $-1.0 \rightarrow -0.01$: `Logarithmic (%.2f)`
  - $-0.01 \rightarrow +0.01$: `Linear`
  - $+0.01 \rightarrow +1.0$: `Exponential (%.2f)`
- **Steps Knobs**:
  - $0$: `Bypass (Continuous)`
  - $1 \dots 96$: `%d steps`
- **Attenuverters**:
  - `Up time CV depth`: `%.1f %%`
  - `Up shape CV depth`: `%.1f %%`
  - `Up steps CV depth`: `%.1f %%`
  - `Down time CV depth`: `%.1f %%`
  - `Down shape CV depth`: `%.1f %%`
  - `Down steps CV depth`: `%.1f %%`

---

## 6. Context Menu & JSON State Persistence

The module right-click context menu provides:
1. **Stepping Mode**:
   - `Equal (Transition Sub-division)` [Default]
   - `Semitone (12-EDO)`
   - `Quarter-tone (24-EDO)`
   - `19-EDO (19-TET)`
   - `22-EDO (22-TET)`
   - `31-EDO (31-TET)`
   - `Just Intonation (5-limit)`
   - `Quarter-comma Meantone`
2. **Quantize Strategy**:
   - `Scale Degree Traversal` [Default]
   - `Subdivided & Nearest Scale Snap`
3. **Step Clock Output Behavior**:
   - `1ms Trigger Pulse` [Default]
   - `Alternating Toggle (Flip-Flop)`
4. **Root Key Selection**:
   - `C`, `C#`, `D`, `D#`, `E`, `F`, `F#`, `G`, `G#`, `A`, `A#`, `B`

All settings serialize cleanly into `dataToJson()` and `dataFromJson()`.
