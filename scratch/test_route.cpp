#include <iostream>
#include <cmath>
#include <algorithm>

inline float clamp(float x, float minVal, float maxVal) {
    return std::max(minVal, std::min(x, maxVal));
}

struct SimpleSchmittTrigger {
    bool state = false;
    bool process(float in) {
        if (!state && in >= 1.5f) {
            state = true;
            return true;
        } else if (state && in <= 0.8f) {
            state = false;
        }
        return false;
    }
    void reset() { state = false; }
};

struct TestRoute {
    SimpleSchmittTrigger triggerX[16];
    SimpleSchmittTrigger triggerY[16];
    bool stateX[16] = {false};
    bool stateY[16] = {false};

    void step(
        int channels,
        const float* inX, const float* inY,
        bool swXConnected, const float* swX, bool swYConnected, const float* swY,
        float* outX1, float* outY1, float* outX2, float* outY2
    ) {
        for (int c = 0; c < channels; c++) {
            float x = inX ? inX[c] : 0.f;
            float y = inY ? inY[c] : 0.f;

            float sx = swXConnected ? swX[c] : 0.f;
            float sy = swYConnected ? swY[c] : sx;

            if (triggerX[c].process(sx)) {
                stateX[c] = !stateX[c];
            }

            if (swYConnected) {
                if (triggerY[c].process(sy)) {
                    stateY[c] = !stateY[c];
                }
            } else {
                stateY[c] = stateX[c];
            }

            float cx = clamp(x, -12.f, 12.f);
            float cy = clamp(y, -12.f, 12.f);

            outX1[c] = !stateX[c] ? cx : 0.f;
            outX2[c] = stateX[c] ? cx : 0.f;

            outY1[c] = !stateY[c] ? cy : 0.f;
            outY2[c] = stateY[c] ? cy : 0.f;
        }
    }
};

#define CHECK(cond) if (!(cond)) { std::cout << "CHECK FAILED at line " << __LINE__ << std::endl; return 1; }

int main() {
    TestRoute rt;

    // Test 1: Initial state routes to Output 1, Output 2 is 0V
    {
        float inX[1] = {5.0f};
        float inY[1] = {-3.0f};
        float outX1[1] = {0.f}, outY1[1] = {0.f}, outX2[1] = {0.f}, outY2[1] = {0.f};

        rt.step(1, inX, inY, false, nullptr, false, nullptr, outX1, outY1, outX2, outY2);
        CHECK(std::abs(outX1[0] - 5.0f) < 1e-5f);
        CHECK(std::abs(outY1[0] - (-3.0f)) < 1e-5f);
        CHECK(std::abs(outX2[0] - 0.0f) < 1e-5f);
        CHECK(std::abs(outY2[0] - 0.0f) < 1e-5f);
        std::cout << "[PASS] Test 1: Initial state passes to Output 1, Output 2 is 0V\n";
    }

    // Test 2: Trigger rising edge toggles to Output 2
    {
        float inX[1] = {5.0f};
        float inY[1] = {-3.0f};
        float swX[1] = {5.0f}; // Trigger pulse
        float outX1[1] = {0.f}, outY1[1] = {0.f}, outX2[1] = {0.f}, outY2[1] = {0.f};

        rt.step(1, inX, inY, true, swX, false, nullptr, outX1, outY1, outX2, outY2);
        CHECK(std::abs(outX1[0] - 0.0f) < 1e-5f);
        CHECK(std::abs(outY1[0] - 0.0f) < 1e-5f);
        CHECK(std::abs(outX2[0] - 5.0f) < 1e-5f);
        CHECK(std::abs(outY2[0] - (-3.0f)) < 1e-5f);
        std::cout << "[PASS] Test 2: First clock pulse toggles to Output 2, Output 1 is 0V\n";
    }

    // Test 3: Holding switch HIGH does not re-toggle (edge sensitive)
    {
        float inX[1] = {5.0f};
        float inY[1] = {-3.0f};
        float swX[1] = {5.0f}; // Still high
        float outX1[1] = {0.f}, outY1[1] = {0.f}, outX2[1] = {0.f}, outY2[1] = {0.f};

        rt.step(1, inX, inY, true, swX, false, nullptr, outX1, outY1, outX2, outY2);
        CHECK(std::abs(outX1[0] - 0.0f) < 1e-5f);
        CHECK(std::abs(outX2[0] - 5.0f) < 1e-5f);
        std::cout << "[PASS] Test 3: Sustained gate pulse does not re-toggle\n";
    }

    // Test 4: Second trigger pulse toggles back to Output 1
    {
        float inX[1] = {5.0f};
        float inY[1] = {-3.0f};
        float swLow[1] = {0.0f};
        float outX1[1] = {0.f}, outY1[1] = {0.f}, outX2[1] = {0.f}, outY2[1] = {0.f};

        // Reset trigger low
        rt.step(1, inX, inY, true, swLow, false, nullptr, outX1, outY1, outX2, outY2);

        // Next rising edge
        float swHigh[1] = {5.0f};
        rt.step(1, inX, inY, true, swHigh, false, nullptr, outX1, outY1, outX2, outY2);
        CHECK(std::abs(outX1[0] - 5.0f) < 1e-5f);
        CHECK(std::abs(outY1[0] - (-3.0f)) < 1e-5f);
        CHECK(std::abs(outX2[0] - 0.0f) < 1e-5f);
        CHECK(std::abs(outY2[0] - 0.0f) < 1e-5f);
        std::cout << "[PASS] Test 4: Second clock pulse toggles back to Output 1\n";
    }

    // Test 5: Distinct X and Y inputs (unpatched Y is 0V)
    {
        float inX[1] = {4.0f};
        float outX1[1] = {0.f}, outY1[1] = {0.f}, outX2[1] = {0.f}, outY2[1] = {0.f};

        rt.step(1, inX, nullptr, false, nullptr, false, nullptr, outX1, outY1, outX2, outY2);
        CHECK(std::abs(outX1[0] - 4.0f) < 1e-5f);
        CHECK(std::abs(outY1[0] - 0.0f) < 1e-5f); // Y is strictly 0V, never normalizes from X!
        std::cout << "[PASS] Test 5: X and Y inputs are strictly distinct\n";
    }

    // Test 6: Independent Y switch trigger
    {
        // Y switch can be toggled independently when patched
        float inX[1] = {2.0f};
        float inY[1] = {6.0f};
        float swX[1] = {0.0f}; // X stays on Output 1
        float swY[1] = {5.0f}; // Y toggles to Output 2
        float outX1[1] = {0.f}, outY1[1] = {0.f}, outX2[1] = {0.f}, outY2[1] = {0.f};

        rt.step(1, inX, inY, true, swX, true, swY, outX1, outY1, outX2, outY2);
        CHECK(std::abs(outX1[0] - 2.0f) < 1e-5f); // X on Output 1
        CHECK(std::abs(outX2[0] - 0.0f) < 1e-5f);
        CHECK(std::abs(outY1[0] - 0.0f) < 1e-5f); // Y on Output 2
        CHECK(std::abs(outY2[0] - 6.0f) < 1e-5f);
        std::cout << "[PASS] Test 6: Independent X and Y switching operates correctly\n";
    }

    std::cout << "\nAll Route DSP verification tests PASSED!\n";
    return 0;
}
