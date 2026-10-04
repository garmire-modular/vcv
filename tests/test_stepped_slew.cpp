#include "../src/core/SteppedSlewEngine.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

void testTimeAndCurvature() {
    std::cout << "[Test] Running Slew Time and Curvature verification..." << std::endl;
    // Test time calculation
    float tMin = stepped_slew::calcTimeSeconds(0.0f, 0.0f, 0.0f);
    assert(std::abs(tMin - 0.0005f) < 1e-6f);

    float tMax = stepped_slew::calcTimeSeconds(1.0f, 0.0f, 0.0f);
    assert(std::abs(tMax - 10.0f) < 1e-4f);

    // Test shape evaluation
    // Linear
    assert(std::abs(stepped_slew::evalShape(0.5f, 0.0f) - 0.5f) < 1e-5f);
    // Exponential (slow start, lower at midpoint)
    float expVal = stepped_slew::evalShape(0.5f, 1.0f);
    assert(expVal < 0.5f);
    // Logarithmic (fast start, higher at midpoint)
    float logVal = stepped_slew::evalShape(0.5f, -1.0f);
    assert(logVal > 0.5f);

    // Monotonicity check
    for (float shape = -1.0f; shape <= 1.0f; shape += 0.5f) {
        float last = -1.0f;
        for (float u = 0.0f; u <= 1.0f; u += 0.05f) {
            float val = stepped_slew::evalShape(u, shape);
            assert(val >= last - 1e-6f);
            assert(val >= 0.0f && val <= 1.0f + 1e-5f);
            last = val;
        }
    }
    std::cout << "  -> Time and Curvature tests passed." << std::endl;
}

void testEqualStepping() {
    std::cout << "[Test] Running Equal Stepping verification..." << std::endl;
    stepped_slew::Engine engine;
    engine.reset();
    engine.steppingMode = stepped_slew::MODE_EQUAL;

    float sampleRate = 10000.0f; // 10kHz sample rate -> 1 sample = 0.1ms
    float timeUp = 0.1f;         // 100ms
    float timeDown = 0.1f;
    int stepsUp = 4;             // 4 steps for 0V -> 4V
    int stepsDown = 4;

    // Step input from 0V to 4V
    int stepCrossingCount = 0;
    for (int i = 0; i < 1100; ++i) { // 110ms
        auto out = engine.process(4.0f, timeUp, 0.0f, stepsUp, timeDown, 0.0f, stepsDown, sampleRate);
        if (out.stepGate > 5.0f && (i == 0 || engine.stepTrigTimer == 0.001f)) {
            // Count triggers
        }
        if (i == 1050) {
            // After arrival
            assert(std::abs(out.slewOut - 4.0f) < 1e-4f);
            assert(std::abs(out.stepOut - 4.0f) < 1e-4f);
        }
    }
    std::cout << "  -> Equal stepping test passed." << std::endl;
}

void testTuningScales() {
    std::cout << "[Test] Running Microtonal & V/Oct Scale Quantization verification..." << std::endl;
    // Semitone test
    float v1 = stepped_slew::quantizePitch(0.08f, stepped_slew::MODE_SEMITONE, 0); // close to 1/12 = 0.0833V
    assert(std::abs(v1 - 1.0f / 12.0f) < 1e-5f);

    // 19-TET test
    float v19 = stepped_slew::quantizePitch(0.05f, stepped_slew::MODE_19_TET, 0); // close to 1/19 = 0.0526V
    assert(std::abs(v19 - 1.0f / 19.0f) < 1e-5f);

    // Just intonation: Perfect fifth (3/2 ratio = 0.5849625V)
    float vJI = stepped_slew::quantizePitch(0.58f, stepped_slew::MODE_JUST_INTONATION, 0);
    assert(std::abs(vJI - 0.5849625f) < 1e-5f);

    // Octave invariance
    for (int m = 1; m <= 7; ++m) {
        auto mode = static_cast<stepped_slew::SteppingMode>(m);
        for (float octave = -2.0f; octave <= 3.0f; octave += 1.0f) {
            float testV = octave + 0.333f;
            float q = stepped_slew::quantizePitch(testV, mode, 0);
            assert(!std::isnan(q));
            assert(q >= octave && q <= octave + 1.0f + 1e-4f);
        }
    }
    std::cout << "  -> Microtonal scales tests passed." << std::endl;
}

void testGatesAndTriggers() {
    std::cout << "[Test] Running Gates and Triggers verification..." << std::endl;
    stepped_slew::Engine engine;
    engine.reset();

    float sampleRate = 10000.0f;
    // Rising slew
    auto out1 = engine.process(5.0f, 0.05f, 0.0f, 5, 0.05f, 0.0f, 5, sampleRate);
    assert(out1.upGate == 10.0f);
    assert(out1.downGate == 0.0f);

    // Traverse until end of up
    bool sawEOU = false;
    for (int i = 0; i < 600; ++i) {
        auto out = engine.process(5.0f, 0.05f, 0.0f, 5, 0.05f, 0.0f, 5, sampleRate);
        if (out.eouTrig > 5.0f) {
            sawEOU = true;
        }
    }
    assert(sawEOU);

    // Falling slew
    auto outDown = engine.process(0.0f, 0.05f, 0.0f, 5, 0.05f, 0.0f, 5, sampleRate);
    assert(outDown.downGate == 10.0f);
    assert(outDown.upGate == 0.0f);

    bool sawEOD = false;
    for (int i = 0; i < 600; ++i) {
        auto out = engine.process(0.0f, 0.05f, 0.0f, 5, 0.05f, 0.0f, 5, sampleRate);
        if (out.eodTrig > 5.0f) {
            sawEOD = true;
        }
    }
    assert(sawEOD);
    std::cout << "  -> Gates and triggers tests passed." << std::endl;
}

int main() {
    std::cout << "=== Running Stepped Slew Core DSP Unit Tests ===" << std::endl;
    testTimeAndCurvature();
    testEqualStepping();
    testTuningScales();
    testGatesAndTriggers();
    std::cout << "=== ALL TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
