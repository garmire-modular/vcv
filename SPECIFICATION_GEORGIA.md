# Georgia — 3D Rotating Wireframe Vector Synthesizer
## Formal Engineering Specification (18 HP)

---

### 1. Architectural Overview & Product Identity

**Georgia** is an 18 HP ($91.44\text{ mm}$) Generate-class 3D rotating wireframe vector synthesizer and audio-reactive oscilloscope displacement engine for VCV Rack v2 and the 4ms MetaModule hardware ecosystem. It generates high-precision $(X(t), Y(t))$ beam trajectories derived from 3D parametric wireframe geometric surfaces (Sphere/Toroid, Cube/Cylinder, Pyramid/Cone) with continuous shape morphing, perspective camera depth projection, 3D Euler rotations, 3D Polly logarithmic stretch, 3D helical twist, and mono audio waveform displacement with an equatorial-to-full surface normal Spread control.

- **Width**: 18 HP ($91.44\text{ mm}$ / $171\text{ px}$ MetaModule bitmap width).
- **Height**: Standard Eurorack 3U ($128.50\text{ mm}$ / $240\text{ px}$ MetaModule bitmap height).
- **Panel Background**: `#6e6e6e` dark neutral gray.
- **Palette Badge**: Left-aligned $2.54\text{ mm} \times 88.90\text{ mm}$ rectangle with `#5d5d5d` initial placeholder.
- **Polyphony**: 1 to 16 voices dynamically allocated based on connected inputs.
- **Safety Standard**: Real-time safe DSP core (`src/core/GeorgiaCore.hpp`) with zero heap allocations in `process()`, zero system locks, zero file I/O, and no external dependencies.
- **Module Version**: `v1.3.1`.

---

### 2. Panel Architecture & Coordinate Layout

Following repository layout invariants:
- **Module Title**: Baseline $Y = 7.620\text{ mm}$, centered at $X = 45.720\text{ mm}$ in lowercase `res/Node.otf` (scale 0.0048).
- **Version Tag**: Baseline $Y = 10.414\text{ mm}$, centered at $X = 45.720\text{ mm}$ in `res/Quicksand-Regular.ttf` (`v1.3.1`, scale 0.0016).
- **5 Columns across 18 HP ($91.44\text{ mm}$)**:
  - Margins: $13.52\text{ mm}$ left and right.
  - Pitch: $\Delta X = 16.10\text{ mm}$ center-to-center.
  - Columns: $X = [13.52, 29.62, 45.72, 61.82, 77.92]\text{ mm}$.
- **Primary Control Knobs (Zone 1 & 2)**:
  - **Row 1 ($Y = 21.59\text{ mm}$)**:
    - Col 0: `FREQ` (Coarse carrier timebase, calibrated default 60 Hz)
    - Col 1: `RANGE` (`TL1105` button at $Y=21.59\text{ mm}$, tri-color LED at $Y=15.50\text{ mm}$)
    - Col 2: `FINE` (Fine tune $\pm 10\%$)
    - Col 3: `SHAPE` (Discrete options: Sphere/Toroid, Cube/Cylinder, Pyramid/Cone, Funnel/Catenoid)
    - Col 4: `MORPH` (Continuous interpolation between 1st and 2nd variation)
  - **Row 2 ($Y = 34.50\text{ mm}$)**:
    - Col 0: `DENSITY` (Wireframe raster grid resolution: 2 to 64 scan rings, default 50%)
    - Col 1: `DEPTH` (2D middle cross-section at 0% to deep 3D perspective wireframe at 100%, default 100%)
    - Col 2: `WINDOW` (Perimeter audio buffer time window: $0.5\text{ ms}$ to $500.0\text{ ms}$, supports direct text editing e.g. "50ms")
    - Col 3: `SPREAD` (Audio scope belt spread: 0% tight equatorial belt $\rightarrow$ 100% full surface normal, default 100%)
    - Col 4: `AMP` (Waveform displacement amplitude, default 0%, max 100%)
  - **Row 3 ($Y = 47.50\text{ mm}$)**:
    - Col 0: `TWIST` (3D helical axis twist: $-100\%$ to $+100\%$, default 0%)
    - Col 1: `ROT X` (Pitch rotation: $0^\circ$ to $360^\circ$, default 0° / initial upright 90° view, supports direct degree text editing)
    - Col 2: `ROT Y` (Yaw rotation: $0^\circ$ to $360^\circ$, default 0°, supports direct degree text editing)
    - Col 3: `ROT Z` (Roll / In-plane rotation: $0^\circ$ to $360^\circ$, default 0°, supports direct degree text editing)
    - Col 4: `STRETCH` (Polly logarithmic spiral & polar stretch: $-100\%$ to $+100\%$, default 0%)
- **Delineator Line 1**: $Y = 53.50\text{ mm}$.
- **Attenuverter Trimpots (Zone 3 - 15 Trimpots across 3 Rows)**:
  - **Trim Row 1 ($Y = 59.50\text{ mm}$)**: `IN`, `FREQ`, `FINE`, `SHAPE`, `MORPH`
  - **Trim Row 2 ($Y = 68.50\text{ mm}$)**: `DENSITY`, `DEPTH`, `WINDOW`, `SPREAD`, `AMP`
  - **Trim Row 3 ($Y = 77.50\text{ mm}$)**: `TWIST`, `ROT X`, `ROT Y`, `ROT Z`, `STRETCH`
- **Delineator Line 2**: $Y = 83.50\text{ mm}$.
- **I/O Jacks (Zone 4 - Bottom-Aligned Upward)**:
  - **Jack Row 1 ($Y = 89.50\text{ mm}$)**: `IN` (Mono Audio), `FREQ` CV, `FINE` CV, `SHAPE` CV, `MORPH` CV
  - **Jack Row 2 ($Y = 99.00\text{ mm}$)**: `DENSITY` CV, `DEPTH` CV, `WINDOW` CV, `SPREAD` CV, `AMP` CV
  - **Jack Row 3 ($Y = 108.50\text{ mm}$)**: `TWIST` CV, `ROT X` CV, `ROT Y` CV, `ROT Z` CV, `STRETCH` CV
  - **Jack Row 4 (Fixed $Y = 118.00\text{ mm}$, 4 jacks)**: `SYNC IN`, `X` (Out), `Y` (Out), `SYNC OUT`

---

### 3. Mathematical & DSP Architecture

#### 3.1 Carrier Oscillator & Timebase Modes
Identical to Ampara/Polly/Circe:
- Default coarse: $c = 0.5477226\text{f}$
- Very Slow: $600\text{ s} - 0.1\text{ s}$ (default $5.12\text{ s}$)
- LFO: $0.01\text{ Hz} - 200\text{ Hz}$ (default $60.0\text{ Hz}$)
- VCO: $150\text{ Hz} - 2000\text{ Hz}$ (default $619.8\text{ Hz}$)
Hard sync via `SYNC IN` resets carrier phase $\phi = 0$; `SYNC OUT` emits $+10\text{V}$ trigger at cycle boundary.

#### 3.2 4 Discrete Geometric Shape Families & Morphing
The Shape knob features **4 discrete options** (0: Sphere/Toroid, 1: Cube/Cylinder, 2: Pyramid/Cone, 3: Funnel/Catenoid) with dedicated specialized DSP algorithms per shape and **zero interpolation between different shape families**. Within each family, the Morph control provides continuous smooth morphing between its two variations:

1. **Family 0: Sphere $\leftrightarrow$ Toroid (Donut)**:
   - Sphere: $x = \cos(u)\sin(v), y = \sin(u)\sin(v), z = \cos(v)$ swept over latitude $v \in [0, \pi]$ with $K$ rings.
   - Toroid (Donut with hollow central hole): The major ring angle completes a full $2\pi$ revolution via $\varphi = 2v \in [0, 2\pi]$ (with major radius $R_1 = 0.65$, tube radius $R_2 = 0.30$, producing inner hole radius $0.35$ and outer radius $0.95$):
     $$x = (R_1 + R_2\cos(u))\cos(2v), \quad y = (R_1 + R_2\cos(u))\sin(2v), \quad z = R_2\sin(u)$$
   - **Resolution Doubling Compensation**: Because the major ring revolution covers $2\pi$ radians (double the angular span of the sphere's $\pi$ latitude), tracing the same $K$ rings would halve spatial ring density. Georgia dynamically scales the effective ring frequency by $K_{\text{toroid}} = K \cdot (1 + m)$. At 100% Toroid ($m = 1$), the number of tube rings is doubled ($2\times K$), fully restoring spatial resolution.
   - Morph blends position and surface normals smoothly.

2. **Family 1: Cube $\leftrightarrow$ Cylinder (Triple Orthogonal 3D Wireframe Grid)**:
   - Cube: Authentic 3D wireframe grid across all 6 faces using a seamless 3-pass orthogonal loop, eliminating spherical concentric rings at the top and bottom:
     - **Pass 0** ($0 \le \phi < \frac{1}{3}$): $Z$-axis square hoops ($XY$ cross-sections stepping along $Z$).
     - **Pass 1** ($\frac{1}{3} \le \phi < \frac{2}{3}$): $X$-axis square hoops ($YZ$ cross-sections stepping along $X$).
     - **Pass 2** ($\frac{2}{3} \le \phi < 1$): $Y$-axis square hoops ($XZ$ cross-sections stepping along $Y$).
   - **Cube Scale Reduction**: Cube scale is reduced by 2.5% ($0.82875$ at $m=0$) to prevent diagonal corner clipping at init settings ($\pm 10\text{V}$ bounds), while Cylinder scaling remains $0.85$ at $m=1$.
   - Cylinder: Smoothly morphs the square cross-sections into circular hoops in Pass 0, while Passes 1 and 2 trace the longitudinal outer cage boundaries and flat endcaps.
   - Morph blends position and surface normals smoothly.

3. **Family 2: Pyramid $\leftrightarrow$ Cone (Exact Sharp Corners & Upright Apex)**:
   - Apex oriented at top of the screen: apex at $z = -0.85$ (mapping to $y_{\text{screen}} = +0.85$ under base $90^\circ$ X rotation), base at $z = +0.85$ ($y_{\text{screen}} = -0.85$).
   - Height fraction $h = v / \pi \in [0, 1]$ from apex to base, $z = -0.85 + h \cdot 1.70$, $R(h) = h \cdot 0.85$.
   - Denominator blends from exact square $\max(|\cos(u)|, |\sin(u)|)$ at Morph 0 to circle $1.0$ at Morph 1.

4. **Family 3: Funnel $\leftrightarrow$ Catenoid (Flared Horn to Hourglass Wormhole)**:
   - Morph 0 is Funnel, Morph 1 is Catenoid.
   - Funnel: Gravitational well / flared horn with wide flared rim at top ($z = -0.85$, $y_{\text{screen}} = +0.85$, $r = 0.85$) and narrow neck at bottom ($z = +0.85$, $y_{\text{screen}} = -0.85$, $r = 0.20$):
     $$r_{\text{fun}}(t) = 0.20 + 0.65 \cdot (1 - t)^{2.2}, \quad t = v / \pi \in [0, 1]$$
   - Catenoid: Symmetrical double-flanged spool / wormhole with narrow central waist:
     $$r_{\text{cat}}(z) = 0.25 \cdot \cosh(z / 0.45), \quad z \in [-0.85, 0.85]$$
   - Morph blends smoothly between the flared Funnel ($m=0$) and double-flanged Catenoid ($m=1$).

#### 3.3 3D Twist & 3D Polly Logarithmic Spiral Stretch
- **Twist**: Helical axial rotation around $Z$: $\theta_{\text{twist}} = \text{Twist} \cdot 2\pi \cdot P_z$.
- **Stretch**: Operates continuously across all shapes including pure Sphere:
  - Radius damping factor: $d = 1.0 + \text{Stretch} \cdot 2.5 \cdot (1.0 - r_{\text{cyl}})$
  - Polar scaling: $P_z' = P_z \cdot (1.0 + \text{Stretch} \cdot 0.75)$
  - Logarithmic spiral twist: $\theta_{\text{spiral}} = \text{Stretch} \cdot 1.5\pi \cdot P_z$

#### 3.4 Scope Waveform Displacement & Spread
- Waveform displacement amplitude: 0% to 100% (default 0%).
- **Spread Parameter**:
  $$k_{\text{spread}} = 80.0 \cdot (1.0 - \text{Spread})^2$$
  $$\text{factor} = \exp\left(-k_{\text{spread}} \cdot \left(v - \frac{\pi}{2}\right)^2\right)$$
  - At $\text{Spread} = 0\%$: Tight equatorial scope belt centered at $v = \pi/2$.
  - At $\text{Spread} = 100\%$: $k = 0$, factor $= 1.0$ uniformly, producing full 3D surface normal hull displacement.
- Right-click context menu preserves 3 modes:
  1. Surface normal displacement (modulated by Spread) [Default]
  2. Dedicated equatorial scope belt
  3. Z-depth relief extrusion

#### 3.5 3D Euler Rotations & Depth Control
- **Rotation Ranges**: `ROT X`, `ROT Y`, and `ROT Z` knobs all span $0^\circ$ to $360^\circ$ ($0.0$ to $1.0$).
- **Base Initial Orientation**: `ROT X` has a base $+90^\circ$ ($+\pi/2$) offset, presenting the shape in the natural upright view at the default knob position ($0^\circ$).
  $$\theta_X = \frac{\pi}{2} + \text{rotX} \cdot 2\pi, \quad \theta_Y = \text{rotY} \cdot 2\pi, \quad \theta_Z = \text{rotZ} \cdot 2\pi$$
- **Depth Control (2D Cross-Section to 3D Perspective)**:
  - At $\text{Depth} = 0\%$: Presents a pure 2D version of the object (middle cross-section) in $(X, Y)$ with zero perspective distortion and zero out-of-plane tilting.
  - As Depth increases towards $100\%$: Smoothly expands and blooms from the flat middle cross-section into the full 3D rotating wireframe mesh with perspective camera projection ($d_{\text{cam}} = 3.0$):
    $$\text{proj} = (1 - \text{Depth}) \cdot \mathbf{P}_{\text{2D}} + \text{Depth} \cdot \left(\frac{\mathbf{P}_{\text{3D}}}{z_{\text{eff}}}\right)$$
- Output deflection: Standard Eurorack $\pm 5\text{V}$, clamped cleanly at $\pm 10\text{V}$ without non-linear cushion distortion.
