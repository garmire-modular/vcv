# Functional Class Directives: Control

This document defines specifications and guidelines for modules belonging to the **Control** class (envelopes, CV sources, sequencers, macro controllers, LFOs, S&H, function generators).

---

## 1. Scope & Identification

- **Tag in `plugin.json`**: `"Control"` (or `"Envelope"`, `"Sequencer"`, `"LFO"` alongside `"Control"`).
- **Core Function**: Generates or manipulates control voltages, gates, triggers, or timing signals to direct other modules.

---

## 2. DSP & Control Guidelines

1. **Gate & Trigger Standards**:
   - Use `rack::dsp::SchmittTrigger` for robust gate/trigger detection on inputs with standard Eurorack thresholds ($0.1\text{ V}$ to $2.0\text{ V}$).
   - Output triggers should standardly deliver $+10\text{ V}$ pulses with a pulse duration of $\approx 1\text{ ms}$ (using `rack::dsp::PulseGenerator`).
   - Output gates should standardly deliver $+10\text{ V}$ (or $+5\text{ V}$ unipolar where specified).

2. **Time Constants & Curves**:
   - Envelopes (attack, decay, release) should offer exponential or variable curves for natural acoustic or percussive responses.
   - Guard against divide-by-zero when time parameters approach $0\text{ ms}$.

3. **State Persistence**:
   - Sequencers and macro controllers must persist their state (active step, pattern memory, bank indices) via `dataToJson()` and `dataFromJson()`.

4. **Polyphony & Multichannel CV**:
   - Distinguish monophonic control from polyphonic modulation channels. Respect `inputs[...].getChannels()` and drive matching output polyphony.
