# Functional Class Directives: Utility

This document defines specifications and guidelines for modules belonging to the **Utility** class (mixers, attenuverters, switches, mutes, routing matrices, splitters, logic operators, clock dividers).

---

## 1. Scope & Identification

- **Tag in `plugin.json`**: `"Utility"` (or `"Mixer"`, `"Switch"` alongside `"Utility"`).
- **Core Function**: Signal routing, switching, attenuation, summing, splitting, or logical conditioning.

---

## 2. DSP & Signal Guidelines

1. **Unity Gain Accuracy**:
   - Ensure $1.0\times$ gain matches exact unity without floating point drift or unintended scaling.
   - For mixers and attenuverters, center detents or default knob positions should hit exact mathematical zero or unity where appropriate.

2. **Jack Normalling & Cascading**:
   - Clearly define unpatched port behaviors. For example:
     - Normalled cascade inputs (Input 1 normalled to Input 2 when unpatched).
     - Fixed DC offsets (e.g. normalled $+5\text{ V}$ or $+10\text{ V}$ on unpatched CV inputs).
   - Test explicitly with `inputs[PORT].isConnected()`.

3. **Switch & Routing Transitions**:
   - Audio switches or mutes should apply micro-crossfades or short slew (~$1\text{ ms}$ to $5\text{ ms}$) to prevent harsh clicks and DC pops when switched mid-cycle.
   - CV switches may switch instantaneously if precise sample-accurate logic switching is intended.

4. **Polyphony**:
   - Utility modules should support multichannel polyphonic cables where practical, processing up to `rack::PORT_MAX_CHANNELS` (16 channels) and propagating channel counts accurately.
