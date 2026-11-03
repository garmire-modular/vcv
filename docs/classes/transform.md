# Functional Class Directives: Transform / Effect

This document defines specifications and guidelines for modules belonging to the **Transform** (or **Effect**) class (waveshapers, filters, slew limiters, spatial/geometric warpers, math operators, distortions, delays).

---

## 1. Scope & Identification

- **Tag in `plugin.json`**: `"Transform"` (or `"Effect"`, `"Filter"` alongside `"Transform"`).
- **Core Function**: Takes one or more input signals (audio or CV) and produces modified, transformed, or warped output signals.

---

## 2. DSP Guidelines & Best Practices

1. **Input Clamping & Headroom**:
   - Audio inputs are typically standard $\pm 5\text{ V}$ ($10\text{ V}_{\text{pp}}$) or $\pm 10\text{ V}$ in Eurorack.
   - Algorithms must gracefully handle out-of-range signals without NaN or infinity propagation. Use `rack::math::clamp` or soft saturation (e.g. `std::tanh` or polynomial approximations).

2. **Denormal & Zero Handling**:
   - Feedback filters and delay lines must prevent subnormal floating point numbers (denormals) that cause CPU spikes. Add small DC offsets or flush subnormals using `rack::dsp::denormal()`.

3. **Bypass / Dry-Wet Mixing**:
   - For effect-type modules, dry/wet crossfading should support equal-power ($3\text{ dB}$ dip compensation) for uncorrelated audio, or linear crossfading for CV / phase-coherent signals.

4. **CV Attenuation & Modulation**:
   - Transform parameters exposed to CV modulation should use attenuverters conforming to `<Parameter> CV depth` sentence case.
   - Check unpatched inputs: when a CV input is disconnected, default modulation depth to zero without altering knob base values.

5. **Real-Time Safety**:
   - No `malloc`, `free`, `new`, locks, mutexes, or I/O inside `process()`. Pre-allocate all buffers (e.g. delay lines, lookup tables) in constructor or `onSampleRateChange()`.
