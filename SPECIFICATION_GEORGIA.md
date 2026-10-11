# Georgia — 3D Rotating Wireframe Vector Synthesizer
## Formal Engineering Specification (18 HP)

---

### 1. Architectural Overview & Product Identity

**Georgia** is an 18 HP ($91.44\text{ mm}$) Generate-class 3D rotating wireframe vector synthesizer and audio-reactive oscilloscope displacement engine for VCV Rack v2 and the 4ms MetaModule hardware ecosystem. It generates high-precision $(X(t), Y(t))$ beam trajectories derived from 3D parametric wireframe geometric surfaces (Sphere/Torus, Cube/Cylinder, Pyramid/Cone) with continuous shape morphing, perspective camera depth projection, 3D Euler rotations, Polly logarithmic stretch, and dual-channel audio waveform displacement.

- **Width**: 18 HP ($91.44\text{ mm}$ / $171\text{ px}$ MetaModule bitmap width).
- **Height**: Standard Eurorack 3U ($128.50\text{ mm}$ / $240\text{ px}$ MetaModule bitmap height).
- **Panel Background**: `#6e6e6e` dark neutral gray.
- **Palette Badge**: Left-aligned $2.54\text{ mm} \times 88.90\text{ mm}$ rectangle with `#5d5d5d` initial placeholder.
- **Polyphony**: 1 to 16 voices dynamically allocated based on connected inputs.
- **Safety Standard**: Real-time safe DSP core (`src/core/GeorgiaCore.hpp`) with zero heap allocations in `process()`, zero system locks, zero file I/O, and no external dependencies.

---

### 2. Panel Architecture & Coordinate Layout

Following repository layout invariants:
- **Module Title**: Baseline $Y = 7.620\text{ mm}$, centered at $X = 45.720\text{ mm}$ in lowercase `res/Node.otf` (scale 0.0048).
- **Version Tag**: Baseline $Y = 10.414\text{ mm}$, centered at $X = 45.720\text{ mm}$ in `res/Quicksand-Regular.ttf` (`v1.0.0`, scale 0.0016).
- **5 Columns across 18 HP ($91.44\text{ mm}$)**:
  - Margins: $13.52\text{ mm}$ left and right.
  - Pitch: $\Delta X = 16.10\text{ mm}$ center-to-center.
  - Columns: $X = [13.52, 29.62, 45.72, 61.82, 77.92]\text{ mm}$.
- **Primary Control Knobs (Zone 1 & 2)**:
  - **Row 1 ($Y = 21.59\text{ mm}$)**:
    - Col 0: `FREQ` (Coarse carrier timebase, default 60 Hz)
    - Col 1: `RANGE` (`TL1105` button at $Y=21.59\text{ mm}$, tri-color LED at $Y=15.50\text{ mm}$)
    - Col 2: `FINE` (Fine tune $\pm 10\%$)
    - Col 3: `SHAPE` (Continuous morph: Sphere/Torus $\rightarrow$ Cube/Cylinder $\rightarrow$ Pyramid/Cone)
    - Col 4: `MORPH` (Continuous interpolation between 1st and 2nd variation)
  - **Row 2 ($Y = 36.50\text{ mm}$)**:
    - Col 0: `DENSITY` (Wireframe raster grid resolution: 2 to 32 scan rings)
    - Col 1: `DEPTH` (Z-depth scaling: 2D orthographic to deep 3D perspective)
    - Col 2: `WINDOW` (Perimeter audio buffer time window: $0.5\text{ ms}$ to $500.0\text{ ms}$)
    - Col 3: `ROUTING` (Stereo audio routing: L+R Average $\rightarrow$ Decoupled L/R)
    - Col 4: `AMP` (Waveform displacement amplitude, capped at $175\%$)
  - **Row 3 ($Y = 51.50\text{ mm}$)**:
    - Col 0: `FOV` (Field of view / camera focal distance)
    - Col 1: `ROT X` (Pitch rotation: $-180^\circ$ to $+180^\circ$)
    - Col 2: `ROT Y` (Yaw rotation: $-180^\circ$ to $+180^\circ$)
    - Col 3: `PHASE` (Roll / In-plane rotation: $0^\circ$ to $360^\circ$)
    - Col 4: `STRETCH` (Polly logarithmic spiral stretch: $-100\%$ to $+100\%$)
- **Delineator Line 1**: $Y = 59.50\text{ mm}$.
- **Attenuverter Trimpots (Zone 3)**:
  - **Trim Row 1 ($Y = 66.00\text{ mm}$)**: `LEFT IN`, `RIGHT IN`, `FREQ`, `SHAPE`, `MORPH`
  - **Trim Row 2 ($Y = 76.00\text{ mm}$)**: `DENSITY`, `DEPTH`, `WINDOW`, `ROUTING`, `AMP`
- **Delineator Line 2**: $Y = 83.50\text{ mm}$.
- **I/O Jacks (Zone 4 - Bottom-Aligned Upward)**:
  - **Jack Row 1 ($Y = 89.50\text{ mm}$)**: `LEFT IN`, `RIGHT IN`, `FREQ` CV, `SHAPE` CV, `MORPH` CV
  - **Jack Row 2 ($Y = 99.00\text{ mm}$)**: `DENSITY` CV, `DEPTH` CV, `WINDOW` CV, `ROUTING` CV, `AMP` CV
  - **Jack Row 3 ($Y = 108.50\text{ mm}$)**: `FOV` CV, `ROT X` CV, `ROT Y` CV, `PHASE` CV, `STRETCH` CV
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
For beam orbit phase $\phi \in [0, 1)$ and wireframe density $K = 2 + 30 \cdot \text{Density}$:
- Parameter $u = 2\pi \cdot (\phi \cdot K \pmod 1)$ (meridian sweep)
- Parameter $v = \pi \cdot \phi$ (latitude progression)

1. **Family 1: Sphere $\leftrightarrow$ Torus**:
   - Sphere: $x = \cos(u)\sin(v), y = \sin(u)\sin(v), z = \cos(v)$
   - Torus (major radius $R_1 = 0.7$, tube radius $R_2 = 0.3$):
     $x = (R_1 + R_2\cos(u))\cos(2v), y = (R_1 + R_2\cos(u))\sin(2v), z = R_2\sin(u)$
   - Morph blends position and surface normals smoothly.

2. **Family 2: Cube $\leftrightarrow$ Cylinder**:
   - Superquadric formulation smoothly morphing square cross-sections into circular rings with flat/closed endcaps.

3. **Family 3: Pyramid $\leftrightarrow$ Cone**:
   - Linear apex taper from square base (Pyramid) to circular base (Cone).

#### 3.3 Scope Waveform Displacement Modes (Context Menu Selectable)
1. **Mode 1: Surface Normal Displacement (Default)**:
   Displaces along the 3D unit normal $\vec{n} = (n_x, n_y, n_z)$:
   $$\vec{P}_{\text{audio}} = \vec{P} + \vec{n} \cdot (\text{waveDisp} \cdot \text{ampGain})$$
2. **Mode 2: Equatorial Scope Belt**:
   Displays the audio trace around the primary wireframe equator.
3. **Mode 3: Z-Depth Relief**:
   Extrudes the $z$-coordinate directly with the audio waveform.

#### 3.4 3D Euler Rotations
Rotates $\vec{P}_{\text{audio}}$ by angles $\theta_X$ (Pitch), $\theta_Y$ (Yaw), and $\theta_Z$ (Roll / Phase):
$$\vec{P}' = \mathbf{R}_z(\theta_Z) \mathbf{R}_y(\theta_Y) \mathbf{R}_x(\theta_X) \vec{P}$$

#### 3.5 Perspective Camera Projection
With camera distance $d_{\text{cam}} = 2.0 + 3.0 \cdot \text{FOV}$ and depth factor $Z_{\text{depth}} \in [0, 1]$:
$$z_{\text{eff}} = 1.0 + Z_{\text{depth}} \cdot \frac{P'_z}{d_{\text{cam}}}$$
$$X_{\text{proj}} = \frac{P'_x}{z_{\text{eff}}}, \quad Y_{\text{proj}} = \frac{P'_y}{z_{\text{eff}}}$$
At $Z_{\text{depth}} = 0$, projection is perfectly orthographic (isometric 2D); at $Z_{\text{depth}} = 1.0$, deep 3D perspective foreshortening occurs.

#### 3.6 Pre-Amplitude Stretch & Balanced Amplitude Headroom
Polly's logarithmic spiral stretch deforms the projected coordinates:
$$\text{geomScale} = \frac{1.0}{1.0 + 0.35 \cdot \text{ampGain}}$$
Output voltages: Eurorack $\pm 5\text{V}$ ($10\text{Vpp}$), clamped safely at $\pm 10\text{V}$.
