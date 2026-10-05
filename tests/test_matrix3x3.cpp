#include <cassert>
#include <iostream>
#include <cmath>
#include "../src/core/Matrix3x3Core.hpp"

using namespace matrix3x3;

void test_center_scan() {
    Matrix3x3Engine engine;
    engine.voltageRange = RANGE_0_5V;

    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    EngineOutput out = engine.process(0.0f, 0.0f, 0.2f, atts);

    // Cell 4 is center (0.0, 0.0)
    assert(out.cellWeights[4] > 0.99f);
    assert(std::abs(out.outVolts[4] - 5.0f) < 0.05f);

    // Corner cells (-0.75, 0.75) should be noticeably lower than center
    assert(out.cellWeights[0] < out.cellWeights[4]);
    assert(out.cellWeights[8] < out.cellWeights[4]);
    std::cout << "[PASS] test_center_scan" << std::endl;
}

void test_corner_scan() {
    Matrix3x3Engine engine;
    engine.voltageRange = RANGE_0_10V;

    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    // Focus on top-left: (-0.75, 0.75) -> Cell 0
    EngineOutput out = engine.process(-0.75f, 0.75f, 0.0f, atts);

    assert(out.cellWeights[0] > 0.99f);
    assert(std::abs(out.outVolts[0] - 10.0f) < 0.05f);
    // Far corner Cell 8 (+0.75, -0.75) should be near 0
    assert(out.cellWeights[8] < 0.01f);
    std::cout << "[PASS] test_corner_scan" << std::endl;
}

void test_attenuators() {
    Matrix3x3Engine engine;
    engine.voltageRange = RANGE_0_1V;

    float atts[9] = {0.0f, 0.5f, 1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.5f, 1.0f};
    EngineOutput out = engine.process(0.0f, 0.0f, 0.2f, atts);

    // Center cell (Cell 4) has weight ~1.0, atten 0.5, maxV = 1.0V -> ~0.5V
    assert(std::abs(out.outVolts[4] - 0.5f) < 0.02f);
    std::cout << "[PASS] test_attenuators" << std::endl;
}

void test_bleed_dispersion() {
    Matrix3x3Engine engine;
    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    // Low bleed vs high bleed
    EngineOutput outTight = engine.process(0.0f, 0.0f, 0.0f, atts);
    EngineOutput outWide = engine.process(0.0f, 0.0f, 1.0f, atts);

    // With higher bleed, corner cell (Cell 0) should receive significantly more weight
    assert(outWide.cellWeights[0] > outTight.cellWeights[0]);
    std::cout << "[PASS] test_bleed_dispersion" << std::endl;
}

int main() {
    std::cout << "--- Running Matrix 3x3 Unit Tests ---" << std::endl;
    test_center_scan();
    test_corner_scan();
    test_attenuators();
    test_bleed_dispersion();
    std::cout << "All Matrix 3x3 tests passed successfully!" << std::endl;
    return 0;
}
