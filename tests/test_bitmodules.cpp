#include <cassert>
#include <iostream>
#include <cmath>
#include "../src/core/BitDSPCore.hpp"

using namespace bitdsp;

void test_all_modules_bypass() {
    // When mix is 0.0, every module must return input untouched
    PointXY in = {1.23f, -2.45f};
    assert(processMorton(in.x, in.y, 5.0f, 1.0f, 0.5f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    assert(processReverse(in.x, in.y, 8.0f, 128.0f, 1.0f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    assert(processTranspose(in.x, in.y, 2.0f, 4.0f, 7.0f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    assert(processValanche(in.x, in.y, 255.0f, 2.0f, 0.0f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    assert(processPermute(in.x, in.y, 1.0f, 5.0f, 2.0f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    assert(processGrayBin(in.x, in.y, 8.0f, 1.0f, 3.0f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    assert(processGalois(in.x, in.y, 0.0f, 3.0f, 0.0f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    assert(processBitomata(in.x, in.y, 90.0f, 2.0f, 1.0f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    assert(processHamming(in.x, in.y, 8.0f, 0.0f, 0.5f, 0.0f, RANGE_BIPOLAR_5V).x == in.x);
    std::cout << "[PASS] test_all_modules_bypass" << std::endl;
}

void test_morton_transform() {
    PointXY in = {0.0f, 0.0f}; // 512, 512
    PointXY out = processMorton(in.x, in.y, 1.0f, 0.0f, 0.0f, 1.0f, RANGE_BIPOLAR_5V);
    // Rotating key changes 10-bit coordinates
    assert(out.x >= -5.0f && out.x <= 5.0f);
    assert(out.y >= -5.0f && out.y <= 5.0f);
    std::cout << "[PASS] test_morton_transform" << std::endl;
}

void test_galois_identity() {
    PointXY in = {0.0f, 0.0f}; // 512, 512
    // Alpha = 1 in Linear mode (p3 = 0) is identity
    PointXY out = processGalois(in.x, in.y, 0.0f, 1.0f, 0.0f, 1.0f, RANGE_BIPOLAR_5V);
    assert(std::abs(out.x - in.x) < 0.02f);
    assert(std::abs(out.y - in.y) < 0.02f);
    std::cout << "[PASS] test_galois_identity" << std::endl;
}

int main() {
    std::cout << "--- Running BitModules Unit Tests ---" << std::endl;
    test_all_modules_bypass();
    test_morton_transform();
    test_galois_identity();
    std::cout << "All BitModules tests passed successfully!" << std::endl;
    return 0;
}
