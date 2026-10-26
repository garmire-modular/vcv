#include "plugin.hpp"
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

struct ReveriePort : app::SvgPort {
    ReveriePort() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
    }
};

struct ReverieKnob : RoundKnob {
    ReverieKnob() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
    }
};

struct ReverieSnapKnob : ReverieKnob {
    ReverieSnapKnob() {
        snap = true;
    }
};

struct ReverieTrimpot : RoundKnob {
    ReverieTrimpot() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
    }
};

struct ReverieButton : app::SvgSwitch {
    ReverieButton() {
        momentary = true;
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
        addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
    }
};

// Base64 helpers for JSON serialization of float wavetables
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = ((uint32_t)data[i]) << 16;
        if (i + 1 < len) n |= ((uint32_t)data[i + 1]) << 8;
        if (i + 2 < len) n |= ((uint32_t)data[i + 2]);

        out.push_back(b64_table[(n >> 18) & 63]);
        out.push_back(b64_table[(n >> 12) & 63]);
        out.push_back((i + 1 < len) ? b64_table[(n >> 6) & 63] : '=');
        out.push_back((i + 2 < len) ? b64_table[n & 63] : '=');
    }
    return out;
}

std::vector<uint8_t> base64_decode(const char* str) {
    std::vector<uint8_t> out;
    if (!str) return out;
    size_t len = std::strlen(str);
    out.reserve((len * 3) / 4);

    int val = 0, valb = -8;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)str[i];
        int d = -1;
        if (c >= 'A' && c <= 'Z') d = c - 'A';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52;
        else if (c == '+') d = 62;
        else if (c == '/') d = 63;
        else continue;

        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static const int CYCLE_OPTIONS[7] = {1, 8, 16, 24, 48, 96, 128};

} // anonymous namespace


struct Reverie : Module {
    enum ParamIds {
        FREQ_PARAM,
        RANGE_PARAM,
        FINE_PARAM,
        CYCLES_PARAM,
        REC_BUTTON_PARAM,
        MORPH_PARAM,
        X_SCAN_PARAM,
        Y_SCAN_PARAM,
        MORPH_ATTEN_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        SYNC_INPUT,
        X_INPUT,
        Y_INPUT,
        FREQ_CV_INPUT,
        R_INPUT,
        G_INPUT,
        B_INPUT,
        MORPH_CV_INPUT,
        Z_INPUT,
        X_SCAN_INPUT,
        Y_SCAN_INPUT,
        REC_GATE_INPUT,
        RANDOM_GATE_INPUT,
        ENUMS(PRESET_GATE_INPUT, 16),
        NUM_INPUTS
    };
    enum OutputIds {
        X_OUTPUT,
        Y_OUTPUT,
        R_OUTPUT,
        G_OUTPUT,
        B_OUTPUT,
        Z_OUTPUT,
        SYNC_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    enum SkinTheme {
        THEME_MEDIUM = 0,
        THEME_LIGHT = 1,
        THEME_DARK = 2
    };
    SkinTheme skinTheme = THEME_MEDIUM;

    static constexpr int NUM_SLOTS = 16;
    static constexpr int NUM_CHANNELS = 6; // X, Y, R, G, B, Z/I
    static constexpr int SAMPLES_PER_CYCLE = 4096;
    static constexpr int MAX_CYCLES = 128;
    static constexpr int MAX_CAPTURE_SAMPLES = 32768; // per channel during capture (~0.68s @ 48kHz, resampled per cycle)

    enum SlotState {
        STATE_EMPTY = 0,
        STATE_ARMED = 1,
        STATE_RECORDING = 2,
        STATE_RECORDED = 3
    };

    struct PresetSlot {
        SlotState state = STATE_EMPTY;
        int numCycles = 16;
        float originalFreq = 60.0f;
        // Downsampled/normalized wavetable: [channel][cycle * SAMPLES_PER_CYCLE + sampleIdx]
        std::vector<float> wave[NUM_CHANNELS];

        void allocate(int cycles) {
            numCycles = clamp(cycles, 1, MAX_CYCLES);
            size_t total = (size_t)numCycles * SAMPLES_PER_CYCLE;
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                wave[ch].assign(total, 0.f);
            }
        }

        void clear() {
            state = STATE_EMPTY;
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                wave[ch].clear();
            }
        }

        // Read interpolated sample for a continuous cycle index `cyclePhase`
        // where 1.0 unit of `cyclePhase` equals 1 single recorded cycle.
        float readSample(int ch, double cyclePhase) const {
            if (state != STATE_RECORDED || wave[ch].empty() || numCycles <= 0)
                return 0.f;
            size_t total = wave[ch].size();
            double cycleInSlot = std::fmod(cyclePhase, (double)numCycles);
            if (cycleInSlot < 0.0)
                cycleInSlot += (double)numCycles;
            double pos = cycleInSlot * (double)SAMPLES_PER_CYCLE;
            size_t i0 = ((size_t)pos) % total;
            size_t i1 = (i0 + 1) % total;
            float frac = (float)(pos - std::floor(pos));
            return wave[ch][i0] + frac * (wave[ch][i1] - wave[ch][i0]);
        }
    };

    PresetSlot slots[NUM_SLOTS];

    // Pre-allocated single-cycle scratch buffer for real-time capture
    std::vector<float> cycleScratch[NUM_CHANNELS];
    int captureWritePos = 0;
    int activeCaptureSlot = -1;
    int capturedCyclesCount = 0;
    int targetCaptureCycles = 16;
    float captureElapsedSeconds = 0.f;

    // Sync edge detector and frequency estimator
    dsp::SchmittTrigger syncTrigger;
    dsp::SchmittTrigger rangeTrigger;
    dsp::SchmittTrigger recButtonTrigger;
    dsp::SchmittTrigger recGateTrigger;
    dsp::SchmittTrigger randomGateTrigger;
    dsp::SchmittTrigger presetGateTriggers[NUM_SLOTS];
    dsp::PulseGenerator syncOutPulse;
    float syncPeriodTimer = 0.f;
    float measuredSyncFreq = 60.0f;
    int rangeMode = 1; // 0: Slow, 1: LFO (default), 2: Audio

    // Playback & Morphing state
    int selectedSlot = 0;
    float morphStartX = 0.f;
    float morphStartY = 0.f;
    float morphTargetX = 0.f;
    float morphTargetY = 0.f;
    float morphX = 0.f;
    float morphY = 0.f;
    float morphProgress = 1.0f;
    bool isMorphing = false;
    bool manualScanActive = false;
    float lastXScanKnob = 0.f;
    float lastYScanKnob = 0.f;

    // Continuous cycle phase (1.0 = 1 single cycle; wraps modulo 384.0, the LCM of {1,8,16,24,48,96,128})
    double cyclePhase = 0.0;

    struct CyclesParamQuantity : ParamQuantity {
        std::string getDisplayValueString() override {
            int idx = clamp((int)std::round(getValue()), 0, 6);
            return std::to_string(CYCLE_OPTIONS[idx]) + (CYCLE_OPTIONS[idx] == 1 ? " cycle" : " cycles");
        }
    };

    struct RangeParamQuantity : ParamQuantity {
        std::string getDisplayValueString() override {
            Reverie* m = dynamic_cast<Reverie*>(module);
            if (!m) return "Medium (0.01 - 200 Hz)";
            if (m->rangeMode == 0) return "Slow (5 min - 2 Hz)";
            if (m->rangeMode == 1) return "Medium (0.01 - 200 Hz)";
            return "Audio (10 - 20,000 Hz)";
        }
    };

    struct FreqParamQuantity : ParamQuantity {
        float computeFreq(int mode, float normVal, float fineVal) const {
            float fineMult = std::pow(2.f, (fineVal * 100.f) / 1200.f);
            if (mode == 0) {
                float p = (1.f / 300.f) * std::pow(600.f, normVal);
                return clamp(p * fineMult, 1.f / 300.f, 2.0f);
            } else if (mode == 1) {
                float p = 0.01f * std::pow(20000.f, normVal);
                return clamp(p * fineMult, 0.01f, 200.f);
            } else {
                float p = 10.f * std::pow(2000.f, normVal);
                return clamp(p * fineMult, 10.f, 20000.f);
            }
        }

        std::string getDisplayValueString() override {
            Reverie* mod = dynamic_cast<Reverie*>(module);
            int mode = mod ? mod->rangeMode : 1;
            float fineVal = mod ? mod->params[FINE_PARAM].getValue() : 0.f;
            float f = computeFreq(mode, getValue(), fineVal);
            if (f < 1.0f) {
                float period = 1.0f / std::max(f, 1e-6f);
                if (period >= 60.0f) {
                    return string::f("%.2f min (%.4f Hz)", period / 60.0f, f);
                }
                return string::f("%.2f s (%.3f Hz)", period, f);
            } else if (f >= 1000.0f) {
                return string::f("%.2f kHz", f / 1000.0f);
            }
            return string::f("%.2f Hz", f);
        }

        void setDisplayValueString(std::string s) override {
            Reverie* mod = dynamic_cast<Reverie*>(module);
            int mode = mod ? mod->rangeMode : 1;

            std::string clean;
            for (char c : s) {
                if (!std::isspace((unsigned char)c))
                    clean += (char)std::tolower((unsigned char)c);
            }
            if (clean.empty()) return;

            char* endptr = nullptr;
            float val = std::strtof(clean.c_str(), &endptr);
            if (endptr == clean.c_str() || val <= 0.f) return;

            std::string unit(endptr);
            float targetHz = val;
            if (unit == "ms" || unit == "msec" || unit == "millisecond" || unit == "milliseconds") {
                targetHz = 1000.0f / val;
            } else if (unit == "s" || unit == "sec" || unit == "secs" || unit == "second" || unit == "seconds") {
                targetHz = 1.0f / val;
            } else if (unit == "m" || unit == "min" || unit == "mins" || unit == "minute" || unit == "minutes") {
                targetHz = 1.0f / (val * 60.0f);
            } else if (unit == "khz") {
                targetHz = val * 1000.0f;
            } else if (unit == "hz" || unit.empty()) {
                targetHz = val;
            }

            float norm = 0.5f;
            if (mode == 0) {
                targetHz = clamp(targetHz, 1.f / 300.f, 2.0f);
                norm = std::log(targetHz * 300.f) / std::log(600.f);
            } else if (mode == 1) {
                targetHz = clamp(targetHz, 0.01f, 200.f);
                norm = std::log(targetHz / 0.01f) / std::log(20000.f);
            } else {
                targetHz = clamp(targetHz, 10.f, 20000.f);
                norm = std::log(targetHz / 10.f) / std::log(2000.f);
            }
            setValue(clamp(norm, 0.f, 1.f));
        }

        std::string getUnit() override {
            return "";
        }
    };

    Reverie() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        // Default FREQ_PARAM = 0.878321f -> 60.00 Hz in Medium (LFO) range
        configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.878321f, "Frequency");
        configParam<RangeParamQuantity>(RANGE_PARAM, 0.f, 1.f, 0.f, "Range time-scale");
        configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", " cents", 0.f, 100.f);

        // Default CYCLES_PARAM = 2.0f -> 16 cycles
        configParam<CyclesParamQuantity>(CYCLES_PARAM, 0.f, 6.f, 2.f, "Capture cycles");
        configParam(REC_BUTTON_PARAM, 0.f, 1.f, 0.f, "Record");
        configParam(MORPH_PARAM, 0.f, 10.f, 0.5f, "Morph time", " s");

        // Row 3 trimpot-sized knobs: X scan, Y scan, Morph time CV depth
        configParam(X_SCAN_PARAM, 0.f, 1.f, 0.f, "X scan");
        configParam(Y_SCAN_PARAM, 0.f, 1.f, 0.f, "Y scan");
        configParam(MORPH_ATTEN_PARAM, -1.f, 1.f, 0.f, "Morph time CV depth");

        configInput(SYNC_INPUT, "Sync");
        configInput(X_INPUT, "X");
        configInput(Y_INPUT, "Y");
        configInput(FREQ_CV_INPUT, "Frequency CV");
        configInput(R_INPUT, "R");
        configInput(G_INPUT, "G");
        configInput(B_INPUT, "B");
        configInput(MORPH_CV_INPUT, "Morph time CV");
        configInput(Z_INPUT, "Z/I");
        configInput(X_SCAN_INPUT, "X scan CV");
        configInput(Y_SCAN_INPUT, "Y scan CV");
        configInput(REC_GATE_INPUT, "Record gate");
        configInput(RANDOM_GATE_INPUT, "Random preset gate");

        for (int i = 0; i < NUM_SLOTS; ++i) {
            configInput(PRESET_GATE_INPUT + i, string::f("Preset %d gate", i + 1));
        }

        configOutput(X_OUTPUT, "X");
        configOutput(Y_OUTPUT, "Y");
        configOutput(R_OUTPUT, "R");
        configOutput(G_OUTPUT, "G");
        configOutput(B_OUTPUT, "B");
        configOutput(Z_OUTPUT, "Z/I");
        configOutput(SYNC_OUTPUT, "Sync");

        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            cycleScratch[ch].assign(MAX_CAPTURE_SAMPLES, 0.f);
        }
        for (int s = 0; s < NUM_SLOTS; ++s) {
            slots[s].allocate(16);
            slots[s].state = STATE_EMPTY;
        }
    }

    int getTargetCycles() {
        int idx = clamp((int)std::round(params[CYCLES_PARAM].getValue()), 0, 6);
        return CYCLE_OPTIONS[idx];
    }

    float computePlaybackFreq() {
        float freqNorm = params[FREQ_PARAM].getValue();
        float fineVal = params[FINE_PARAM].getValue();
        float fineMult = std::pow(2.f, (fineVal * 100.f) / 1200.f);
        float cvOct = inputs[FREQ_CV_INPUT].isConnected() ? clamp(inputs[FREQ_CV_INPUT].getVoltage(), -10.f, 10.f) : 0.f;
        float cvMult = std::pow(2.f, cvOct);

        if (rangeMode == 0) {
            float p = (1.f / 300.f) * std::pow(600.f, freqNorm);
            return clamp(p * fineMult * cvMult, 1.f / 300.f, 20.0f);
        } else if (rangeMode == 1) {
            float p = 0.01f * std::pow(20000.f, freqNorm);
            return clamp(p * fineMult * cvMult, 0.01f, 2000.f);
        } else {
            float p = 10.f * std::pow(2000.f, freqNorm);
            return clamp(p * fineMult * cvMult, 1.f, 24000.f);
        }
    }

    // Request capture on a slot
    void armSlotForCapture(int slotIdx) {
        if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return;
        targetCaptureCycles = getTargetCycles();
        slots[slotIdx].allocate(targetCaptureCycles);
        slots[slotIdx].state = STATE_ARMED;
        activeCaptureSlot = slotIdx;
        capturedCyclesCount = 0;
        captureWritePos = 0;
        captureElapsedSeconds = 0.f;
    }

    // Trigger record (via RECORD button or RECORD gate input):
    // If the currently selected slot is empty, record into it; otherwise find the first free slot.
    // If all 16 slots are full, nothing gets overwritten.
    void triggerRecordSlot() {
        if (activeCaptureSlot >= 0) return; // already recording/armed
        if (selectedSlot >= 0 && selectedSlot < NUM_SLOTS && slots[selectedSlot].state == STATE_EMPTY) {
            armSlotForCapture(selectedSlot);
            return;
        }
        for (int i = 0; i < NUM_SLOTS; ++i) {
            if (slots[i].state == STATE_EMPTY) {
                selectedSlot = i;
                int col = i % 4;
                int row = i / 4;
                morphX = morphTargetX = morphStartX = col / 3.0f;
                morphY = morphTargetY = morphStartY = row / 3.0f;
                isMorphing = false;
                manualScanActive = false;
                armSlotForCapture(i);
                return;
            }
        }
    }

    // Trigger random filled preset selection (via RANDOM gate input)
    void triggerRandomRecordedSlot() {
        int recordedIndices[NUM_SLOTS];
        int count = 0;
        for (int i = 0; i < NUM_SLOTS; ++i) {
            if (slots[i].state == STATE_RECORDED) {
                recordedIndices[count++] = i;
            }
        }
        if (count == 0) return;
        if (count == 1) {
            selectRecordedSlot(recordedIndices[0]);
            return;
        }
        // Pick a random filled slot different from selectedSlot if possible
        int pick = recordedIndices[random::u32() % count];
        if (pick == selectedSlot) {
            for (int tries = 0; tries < 4; ++tries) {
                int candidate = recordedIndices[random::u32() % count];
                if (candidate != selectedSlot) {
                    pick = candidate;
                    break;
                }
            }
        }
        selectRecordedSlot(pick);
    }

    // Select an empty slot for live preview without recording
    void selectEmptySlotForPreview(int slotIdx) {
        if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return;
        selectedSlot = slotIdx;
        int col = slotIdx % 4;
        int row = slotIdx / 4;
        morphX = morphTargetX = morphStartX = col / 3.0f;
        morphY = morphTargetY = morphStartY = row / 3.0f;
        morphProgress = 1.0f;
        isMorphing = false;
        manualScanActive = false;
    }

    // Select a recorded slot and start morphing to it
    void selectRecordedSlot(int slotIdx) {
        if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return;
        selectedSlot = slotIdx;
        int col = slotIdx % 4;
        int row = slotIdx / 4;
        float tx = col / 3.0f;
        float ty = row / 3.0f;

        morphStartX = morphX;
        morphStartY = morphY;
        morphTargetX = tx;
        morphTargetY = ty;
        morphProgress = 0.0f;
        isMorphing = true;
        manualScanActive = false;
    }

    void clearSlot(int slotIdx) {
        if (slotIdx < 0 || slotIdx >= NUM_SLOTS) return;
        if (activeCaptureSlot == slotIdx) {
            activeCaptureSlot = -1;
        }
        slots[slotIdx].clear();
    }

    void clearAllSlots() {
        activeCaptureSlot = -1;
        for (int i = 0; i < NUM_SLOTS; ++i) {
            slots[i].clear();
        }
    }

    // Resample single captured cycle of length `len` into slot's cycle index `cycleIdx`
    void commitCapturedCycle(int slotIdx, int cycleIdx, int len) {
        if (slotIdx < 0 || slotIdx >= NUM_SLOTS || len < 2) return;
        PresetSlot& slot = slots[slotIdx];
        if (cycleIdx < 0 || cycleIdx >= slot.numCycles) return;

        size_t baseOffset = (size_t)cycleIdx * SAMPLES_PER_CYCLE;
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            if (slot.wave[ch].size() < baseOffset + SAMPLES_PER_CYCLE) continue;
            const std::vector<float>& src = cycleScratch[ch];
            for (int i = 0; i < SAMPLES_PER_CYCLE; ++i) {
                float pos = ((float)i / (float)SAMPLES_PER_CYCLE) * (float)(len - 1);
                int i0 = (int)pos;
                int i1 = std::min(i0 + 1, len - 1);
                float frac = pos - (float)i0;
                slot.wave[ch][baseOffset + i] = src[i0] + frac * (src[i1] - src[i0]);
            }
        }
    }

    void process(const ProcessArgs& args) override {
        // Range button
        if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
            rangeMode = (rangeMode + 1) % 3;
        }

        // Record button or Record gate input
        bool recBtnTrig = recButtonTrigger.process(params[REC_BUTTON_PARAM].getValue() > 0.5f);
        bool recGateTrig = inputs[REC_GATE_INPUT].isConnected() && recGateTrigger.process(inputs[REC_GATE_INPUT].getVoltage());
        if (recBtnTrig || recGateTrig) {
            triggerRecordSlot();
        }

        // Random preset gate input
        if (inputs[RANDOM_GATE_INPUT].isConnected() && randomGateTrigger.process(inputs[RANDOM_GATE_INPUT].getVoltage())) {
            triggerRandomRecordedSlot();
        }

        // 4x4 Preset gate inputs: rising edge selects/morphs to that preset (or selects empty slot for preview)
        for (int i = 0; i < NUM_SLOTS; ++i) {
            if (inputs[PRESET_GATE_INPUT + i].isConnected()) {
                if (presetGateTriggers[i].process(inputs[PRESET_GATE_INPUT + i].getVoltage())) {
                    if (slots[i].state == STATE_RECORDED) {
                        selectRecordedSlot(i);
                    } else if (slots[i].state == STATE_EMPTY) {
                        selectEmptySlotForPreview(i);
                    }
                }
            }
        }

        // 1. Measure incoming Sync frequency and detect Sync trigger
        syncPeriodTimer += args.sampleTime;
        bool syncConnected = inputs[SYNC_INPUT].isConnected();
        bool syncRising = false;
        if (syncConnected) {
            syncRising = syncTrigger.process(inputs[SYNC_INPUT].getVoltage());
            if (syncRising) {
                if (syncPeriodTimer > 0.00005f && syncPeriodTimer < 10.0f) {
                    measuredSyncFreq = 1.0f / syncPeriodTimer;
                }
                syncPeriodTimer = 0.f;
            }
        } else {
            // Internal fallback sync trigger for un-synced capture based on current playback frequency
            float fPlay = computePlaybackFreq();
            float fallbackPeriod = 1.0f / std::max(fPlay, 0.01f);
            if (syncPeriodTimer >= fallbackPeriod) {
                syncRising = true;
                measuredSyncFreq = fPlay;
                syncPeriodTimer = 0.f;
            }
        }

        // Read live inputs
        float inVals[NUM_CHANNELS];
        inVals[0] = inputs[X_INPUT].getVoltage();
        inVals[1] = inputs[Y_INPUT].getVoltage();
        inVals[2] = inputs[R_INPUT].getVoltage();
        inVals[3] = inputs[G_INPUT].getVoltage();
        inVals[4] = inputs[B_INPUT].getVoltage();
        inVals[5] = inputs[Z_INPUT].getVoltage();

        // 2. Handle Capture State Machine
        bool isCurrentlyCapturing = (activeCaptureSlot >= 0 && activeCaptureSlot < NUM_SLOTS);
        if (isCurrentlyCapturing) {
            PresetSlot& slot = slots[activeCaptureSlot];

            if (slot.state == STATE_ARMED) {
                if (syncRising) {
                    slot.state = STATE_RECORDING;
                    capturedCyclesCount = 0;
                    captureWritePos = 0;
                    captureElapsedSeconds = 0.f;
                }
            } else if (slot.state == STATE_RECORDING) {
                captureElapsedSeconds += args.sampleTime;

                if (syncRising && captureWritePos > 1) {
                    commitCapturedCycle(activeCaptureSlot, capturedCyclesCount, captureWritePos);
                    capturedCyclesCount++;
                    captureWritePos = 0;

                    if (capturedCyclesCount >= slot.numCycles) {
                        slot.state = STATE_RECORDED;
                        if (captureElapsedSeconds > 0.0001f) {
                            slot.originalFreq = (float)slot.numCycles / captureElapsedSeconds;
                        } else {
                            slot.originalFreq = measuredSyncFreq;
                        }
                        int finishedSlot = activeCaptureSlot;
                        activeCaptureSlot = -1;
                        isCurrentlyCapturing = false;
                        // Automatically snap morph coordinates to the newly recorded slot
                        selectedSlot = finishedSlot;
                        int col = finishedSlot % 4;
                        int row = finishedSlot / 4;
                        morphX = morphTargetX = morphStartX = col / 3.0f;
                        morphY = morphTargetY = morphStartY = row / 3.0f;
                        isMorphing = false;
                        manualScanActive = false;
                    }
                }

                if (isCurrentlyCapturing && slot.state == STATE_RECORDING) {
                    if (captureWritePos < MAX_CAPTURE_SAMPLES) {
                        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                            cycleScratch[ch][captureWritePos] = inVals[ch];
                        }
                        captureWritePos++;
                    } else {
                        commitCapturedCycle(activeCaptureSlot, capturedCyclesCount, captureWritePos);
                        capturedCyclesCount++;
                        captureWritePos = 0;
                        if (capturedCyclesCount >= slot.numCycles) {
                            slot.state = STATE_RECORDED;
                            slot.originalFreq = measuredSyncFreq;
                            int finishedSlot = activeCaptureSlot;
                            activeCaptureSlot = -1;
                            isCurrentlyCapturing = false;
                            selectedSlot = finishedSlot;
                            int col = finishedSlot % 4;
                            int row = finishedSlot / 4;
                            morphX = morphTargetX = morphStartX = col / 3.0f;
                            morphY = morphTargetY = morphStartY = row / 3.0f;
                            isMorphing = false;
                            manualScanActive = false;
                        }
                    }
                }
            }
        }

        // Silent outputs while capturing (armed or recording)
        if (isCurrentlyCapturing) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                outputs[X_OUTPUT + ch].setVoltage(0.f);
            }
            outputs[SYNC_OUTPUT].setVoltage(0.f);
            return;
        }

        // Check if any slot is recorded
        bool anyRecorded = false;
        for (int s = 0; s < NUM_SLOTS; ++s) {
            if (slots[s].state == STATE_RECORDED) {
                anyRecorded = true;
                break;
            }
        }

        // Check if user moved X or Y scan knobs
        float xScanKnob = params[X_SCAN_PARAM].getValue();
        float yScanKnob = params[Y_SCAN_PARAM].getValue();
        if (std::fabs(xScanKnob - lastXScanKnob) > 1e-4f || std::fabs(yScanKnob - lastYScanKnob) > 1e-4f) {
            lastXScanKnob = xScanKnob;
            lastYScanKnob = yScanKnob;
            manualScanActive = true;
            isMorphing = false;
        }

        bool xScanCvConn = inputs[X_SCAN_INPUT].isConnected();
        bool yScanCvConn = inputs[Y_SCAN_INPUT].isConnected();

        // If an empty slot is currently selected for preview (and not morphing or scanning), pass through live inputs!
        if (!anyRecorded || (!isMorphing && !manualScanActive && !xScanCvConn && !yScanCvConn &&
            selectedSlot >= 0 && selectedSlot < NUM_SLOTS && slots[selectedSlot].state == STATE_EMPTY)) {
            outputs[X_OUTPUT].setVoltage(inVals[0]);
            outputs[Y_OUTPUT].setVoltage(inVals[1]);
            outputs[R_OUTPUT].setVoltage(inVals[2]);
            outputs[G_OUTPUT].setVoltage(inVals[3]);
            outputs[B_OUTPUT].setVoltage(inVals[4]);
            outputs[Z_OUTPUT].setVoltage(inVals[5]);
            outputs[SYNC_OUTPUT].setVoltage(inputs[SYNC_INPUT].getVoltage());
            return;
        }

        // 3. Determine 2D Morph/Scan Coordinates (morphX, morphY in [0, 1])
        if (manualScanActive || xScanCvConn || yScanCvConn) {
            isMorphing = false;
            float xCv = xScanCvConn ? (inputs[X_SCAN_INPUT].getVoltage() / 10.0f) : 0.f;
            float yCv = yScanCvConn ? (inputs[Y_SCAN_INPUT].getVoltage() / 10.0f) : 0.f;
            if (manualScanActive || xScanCvConn) {
                morphX = clamp(xScanKnob + xCv, 0.0f, 1.0f);
            }
            if (manualScanActive || yScanCvConn) {
                morphY = clamp(yScanKnob + yCv, 0.0f, 1.0f);
            }
        } else if (isMorphing) {
            float morphTime = params[MORPH_PARAM].getValue();
            if (inputs[MORPH_CV_INPUT].isConnected()) {
                morphTime += inputs[MORPH_CV_INPUT].getVoltage() * params[MORPH_ATTEN_PARAM].getValue();
            }
            morphTime = clamp(morphTime, 0.0f, 10.0f);

            if (morphTime <= 0.001f) {
                morphProgress = 1.0f;
                morphX = morphTargetX;
                morphY = morphTargetY;
                isMorphing = false;
            } else {
                morphProgress += args.sampleTime / morphTime;
                if (morphProgress >= 1.0f) {
                    morphProgress = 1.0f;
                    isMorphing = false;
                }
                // Smoothstep easing for organic transitions
                float t = morphProgress * morphProgress * (3.0f - 2.0f * morphProgress);
                morphX = morphStartX + t * (morphTargetX - morphStartX);
                morphY = morphStartY + t * (morphTargetY - morphStartY);
            }
        }

        // 4. Advance Playback Oscillator Phase
        // 1.0 unit of cyclePhase = 1 single recorded cycle, so 60 Hz internal frequency plays back 60 cycles/sec
        float fPlay = computePlaybackFreq();
        double prevCycleFloor = std::floor(cyclePhase);
        cyclePhase += (double)fPlay * (double)args.sampleTime;
        if (std::floor(cyclePhase) > prevCycleFloor) {
            syncOutPulse.trigger(1e-3f);
        }
        // Wrap modulo 384.0 (exact LCM of {1, 8, 16, 24, 48, 96, 128})
        if (cyclePhase >= 384.0) {
            cyclePhase -= 384.0 * std::floor(cyclePhase / 384.0);
        }

        // 5. 2D Bilinear Interpolation across the 4x4 Grid (skipping empty slots gracefully)
        float gx = clamp(morphX * 3.0f, 0.0f, 3.0f);
        float gy = clamp(morphY * 3.0f, 0.0f, 3.0f);
        int c0 = std::min((int)std::floor(gx), 2);
        int r0 = std::min((int)std::floor(gy), 2);
        int c1 = c0 + 1;
        int r1 = r0 + 1;
        float fx = gx - (float)c0;
        float fy = gy - (float)r0;

        int idx00 = r0 * 4 + c0;
        int idx10 = r0 * 4 + c1;
        int idx01 = r1 * 4 + c0;
        int idx11 = r1 * 4 + c1;

        float w00 = (1.0f - fx) * (1.0f - fy) * (slots[idx00].state == STATE_RECORDED ? 1.0f : 0.0f);
        float w10 = fx * (1.0f - fy) * (slots[idx10].state == STATE_RECORDED ? 1.0f : 0.0f);
        float w01 = (1.0f - fx) * fy * (slots[idx01].state == STATE_RECORDED ? 1.0f : 0.0f);
        float w11 = fx * fy * (slots[idx11].state == STATE_RECORDED ? 1.0f : 0.0f);
        float wSum = w00 + w10 + w01 + w11;

        float outVals[NUM_CHANNELS] = {0.f};

        if (wSum > 1e-5f) {
            w00 /= wSum;
            w10 /= wSum;
            w01 /= wSum;
            w11 /= wSum;
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                float v = 0.f;
                if (w00 > 0.f) v += w00 * slots[idx00].readSample(ch, cyclePhase);
                if (w10 > 0.f) v += w10 * slots[idx10].readSample(ch, cyclePhase);
                if (w01 > 0.f) v += w01 * slots[idx01].readSample(ch, cyclePhase);
                if (w11 > 0.f) v += w11 * slots[idx11].readSample(ch, cyclePhase);
                outVals[ch] = v;
            }
        } else {
            // Fallback: inverse-distance weighted blend of all recorded slots
            float totalWeight = 0.f;
            for (int s = 0; s < NUM_SLOTS; ++s) {
                if (slots[s].state != STATE_RECORDED) continue;
                float sc = (float)(s % 4);
                float sr = (float)(s / 4);
                float distSq = (gx - sc) * (gx - sc) + (gy - sr) * (gy - sr);
                float w = 1.0f / (distSq + 0.01f);
                totalWeight += w;
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    outVals[ch] += w * slots[s].readSample(ch, cyclePhase);
                }
            }
            if (totalWeight > 0.f) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    outVals[ch] /= totalWeight;
                }
            }
        }

        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            outputs[X_OUTPUT + ch].setVoltage(outVals[ch]);
        }

        bool syncPulse = syncOutPulse.process(args.sampleTime);
        outputs[SYNC_OUTPUT].setVoltage(syncPulse ? 10.0f : 0.0f);
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
        json_object_set_new(rootJ, "rangeMode", json_integer(rangeMode));
        json_object_set_new(rootJ, "selectedSlot", json_integer(selectedSlot));
        json_object_set_new(rootJ, "morphX", json_real(morphX));
        json_object_set_new(rootJ, "morphY", json_real(morphY));

        json_t* slotsJ = json_array();
        for (int s = 0; s < NUM_SLOTS; ++s) {
            json_t* sJ = json_object();
            bool recorded = (slots[s].state == STATE_RECORDED);
            json_object_set_new(sJ, "recorded", json_boolean(recorded));
            json_object_set_new(sJ, "numCycles", json_integer(slots[s].numCycles));
            json_object_set_new(sJ, "originalFreq", json_real(slots[s].originalFreq));

            if (recorded) {
                json_t* chArr = json_array();
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    const auto& vec = slots[s].wave[ch];
                    std::string b64 = base64_encode(reinterpret_cast<const uint8_t*>(vec.data()), vec.size() * sizeof(float));
                    json_array_append_new(chArr, json_string(b64.c_str()));
                }
                json_object_set_new(sJ, "wave", chArr);
            }
            json_array_append_new(slotsJ, sJ);
        }
        json_object_set_new(rootJ, "slots", slotsJ);
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* themeJ = json_object_get(rootJ, "skinTheme");
        if (themeJ) skinTheme = (SkinTheme)clamp((int)json_integer_value(themeJ), 0, 2);

        json_t* rmJ = json_object_get(rootJ, "rangeMode");
        if (rmJ) rangeMode = clamp((int)json_integer_value(rmJ), 0, 2);

        json_t* selJ = json_object_get(rootJ, "selectedSlot");
        if (selJ) selectedSlot = clamp((int)json_integer_value(selJ), 0, NUM_SLOTS - 1);

        json_t* mxJ = json_object_get(rootJ, "morphX");
        if (mxJ) morphX = morphTargetX = morphStartX = clamp((float)json_real_value(mxJ), 0.f, 1.f);

        json_t* myJ = json_object_get(rootJ, "morphY");
        if (myJ) morphY = morphTargetY = morphStartY = clamp((float)json_real_value(myJ), 0.f, 1.f);

        json_t* slotsJ = json_object_get(rootJ, "slots");
        if (slotsJ && json_is_array(slotsJ)) {
            for (int s = 0; s < NUM_SLOTS; ++s) {
                json_t* sJ = json_array_get(slotsJ, s);
                if (!sJ) continue;
                bool recorded = json_boolean_value(json_object_get(sJ, "recorded"));
                int nc = (int)json_integer_value(json_object_get(sJ, "numCycles"));
                float origF = (float)json_real_value(json_object_get(sJ, "originalFreq"));

                if (recorded && nc > 0 && nc <= MAX_CYCLES) {
                    slots[s].allocate(nc);
                    slots[s].originalFreq = (origF > 0.f) ? origF : 60.0f;
                    json_t* chArr = json_object_get(sJ, "wave");
                    bool valid = true;
                    if (chArr && json_is_array(chArr)) {
                        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                            json_t* b64J = json_array_get(chArr, ch);
                            if (b64J && json_is_string(b64J)) {
                                std::vector<uint8_t> raw = base64_decode(json_string_value(b64J));
                                size_t expectedBytes = slots[s].wave[ch].size() * sizeof(float);
                                if (raw.size() == expectedBytes) {
                                    std::memcpy(slots[s].wave[ch].data(), raw.data(), expectedBytes);
                                } else {
                                    valid = false;
                                }
                            } else {
                                valid = false;
                            }
                        }
                    } else {
                        valid = false;
                    }
                    slots[s].state = valid ? STATE_RECORDED : STATE_EMPTY;
                } else {
                    slots[s].clear();
                }
            }
        }
    }
};


// Custom 3-Color Concentric Range Light Ring around the RANGE button
struct ReverieRangeLightRing : TransparentWidget {
    Reverie* module = nullptr;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            int mode = module ? module->rangeMode : 1;
            NVGcolor c;
            if (mode == 0) {
                c = nvgRGBA(0x3a, 0x96, 0xff, 230); // Slow: Soft Cyan/Blue
            } else if (mode == 1) {
                c = nvgRGBA(0x38, 0xd8, 0x64, 230); // LFO: Crisp Green
            } else {
                c = nvgRGBA(0xff, 0x52, 0x38, 230); // Audio: Warm Red-Orange
            }

            Vec center = box.size.div(2.0f);
            float innerR = mm2px(2.90f);
            float outerR = mm2px(3.95f);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, center.x, center.y, outerR);
            nvgCircle(args.vg, center.x, center.y, innerR);
            nvgPathWinding(args.vg, NVG_HOLE);
            nvgFillColor(args.vg, c);
            nvgFill(args.vg);
        }
        TransparentWidget::drawLayer(args, layer);
    }
};


// Custom 4x4 Interactive Preset Button Matrix Widget
// Exact button column centers: 52.00, 66.20, 80.40, 94.60 mm (pitch 14.20 mm)
// Exact button row centers:    18.00, 31.00, 44.00, 57.00 mm (pitch 13.00 mm)
struct PresetGridWidget : OpaqueWidget {
    Reverie* module = nullptr;
    int heldSlot = -1;
    double holdStartTime = 0.0;
    bool holdTriggered = false;

    static constexpr float BTN_SIZE_MM = 10.60f;
    static constexpr float PITCH_X_MM = 14.20f;
    static constexpr float PITCH_Y_MM = 13.00f;
    static constexpr float MARGIN_MM = 5.30f; // half of BTN_SIZE_MM so col 0 is at left + 5.30 mm

    PresetGridWidget() {
        // Span X: 3 * 14.20 + 10.60 = 53.20 mm (centered at X = 73.30 mm -> left edge at 46.70 mm)
        // Span Y: 3 * 13.00 + 10.60 = 49.60 mm (centered at Y = 37.50 mm -> top edge at 12.70 mm)
        box.size = mm2px(Vec(53.20f, 49.60f));
    }

    Vec getSlotCenterPx(int col, int row) const {
        float cx = MARGIN_MM + col * PITCH_X_MM;
        float cy = MARGIN_MM + row * PITCH_Y_MM;
        return mm2px(Vec(cx, cy));
    }

    int getSlotAtPos(Vec pos) const {
        float btnHalf = mm2px(BTN_SIZE_MM * 0.5f);
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                Vec center = getSlotCenterPx(c, r);
                if (std::fabs(pos.x - center.x) <= btnHalf && std::fabs(pos.y - center.y) <= btnHalf) {
                    return r * 4 + c;
                }
            }
        }
        return -1;
    }

    void step() override {
        OpaqueWidget::step();
        if (module && heldSlot >= 0 && !holdTriggered) {
            double elapsed = system::getTime() - holdStartTime;
            if (elapsed >= 0.45) {
                // Long-hold (>0.45s) on any slot arms/overwrites it with a new capture
                module->armSlotForCapture(heldSlot);
                holdTriggered = true;
            }
        }
    }

    void onButton(const event::Button& e) override {
        if (!module) return;
        int slotIdx = getSlotAtPos(e.pos);
        if (slotIdx < 0) return;

        if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
            if (e.action == GLFW_PRESS) {
                heldSlot = slotIdx;
                holdStartTime = system::getTime();
                holdTriggered = false;
                e.consume(this);
            } else if (e.action == GLFW_RELEASE) {
                if (heldSlot == slotIdx && !holdTriggered) {
                    auto state = module->slots[slotIdx].state;
                    if (state == Reverie::STATE_EMPTY) {
                        if (module->selectedSlot == slotIdx) {
                            // Second click on an already-selected empty slot -> arm/record!
                            module->armSlotForCapture(slotIdx);
                        } else {
                            // First click on an empty slot -> select for live preview without recording
                            module->selectEmptySlotForPreview(slotIdx);
                        }
                    } else if (state == Reverie::STATE_RECORDED) {
                        module->selectRecordedSlot(slotIdx);
                    } else if (state == Reverie::STATE_ARMED || state == Reverie::STATE_RECORDING) {
                        // Cancel capture if clicked again while armed/recording
                        module->clearSlot(slotIdx);
                    }
                }
                heldSlot = -1;
                e.consume(this);
            }
        } else if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
            heldSlot = -1;
            createSlotContextMenu(slotIdx);
            e.consume(this);
        }
    }

    void onDragEnd(const event::DragEnd& e) override {
        heldSlot = -1;
    }

    void createSlotContextMenu(int slotIdx) {
        if (!module) return;
        ui::Menu* menu = createMenu();
        menu->addChild(createMenuLabel(string::f("Preset Slot %d", slotIdx + 1)));
        if (module->slots[slotIdx].state == Reverie::STATE_RECORDED) {
            menu->addChild(createMenuLabel(string::f("%d cycles @ %.1f Hz",
                module->slots[slotIdx].numCycles, module->slots[slotIdx].originalFreq)));
        }
        menu->addChild(new ui::MenuSeparator());
        menu->addChild(createMenuItem("Select slot", "", [=]() {
            if (module->slots[slotIdx].state == Reverie::STATE_RECORDED) {
                module->selectRecordedSlot(slotIdx);
            } else {
                module->selectEmptySlotForPreview(slotIdx);
            }
        }));
        menu->addChild(createMenuItem("Record / Overwrite slot", "Long-press", [=]() {
            module->armSlotForCapture(slotIdx);
        }));
        menu->addChild(createMenuItem("Clear slot", "", [=]() {
            module->clearSlot(slotIdx);
        }));
    }

    void drawButtonFallback(NVGcontext* vg, float x, float y, float sz, NVGcolor rimColor, NVGcolor midColor, NVGcolor faceColor) {
        float rOuter = sz * 0.18f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, sz, sz, rOuter);
        nvgFillColor(vg, rimColor);
        nvgFill(vg);

        float pad1 = sz * 0.055f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + pad1, y + pad1, sz - 2.f * pad1, sz - 2.f * pad1, rOuter * 0.82f);
        nvgFillColor(vg, midColor);
        nvgFill(vg);

        float pad2 = sz * 0.125f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + pad2, y + pad2, sz - 2.f * pad2, sz - 2.f * pad2, rOuter * 0.65f);
        nvgFillColor(vg, faceColor);
        nvgFill(vg);
    }

    void draw(const DrawArgs& args) override {
        float btnSz = mm2px(BTN_SIZE_MM);
        float halfSz = btnSz * 0.5f;

        std::shared_ptr<window::Image> imgEmpty = APP->window->loadImage(asset::plugin(pluginInstance, "res/preset-empty.png"));
        std::shared_ptr<window::Image> imgRec   = APP->window->loadImage(asset::plugin(pluginInstance, "res/preset-recording.png"));
        std::shared_ptr<window::Image> imgStore = APP->window->loadImage(asset::plugin(pluginInstance, "res/preset-stored.png"));
        std::shared_ptr<window::Image> imgSel   = APP->window->loadImage(asset::plugin(pluginInstance, "res/preset-selected.png"));
        std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/D-DINCondensed-Bold.otf"));

        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                int idx = r * 4 + c;
                Vec center = getSlotCenterPx(c, r);
                float bx = center.x - halfSz;
                float by = center.y - halfSz;

                Reverie::SlotState state = module ? module->slots[idx].state : Reverie::STATE_EMPTY;
                bool isSelected = module && (module->selectedSlot == idx);

                std::shared_ptr<window::Image> targetImg = imgEmpty;
                if (state == Reverie::STATE_ARMED || state == Reverie::STATE_RECORDING) {
                    targetImg = imgRec;
                } else if (isSelected) {
                    targetImg = imgSel;
                } else if (state == Reverie::STATE_RECORDED) {
                    targetImg = imgStore;
                } else {
                    targetImg = imgEmpty;
                }

                if (targetImg && targetImg->handle >= 0) {
                    NVGpaint imgPaint = nvgImagePattern(args.vg, bx, by, btnSz, btnSz, 0.0f, targetImg->handle, 1.0f);
                    nvgBeginPath(args.vg);
                    nvgRect(args.vg, bx, by, btnSz, btnSz);
                    nvgFillPaint(args.vg, imgPaint);
                    nvgFill(args.vg);
                } else {
                    if (state == Reverie::STATE_ARMED || state == Reverie::STATE_RECORDING) {
                        drawButtonFallback(args.vg, bx, by, btnSz,
                            nvgRGB(108, 39, 39), nvgRGB(239, 108, 108), nvgRGB(209, 69, 69));
                    } else if (isSelected) {
                        drawButtonFallback(args.vg, bx, by, btnSz,
                            nvgRGB(39, 108, 45), nvgRGB(108, 239, 117), nvgRGB(69, 209, 90));
                    } else if (state == Reverie::STATE_RECORDED) {
                        drawButtonFallback(args.vg, bx, by, btnSz,
                            nvgRGB(39, 77, 108), nvgRGB(108, 198, 239), nvgRGB(69, 153, 209));
                    } else {
                        drawButtonFallback(args.vg, bx, by, btnSz,
                            nvgRGB(106, 106, 106), nvgRGB(239, 239, 239), nvgRGB(209, 209, 209));
                    }
                }

                // If selected AND recorded, draw a subtle azure dot in the corner so you know it's stored vs empty-selected
                if (isSelected && state == Reverie::STATE_RECORDED) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, bx + btnSz * 0.78f, by + btnSz * 0.22f, mm2px(0.85f));
                    nvgFillColor(args.vg, nvgRGB(39, 115, 185));
                    nvgFill(args.vg);
                }

                // Recording progress bar along the bottom of the button
                if (module && state == Reverie::STATE_RECORDING && module->slots[idx].numCycles > 0) {
                    float prog = (float)module->capturedCyclesCount / (float)module->slots[idx].numCycles;
                    prog = clamp(prog, 0.f, 1.f);
                    float pad = btnSz * 0.14f;
                    nvgBeginPath(args.vg);
                    nvgRoundedRect(args.vg, bx + pad, by + btnSz - pad - mm2px(1.1f), (btnSz - 2.f * pad) * prog, mm2px(1.1f), mm2px(0.4f));
                    nvgFillColor(args.vg, nvgRGBA(255, 255, 255, 225));
                    nvgFill(args.vg);
                }

                // Slot Number Overlay (1..16)
                if (font && font->handle >= 0) {
                    nvgFontFaceId(args.vg, font->handle);
                    nvgFontSize(args.vg, 11.5f);
                    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                    NVGcolor txtCol = (state == Reverie::STATE_EMPTY && !isSelected)
                        ? nvgRGBA(40, 40, 40, 215)
                        : nvgRGBA(20, 25, 30, 220);
                    nvgFillColor(args.vg, txtCol);
                    std::string numStr = std::to_string(idx + 1);
                    nvgText(args.vg, center.x, center.y, numStr.c_str(), nullptr);
                }
            }
        }

        // Draw 2D Morph Position Indicator Crosshair across the 4x4 grid
        if (module) {
            Vec p00 = getSlotCenterPx(0, 0);
            Vec p33 = getSlotCenterPx(3, 3);
            float mx = p00.x + clamp(module->morphX, 0.f, 1.f) * (p33.x - p00.x);
            float my = p00.y + clamp(module->morphY, 0.f, 1.f) * (p33.y - p00.y);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, mx, my, mm2px(2.1f));
            nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 215));
            nvgStrokeWidth(args.vg, 1.4f);
            nvgStroke(args.vg);

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, mx, my, mm2px(0.8f));
            nvgFillColor(args.vg, nvgRGBA(255, 220, 90, 245));
            nvgFill(args.vg);
        }
    }
};


struct ReverieWidget : ModuleWidget {
    Reverie::SkinTheme lastTheme = Reverie::THEME_MEDIUM;

    ReverieWidget(Reverie* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Reverie.svg")));

        // =====================================================================
        // LEFT CONTROLS (Columns: 7.00, 22.65, 38.30 mm)
        // =====================================================================
        // Row 1 (Y = 18.00 mm): FREQ, RANGE (Button + Light Ring), FINE
        addParam(createParamCentered<ReverieKnob>(mm2px(Vec(7.00f, 18.00f)), module, Reverie::FREQ_PARAM));
        addParam(createParamCentered<ReverieButton>(mm2px(Vec(22.65f, 18.00f)), module, Reverie::RANGE_PARAM));
        {
            ReverieRangeLightRing* ring = new ReverieRangeLightRing();
            ring->module = module;
            ring->box.size = mm2px(Vec(8.20f, 8.20f));
            ring->box.pos = mm2px(Vec(22.65f, 18.00f)).minus(ring->box.size.div(2.0f));
            addChild(ring);
        }
        addParam(createParamCentered<ReverieKnob>(mm2px(Vec(38.30f, 18.00f)), module, Reverie::FINE_PARAM));

        // Row 2 (Y = 34.00 mm): CYCLES, RECORD Button, MORPH
        addParam(createParamCentered<ReverieSnapKnob>(mm2px(Vec(7.00f, 34.00f)), module, Reverie::CYCLES_PARAM));
        addParam(createParamCentered<ReverieButton>(mm2px(Vec(22.65f, 34.00f)), module, Reverie::REC_BUTTON_PARAM));
        addParam(createParamCentered<ReverieKnob>(mm2px(Vec(38.30f, 34.00f)), module, Reverie::MORPH_PARAM));

        // Row 3 (Y = 50.00 mm): X Scan, Y Scan, Morph Time CV Depth
        addParam(createParamCentered<ReverieTrimpot>(mm2px(Vec(7.00f, 50.00f)), module, Reverie::X_SCAN_PARAM));
        addParam(createParamCentered<ReverieTrimpot>(mm2px(Vec(22.65f, 50.00f)), module, Reverie::Y_SCAN_PARAM));
        addParam(createParamCentered<ReverieTrimpot>(mm2px(Vec(38.30f, 50.00f)), module, Reverie::MORPH_ATTEN_PARAM));

        // =====================================================================
        // LEFT CONTROL JACKS (4 columns: 7.00, 17.43, 27.87, 38.30 mm)
        // 5 rows: 66.00, 78.75, 91.50, 104.25, 117.00 mm
        // =====================================================================
        const float lCols[4] = {7.00f, 17.43f, 27.87f, 38.30f};
        const float lRows[5] = {66.00f, 78.75f, 91.50f, 104.25f, 117.00f};

        // Row 0 (Y = 66.00 mm): SYNC, X, Y, FREQ
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[0], lRows[0])), module, Reverie::SYNC_INPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[1], lRows[0])), module, Reverie::X_INPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[2], lRows[0])), module, Reverie::Y_INPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[3], lRows[0])), module, Reverie::FREQ_CV_INPUT));

        // Row 1 (Y = 78.75 mm): R, G, B, MORPH
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[0], lRows[1])), module, Reverie::R_INPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[1], lRows[1])), module, Reverie::G_INPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[2], lRows[1])), module, Reverie::B_INPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[3], lRows[1])), module, Reverie::MORPH_CV_INPUT));

        // Row 2 (Y = 91.50 mm): Z/I, X Out, Y Out, X SCAN
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[0], lRows[2])), module, Reverie::Z_INPUT));
        addOutput(createOutputCentered<ReveriePort>(mm2px(Vec(lCols[1], lRows[2])), module, Reverie::X_OUTPUT));
        addOutput(createOutputCentered<ReveriePort>(mm2px(Vec(lCols[2], lRows[2])), module, Reverie::Y_OUTPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[3], lRows[2])), module, Reverie::X_SCAN_INPUT));

        // Row 3 (Y = 104.25 mm): R Out, G Out, B Out, Y SCAN
        addOutput(createOutputCentered<ReveriePort>(mm2px(Vec(lCols[0], lRows[3])), module, Reverie::R_OUTPUT));
        addOutput(createOutputCentered<ReveriePort>(mm2px(Vec(lCols[1], lRows[3])), module, Reverie::G_OUTPUT));
        addOutput(createOutputCentered<ReveriePort>(mm2px(Vec(lCols[2], lRows[3])), module, Reverie::B_OUTPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[3], lRows[3])), module, Reverie::Y_SCAN_INPUT));

        // Row 4 (Y = 117.00 mm): Z/I Out, SYNC Out, RECORD Gate, RANDOM Gate
        addOutput(createOutputCentered<ReveriePort>(mm2px(Vec(lCols[0], lRows[4])), module, Reverie::Z_OUTPUT));
        addOutput(createOutputCentered<ReveriePort>(mm2px(Vec(lCols[1], lRows[4])), module, Reverie::SYNC_OUTPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[2], lRows[4])), module, Reverie::REC_GATE_INPUT));
        addInput(createInputCentered<ReveriePort>(mm2px(Vec(lCols[3], lRows[4])), module, Reverie::RANDOM_GATE_INPUT));

        // =====================================================================
        // RIGHT 4x4 PRESET MATRIX & 4x4 GATE INPUT GRID
        // Columns: 52.00, 66.20, 80.40, 94.60 mm
        // =====================================================================
        // Top 4x4 Preset Button Matrix (Rows: 18.00, 31.00, 44.00, 57.00 mm -> center (73.30, 37.50) mm)
        PresetGridWidget* grid = new PresetGridWidget();
        grid->module = module;
        grid->box.pos = mm2px(Vec(73.30f, 37.50f)).minus(grid->box.size.div(2.0f));
        addChild(grid);

        // Bottom 4x4 Gate Input Grid (Rows: 78.00, 91.00, 104.00, 117.00 mm)
        const float gCols[4] = {52.00f, 66.20f, 80.40f, 94.60f};
        const float gRows[4] = {78.00f, 91.00f, 104.00f, 117.00f};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                int idx = r * 4 + c;
                addInput(createInputCentered<ReveriePort>(mm2px(Vec(gCols[c], gRows[r])), module, Reverie::PRESET_GATE_INPUT + idx));
            }
        }
    }

    void updatePanelTheme(Reverie::SkinTheme theme) {
        lastTheme = theme;
        std::string svgFile = "res/Reverie.svg";
        if (theme == Reverie::THEME_LIGHT) svgFile = "res/Reverie_Light.svg";
        else if (theme == Reverie::THEME_DARK) svgFile = "res/Reverie_Dark.svg";
        setPanel(createPanel(asset::plugin(pluginInstance, svgFile)));
    }

    void step() override {
        Reverie* mod = dynamic_cast<Reverie*>(module);
        if (mod && mod->skinTheme != lastTheme) {
            updatePanelTheme(mod->skinTheme);
        }
        ModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        Reverie* mod = dynamic_cast<Reverie*>(module);
        if (!mod) return;

        menu->addChild(new MenuSeparator());
        menu->addChild(createMenuLabel("Panel Theme"));
        menu->addChild(createCheckMenuItem("Medium (Default)", "",
            [=]() { return mod->skinTheme == Reverie::THEME_MEDIUM; },
            [=]() { mod->skinTheme = Reverie::THEME_MEDIUM; }
        ));
        menu->addChild(createCheckMenuItem("Light", "",
            [=]() { return mod->skinTheme == Reverie::THEME_LIGHT; },
            [=]() { mod->skinTheme = Reverie::THEME_LIGHT; }
        ));
        menu->addChild(createCheckMenuItem("Dark", "",
            [=]() { return mod->skinTheme == Reverie::THEME_DARK; },
            [=]() { mod->skinTheme = Reverie::THEME_DARK; }
        ));

        menu->addChild(new MenuSeparator());
        menu->addChild(createMenuItem("Clear all 16 preset slots", "", [=]() {
            mod->clearAllSlots();
        }));
    }
};

Model* modelReverie = createModel<Reverie, ReverieWidget>("Reverie");
