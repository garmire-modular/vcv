#include "../src/core/BitterrootCore.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void testVoltageConversions() {
    std::cout << "[Test] Voltage Conversions & 10-bit Mapping..." << std::endl;
    bitterroot::CoreEngine engine;

    // Bipolar Mode (-5V .. +5V)
    engine.voltageRange = bitterroot::RANGE_BIPOLAR_5V;
    assert(engine.voltageTo10Bit(-5.0f) == 0);
    assert(engine.voltageTo10Bit(0.0f) == 512);
    assert(engine.voltageTo10Bit(5.0f) == 1023);

    // Clamping
    assert(engine.voltageTo10Bit(-10.0f) == 0);
    assert(engine.voltageTo10Bit(12.0f) == 1023);

    // DAC Reconstruction
    assert(std::abs(engine.tenBitToVoltage(0) - (-5.0f)) < 1e-3f);
    assert(std::abs(engine.tenBitToVoltage(512) - 0.004887f) < 0.02f);
    assert(std::abs(engine.tenBitToVoltage(1023) - 5.0f) < 1e-3f);

    // Unipolar Mode (0V .. 10V)
    engine.voltageRange = bitterroot::RANGE_UNIPOLAR_10V;
    assert(engine.voltageTo10Bit(0.0f) == 0);
    assert(engine.voltageTo10Bit(5.0f) == 512);
    assert(engine.voltageTo10Bit(10.0f) == 1023);
    assert(std::abs(engine.tenBitToVoltage(0) - 0.0f) < 1e-3f);
    assert(std::abs(engine.tenBitToVoltage(1023) - 10.0f) < 1e-3f);

    std::cout << "  -> Voltage Conversions PASSED." << std::endl;
}

void testBlockA_Morton() {
    std::cout << "[Test] Block A: Morton Order & Space-Filling Curves..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 0.0f; // Shift = 0
    p.p2 = 0.0f; // Stride = Standard
    p.p3 = 0.0f; // Hilbert = 0

    // With shift 0, Morton encode then decode returns identical values
    auto pt = engine.processBlockA(256, 512, p);
    assert(pt.x == 256);
    assert(pt.y == 512);
    assert(pt.z >= 0.0f && pt.z <= 1.0f);

    // Shift 1 bit
    p.p1 = 1.0f;
    auto ptRot = engine.processBlockA(256, 512, p);
    assert(ptRot.x <= 1023 && ptRot.y <= 1023);

    // Hilbert Morph
    p.p3 = 1.0f;
    auto ptHilb = engine.processBlockA(256, 512, p);
    assert(ptHilb.x <= 1023 && ptHilb.y <= 1023);

    std::cout << "  -> Block A PASSED." << std::endl;
}

void testBlockB_Reversal() {
    std::cout << "[Test] Block B: Bitwise Reversal..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 10.0f; // 10 bits reversed
    p.p2 = 0.0f;  // offset 0
    p.p3 = 0.0f;  // skew 0

    // Value 1 (0000000001_2) reversed over 10 bits becomes 512 (1000000000_2)
    auto pt = engine.processBlockB(1, 1, p);
    assert(pt.x == 512);
    assert(pt.y == 512);

    // Reversal of 512 gives 1
    auto pt2 = engine.processBlockB(512, 512, p);
    assert(pt2.x == 1);
    assert(pt2.y == 1);

    // Skew test: X has 10 bits reversed, Y has 5 bits
    p.p3 = 5.0f; // skew +5: wX = 10, wY = 5
    auto ptSkew = engine.processBlockB(1, 1, p);
    assert(ptSkew.x == 512);
    assert(ptSkew.y == 16); // 1 reversed in 5 bits is 16 (10000_2)

    std::cout << "  -> Block B PASSED." << std::endl;
}

void testBlockC_Transpose() {
    std::cout << "[Test] Block C: Bit-Plane Transposition..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 0.0f; // Bit 0
    p.p2 = 1.0f; // Bit 1
    p.p3 = 2.0f; // Bit 2

    // X = 1 (bit 0 = 1), Y = 0
    // Cycle: X[pa=0] <- Y[pb=1] (0)
    //        Y[pb=1] <- X[pc=2] (0)
    //        X[pc=2] <- X[pa=0] (1)
    // Result: X has bit 2 set = 4, Y has 0
    auto pt = engine.processBlockC(1, 0, p);
    assert(pt.x == 4);
    assert(pt.y == 0);

    std::cout << "  -> Block C PASSED." << std::endl;
}

void testBlockD_Avalanche() {
    std::cout << "[Test] Block D: Carry-Propagated Cross-Modulation..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 1023.0f; // Full mask
    p.p2 = 0.0f;    // Shift 0
    p.p3 = 0.0f;    // Addition

    auto pt = engine.processBlockD(511, 1, p);
    assert(pt.x <= 1023 && pt.y <= 1023);
    assert(pt.z >= 0.0f && pt.z <= 1.0f);

    // Borrow mode
    p.p3 = 1.0f;
    auto ptBorrow = engine.processBlockD(511, 1, p);
    assert(ptBorrow.x <= 1023 && ptBorrow.y <= 1023);

    std::cout << "  -> Block D PASSED." << std::endl;
}

void testBlockE_Permutation() {
    std::cout << "[Test] Block E: Circular Permutation Matrix..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 1.0f; // Perfect shuffle
    p.p2 = 0.0f; // Rotate 0
    p.p3 = 1.0f; // Stride index 1 (stride 3)

    auto pt = engine.processBlockE(256, 512, p);
    assert(pt.x <= 1023 && pt.y <= 1023);
    assert(pt.z >= 0.0f && pt.z <= 1.0f);

    std::cout << "  -> Block E PASSED." << std::endl;
}

void testBlockF_Gray() {
    std::cout << "[Test] Block F: Gray Code Dyadic Folding..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 10.0f; // 10 bits
    p.p2 = 0.0f;  // Binary to Gray
    p.p3 = 1.0f;  // Tap distance 1

    // Gray code of 2 (0b10) is 3 (0b11)
    auto pt = engine.processBlockF(2, 2, p);
    assert(pt.x == 3);
    assert(pt.y == 3);

    // Gray to Binary of 3 is 2
    p.p2 = 1.0f; // Gray to Binary
    auto pt2 = engine.processBlockF(3, 3, p);
    assert(pt2.x == 2);
    assert(pt2.y == 2);

    std::cout << "  -> Block F PASSED." << std::endl;
}

void testBlockG_Galois() {
    std::cout << "[Test] Block G: Galois Field GF(2^10) Scramble..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 0.0f; // Poly 0 (0x409)
    p.p2 = 3.0f; // Alpha = 3
    p.p3 = 0.0f; // Linear

    auto pt = engine.processBlockG(100, 200, p);
    assert(pt.x <= 1023 && pt.y <= 1023);

    // Inversion
    p.p3 = 1.0f; // Inversion
    auto ptInv = engine.processBlockG(100, 200, p);
    assert(ptInv.x <= 1023 && ptInv.y <= 1023);

    std::cout << "  -> Block G PASSED." << std::endl;
}

void testBlockH_Automata() {
    std::cout << "[Test] Block H: 1D Cellular Automata Mesh..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 90.0f; // Rule 90 (Sierpinski triangle XOR rule)
    p.p2 = 2.0f;  // 2 steps
    p.p3 = 0.0f;  // Edge seeding

    auto pt = engine.processBlockH(512, 256, p);
    assert(pt.x <= 1023 && pt.y <= 1023);
    assert(pt.z >= 0.0f && pt.z <= 1.0f);

    std::cout << "  -> Block H PASSED." << std::endl;
}

void testBlockI_Hamming() {
    std::cout << "[Test] Block I: Popcount / Hamming Dispersion..." << std::endl;
    bitterroot::CoreEngine engine;
    bitterroot::BlockParams p;
    p.active = true;
    p.p1 = 4.0f; // Gain 4
    p.p2 = 0.0f; // Sign flip
    p.p3 = 0.5f; // 50% mutual Hamming coupling

    auto pt = engine.processBlockI(512, 256, p);
    assert(pt.x <= 1023 && pt.y <= 1023);
    assert(pt.z >= 0.0f && pt.z <= 1.0f);

    std::cout << "  -> Block I PASSED." << std::endl;
}

void testFullEngineProcessing() {
    std::cout << "[Test] Full Engine (Cascade Serial & Matrix Scan)..." << std::endl;
    bitterroot::CoreEngine engine;
    // Helper lambda to make full wet blocks
    bitterroot::BlockParams blocks[9];
    for (int i = 0; i < 9; ++i) {
        blocks[i].active = true;
        blocks[i].p1 = 1.0f;
        blocks[i].p2 = 1.0f;
        blocks[i].p3 = 0.0f;
        blocks[i].mix = 1.0f;
    }

    // Test Serial Route Mode
    engine.routeMode = bitterroot::ROUTE_SERIAL;
    engine.zScaleMode = bitterroot::Z_SCALE_5V;
    auto out1 = engine.process(0.0f, 0.0f, blocks, 0.0f, 0.0f, 0.5f, 48000.0f);
    assert(std::isfinite(out1.outX) && std::isfinite(out1.outY) && std::isfinite(out1.outZ));
    assert(out1.outZ >= 0.0f && out1.outZ <= 5.0f);

    // Test Dry/Wet Mix Bypass: When all mix are 0.0f, output matches input exactly (100% bypass)
    bitterroot::BlockParams dryBlocks[9];
    for (int i = 0; i < 9; ++i) {
        dryBlocks[i].active = false;
        dryBlocks[i].p1 = 1.0f;
        dryBlocks[i].p2 = 1.0f;
        dryBlocks[i].p3 = 0.0f;
        dryBlocks[i].mix = 0.0f;
    }
    auto outDry = engine.process(2.5f, -3.0f, dryBlocks, 0.0f, 0.0f, 0.5f, 48000.0f);
    assert(std::abs(outDry.outX - 2.5f) < 0.02f);
    assert(std::abs(outDry.outY - (-3.0f)) < 0.02f);

    // Test Z Scaling
    engine.zScaleMode = bitterroot::Z_SCALE_1V;
    assert(engine.getZMaxVoltage() == 1.0f);
    auto outZ1 = engine.process(0.0f, 0.0f, blocks, 0.0f, 0.0f, 0.0f, 48000.0f);
    assert(outZ1.outZ <= 1.0001f);

    engine.zScaleMode = bitterroot::Z_SCALE_10V;
    assert(engine.getZMaxVoltage() == 10.0f);
    auto outZ10 = engine.process(0.0f, 0.0f, blocks, 0.0f, 0.0f, 0.0f, 48000.0f);
    assert(outZ10.outZ <= 10.0001f);

    engine.zScaleMode = bitterroot::Z_SCALE_5V;

    // Test Matrix Scan
    engine.routeMode = bitterroot::ROUTE_MATRIX_SCAN;
    auto out2 = engine.process(2.5f, -2.5f, blocks, 0.0f, 0.0f, 0.5f, 48000.0f);
    assert(std::isfinite(out2.outX) && std::isfinite(out2.outY) && std::isfinite(out2.outZ));
    assert(out2.outZ >= 0.0f && out2.outZ <= 5.0f);

    // Verify cell activities
    for (int i = 0; i < 9; ++i) {
        assert(out2.cellActivity[i] >= 0.0f && out2.cellActivity[i] <= 1.0f);
    }

    std::cout << "  -> Full Engine Processing, Mix Bypass & Z-Scaling PASSED." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "BITTERROOT CORE DSP UNIT TEST SUITE" << std::endl;
    std::cout << "========================================" << std::endl;

    testVoltageConversions();
    testBlockA_Morton();
    testBlockB_Reversal();
    testBlockC_Transpose();
    testBlockD_Avalanche();
    testBlockE_Permutation();
    testBlockF_Gray();
    testBlockG_Galois();
    testBlockH_Automata();
    testBlockI_Hamming();
    testFullEngineProcessing();

    std::cout << "\nALL 11 UNIT TESTS COMPLETED SUCCESSFULLY!" << std::endl;
    return 0;
}
