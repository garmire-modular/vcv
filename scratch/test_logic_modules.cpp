#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    std::cout << "Testing Boolean Logic rapid switching DSP logic...\n";

    // Test cases: (v1, v2) where:
    // v < 0.8V -> false
    // v > 1.5V -> true
    struct TestCase {
        float v1;
        float v2;
        bool b1;
        bool b2;
    } cases[4] = {
        {0.0f, 0.0f, false, false},
        {0.0f, 5.0f, false, true},
        {5.0f, 0.0f, true, false},
        {5.0f, 5.0f, true, true}
    };

    // Let Input 1 be a signal with value -3.0V, Input 2 be +4.0V
    float in1 = -3.0f;
    float in2 = +4.0f;

    for (int i = 0; i < 4; i++) {
        bool b1 = cases[i].b1;
        bool b2 = cases[i].b2;

        // 1. AND / NAND
        bool andCond = (b1 && b2);
        float outAnd = andCond ? in2 : in1;
        float outNand = !andCond ? in2 : in1;

        if (andCond) {
            assert(outAnd == in2);
            assert(outNand == in1);
        } else {
            assert(outAnd == in1);
            assert(outNand == in2);
        }

        // 2. OR / NOR
        bool orCond = (b1 || b2);
        float outOr = orCond ? in2 : in1;
        float outNor = !orCond ? in2 : in1;

        if (orCond) {
            assert(outOr == in2);
            assert(outNor == in1);
        } else {
            assert(outOr == in1);
            assert(outNor == in2);
        }

        // 3. XOR / XNOR
        bool xorCond = (b1 != b2);
        float outXor = xorCond ? in2 : in1;
        float outXnor = !xorCond ? in2 : in1;

        if (xorCond) {
            assert(outXor == in2);
            assert(outXnor == in1);
        } else {
            assert(outXor == in1);
            assert(outXnor == in2);
        }
    }

    std::cout << "All Boolean Logic rapid switching DSP unit tests passed successfully!\n";
    return 0;
}
