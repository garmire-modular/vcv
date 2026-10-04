#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace stepped_slew {

enum SteppingMode {
    MODE_EQUAL = 0,
    MODE_SEMITONE = 1,
    MODE_QUARTER_TONE = 2,
    MODE_19_TET = 3,
    MODE_22_TET = 4,
    MODE_31_TET = 5,
    MODE_JUST_INTONATION = 6,
    MODE_MEANTONE = 7
};

enum QuantizeStrategy {
    STRATEGY_SCALE_TRAVERSAL = 0,
    STRATEGY_NEAREST_GRID = 1
};

enum ClockMode {
    CLOCK_TRIGGER = 0,
    CLOCK_TOGGLE = 1
};

// 5-limit Just Intonation semitone pitch offsets in volts (V/oct)
// C, C#, D, Eb, E, F, F#, G, Ab, A, Bb, B
// Ratios: 1/1, 16/15, 9/8, 6/5, 5/4, 4/3, 45/32, 3/2, 8/5, 5/3, 9/5, 15/8
inline const float* getJustIntonationScale() {
    static const float scale[12] = {
        0.0f,                   // 1/1
        0.093109404f,           // 16/15
        0.169925001f,           // 9/8
        0.263034406f,           // 6/5
        0.321928095f,           // 5/4
        0.415037499f,           // 4/3
        0.490861460f,           // 45/32
        0.584962501f,           // 3/2
        0.678071905f,           // 8/5
        0.736965594f,           // 5/3
        0.847996907f,           // 9/5
        0.906890596f            // 15/8
    };
    return scale;
}

// 1/4-comma Meantone pitch offsets in volts (V/oct)
// Based on pure 5/4 major third (386.31 cents = 0.321928V) and quarter-comma fifth (696.58 cents = 0.580482V)
inline const float* getMeantoneScale() {
    static const float scale[12] = {
        0.0f,                   // C: 0 cents
        0.06324f,               // C#: 76.0 cents
        0.16096f,               // D: 193.2 cents
        0.25869f,               // Eb: 310.3 cents
        0.32193f,               // E: 386.3 cents
        0.41965f,               // F: 503.4 cents
        0.48289f,               // F#: 579.5 cents
        0.58048f,               // G: 696.6 cents
        0.64372f,               // G#: 772.5 cents
        0.74145f,               // A: 889.7 cents
        0.83917f,               // Bb: 1007.0 cents
        0.90241f                // B: 1082.9 cents
    };
    return scale;
}

// Quantizes a voltage to the nearest pitch of a scale
inline float quantizePitch(float voltage, SteppingMode mode, int rootKey = 0) {
    if (mode == MODE_EQUAL) {
        return voltage;
    }

    float rootOffset = static_cast<float>(rootKey) / 12.0f;
    float v = voltage - rootOffset;
    float oct = std::floor(v);
    float frac = v - oct; // [0.0, 1.0)

    float quantizedFrac = frac;

    switch (mode) {
        case MODE_SEMITONE: {
            quantizedFrac = std::round(frac * 12.0f) / 12.0f;
            break;
        }
        case MODE_QUARTER_TONE: {
            quantizedFrac = std::round(frac * 24.0f) / 24.0f;
            break;
        }
        case MODE_19_TET: {
            quantizedFrac = std::round(frac * 19.0f) / 19.0f;
            break;
        }
        case MODE_22_TET: {
            quantizedFrac = std::round(frac * 22.0f) / 22.0f;
            break;
        }
        case MODE_31_TET: {
            quantizedFrac = std::round(frac * 31.0f) / 31.0f;
            break;
        }
        case MODE_JUST_INTONATION: {
            const float* scale = getJustIntonationScale();
            float minDiff = 100.0f;
            float best = 0.0f;
            // Test 12 scale degrees plus 1.0 (octave)
            for (int i = 0; i < 12; ++i) {
                float diff = std::abs(frac - scale[i]);
                if (diff < minDiff) {
                    minDiff = diff;
                    best = scale[i];
                }
            }
            if (std::abs(frac - 1.0f) < minDiff) {
                best = 1.0f;
            }
            quantizedFrac = best;
            break;
        }
        case MODE_MEANTONE: {
            const float* scale = getMeantoneScale();
            float minDiff = 100.0f;
            float best = 0.0f;
            for (int i = 0; i < 12; ++i) {
                float diff = std::abs(frac - scale[i]);
                if (diff < minDiff) {
                    minDiff = diff;
                    best = scale[i];
                }
            }
            if (std::abs(frac - 1.0f) < minDiff) {
                best = 1.0f;
            }
            quantizedFrac = best;
            break;
        }
        default:
            break;
    }

    return oct + quantizedFrac + rootOffset;
}

// Computes the trajectory curvature function f(u) for u in [0, 1]
// shape in [-1, +1]: -1 = log (fast start), 0 = linear, +1 = exp (slow start)
inline float evalShape(float u, float shape) {
    u = std::max(0.0f, std::min(1.0f, u));
    if (std::abs(shape) < 0.001f) {
        return u;
    }
    if (shape > 0.0f) {
        // Exponential (slow-start): f(u) = u^(1 + 5*shape)
        float alpha = 1.0f + 5.0f * shape;
        return std::pow(u, alpha);
    } else {
        // Logarithmic (fast-start): f(u) = 1 - (1 - u)^(1 + 5*(-shape))
        float beta = 1.0f + 5.0f * (-shape);
        return 1.0f - std::pow(1.0f - u, beta);
    }
}

// Converts time knob + CV to slew duration T in seconds [0.0005, 10.0]
inline float calcTimeSeconds(float timeKnob, float cvIn, float attenuverter) {
    float mod = timeKnob + attenuverter * (cvIn / 10.0f);
    mod = std::max(0.0f, std::min(1.0f, mod));
    // T = Tmin * (Tmax / Tmin)^mod = 0.0005 * (20000)^mod
    return 0.0005f * std::pow(20000.0f, mod);
}

// Computes integer steps [0, 96]
inline int calcSteps(float stepsKnob, float cvIn, float attenuverter) {
    float mod = stepsKnob + attenuverter * (cvIn / 10.0f) * 96.0f;
    mod = std::max(0.0f, std::min(96.0f, mod));
    return static_cast<int>(std::round(mod));
}

struct Engine {
    // Current continuous output voltage
    float currentSlew = 0.0f;
    // Current stepped output voltage
    float currentStep = 0.0f;

    // Transition tracking
    float startVoltage = 0.0f;
    float targetVoltage = 0.0f;
    float progress = 1.0f;      // u in [0, 1]; 1.0 = arrived/idle
    int currentStepIndex = 0;
    bool isRising = false;
    bool isFalling = false;

    // Trigger state machines (duration in seconds)
    float eouTimer = 0.0f;
    float eodTimer = 0.0f;
    float stepTrigTimer = 0.0f;
    bool stepToggleState = false;

    // LED indicators (decay timers in seconds)
    float ledUpTimer = 0.0f;
    float ledDownTimer = 0.0f;

    // Configuration
    SteppingMode steppingMode = MODE_EQUAL;
    QuantizeStrategy quantizeStrategy = STRATEGY_SCALE_TRAVERSAL;
    ClockMode clockMode = CLOCK_TRIGGER;
    int rootKey = 0; // 0 = C

    void reset() {
        currentSlew = 0.0f;
        currentStep = 0.0f;
        startVoltage = 0.0f;
        targetVoltage = 0.0f;
        progress = 1.0f;
        currentStepIndex = 0;
        isRising = false;
        isFalling = false;
        eouTimer = 0.0f;
        eodTimer = 0.0f;
        stepTrigTimer = 0.0f;
        stepToggleState = false;
        ledUpTimer = 0.0f;
        ledDownTimer = 0.0f;
    }

    struct Output {
        float slewOut;
        float stepOut;
        float upGate;
        float eouTrig;
        float downGate;
        float eodTrig;
        float stepGate;
        float ledUpBrightness;
        float ledDownBrightness;
    };

    Output process(
        float inputVoltage,
        float timeUp, float shapeUp, int stepsUp,
        float timeDown, float shapeDown, int stepsDown,
        float sampleRate
    ) {
        float dt = (sampleRate > 0.0f) ? (1.0f / sampleRate) : (1.0f / 44100.0f);
        constexpr float EPSILON = 1e-5f;
        constexpr float TRIG_DURATION = 0.001f; // 1 ms
        constexpr float LED_DECAY = 0.040f;     // 40 ms

        // Detect target voltage change mid-flight or new step
        if (std::abs(inputVoltage - targetVoltage) > EPSILON) {
            startVoltage = currentSlew;
            targetVoltage = inputVoltage;
            progress = 0.0f;
            currentStepIndex = 0;

            if (targetVoltage > startVoltage + EPSILON) {
                isRising = true;
                isFalling = false;
            } else if (targetVoltage < startVoltage - EPSILON) {
                isRising = false;
                isFalling = true;
            } else {
                isRising = false;
                isFalling = false;
                progress = 1.0f;
            }
        }

        // Active slew trajectory traversal
        int previousStepIndex = currentStepIndex;
        bool stepCrossed = false;

        if (progress < 1.0f) {
            float slewDuration = isRising ? timeUp : timeDown;
            slewDuration = std::max(0.0001f, slewDuration);

            float shape = isRising ? shapeUp : shapeDown;
            int steps = isRising ? stepsUp : stepsDown;

            // Advance progress
            progress += dt / slewDuration;

            if (progress >= 1.0f) {
                // Arrived at target
                progress = 1.0f;
                currentSlew = targetVoltage;
                currentStep = targetVoltage;

                if (isRising) {
                    eouTimer = TRIG_DURATION;
                    isRising = false;
                } else if (isFalling) {
                    eodTimer = TRIG_DURATION;
                    isFalling = false;
                }

                // Check final step arrival
                if (steps > 0 && currentStepIndex != steps) {
                    stepCrossed = true;
                    currentStepIndex = steps;
                }
            } else {
                // Continuous slew calculation
                float curve = evalShape(progress, shape);
                currentSlew = startVoltage + (targetVoltage - startVoltage) * curve;

                // Stepped output calculation
                if (steps == 0) {
                    // Bypass stepping
                    currentStep = currentSlew;
                } else {
                    if (steppingMode == MODE_EQUAL) {
                        int m = static_cast<int>(std::floor(progress * static_cast<float>(steps)));
                        if (m != currentStepIndex) {
                            stepCrossed = true;
                            currentStepIndex = m;
                        }
                        float deltaV = targetVoltage - startVoltage;
                        float h = deltaV / static_cast<float>(steps);
                        currentStep = startVoltage + static_cast<float>(currentStepIndex) * h;
                    } else {
                        // V/Oct aware quantizer modes
                        if (quantizeStrategy == STRATEGY_SCALE_TRAVERSAL) {
                            // Scale Traversal: quantize the continuous slew curve, with step cap
                            float quantizedSlew = quantizePitch(currentSlew, steppingMode, rootKey);
                            int m = static_cast<int>(std::floor(progress * static_cast<float>(steps)));
                            if (m != currentStepIndex) {
                                stepCrossed = true;
                                currentStepIndex = m;
                            }
                            currentStep = quantizedSlew;
                        } else {
                            // Subdivided & Nearest Scale Snap
                            int m = static_cast<int>(std::floor(progress * static_cast<float>(steps)));
                            if (m != currentStepIndex) {
                                stepCrossed = true;
                                currentStepIndex = m;
                            }
                            float deltaV = targetVoltage - startVoltage;
                            float h = deltaV / static_cast<float>(steps);
                            float rawStep = startVoltage + static_cast<float>(currentStepIndex) * h;
                            currentStep = quantizePitch(rawStep, steppingMode, rootKey);
                        }
                    }
                }
            }
        } else {
            // Idle / Settled state
            currentSlew = targetVoltage;
            if (steppingMode == MODE_EQUAL) {
                currentStep = targetVoltage;
            } else {
                currentStep = quantizePitch(targetVoltage, steppingMode, rootKey);
            }
            isRising = false;
            isFalling = false;
        }

        // Handle step crossing triggers/events
        if (stepCrossed) {
            stepTrigTimer = TRIG_DURATION;
            stepToggleState = !stepToggleState;
            if (isRising) {
                ledUpTimer = LED_DECAY;
            } else if (isFalling) {
                ledDownTimer = LED_DECAY;
            }
        }

        // Update timers
        if (eouTimer > 0.0f) {
            eouTimer = std::max(0.0f, eouTimer - dt);
        }
        if (eodTimer > 0.0f) {
            eodTimer = std::max(0.0f, eodTimer - dt);
        }
        if (stepTrigTimer > 0.0f) {
            stepTrigTimer = std::max(0.0f, stepTrigTimer - dt);
        }
        if (ledUpTimer > 0.0f) {
            ledUpTimer = std::max(0.0f, ledUpTimer - dt);
        }
        if (ledDownTimer > 0.0f) {
            ledDownTimer = std::max(0.0f, ledDownTimer - dt);
        }

        Output out;
        out.slewOut = currentSlew;
        out.stepOut = currentStep;
        out.upGate = (isRising && progress < 1.0f) ? 10.0f : 0.0f;
        out.eouTrig = (eouTimer > 0.0f) ? 10.0f : 0.0f;
        out.downGate = (isFalling && progress < 1.0f) ? 10.0f : 0.0f;
        out.eodTrig = (eodTimer > 0.0f) ? 10.0f : 0.0f;

        if (clockMode == CLOCK_TRIGGER) {
            out.stepGate = (stepTrigTimer > 0.0f) ? 10.0f : 0.0f;
        } else {
            out.stepGate = stepToggleState ? 10.0f : 0.0f;
        }

        out.ledUpBrightness = (ledUpTimer > 0.0f) ? (ledUpTimer / LED_DECAY) : 0.0f;
        out.ledDownBrightness = (ledDownTimer > 0.0f) ? (ledDownTimer / LED_DECAY) : 0.0f;

        return out;
    }
};

} // namespace stepped_slew
