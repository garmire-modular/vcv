#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>

template <typename T>
T clamp(T val, T min, T max) {
    return std::max(min, std::min(max, val));
}

int main() {
    std::cout << "Testing SumMult DSP logic...\n";

    // Test 1: Simple Addition
    {
        float inX1 = 2.5f, inX2 = 3.5f;
        float inY1 = -1.0f, inY2 = 4.0f;

        float sumX = clamp(inX1 + inX2, -12.f, 12.f);
        float sumY = clamp(inY1 + inY2, -12.f, 12.f);

        assert(std::abs(sumX - 6.0f) < 1e-5);
        assert(std::abs(sumY - 3.0f) < 1e-5);
    }

    // Test 2: Multiplication scaling by 5V (standard Eurorack 4-quadrant multiplier)
    {
        float inX1 = 5.0f, inX2 = 5.0f;
        float multX = clamp((inX1 * inX2) * 0.2f, -12.f, 12.f);
        assert(std::abs(multX - 5.0f) < 1e-5); // 5V * 5V / 5 = 5V

        float inY1 = -5.0f, inY2 = 2.5f;
        float multY = clamp((inY1 * inY2) * 0.2f, -12.f, 12.f);
        assert(std::abs(multY - (-2.5f)) < 1e-5); // -5V * 2.5V / 5 = -2.5V
    }

    // Test 3: Clamping at galvo limits (-12V to +12V)
    {
        float inX1 = 10.0f, inX2 = 8.0f;
        float sumX = clamp(inX1 + inX2, -12.f, 12.f);
        assert(sumX == 12.0f);

        float inY1 = -8.0f, inY2 = -8.0f;
        float sumY = clamp(inY1 + inY2, -12.f, 12.f);
        assert(sumY == -12.0f);

        float inMultBig = clamp((10.0f * 10.0f) * 0.2f, -12.f, 12.f); // 100 / 5 = 20 -> clamped to 12
        assert(inMultBig == 12.0f);
    }

    // Test 4: Zero inputs (unpatched)
    {
        float inX1 = 3.0f, inX2 = 0.0f;
        float sumX = clamp(inX1 + inX2, -12.f, 12.f);
        float multX = clamp((inX1 * inX2) * 0.2f, -12.f, 12.f);
        assert(sumX == 3.0f);
        assert(multX == 0.0f);
    }

    std::cout << "All SumMult DSP tests passed successfully!\n";
    return 0;
}
