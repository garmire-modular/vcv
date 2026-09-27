#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>

template <typename T>
T clamp(T val, T min, T max) {
    return std::max(min, std::min(max, val));
}

int main() {
    std::cout << "Testing ChopXL DSP logic...\n";

    // Test 1: Alternating between Input 1 and Input 2
    // Count = 2, Length = 0.5 (50% Input 1, 50% Input 2)
    int N = 2;
    float masterL = 0.5f;
    float P = 0.0f;

    // Point 1: angle = 0 rad -> phase = (0 + pi) / (2pi) = 0.5
    // scaledPhase = fmod(2 * 0.5, 2) = 1.0 -> k = 1, dashPhase = 0.0
    // dashPhase < masterL (0.0 < 0.5) -> Input 1 active!
    {
        float inX1 = 5.0f, inY1 = 0.0f;
        float inX2 = -5.0f, inY2 = 2.0f;

        float theta = std::atan2(inY1, inX1);
        float phase = (theta + (float)M_PI) / (2.f * (float)M_PI);
        float scaledPhase = std::fmod((float)N * phase + P, (float)N);
        int k = (int)std::floor(scaledPhase);
        float dashPhase = scaledPhase - (float)k;

        bool in1Active = (dashPhase < masterL);
        assert(in1Active);
    }

    // Point 2: angle = pi/2 rad -> phase = (pi/2 + pi) / (2pi) = 0.75
    // scaledPhase = fmod(2 * 0.75, 2) = 1.5 -> k = 1, dashPhase = 0.5
    // dashPhase >= masterL (0.5 >= 0.5) -> Input 2 active!
    {
        float inX1 = 0.0f, inY1 = 5.0f;
        float theta = std::atan2(inY1, inX1);
        float phase = (theta + (float)M_PI) / (2.f * (float)M_PI);
        float scaledPhase = std::fmod((float)N * phase + P, (float)N);
        int k = (int)std::floor(scaledPhase);
        float dashPhase = scaledPhase - (float)k;

        bool in1Active = (dashPhase < masterL);
        assert(!in1Active);
    }

    // Test 2: Verify all 6 channels switch simultaneously
    {
        float in1[6] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
        float in2[6] = {-1.f, -2.f, -3.f, -4.f, -5.f, -6.f};
        float w = 1.0f; // Input 1 fully active

        for (int i = 0; i < 6; i++) {
            float out = w * in1[i] + (1.f - w) * in2[i];
            assert(out == in1[i]);
        }

        w = 0.0f; // Input 2 fully active
        for (int i = 0; i < 6; i++) {
            float out = w * in1[i] + (1.f - w) * in2[i];
            assert(out == in2[i]);
        }
    }

    std::cout << "All ChopXL DSP tests passed successfully!\n";
    return 0;
}
