# Garmire Series Module Audit & Specifications Matrix

> **Plugin Version:** `2.26.0` | **Total Modules:** `54` | **Total Width:** `367 HP`

## 1. Executive Summary & Class Distribution

| Functional Class | Module Count | Percentage | Description |
| :--- | :---: | :---: | :--- |
| **Generate** | 11 | 20.4% | Autonomous trajectory oscillators, LFOs/drift, color voltage generators, complex polyphonic vector synthesizers |
| **Transform** | 28 | 51.9% | Geometric affine warpers, wavefolders, rosette multipliers, smoothing filters, dyadic bitwise processors |
| **Utility** | 12 | 22.2% | Multi-channel A/B routers, toggle switches, boolean logic, multiplexers, precision adders, normalizers, VCAs |
| **Control** | 3 | 5.6% | Organic dual sample & hold, dual-slope stepped slew/quantizers, 2D matrix crossfade scanners |

### Hardware Form Factor Distribution

| Width | Module Count | Percentage |
| :--- | :---: | :---: |
| **3 HP** | 7 | 13.0% |
| **6 HP** | 32 | 59.3% |
| **8 HP** | 8 | 14.8% |
| **10 HP** | 2 | 3.7% |
| **12 HP** | 3 | 5.6% |
| **14 HP** | 1 | 1.9% |
| **20 HP** | 1 | 1.9% |

---

## 2. Complete Module Audit Matrix

| Name | Version | Class | HP | VCV Rack Module Description | Tags |
| :--- | :---: | :---: | :---: | :--- | :--- |
| **Chromance** | `v2.10.0` | `Generate` | 10 | Single-strip laser channel oscillator with 3-position S&H/T&H mode, continuous waveform morphing, and hard sync | Visual, Utility, Low-frequency oscillator, Oscillator |
| **Ants** | `v1.0.2` | `Transform` | 6 | Vector trajectory dashed line and marching ants blanking generator | Visual, Utility, Dual |
| **HSV / HSL to RGB** | `v2.17.0` | `Generate` | 6 | CV-controllable color space bias generator with HSV/HSL mode toggle and linear RGB outputs | Visual, Utility |
| **OKLCH** | `v2.17.0` | `Generate` | 6 | Perceptually uniform color generator with Lightness, Chroma, and Hue inputs and linear RGB outputs | Visual, Utility |
| **RGB** | `v2.17.0` | `Generate` | 6 | Direct linear RGB voltage generator with CV modulation and real-time color preview bar | Visual, Utility |
| **Instability** | `v2.24.0` | `Generate` | 6 | Standalone 3-axis organic drift CV generator for frequency, phase, and amplitude drift | Visual, Utility, Low-frequency oscillator, Noise |
| **Scale (X/Y)** | `v1.0.2` | `Transform` | 6 | Dual X/Y laser trajectory voltage scale transformer with normalized CV modulation | Visual, Utility, Dual, Attenuator |
| **Position (X/Y)** | `v1.0.1` | `Transform` | 6 | Dual X/Y laser trajectory voltage position offset transformer with normalized CV modulation | Visual, Utility, Dual, Offset |
| **Rotate (X/Y)** | `v1.0.2` | `Transform` | 6 | Dual X/Y laser trajectory 2D rotation transformer with continuous auto-rotation frequency control | Visual, Utility, Dual |
| **Spin (X/Y)** | `v1.0.2` | `Transform` | 6 | Dual X/Y laser trajectory independent dual-axis spin frequency transformer | Visual, Utility, Dual |
| **Fold (X/Y)** | `v1.0.2` | `Transform` | 6 | Dual X/Y laser trajectory quadratic x^2 voltage wavefolder transformer | Visual, Utility, Dual, Waveshaper |
| **Stretch (X/Y)** | `v1.0.2` | `Transform` | 6 | Dual X/Y laser trajectory cubic x^3 voltage stretch transformer | Visual, Utility, Dual, Waveshaper |
| **Shear (X/Y)** | `v1.0.0` | `Transform` | 6 | Dual X/Y laser trajectory 2D affine shear transformer with normalized CV modulation | Visual, Utility, Dual |
| **Smooth (X/Y)** | `v1.0.4` | `Transform` | 6 | Dual X/Y laser trajectory exponential 1-pole lowpass corner smoother and inertia filter | Visual, Utility, Dual, Filter, Slew rate limiter |
| **Wiggle (X/Y)** | `v1.0.2` | `Transform` | 6 | Dual X/Y laser trajectory organic multi-harmonic wobble and displacement engine | Visual, Utility, Dual, Low-frequency oscillator |
| **Crest (X/Y)** | `v1.0.5` | `Transform` | 6 | Dual X/Y 2-cycle trigonometric waveshaper and harmonic trajectory folder | Visual, Utility, Dual, Waveshaper |
| **Quake (X/Y)** | `v1.0.2` | `Transform` | 6 | Dual X/Y laser trajectory organic timing and micro-jitter humanizer engine | Visual, Utility, Dual, Noise |
| **Steps (X/Y)** | `v1.0.1` | `Transform` | 6 | Dual X/Y laser trajectory unquantized continuous step quantizer and resolution reducer | Visual, Utility, Dual, Digital |
| **Copycat** | `v1.0.8` | `Transform` | 6 | Multi-pass vector duplicator and spatial shape copier with spread and rotation offset | Visual, Utility, Dual, Delay |
| **Petals** | `v1.0.1` | `Transform` | 6 | Dual X/Y rotational vector unfold and angular 16-segment petal multiplier transformer | Visual, Utility, Dual, Waveshaper |
| **Knots** | `v2.25.0` | `Transform` | 6 | Dual X/Y polar angle harmonic multiplier and Lissajous knot transformer | Visual, Utility, Dual, Waveshaper |
| **Vortex** | `v1.0.1` | `Transform` | 6 | Dual X/Y angular phase multiplier and multi-centered rosette swirl engine | Visual, Utility, Dual, Waveshaper |
| **Trough (X/Y)** | `v2.17.0` | `Transform` | 6 | Dual X/Y lower half-space center-clipping trough waveshaper and harmonic interpolator | Visual, Utility, Dual, Waveshaper |
| **Switch (X/Y)** | `v2.19.0` | `Utility` | 3 | CV-controlled dual X/Y A/B router passing main X/Y inputs when track pulse is LOW and alternate X/Y inputs when HIGH | Visual, Utility, Dual, Switch, Logic |
| **Route (X/Y)** | `v2.20.0` | `Utility` | 3 | Dual X/Y 1:2 alternating toggle router and Bernoulli gate routing inputs between two output pairs on each clock/switch pulse | Visual, Utility, Dual, Switch, Logic |
| **Sum/Mult** | `v2.21.0` | `Utility` | 3 | Dual X/Y precision adder and four-quadrant multiplier for simultaneous sum and product trajectory modulation | Visual, Utility, Dual, Ring Modulator, Logic |
| **Switch XL** | `v2.22.0` | `Utility` | 8 | 6-channel dual CV switch and A/B router for full laser trajectory, color, and intensity signals | Visual, Utility, Switch, Logic |
| **Route XL** | `v2.22.0` | `Utility` | 8 | 6-channel 1:2 alternating toggle router and Bernoulli gate for full laser trajectory, color, and intensity signals | Visual, Utility, Switch, Logic |
| **Sum/Mix XL** | `v2.22.0` | `Utility` | 8 | 6-channel precision adder and four-quadrant multiplier for simultaneous sum and product modulation of laser trajectory, color, and intensity | Visual, Utility, Ring Modulator, Logic |
| **Andxy** | `v2.23.0` | `Utility` | 3 | Dual X/Y rapid switching boolean AND / NAND router passing Input 2 when condition is met and Input 1 otherwise | Visual, Utility, Dual, Logic |
| **Orxy** | `v2.23.0` | `Utility` | 3 | Dual X/Y rapid switching boolean OR / NOR router passing Input 2 when condition is met and Input 1 otherwise | Visual, Utility, Dual, Logic |
| **Xorxy** | `v2.23.0` | `Utility` | 3 | Dual X/Y rapid switching boolean XOR / XNOR router passing Input 2 when condition is met and Input 1 otherwise | Visual, Utility, Dual, Logic |
| **Chop XL** | `v2.24.0` | `Utility` | 12 | 6-channel alternating segment multiplexer slicing trajectories between two XYRGBI input sets with count, length, position, and variety controls | Visual, Utility, Switch |
| **Rescale** | `v2.25.0` | `Utility` | 6 | Multi-range precision voltage scaler, normalizer, and converter across 1V, 5V, and 10V unipolar and bipolar standards | Visual, Utility, Dual |
| **Bleed** | `v2.25.0` | `Transform` | 8 | Cross-channel X/Y mix and crosstalk module with per-channel bleed control and phase-offset feedback loop | Visual, Utility, Dual, Waveshaper |
| **Lisa** | `v2.25.0` | `Generate` | 8 | Lissajous curve and harmonic orbital trajectory generator with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO), carrier-normalized linear FM, X/Y harmonic ratios, bipolar phase shift, and stretch control | Visual, Oscillator, Dual |
| **Daisy** | `v2.25.0` | `Generate` | 8 | Rhodonea rose curve and Limaçon trajectory generator with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO), carrier-normalized linear FM, harmonic petal count, center offset, bipolar phase shift, and stretch control | Visual, Oscillator, Dual |
| **Polly** | `v2.25.0` | `Generate` | 14 | Polygon vector synthesizer and geometric oscillator with variable vertex sides, rotation, pinch/twist secondary vertices, corner bevel, 5-stage serial vertex distribution (Split, Pair, Trio, Group, Bunch), bipolar phase shift, and logarithmic stretch | Visual, Oscillator, Dual |
| **Circe** | `v2.25.0` | `Generate` | 8 | Geometric circle, ellipse, and crescent vector trajectory generator with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO), carrier-normalized linear FM, knife-edge Crop (half-circles), intersecting Circle Eclipse (C-shapes), bipolar Stretch (aspect ratio), and bipolar Phase orientation | Visual, Oscillator, Dual |
| **Maude** | `v2.25.0` | `Generate` | 12 | Subharmonic ring modulator and folding orbital rosette vector generator with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO), carrier waveshape morph, 1-12 integer divisions, 1-12 ring modulation ratio, continuous depth crossfade, dedicated Split (bifurcation), Fold (wavefolding), Twist (quadrature skew), and bipolar Stretch | Visual, Oscillator, Utility, Ring Modulator, Dual |
| **Eunice** | `v2.26.0` | `Control` | 8 | Organic dual sample/track & hold with internal noise-jittered triangle generator, distribution tilt shaping, correlation feedback, and asymmetrical slew limiter | Sample and hold, Random, Polyphonic, Utility |
| **Stepped Slew** | `v2.26.0` | `Control` | 10 | Asymmetric dual-slope slew limiter with variable curvature shaping, microtonal/chromatic quantization stepping, and event gates | Slew limiter, Quantizer, Dual, Utility |
| **Bit Morton** | `v2.26.0` | `Transform` | 6 | 6 HP 10-bit Morton space-filling Z-order trajectory interleaver and Hilbert quadrant folding | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Reverse** | `v2.26.0` | `Transform` | 6 | 6 HP 10-bit bitwise reversal and dyadic coordinate reflection with skew warping | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Transpose** | `v2.26.0` | `Transform` | 6 | 6 HP 10-bit bit-plane transposition and 3-way permutation cyclic routing | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Valanche** | `v2.26.0` | `Transform` | 6 | 6 HP 10-bit carry-propagated avalanche cross-modulation and ripple-borrow shearing | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Permute** | `v2.26.0` | `Transform` | 6 | 6 HP 20-bit circular permutation matrix with odd/even split, perfect shuffle, and stride cycles | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Graybin** | `v2.26.0` | `Transform` | 6 | 6 HP 10-bit binary-to-Gray and Gray-to-binary reflected code dyadic folder | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Galois** | `v2.26.0` | `Transform` | 6 | 6 HP GF(2^10) Galois field irreducible polynomial scrambler, inverter, and cryptographic S-box | Visual, Waveshaper, Digital, Dual, Utility |
| **Bitomata** | `v2.26.0` | `Transform` | 6 | 6 HP 1D elementary cellular automata mesh with cross-axis seeding and iterative evolution | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Hamming** | `v2.26.0` | `Transform` | 6 | 6 HP 10-bit popcount and Hamming distance metric cross-axis vector disperser | Visual, Waveshaper, Digital, Dual, Utility |
| **RGB/I/VCA** | `v2.26.0` | `Utility` | 3 | 3HP Triple VCA for laser Z/intensity control and RGB signal scaling | Visual, Utility, VCA, Polyphonic |
| **Matrix 3x3** | `v2.26.0` | `Control` | 12 | 12 HP 2D spatial crossfade scanner and voltage distributor with 3x3 LED matrix display, bleed dispersion, and per-cell attenuverters | Visual, Utility, Dual, Attenuator |
| **Ampara** | `v2.26.0` | `Generate` | 20 | 16 HP Audio-reactive vector polygon synthesizer and oscilloscope displacement generator with 3-range carrier oscillator, dual envelope followers, Milkdrop polar warp, and stereo morphing | Visual, Oscillator, Low-frequency oscillator, Polyphonic, Waveshaper |

---

## 3. Class-Grouped Directory

### Generate Modules (11 modules, 104 HP)

| Name | Version | HP | Key Capabilities | Tags |
| :--- | :---: | :---: | :--- | :--- |
| **Chromance** | `v2.10.0` | 10 HP | Single-strip laser channel oscillator with 3-position S&H/T&H mode, continuous waveform morphing, and hard sync | Visual, Utility, Low-frequency oscillator, Oscillator |
| **HSV / HSL to RGB** | `v2.17.0` | 6 HP | CV-controllable color space bias generator with HSV/HSL mode toggle and linear RGB outputs | Visual, Utility |
| **OKLCH** | `v2.17.0` | 6 HP | Perceptually uniform color generator with Lightness, Chroma, and Hue inputs and linear RGB outputs | Visual, Utility |
| **RGB** | `v2.17.0` | 6 HP | Direct linear RGB voltage generator with CV modulation and real-time color preview bar | Visual, Utility |
| **Instability** | `v2.24.0` | 6 HP | Standalone 3-axis organic drift CV generator for frequency, phase, and amplitude drift | Visual, Utility, Low-frequency oscillator, Noise |
| **Lisa** | `v2.25.0` | 8 HP | Lissajous curve and harmonic orbital trajectory generator with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO), carrier-normalized linear FM, X/Y harmonic ratios, bipolar phase shift, and stretch control | Visual, Oscillator, Dual |
| **Daisy** | `v2.25.0` | 8 HP | Rhodonea rose curve and Limaçon trajectory generator with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO), carrier-normalized linear FM, harmonic petal count, center offset, bipolar phase shift, and stretch control | Visual, Oscillator, Dual |
| **Polly** | `v2.25.0` | 14 HP | Polygon vector synthesizer and geometric oscillator with variable vertex sides, rotation, pinch/twist secondary vertices, corner bevel, 5-stage serial vertex distribution (Split, Pair, Trio, Group, Bunch), bipolar phase shift, and logarithmic stretch | Visual, Oscillator, Dual |
| **Circe** | `v2.25.0` | 8 HP | Geometric circle, ellipse, and crescent vector trajectory generator with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO), carrier-normalized linear FM, knife-edge Crop (half-circles), intersecting Circle Eclipse (C-shapes), bipolar Stretch (aspect ratio), and bipolar Phase orientation | Visual, Oscillator, Dual |
| **Maude** | `v2.25.0` | 12 HP | Subharmonic ring modulator and folding orbital rosette vector generator with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO), carrier waveshape morph, 1-12 integer divisions, 1-12 ring modulation ratio, continuous depth crossfade, dedicated Split (bifurcation), Fold (wavefolding), Twist (quadrature skew), and bipolar Stretch | Visual, Oscillator, Utility, Ring Modulator, Dual |
| **Ampara** | `v2.26.0` | 20 HP | 16 HP Audio-reactive vector polygon synthesizer and oscilloscope displacement generator with 3-range carrier oscillator, dual envelope followers, Milkdrop polar warp, and stereo morphing | Visual, Oscillator, Low-frequency oscillator, Polyphonic, Waveshaper |

### Transform Modules (28 modules, 170 HP)

| Name | Version | HP | Key Capabilities | Tags |
| :--- | :---: | :---: | :--- | :--- |
| **Ants** | `v1.0.2` | 6 HP | Vector trajectory dashed line and marching ants blanking generator | Visual, Utility, Dual |
| **Scale (X/Y)** | `v1.0.2` | 6 HP | Dual X/Y laser trajectory voltage scale transformer with normalized CV modulation | Visual, Utility, Dual, Attenuator |
| **Position (X/Y)** | `v1.0.1` | 6 HP | Dual X/Y laser trajectory voltage position offset transformer with normalized CV modulation | Visual, Utility, Dual, Offset |
| **Rotate (X/Y)** | `v1.0.2` | 6 HP | Dual X/Y laser trajectory 2D rotation transformer with continuous auto-rotation frequency control | Visual, Utility, Dual |
| **Spin (X/Y)** | `v1.0.2` | 6 HP | Dual X/Y laser trajectory independent dual-axis spin frequency transformer | Visual, Utility, Dual |
| **Fold (X/Y)** | `v1.0.2` | 6 HP | Dual X/Y laser trajectory quadratic x^2 voltage wavefolder transformer | Visual, Utility, Dual, Waveshaper |
| **Stretch (X/Y)** | `v1.0.2` | 6 HP | Dual X/Y laser trajectory cubic x^3 voltage stretch transformer | Visual, Utility, Dual, Waveshaper |
| **Shear (X/Y)** | `v1.0.0` | 6 HP | Dual X/Y laser trajectory 2D affine shear transformer with normalized CV modulation | Visual, Utility, Dual |
| **Smooth (X/Y)** | `v1.0.4` | 6 HP | Dual X/Y laser trajectory exponential 1-pole lowpass corner smoother and inertia filter | Visual, Utility, Dual, Filter, Slew rate limiter |
| **Wiggle (X/Y)** | `v1.0.2` | 6 HP | Dual X/Y laser trajectory organic multi-harmonic wobble and displacement engine | Visual, Utility, Dual, Low-frequency oscillator |
| **Crest (X/Y)** | `v1.0.5` | 6 HP | Dual X/Y 2-cycle trigonometric waveshaper and harmonic trajectory folder | Visual, Utility, Dual, Waveshaper |
| **Quake (X/Y)** | `v1.0.2` | 6 HP | Dual X/Y laser trajectory organic timing and micro-jitter humanizer engine | Visual, Utility, Dual, Noise |
| **Steps (X/Y)** | `v1.0.1` | 6 HP | Dual X/Y laser trajectory unquantized continuous step quantizer and resolution reducer | Visual, Utility, Dual, Digital |
| **Copycat** | `v1.0.8` | 6 HP | Multi-pass vector duplicator and spatial shape copier with spread and rotation offset | Visual, Utility, Dual, Delay |
| **Petals** | `v1.0.1` | 6 HP | Dual X/Y rotational vector unfold and angular 16-segment petal multiplier transformer | Visual, Utility, Dual, Waveshaper |
| **Knots** | `v2.25.0` | 6 HP | Dual X/Y polar angle harmonic multiplier and Lissajous knot transformer | Visual, Utility, Dual, Waveshaper |
| **Vortex** | `v1.0.1` | 6 HP | Dual X/Y angular phase multiplier and multi-centered rosette swirl engine | Visual, Utility, Dual, Waveshaper |
| **Trough (X/Y)** | `v2.17.0` | 6 HP | Dual X/Y lower half-space center-clipping trough waveshaper and harmonic interpolator | Visual, Utility, Dual, Waveshaper |
| **Bleed** | `v2.25.0` | 8 HP | Cross-channel X/Y mix and crosstalk module with per-channel bleed control and phase-offset feedback loop | Visual, Utility, Dual, Waveshaper |
| **Bit Morton** | `v2.26.0` | 6 HP | 6 HP 10-bit Morton space-filling Z-order trajectory interleaver and Hilbert quadrant folding | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Reverse** | `v2.26.0` | 6 HP | 6 HP 10-bit bitwise reversal and dyadic coordinate reflection with skew warping | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Transpose** | `v2.26.0` | 6 HP | 6 HP 10-bit bit-plane transposition and 3-way permutation cyclic routing | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Valanche** | `v2.26.0` | 6 HP | 6 HP 10-bit carry-propagated avalanche cross-modulation and ripple-borrow shearing | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Permute** | `v2.26.0` | 6 HP | 6 HP 20-bit circular permutation matrix with odd/even split, perfect shuffle, and stride cycles | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Graybin** | `v2.26.0` | 6 HP | 6 HP 10-bit binary-to-Gray and Gray-to-binary reflected code dyadic folder | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Galois** | `v2.26.0` | 6 HP | 6 HP GF(2^10) Galois field irreducible polynomial scrambler, inverter, and cryptographic S-box | Visual, Waveshaper, Digital, Dual, Utility |
| **Bitomata** | `v2.26.0` | 6 HP | 6 HP 1D elementary cellular automata mesh with cross-axis seeding and iterative evolution | Visual, Waveshaper, Digital, Dual, Utility |
| **Bit Hamming** | `v2.26.0` | 6 HP | 6 HP 10-bit popcount and Hamming distance metric cross-axis vector disperser | Visual, Waveshaper, Digital, Dual, Utility |

### Utility Modules (12 modules, 63 HP)

| Name | Version | HP | Key Capabilities | Tags |
| :--- | :---: | :---: | :--- | :--- |
| **Switch (X/Y)** | `v2.19.0` | 3 HP | CV-controlled dual X/Y A/B router passing main X/Y inputs when track pulse is LOW and alternate X/Y inputs when HIGH | Visual, Utility, Dual, Switch, Logic |
| **Route (X/Y)** | `v2.20.0` | 3 HP | Dual X/Y 1:2 alternating toggle router and Bernoulli gate routing inputs between two output pairs on each clock/switch pulse | Visual, Utility, Dual, Switch, Logic |
| **Sum/Mult** | `v2.21.0` | 3 HP | Dual X/Y precision adder and four-quadrant multiplier for simultaneous sum and product trajectory modulation | Visual, Utility, Dual, Ring Modulator, Logic |
| **Switch XL** | `v2.22.0` | 8 HP | 6-channel dual CV switch and A/B router for full laser trajectory, color, and intensity signals | Visual, Utility, Switch, Logic |
| **Route XL** | `v2.22.0` | 8 HP | 6-channel 1:2 alternating toggle router and Bernoulli gate for full laser trajectory, color, and intensity signals | Visual, Utility, Switch, Logic |
| **Sum/Mix XL** | `v2.22.0` | 8 HP | 6-channel precision adder and four-quadrant multiplier for simultaneous sum and product modulation of laser trajectory, color, and intensity | Visual, Utility, Ring Modulator, Logic |
| **Andxy** | `v2.23.0` | 3 HP | Dual X/Y rapid switching boolean AND / NAND router passing Input 2 when condition is met and Input 1 otherwise | Visual, Utility, Dual, Logic |
| **Orxy** | `v2.23.0` | 3 HP | Dual X/Y rapid switching boolean OR / NOR router passing Input 2 when condition is met and Input 1 otherwise | Visual, Utility, Dual, Logic |
| **Xorxy** | `v2.23.0` | 3 HP | Dual X/Y rapid switching boolean XOR / XNOR router passing Input 2 when condition is met and Input 1 otherwise | Visual, Utility, Dual, Logic |
| **Chop XL** | `v2.24.0` | 12 HP | 6-channel alternating segment multiplexer slicing trajectories between two XYRGBI input sets with count, length, position, and variety controls | Visual, Utility, Switch |
| **Rescale** | `v2.25.0` | 6 HP | Multi-range precision voltage scaler, normalizer, and converter across 1V, 5V, and 10V unipolar and bipolar standards | Visual, Utility, Dual |
| **RGB/I/VCA** | `v2.26.0` | 3 HP | 3HP Triple VCA for laser Z/intensity control and RGB signal scaling | Visual, Utility, VCA, Polyphonic |

### Control Modules (3 modules, 30 HP)

| Name | Version | HP | Key Capabilities | Tags |
| :--- | :---: | :---: | :--- | :--- |
| **Eunice** | `v2.26.0` | 8 HP | Organic dual sample/track & hold with internal noise-jittered triangle generator, distribution tilt shaping, correlation feedback, and asymmetrical slew limiter | Sample and hold, Random, Polyphonic, Utility |
| **Stepped Slew** | `v2.26.0` | 10 HP | Asymmetric dual-slope slew limiter with variable curvature shaping, microtonal/chromatic quantization stepping, and event gates | Slew limiter, Quantizer, Dual, Utility |
| **Matrix 3x3** | `v2.26.0` | 12 HP | 12 HP 2D spatial crossfade scanner and voltage distributor with 3x3 LED matrix display, bleed dispersion, and per-cell attenuverters | Visual, Utility, Dual, Attenuator |

