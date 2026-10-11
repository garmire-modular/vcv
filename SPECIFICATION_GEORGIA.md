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
- **Module Version**: `v1.1.0`.

---

### 2. Panel Architecture & Coordinate Layout

Following repository layout invariants:
- **Module Title**: Baseline $Y = 7.620\text{ mm}$, centered at $X = 45.720\text{ mm}$ in lowercase `res/Node.otf` (scale 0.0048).
- **Version Tag**: Baseline $Y = 10.414\text{ mm}$, centered at $X = 45.720\text{ mm}$ in `res/Quicksand-Regular.ttf` (`v1.1.0`, scale 0.0016).
- **5 Columns across 18 HP ($91.44\text{ mm}$)**:
  - Margins: $13.52\text{ mm}$ left and right.
  - Pitch: $\Delta X = 16.10\text{ mm}$ center-to-center.
  - Columns: $X = [13.52, 29.62, 45.72, 61.82, 77.92]\text{ mm}$.
- **Primary Control Knobs (Zone 1 & 2)**:
  - **Row 1 ($Y = 21.59\text{ mm}$)**:
    - Col 0: `FREQ` (Coarse carrier timebase, calibrated default 60 Hz)
    - Col 1: `RANGE` (`TL1105` button at $Y=21.59\text{ mm}$, tri-color LED at $Y=15.50\text{ mm}$)
    - Col 2: `FINE` (Fine tune $\pm 10\%$)
    - Col 3: `SHAPE` (Continuous morph: Sphere/Toroid $\rightarrow$ Cube/Cylinder $\rightarrow$ Pyramid/Cone)
    - Col 4: `MORPH` (Continuous interpolation between 1st and 2nd variation)
  - **Row 2 ($Y = 34.50\text{ mm}$)**:
    - Col 0: `DENSITY` (Wireframe raster grid resolution: 2 to 64 scan rings, 2x increased upper ceiling)
    - Col 1: `DEPTH` (Z-depth perspective camera scaling: 2D orthographic to deep 3D perspective)
    - Col 2: `WINDOW` (Perimeter audio buffer time window: $0.5\text{ ms}$ to $500.0\text{ ms}$)
    - Col 3: `SPREAD` (Audio scope belt spread: 0% tight equatorial belt $\rightarrow$ 100% full surface normal)
    - Col 4: `AMP` (Waveform displacement amplitude, default 0%, max 100%)
  - **Row 3 ($Y = 47.50\text{ mm}$)**:
    - Col 0: `TWIST` (3D helical axis twist: $-100\%$ to $+100\%$)
    - Col 1: `ROT X` (Pitch rotation: $-180^\circ$ to $+180^\circ$)
    - Col 2: `ROT Y` (Yaw rotation: $-180^\circ$ to $+180^\circ$)
    - Col 3: `ROT Z` (Roll / In-plane rotation: $0^\circ$ to $360^\circ$)
    - Col 4: `STRETCH` (Polly logarithmic spiral & polar stretch: $-100\%$ to $+100\%$)
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

#### 3.2 3D Geometric Surface Families & Morphing
For beam orbit phase $\phi \in [0, 1)$ and wireframe density $K = 2 + 62 \cdot \text{Density}^2$:
- Parameter $u = 2\pi \cdot (\phi \cdot K \pmod 1)$ (meridian sweep)
- Parameter $v = \pi \cdot \phi$ (latitude progression)

1. **Family 1: Sphere $\leftrightarrow$ Toroid**:
   - Sphere: $x = \cos(u)\sin(v), y = \sin(u)\sin(v), z = \cos(v)$
   - Toroid (major radius $R_1 = 0.70$, tube radius $R_2 = 0.35$):
     $x = (R_1 + R_2\cos(u))\cos(v), y = (R_1 + R_2\cos(u))\sin(v), z = R_2\sin(u)$
   - Morph blends position and surface normals smoothly.

2. **Family 2: Cube $\leftrightarrow$ Cylinder (Exact Sharp Corners)**:
   - Cube: Exact ray projection onto 3D unit cube $[-0.85, 0.85]^3$:
     $$\vec{P}_{\text{cube}} = 0.85 \cdot \frac{(\cos(u)\sin(v), \sin(u)\sin(v), \cos(v))}{\max(|\cos(u)\sin(v)|, |\sin(u)\sin(v)|, |\cos(v)|)}$$
     Produces razor-sharp corners and flat faces across all angles.
   - Cylinder: Exact ray projection onto unit cylinder with flat endcaps:
     $$\vec{P}_{\text{cyl}} = 0.85 \cdot \frac{(\cos(u)\sin(v), \sin(u)\sin(v), \cos(v))}{\max(\sin(v), |\cos(v)|)}$$
   - Morph blends position and surface normals smoothly.

3. **Family 3: Pyramid $\leftrightarrow$ Cone (Exact Sharp Corners)**:
   - Height fraction $h = v / \pi \in [0, 1]$, $z = (1 - h) \cdot 1.7 - 0.85$, $R(h) = h \cdot 0.85$.
   - Denominator blends from exact square $\max(|\cos(u)|, |\sin(u)|)$ at Morph 0 to circle $1.0$ at Morph 1.

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

#### 3.5 3D Euler Rotations & Output Scaling
- Rotates $\vec{P}$ by angles $\theta_X$ (Pitch), $\theta_Y$ (Yaw), and $\theta_Z$ (Roll / Rot Z):
  $$\vec{P}' = \mathbf{R}_z(\theta_Z) \mathbf{R}_y(\theta_Y) \mathbf{R}_x(\theta_X) \vec{P}$$
- Perspective camera projection with focal distance $d_{\text{cam}} = 3.0$:
  $$z_{\text{eff}} = 1.0 + \text{Depth} \cdot \left(\frac{z'}{d_{\text{cam}}}\right)$$
- Output deflection: Standard Eurorack $\pm 5\text{V}$, clamped cleanly at $\pm 10\text{V}$ without non-linear cushion distortion.
