#include <cassert>
#include <iostream>
#include <cmath>
#include "../src/core/Matrix3x3Core.hpp"

using namespace matrix3x3;

void test_center_scan_no_crosstalk() {
    Matrix3x3Engine engine;
    engine.voltageRange = RANGE_0_5V;

    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    
    // At bleed = 0.0, center cell (Cell 4 at 0, 0) should be active, but all 8 adjacent cells MUST BE EXACTLY 0.0
    EngineOutput out = engine.process(0.0f, 0.0f, 0.0f, atts);

    assert(out.cellWeights[4] > 0.99f);
    assert(std::abs(out.outVolts[4] - 5.0f) < 0.01f);

    for (int i = 0; i < 9; ++i) {
        if (i != 4) {
            assert(out.cellWeights[i] == 0.0f);
            assert(out.outVolts[i] == 0.0f);
        }
    }
    std::cout << "[PASS] test_center_scan_no_crosstalk" << std::endl;
}

void test_corner_scan_no_crosstalk() {
    Matrix3x3Engine engine;
    engine.voltageRange = RANGE_0_10V;

    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    // Top-left corner: (-0.75, 0.75) -> Cell 0
    EngineOutput out = engine.process(-0.75f, 0.75f, 0.0f, atts);

    assert(out.cellWeights[0] > 0.99f);
    assert(std::abs(out.outVolts[0] - 10.0f) < 0.01f);

    // All other 8 cells must be completely 0.0
    for (int i = 1; i < 9; ++i) {
        assert(out.cellWeights[i] == 0.0f);
        assert(out.outVolts[i] == 0.0f);
    }
    std::cout << "[PASS] test_corner_scan_no_crosstalk" << std::endl;
}

void test_bleed_expansion() {
    Matrix3x3Engine engine;
    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    // At 50% bleed, adjacent cells begin receiving crossfade energy smoothly
    EngineOutput out = engine.process(0.0f, 0.0f, 0.5f, atts);
    assert(out.cellWeights[4] > 0.8f);
    // Orthogonally adjacent cells (Cell 1, 3, 5, 7) receive energy
    assert(out.cellWeights[1] > 0.05f);
    assert(out.cellWeights[3] > 0.05f);
    assert(out.cellWeights[5] > 0.05f);
    assert(out.cellWeights[7] > 0.05f);
    std::cout << "[PASS] test_bleed_expansion" << std::endl;
}

int main() {
    std::cout << "--- Running Matrix 3x3 Unit Tests ---" << std::endl;
    test_center_scan_no_crosstalk();
    test_corner_scan_no_crosstalk();
    test_bleed_expansion();
    std::cout << "All Matrix 3x3 tests passed successfully!" << std::endl;
    return 0;
}
