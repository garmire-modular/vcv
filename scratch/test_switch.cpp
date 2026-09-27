#include <iostream>
#include <cmath>
#include <algorithm>

inline float clamp(float x, float minVal, float maxVal) {
    return std::max(minVal, std::min(x, maxVal));
}

struct TestSwitch {
    bool highX[16] = {false};
    bool highY[16] = {false};

    void step(
        int channels,
        const float* inX1, const float* inY1,
        const float* inX2, const float* inY2,
        bool swXConnected, const float* swX, bool swYConnected, const float* swY,
        float* outX, float* outY
    ) {
        for (int c = 0; c < channels; c++) {
            float x1 = inX1 ? inX1[c] : 0.f;
            float y1 = inY1 ? inY1[c] : 0.f;

            float x2 = inX2 ? inX2[c] : 0.f;
            float y2 = inY2 ? inY2[c] : 0.f;

            float sx = swXConnected ? swX[c] : 0.f;
            float sy = swYConnected ? swY[c] : sx;

            if (!highX[c] && sx >= 1.5f) {
                highX[c] = true;
            } else if (highX[c] && sx <= 0.8f) {
                highX[c] = false;
            }

            if (!highY[c] && sy >= 1.5f) {
                highY[c] = true;
            } else if (highY[c] && sy <= 0.8f) {
                highY[c] = false;
            }

            float ox = highX[c] ? x2 : x1;
            float oy = highY[c] ? y2 : y1;

            outX[c] = clamp(ox, -12.f, 12.f);
            outY[c] = clamp(oy, -12.f, 12.f);
        }
    }
};

#define CHECK(cond) if (!(cond)) { std::cout << "CHECK FAILED at line " << __LINE__ << std::endl; return 1; }

int main() {
    TestSwitch sw;

    // Test 1: Unpatched switch defaults to input 1 (main)
    {
        float inX1[1] = {3.5f};
        float inY1[1] = {2.0f};
        float inX2[1] = {-4.0f};
        float inY2[1] = {-1.0f};
        float outX[1] = {0.f};
        float outY[1] = {0.f};

        sw.step(1, inX1, inY1, inX2, inY2, false, nullptr, false, nullptr, outX, outY);
        CHECK(std::abs(outX[0] - 3.5f) < 1e-5f);
        CHECK(std::abs(outY[0] - 2.0f) < 1e-5f);
        std::cout << "[PASS] Test 1: Unpatched switch passes input 1\n";
    }

    // Test 2: High switch pulse (>= 1.5V) passes input 2 (alternate)
    {
        float inX1[1] = {3.5f};
        float inY1[1] = {2.0f};
        float inX2[1] = {-4.0f};
        float inY2[1] = {-1.0f};
        float swX[1] = {5.0f};
        float outX[1] = {0.f};
        float outY[1] = {0.f};

        // Y switch unpatched -> normalizes to X switch
        sw.step(1, inX1, inY1, inX2, inY2, true, swX, false, nullptr, outX, outY);
        CHECK(std::abs(outX[0] - (-4.0f)) < 1e-5f);
        CHECK(std::abs(outY[0] - (-1.0f)) < 1e-5f);
        std::cout << "[PASS] Test 2: High track pulse passes input 2 (with Y switch normalizing from X switch)\n";
    }

    // Test 3: X and Y are distinct (unpatched Y is 0V, never normalizes to X)
    {
        float inX1[1] = {7.0f};
        float inX2[1] = {-8.0f};
        float outX[1] = {0.f};
        float outY[1] = {0.f};

        // Track LOW -> passes input 1
        float swLow[1] = {0.0f};
        sw.step(1, inX1, nullptr, inX2, nullptr, true, swLow, false, nullptr, outX, outY);
        CHECK(std::abs(outX[0] - 7.0f) < 1e-5f);
        CHECK(std::abs(outY[0] - 0.0f) < 1e-5f); // Y is 0, NOT 7!

        // Track HIGH -> passes input 2
        float swHigh[1] = {5.0f};
        sw.step(1, inX1, nullptr, inX2, nullptr, true, swHigh, false, nullptr, outX, outY);
        CHECK(std::abs(outX[0] - (-8.0f)) < 1e-5f);
        CHECK(std::abs(outY[0] - 0.0f) < 1e-5f); // Y is 0, NOT -8!
        std::cout << "[PASS] Test 3: X and Y inputs are strictly distinct (no normalization)\n";
    }

    // Test 4: Independent X and Y switching
    {
        float inX1[1] = {1.0f};
        float inY1[1] = {2.0f};
        float inX2[1] = {10.0f};
        float inY2[1] = {9.0f};
        float swX[1] = {0.0f}; // LOW -> X passes inX1 (1.0)
        float swY[1] = {5.0f}; // HIGH -> Y passes inY2 (9.0)
        float outX[1] = {0.f};
        float outY[1] = {0.f};

        sw.step(1, inX1, inY1, inX2, inY2, true, swX, true, swY, outX, outY);
        CHECK(std::abs(outX[0] - 1.0f) < 1e-5f);
        CHECK(std::abs(outY[0] - 9.0f) < 1e-5f);
        std::cout << "[PASS] Test 4: Independent X and Y switching operates correctly\n";
    }

    std::cout << "\nAll Switch DSP verification tests PASSED!\n";
    return 0;
}
