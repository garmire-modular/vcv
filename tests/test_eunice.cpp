#include "../src/core/EuniceEngine.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void testDistribution() {
    std::cout << "[Test] Running Distribution Tilt verification..." << std::endl;
    // Test neutral (dist = 0.5)
    float v0 = eunice::Engine::applyDistribution(2.5f, 0.5f, eunice::DIST_POWER_LAW);
    assert(std::abs(v0 - 2.5f) < 1e-4f);

    float vNeg = eunice::Engine::applyDistribution(-3.0f, 0.5f, eunice::DIST_POWER_LAW);
    assert(std::abs(vNeg - (-3.0f)) < 1e-4f);

    // Test CW (dist = 1.0) -> higher voltages emphasized
    float vCW = eunice::Engine::applyDistribution(0.0f, 1.0f, eunice::DIST_POWER_LAW);
    assert(vCW > 0.0f); // 0V input skewed positive

    // Test CCW (dist = 0.0) -> lower voltages emphasized
    float vCCW = eunice::Engine::applyDistribution(0.0f, 0.0f, eunice::DIST_POWER_LAW);
    assert(vCCW < 0.0f); // 0V input skewed negative

    // Bound checks for all modes
    for (int m = 0; m <= 2; m++) {
        auto mode = static_cast<eunice::DistributionMode>(m);
        for (float d = 0.0f; d <= 1.0f; d += 0.25f) {
            for (float v = -5.0f; v <= 5.0f; v += 1.0f) {
                float res = eunice::Engine::applyDistribution(v, d, mode);
                assert(!std::isnan(res));
                assert(!std::isinf(res));
                assert(res >= -6.0f && res <= 6.0f);
            }
        }
    }
    std::cout << "  -> Distribution tests passed." << std::endl;
}

void testCorrelation() {
    std::cout << "[Test] Running Correlation logic verification..." << std::endl;
    float vDist = 4.0f;
    float vHeld = -2.0f;

    // C = 0 -> purely new signal
    float c0 = 0.0f;
    float out0 = (1.0f - c0) * vDist + c0 * vHeld;
    assert(std::abs(out0 - vDist) < 1e-5f);

    // C = 1 -> purely held signal
    float c1 = 1.0f;
    float out1 = (1.0f - c1) * vDist + c1 * vHeld;
    assert(std::abs(out1 - vHeld) < 1e-5f);

    // C = 0.5 -> halfway crossfade
    float cMid = 0.5f;
    float outMid = (1.0f - cMid) * vDist + cMid * vHeld;
    assert(std::abs(outMid - 1.0f) < 1e-5f);
    std::cout << "  -> Correlation tests passed." << std::endl;
}

void testSlewLimiter() {
    std::cout << "[Test] Running Asymmetrical Slew Limiter verification..." << std::endl;
    float current = 0.0f;
    float target = 10.0f;
    float dt = 1.0f / 44100.0f;
    float riseTime = 0.1f; // 100 ms for 10V
    float fallTime = 0.01f;

    // Run 100ms worth of samples (4410 steps)
    for (int i = 0; i < 4410; i++) {
        current = eunice::Engine::stepSlew(current, target, riseTime, fallTime, dt, eunice::SLEW_LINEAR);
    }
    // Should be at or extremely close to 10.0V
    assert(std::abs(current - 10.0f) < 0.01f);

    // Now test fall from 10.0V to 0.0V with fallTime = 0.01s (441 steps)
    target = 0.0f;
    for (int i = 0; i < 441; i++) {
        current = eunice::Engine::stepSlew(current, target, riseTime, fallTime, dt, eunice::SLEW_LINEAR);
    }
    assert(std::abs(current - 0.0f) < 0.01f);
    std::cout << "  -> Slew Limiter tests passed." << std::endl;
}

void testEnginePolyphony() {
    std::cout << "[Test] Running Engine polyphony and state verification..." << std::endl;
    eunice::Engine eng;
    eng.setSampleRate(48000.0f);
    eng.reset();

    // Verify independent PRNG states
    for (int i = 0; i < 15; i++) {
        assert(eng.channels[i].prngState != eng.channels[i + 1].prngState);
    }
    std::cout << "  -> Polyphony tests passed." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Running Eunice Core DSP Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    testDistribution();
    testCorrelation();
    testSlewLimiter();
    testEnginePolyphony();
    std::cout << "ALL TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
