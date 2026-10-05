#include <cassert>
#include <iostream>
#include <cmath>
#include "../src/core/Matrix3x3Core.hpp"

using namespace matrix3x3;

void test_bottom_left_at_0_percent() {
    Matrix3x3Engine engine;
    engine.voltageRange = RANGE_0_5V;

    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    // When Scan X = 0% (0.0) and Scan Y = 0% (0.0), bottom-left LED (Cell 7 / index 6) must be full brightness (1.0)
    EngineOutput out = engine.process(0.0f, 0.0f, 0.0f, atts);

    assert(out.cellWeights[6] > 0.99f);
    assert(std::abs(out.outVolts[6] - 5.0f) < 0.01f);

    // With bleed at 0%, all other 8 cells must be exactly 0.0
    for (int i = 0; i < 9; ++i) {
        if (i != 6) {
            assert(out.cellWeights[i] == 0.0f);
            assert(out.outVolts[i] == 0.0f);
        }
    }
    std::cout << "[PASS] test_bottom_left_at_0_percent" << std::endl;
}

void test_center_at_50_percent() {
    Matrix3x3Engine engine;
    engine.voltageRange = RANGE_0_10V;

    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    // When Scan X = 50% (0.5) and Scan Y = 50% (0.5), center LED (Cell 5 / index 4) must be full brightness
    EngineOutput out = engine.process(0.5f, 0.5f, 0.0f, atts);

    assert(out.cellWeights[4] > 0.99f);
    assert(std::abs(out.outVolts[4] - 10.0f) < 0.01f);

    for (int i = 0; i < 9; ++i) {
        if (i != 4) {
            assert(out.cellWeights[i] == 0.0f);
            assert(out.outVolts[i] == 0.0f);
        }
    }
    std::cout << "[PASS] test_center_at_50_percent" << std::endl;
}

void test_top_right_at_100_percent() {
    Matrix3x3Engine engine;
    engine.voltageRange = RANGE_0_1V;

    float atts[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    // When Scan X = 100% (1.0) and Scan Y = 100% (1.0), top-right LED (Cell 3 / index 2) must be full brightness
    EngineOutput out = engine.process(1.0f, 1.0f, 0.0f, atts);

    assert(out.cellWeights[2] > 0.99f);
    assert(std::abs(out.outVolts[2] - 1.0f) < 0.01f);

    for (int i = 0; i < 9; ++i) {
        if (i != 2) {
            assert(out.cellWeights[i] == 0.0f);
            assert(out.outVolts[i] == 0.0f);
        }
    }
    std::cout << "[PASS] test_top_right_at_100_percent" << std::endl;
}

int main() {
    std::cout << "--- Running Matrix 3x3 Coordinate Tests ---" << std::endl;
    test_bottom_left_at_0_percent();
    test_center_at_50_percent();
    test_top_right_at_100_percent();
    std::cout << "All Matrix 3x3 tests passed successfully!" << std::endl;
    return 0;
}
