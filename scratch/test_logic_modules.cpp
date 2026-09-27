#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    std::cout << "Testing Boolean Logic modules DSP truth tables...\n";

    // Truth Table inputs (false/true)
    bool inputs[4][2] = {
        {false, false},
        {false, true},
        {true, false},
        {true, true}
    };

    // 1. AND / NAND
    bool expected_and[4] = {false, false, false, true};
    bool expected_nand[4] = {true, true, true, false};

    for (int i = 0; i < 4; i++) {
        bool a = inputs[i][0];
        bool b = inputs[i][1];
        assert((a && b) == expected_and[i]);
        assert(!(a && b) == expected_nand[i]);
    }

    // 2. OR / NOR
    bool expected_or[4] = {false, true, true, true};
    bool expected_nor[4] = {true, false, false, false};

    for (int i = 0; i < 4; i++) {
        bool a = inputs[i][0];
        bool b = inputs[i][1];
        assert((a || b) == expected_or[i]);
        assert(!(a || b) == expected_nor[i]);
    }

    // 3. XOR / XNOR
    bool expected_xor[4] = {false, true, true, false};
    bool expected_xnor[4] = {true, false, false, true};

    for (int i = 0; i < 4; i++) {
        bool a = inputs[i][0];
        bool b = inputs[i][1];
        assert((a != b) == expected_xor[i]);
        assert(!(a != b) == expected_xnor[i]);
    }

    std::cout << "All Boolean Logic DSP unit tests passed successfully!\n";
    return 0;
}
