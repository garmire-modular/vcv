# Ampara — Audio-Reactive Vector Polygon Synthesizer
## Formal Engineering Specification (16 HP)

---

### 1. Architectural Overview & Product Identity

**Ampara** is a 16 HP ($81.28\text{ mm}$) Generate-class vector synthesizer and oscilloscope visual displacement module for VCV Rack v2 and the 4ms MetaModule hardware ecosystem. It generates high-precision $(X(t), Y(t))$ beam deflection trajectories derived from continuous geometric $N$-sided regular polygons (Circle through Octagon) wrapped with real-time stereo audio waveforms and shaped by dual-channel envelope followers.

- **Width**: 16 HP ($81.28\text{ mm}$ / $152\text{ px}$ MetaModule bitmap width).
- **Height**: Standard Eurorack 3U ($128.50\text{ mm}$ / $240\text{ px}$ MetaModule bitmap height).
- **Panel Background**: `#6e6e6e` dark neutral gray.
- **Palette Badge**: Left-aligned $2.54\text{ mm} \times 88.90\text{ mm}$ rectangle with `#5d5d5d` initial placeholder.
- **Polyphony**: 1 to 16 voices dynamically allocated based on connected inputs.
- **Safety Standard**: Real-time safe DSP core (`src/core/AmparaCore.hpp`) with zero heap allocations, zero system locks, zero file I/O, and no external dependencies.

---

### 2. Panel Architecture & Coordinate Layout

Following repository layout invariants:
- **Module Title**: Baseline $Y = 7.620\text{ mm}$, centered at $X = 40.640\text{ mm}$ in lowercase `res/Node.otf` (scale 0.0048).
- **Version Tag**: Baseline $Y = 10.414\text{ mm}$, centered at $X = 40.640\text{ mm}$ in `res/Quicksand-Regular.ttf` (`v2.25.0`, scale 0.0016).
- **Knob Grid (Zone 1 & 2)**:
  - 10 full standard knobs (`RoundBlackKnob`, diameter $10.0\text{ mm}$).
  - 5 Columns symmetrically distributed across $81.28\text{ mm}$:
    - $X = [12.14, 26.39, 40.64, 54.89, 69.14]\text{ mm}$ ($\Delta X = 14.25\text{ mm}$)
  - Row 1 ($Y = 21.59\text{ mm}$):
    - Col 1: `FREQ` (Coarse timebase carrier)
    - Col 2: `FINE` (Fine tune timebase $\pm 10\%$)
    - Centered between Col 1 & 2 ($X = 19.265\text{ mm}$): `RANGE` button (`TL1105`) at $Y = 21.59\text{ mm}$ with `RANGE` RGB LED (`RangeLightWidget`) at $Y = 15.50\text{ mm}$.
    - Col 3: `SHAPE` (Circle $\rightarrow$ Octagon)
    - Col 4: `WARP` (Milkdrop polar radial swirl / spiral vortex twist)
    - Col 5: `WINDOW` (Buffer time window)
  - Row 2 ($Y = 43.00\text{ mm}$):
    - Col 1: `DISP` (Scope displacement depth)
    - Col 2: `RISE` (Envelope attack time)
    - Col 3: `FALL` (Envelope decay time)
    - Col 4: `MORPH` (Routing: Mono average $\rightarrow$ Decoupled stereo)
    - Col 5: `PHASE` (End-of-line beam rotation $0^\circ\text{--}360^\circ$)
- **Delineator Line 1**: $Y = 56.50\text{ mm}$ separating primary knobs from attenuverters.
- **Attenuverter Trimpots (Zone 3)**:
  - 9 Bipolar Attenuverters (`Trimpot`, diameter $6.5\text{ mm}$):
  - Row 1 ($Y = 66.00\text{ mm}$): `FREQ`, `SHAPE`, `WARP`, `WINDOW`, `DISP` ($X = [12.14, 26.39, 40.64, 54.89, 69.14]\text{ mm}$)
  - Row 2 ($Y = 78.00\text{ mm}$): `RISE`, `FALL`, `MORPH`, `PHASE` ($X = [19.265, 33.515, 47.765, 62.015]\text{ mm}$)
- **Delineator Line 2**: $Y = 86.00\text{ mm}$ separating attenuverters from I/O jacks.
- **I/O Ports (Zone 4 - Bottom-Aligned Upward)**:
  - 16 Jacks total, arranged in 4 rows of 4 jacks, pitch $\Delta Y = 9.50\text{ mm}$, with horizontal positions $X = [13.09, 31.46, 49.82, 68.19]\text{ mm}$ ($\Delta X = 18.36\text{ mm}$):
  - **Jack Row 1 ($Y = 89.50\text{ mm}$)**:
    - Col 1: `FREQ` CV
    - Col 2: `SHAPE` CV
    - Col 3: `WARP` CV
    - Col 4: `WINDOW` CV
  - **Jack Row 2 ($Y = 99.00\text{ mm}$)**:
    - Col 1: `DISP` CV
    - Col 2: `RISE` CV
    - Col 3: `FALL` CV
    - Col 4: `MORPH` CV
  - **Jack Row 3 ($Y = 108.50\text{ mm}$)**:
    - Col 1: `PHASE` CV
    - Col 2: `SYNC IN`
    - Col 3: `IN L` (Audio Left)
    - Col 4: `IN R` (Audio Right, normalled to L)
  - **Jack Row 4 (Fixed $Y = 118.00\text{ mm}$)**:
    - Col 1: `SYNC OUT`
    - Col 2: *(spare / ground anchor)*
    - Col 3: `OUT X` (Oscilloscope X deflection $\pm 5\text{V}$)
    - Col 4: `OUT Y` (Oscilloscope Y deflection $\pm 5\text{V}$)

---

### 3. Mathematical & DSP Architecture

#### 3.1 Carrier Oscillator & Timebase Modes
The beam traces along the perimeter at a carrier frequency $f_0$.
Three range modes are selectable via the `RANGE` button:
1. **Very Slow Mode** (Yellow LED):
   $$T_0 = 600.0 \cdot \left(\frac{0.1}{600.0}\right)^{c} \cdot (1.0 - \text{fine} \cdot 0.10)\quad [\text{seconds}]$$
2. **LFO Mode** (Orange LED):
   $$f_0 = (0.01 + 199.99 \cdot c^2) + \text{fine} \cdot 20.0\quad [\text{Hz}]$$
3. **VCO Mode** (Purple/Magenta LED):
   $$f_0 = 150.0 \cdot \left(\frac{2000.0}{150.0}\right)^{c} \cdot (1.0 + \text{fine} \cdot 0.10)\quad [\text{Hz}]$$
With $1\text{ V/Oct}$ modulation applied:
$$f = f_0 \cdot 2^{\text{CV}_{\text{freq}} \cdot \text{trim}_{\text{freq}}}$$

#### 3.2 Dual Envelope Followers (L & R)
Incoming audio is normalized ($R = L$ if unpatched).
Envelope detection uses a two-stage peak detector with independent attack and release ballistics:
$$\alpha_{\text{rise}} = 1 - e^{-\frac{1}{f_s \cdot \tau_{\text{rise}}}}, \quad \tau_{\text{rise}} \in [0.1\text{ ms}, 200.0\text{ ms}]$$
$$\alpha_{\text{fall}} = 1 - e^{-\frac{1}{f_s \cdot \tau_{\text{fall}}}}, \quad \tau_{\text{fall}} \in [2.0\text{ ms}, 2000.0\text{ ms}]$$
$$E_{L}[n] = \begin{cases} E_L[n-1] + \alpha_{\text{rise}} (|L[n]| - E_L[n-1]), & |L[n]| \ge E_L[n-1] \\ E_L[n-1] + \alpha_{\text{fall}} (|L[n]| - E_L[n-1]), & |L[n]| < E_L[n-1] \end{cases}$$
(and identically for $E_R[n]$).

#### 3.3 Audio Circular Buffer & Perimeter Trace
A preallocated circular ring buffer (16384 samples, real-time safe static storage) stores instantaneous audio:
$$S_{\text{disp}}(\theta) = \text{BufferRead}\left( \text{head} - \frac{\theta}{2\pi} \cdot W_{\text{samples}} \right)$$
where $W_{\text{samples}} = W_{\text{window}} \cdot f_s$, with $W_{\text{window}} \in [0.2\text{ ms}, 50.0\text{ ms}]$.

#### 3.4 Geometric Regular Polygon Formula with Continuous Morphing
For a regular $N$-gon with circumradius $R_0 = 1.0$:
The radial distance to the perimeter edge at angle $\theta$ is:
$$r_N(\theta) = \frac{\cos\left(\frac{\pi}{N}\right)}{\cos\left( (\theta \pmod{\frac{2\pi}{N}}) - \frac{\pi}{N} \right)}$$
For continuous fractional $N \in [2.0, 8.0]$:
- At $N \le 2.0$: $r(\theta) = 1.0$ (Circle).
- For $N \in (2.0, 3.0)$: Smooth interpolation between Circle ($r=1$) and Triangle ($N=3$).
- For $N \in [k, k+1]$ ($k \in \{3, 4, 5, 6, 7\}$): Smooth linear/cosine blend between $r_k(\theta)$ and $r_{k+1}(\theta)$.
Magnetic detent snap centers around integer vertices $N \in \{2, 3, 4, 5, 6, 7, 8\}$ within a $\pm 0.05$ deadband.

#### 3.5 Milkdrop Warp / Vortex Twist
Milkdrop polar swirl transforms the angle $\theta$:
$$\theta_{\text{warped}} = \theta + \text{Warp} \cdot E_{\text{warp}} \cdot \sin(N \cdot \theta)$$
and applies radial vortex suction:
$$r_{\text{warped}} = r(\theta_{\text{warped}}) \cdot \left(1.0 + \text{Warp} \cdot 0.5 \cdot \cos(2\theta)\right)$$

#### 3.6 Continuous Stereo Routing Morph
The MORPH parameter $M \in [0.0, 1.0]$ continuously crossfades between Mono Average and Decoupled Stereo:
- **Displacement Modulation Source**:
  $$D_{\text{audio}} = (1.0 - M) \cdot \left(\frac{L + R}{2}\right) + M \cdot L$$
- **Radial Scale Modulation**:
  $$S_{\text{mod}} = 1.0 + M \cdot \left(E_R \cdot 0.8\right)$$
- **Rotation Modulation**:
  $$\Phi_{\text{mod}} = \Phi_{\text{knob}} + M \cdot \left(E_R \cdot \pi\right)$$

#### 3.7 Output Deflection Synthesis
The final coordinates are computed as:
$$r_{\text{final}} = \left( r_{\text{warped}} + D_{\text{audio}} \cdot \text{Disp} \right) \cdot S_{\text{mod}}$$
$$X(t) = 5.0\text{V} \cdot r_{\text{final}} \cdot \cos(\theta_{\text{warped}} + \Phi_{\text{mod}})$$
$$Y(t) = 5.0\text{V} \cdot r_{\text{final}} \cdot \sin(\theta_{\text{warped}} + \Phi_{\text{mod}})$$
with soft tanh limiting ensuring $|X(t)|, |Y(t)| \le 10.0\text{V}$.

---

### 4. Custom ParamQuantity Formatting Specifications

1. `FreqParamQuantity`:
   - Very Slow: `%.1f s` (period)
   - LFO / VCO: `%.2f Hz` / `%.2f kHz` (frequency)
   - Supports text input with unit suffixes (`s`, `ms`, `Hz`, `kHz`).
2. `ShapeParamQuantity`:
   - $N < 2.5$: `"Circle"`
   - $2.5 \le N < 3.5$: `"Triangle (3)"`
   - $3.5 \le N < 4.5$: `"Square (4)"`
   - $4.5 \le N < 5.5$: `"Pentagon (5)"`
   - $5.5 \le N < 6.5$: `"Hexagon (6)"`
   - $6.5 \le N < 7.5$: `"Heptagon (7)"`
   - $N \ge 7.5$: `"Octagon (8)"`
3. `WindowParamQuantity`:
   - `%.1f ms` (0.2 ms to 50.0 ms)
4. `RiseParamQuantity`:
   - `%.1f ms` (0.1 ms to 200.0 ms)
5. `FallParamQuantity`:
   - `%.1f ms` (2.0 ms to 2000.0 ms)
6. `PhaseParamQuantity`:
   - `%.1f°` (0.0° to 360.0°)
7. `Attenuverter Tooltips`:
   - `"X <param> CV depth"` / `"<Param> CV depth"` in sentence case per AGENTS.md 6.5.4.
