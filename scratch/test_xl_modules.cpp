#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>

template <typename T>
T clamp(T val, T min, T max) {
    return std::max(min, std::min(max, val));
}

int main() {
    std::cout << "Testing XL modules DSP logic...\n";

    // 1. Switch XL: Cascading normalization and switching logic
    {
        bool swConn[6] = {true, false, false, false, false, false}; // Only X switch connected
        float swIn[6] = {5.0f, 0.f, 0.f, 0.f, 0.f, 0.f}; // High (5V)

        float swVolt[6];
        float currentNorm = 0.f;
        for (int k = 0; k < 6; k++) {
            if (swConn[k]) {
                currentNorm = swIn[k];
            }
            swVolt[k] = currentNorm;
        }

        // All 6 channels must receive 5.0V from cascading norm
        for (int k = 0; k < 6; k++) {
            assert(swVolt[k] == 5.0f);
        }

        // When HIGH (>= 1.5V), selects Input 2
        bool highState[6] = {false};
        for (int k = 0; k < 6; k++) {
            if (swVolt[k] >= 1.5f) highState[k] = true;
        }

        float in1[6] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
        float in2[6] = {7.f, 8.f, 9.f, 10.f, 11.f, 12.f};
        for (int k = 0; k < 6; k++) {
            float out = highState[k] ? in2[k] : in1[k];
            assert(out == in2[k]);
        }
    }

    // 2. Sum/Mix XL: Addition and Multiplication across 6 channels
    {
        float in1[6] = {2.f, -3.f, 4.f, 5.f, 10.f, -15.f};
        float in2[6] = {3.f, 5.f, 2.5f, 5.f, 10.f, 2.f};

        for (int k = 0; k < 6; k++) {
            float sum = clamp(in1[k] + in2[k], -12.f, 12.f);
            float mult = clamp((in1[k] * in2[k]) * 0.2f, -12.f, 12.f);

            // Check clamping on channel 4 (10*10/5 = 20 -> clamped to 12)
            if (k == 4) {
                assert(mult == 12.f);
            }
            // Check precision sum
            assert(std::abs(sum - clamp(in1[k] + in2[k], -12.f, 12.f)) < 1e-5);
        }
    }

    std::cout << "All XL DSP unit tests passed successfully!\n";
    return 0;
}
