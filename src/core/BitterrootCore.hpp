#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace bitterroot {

template <typename T>
inline T clamp(T val, T lo, T hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
}

// 10-bit coordinate mask
static constexpr uint16_t MASK_10BIT = 0x03FF; // 1023
static constexpr uint32_t MASK_20BIT = 0x000FFFFF; // 1048575

enum VoltageRange {
    RANGE_BIPOLAR_5V = 0,
    RANGE_UNIPOLAR_10V = 1
};

enum ZScaleMode {
    Z_SCALE_1V = 0,
    Z_SCALE_5V = 1,
    Z_SCALE_10V = 2
};

enum RouteMode {
    ROUTE_SERIAL = 0,
    ROUTE_MATRIX_SCAN = 1
};

enum SlewMode {
    SLEW_OFF = 0,
    SLEW_SUBTLE = 1,
    SLEW_MEDIUM = 2,
    SLEW_HEAVY = 3
};

enum DecimationMode {
    DECIMATE_NONE = 1,
    DECIMATE_48K = 2,
    DECIMATE_24K = 4,
    DECIMATE_12K = 8,
    DECIMATE_6K = 16,
    DECIMATE_3K = 32,
    DECIMATE_1K = 96
};

// Irreducible polynomials for GF(2^10)
static constexpr uint16_t GF10_POLYS[8] = {
    0x0409, // x^10 + x^3 + 1
    0x0481, // x^10 + x^7 + 1
    0x0611, // x^10 + x^9 + x^4 + 1
    0x050D, // x^10 + x^8 + x^3 + x^2 + 1
    0x046F, // x^10 + x^6 + x^5 + x^3 + x^2 + x + 1
    0x0425, // x^10 + x^5 + x^2 + 1
    0x0679, // x^10 + x^9 + x^6 + x^5 + x^4 + x^3 + 1
    0x04D5  // x^10 + x^7 + x^6 + x^4 + x^2 + 1
};

// Helper: Popcount for 10-bit integers
inline uint16_t popcount10(uint16_t v) {
    v &= MASK_10BIT;
    v = v - ((v >> 1) & 0x5555);
    v = (v & 0x3333) + ((v >> 2) & 0x3333);
    return (uint16_t)(((v + (v >> 4)) & 0x0F0F) * 0x0101) >> 8;
}

// Helper: Parity (odd/even)
inline uint16_t parity10(uint16_t v) {
    return popcount10(v) & 1;
}

// Helper: Bit reverse lowest W bits
inline uint16_t bitReverseW(uint16_t val, int width) {
    if (width <= 1) return val & MASK_10BIT;
    uint16_t rev = 0;
    for (int i = 0; i < width; ++i) {
        if ((val >> i) & 1) {
            rev |= (1 << (width - 1 - i));
        }
    }
    uint16_t mask = (1 << width) - 1;
    return ((val & ~mask) | rev) & MASK_10BIT;
}

// Helper: GF(2^10) carry-less polynomial multiplication
inline uint16_t gf10_multiply(uint16_t a, uint16_t b, uint16_t poly) {
    uint16_t res = 0;
    a &= MASK_10BIT;
    b &= MASK_10BIT;
    while (b > 0) {
        if (b & 1) res ^= a;
        b >>= 1;
        bool carry = (a & 0x0200) != 0;
        a = (a << 1) & MASK_10BIT;
        if (carry) a ^= (poly & MASK_10BIT);
    }
    return res;
}

// Helper: GF(2^10) power (a^exp)
inline uint16_t gf10_pow(uint16_t a, uint16_t exp, uint16_t poly) {
    uint16_t res = 1;
    a &= MASK_10BIT;
    while (exp > 0) {
        if (exp & 1) res = gf10_multiply(res, a, poly);
        a = gf10_multiply(a, a, poly);
        exp >>= 1;
    }
    return res;
}

// Helper: GF(2^10) multiplicative inverse
inline uint16_t gf10_inverse(uint16_t a, uint16_t poly) {
    if ((a & MASK_10BIT) == 0) return 0;
    // By Fermat's Little Theorem in GF(2^10), a^(2^10 - 2) = a^1022 = a^-1
    return gf10_pow(a, 1022, poly);
}

// Coordinate Point structure
struct Point10 {
    uint16_t x;
    uint16_t y;
    float z; // Normalized [0.0, 1.0] intensity/blanking factor
};

// Parameter block for single effect
struct BlockParams {
    float p1 = 0.0f;     // Param 1
    float p2 = 0.0f;     // Param 2
    float p3 = 0.0f;     // Param 3 (Bespoke algorithmic parameter)
    float mix = 1.0f;    // Dry/Wet Mix: 0.0 (complete bypass) to 1.0 (100% wet, default 1.0 for standalone DSP struct)
    bool active = true;  // Active (true) or Bypassed (false)
};

// Engine Process Output
struct EngineOutput {
    float outX;
    float outY;
    float outZ;
    float cellActivity[9]; // 3x3 activity levels for visual matrix LEDs
};

class CoreEngine {
public:
    VoltageRange voltageRange = RANGE_BIPOLAR_5V;
    RouteMode routeMode = ROUTE_SERIAL;
    SlewMode slewMode = SLEW_OFF;
    int decimationRatio = 1;

    // Internal state for slew filtering and decimation
    float currentOutX = 0.0f;
    float currentOutY = 0.0f;
    float currentOutZ = 5.0f;

    uint32_t decimateCounter = 0;
    EngineOutput lastOutput = {0.0f, 0.0f, 5.0f, {0.0f}};

    uint16_t prevX = 512;
    uint16_t prevY = 512;

    CoreEngine() {
        reset();
    }

    void reset() {
        currentOutX = 0.0f;
        currentOutY = 0.0f;
        currentOutZ = (voltageRange == RANGE_BIPOLAR_5V) ? 5.0f : 10.0f;
        decimateCounter = 0;
        prevX = 512;
        prevY = 512;
        for (int i = 0; i < 9; ++i) {
            lastOutput.cellActivity[i] = 0.0f;
        }
    }

    // Convert analog voltage into 10-bit integer [0, 1023]
    inline uint16_t voltageTo10Bit(float v) const {
        float norm = 0.0f;
        if (voltageRange == RANGE_BIPOLAR_5V) {
            norm = (v + 5.0f) * 0.1f; // -5V..+5V -> 0.0..1.0
        } else {
            norm = v * 0.1f;          // 0V..+10V -> 0.0..1.0
        }
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        return (uint16_t)std::floor(norm * 1023.0f + 0.5f);
    }

    ZScaleMode zScaleMode = Z_SCALE_5V;

    inline float getZMaxVoltage() const {
        switch (zScaleMode) {
            case Z_SCALE_1V: return 1.0f;
            case Z_SCALE_10V: return 10.0f;
            case Z_SCALE_5V:
            default: return 5.0f;
        }
    }

    // Convert 10-bit integer [0, 1023] into analog voltage
    inline float tenBitToVoltage(uint16_t d) const {
        float norm = (float)(d & MASK_10BIT) / 1023.0f;
        if (voltageRange == RANGE_BIPOLAR_5V) {
            return norm * 10.0f - 5.0f; // -5V..+5V
        } else {
            return norm * 10.0f;        // 0V..+10V
        }
    }

    // Convert normalized Z [0.0, 1.0] into analog Z voltage scaled by zScaleMode
    inline float normZToVoltage(float z) const {
        z = clamp(z, 0.0f, 1.0f);
        return z * getZMaxVoltage();
    }

    // Helper to blend Dry/Wet coordinates
    inline Point10 applyMix(uint16_t inX, uint16_t inY, Point10 fx, float mix) const {
        if (mix >= 0.999f) return fx;
        float blendX = (1.0f - mix) * (float)inX + mix * (float)fx.x;
        float blendY = (1.0f - mix) * (float)inY + mix * (float)fx.y;
        float blendZ = (1.0f - mix) * 1.0f + mix * fx.z;
        return {
            (uint16_t)clamp((int)std::round(blendX), 0, (int)MASK_10BIT),
            (uint16_t)clamp((int)std::round(blendY), 0, (int)MASK_10BIT),
            clamp(blendZ, 0.0f, 1.0f)
        };
    }

    // -------------------------------------------------------------
    // TRANSFORMATION EFFECT BLOCKS (A through I)
    // -------------------------------------------------------------

    // Block A: Morton Order & Space-Filling Curves
    inline Point10 processBlockA(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        int shift = (int)std::floor(p.p1 + 0.5f);
        shift = clamp(shift, 0, 19);

        int strideMode = (int)std::floor(p.p2 + 0.5f);
        strideMode = clamp(strideMode, 0, 2);

        float hilbertMorph = clamp(p.p3, 0.0f, 1.0f);

        // Interleave 10 bits of X and Y into 20-bit key
        uint32_t z = 0;
        if (strideMode == 1) { // Inverted axis priority
            for (int i = 0; i < 10; ++i) {
                z |= (((inY >> i) & 1) << (2 * i)) | (((inX >> i) & 1) << (2 * i + 1));
            }
        } else if (strideMode == 2) { // 2-bit block interleave
            for (int i = 0; i < 5; ++i) {
                uint32_t bx = (inX >> (2 * i)) & 3;
                uint32_t by = (inY >> (2 * i)) & 3;
                z |= (bx << (4 * i)) | (by << (4 * i + 2));
            }
        } else { // Standard Morton
            for (int i = 0; i < 10; ++i) {
                z |= (((inX >> i) & 1) << (2 * i)) | (((inY >> i) & 1) << (2 * i + 1));
            }
        }

        // Apply circular bit rotation
        uint32_t zRot = ((z >> shift) | (z << (20 - shift))) & MASK_20BIT;

        // De-interleave
        uint16_t outX = 0, outY = 0;
        if (strideMode == 1) {
            for (int i = 0; i < 10; ++i) {
                outY |= ((zRot >> (2 * i)) & 1) << i;
                outX |= ((zRot >> (2 * i + 1)) & 1) << i;
            }
        } else if (strideMode == 2) {
            for (int i = 0; i < 5; ++i) {
                outX |= ((zRot >> (4 * i)) & 3) << (2 * i);
                outY |= ((zRot >> (4 * i + 2)) & 3) << (2 * i);
            }
        } else {
            for (int i = 0; i < 10; ++i) {
                outX |= ((zRot >> (2 * i)) & 1) << i;
                outY |= ((zRot >> (2 * i + 1)) & 1) << i;
            }
        }

        // Hilbert morph: reflect alternating quadrants
        if (hilbertMorph > 0.01f) {
            uint16_t qx = outX;
            uint16_t qy = outY;
            if ((qx & 0x200) != 0) qy ^= 0x1FF;
            if ((qy & 0x200) != 0) qx ^= 0x1FF;
            outX = (uint16_t)std::round((1.0f - hilbertMorph) * outX + hilbertMorph * qx);
            outY = (uint16_t)std::round((1.0f - hilbertMorph) * outY + hilbertMorph * qy);
        }

        // Jump blanking: dim on massive leaps across quadrants
        float leap = (std::abs((int)outX - (int)inX) + std::abs((int)outY - (int)inY)) / 1024.0f;
        float zInt = clamp(1.0f - leap * 0.85f, 0.1f, 1.0f);

        Point10 wet = { (uint16_t)(outX & MASK_10BIT), (uint16_t)(outY & MASK_10BIT), zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // Block B: Bitwise Reversal (Dyadic Reflection)
    inline Point10 processBlockB(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        int width = (int)std::floor(p.p1 + 0.5f);
        width = clamp(width, 1, 10);

        uint16_t offset = (uint16_t)std::floor(p.p2 + 0.5f) & MASK_10BIT;
        int skew = (int)std::floor(p.p3 + 0.5f);
        skew = clamp(skew, -5, 5);

        int wX = clamp(width + skew, 1, 10);
        int wY = clamp(width - skew, 1, 10);

        uint16_t preX = (inX ^ offset) & MASK_10BIT;
        uint16_t preY = (inY ^ offset) & MASK_10BIT;

        uint16_t outX = bitReverseW(preX, wX);
        uint16_t outY = bitReverseW(preY, wY);

        // Dyadic slew blanking
        float leap = (std::abs((int)outX - (int)inX) + std::abs((int)outY - (int)inY)) / 1024.0f;
        float zInt = clamp(1.0f - leap * 0.9f, 0.05f, 1.0f);

        Point10 wet = { outX, outY, zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // Block C: Bit-Plane Transposition
    inline Point10 processBlockC(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        int pa = clamp((int)std::floor(p.p1 + 0.5f), 0, 9);
        int pb = clamp((int)std::floor(p.p2 + 0.5f), 0, 9);
        int pc = clamp((int)std::floor(p.p3 + 0.5f), 0, 9);

        uint16_t outX = inX;
        uint16_t outY = inY;

        uint16_t bitXa = (inX >> pa) & 1;
        uint16_t bitYb = (inY >> pb) & 1;
        uint16_t bitXc = (inX >> pc) & 1;

        // 3-way permutation cycle: X[pa] <- Y[pb] <- X[pc] <- X[pa]
        outX = (outX & ~(1 << pa)) | (bitYb << pa);
        outY = (outY & ~(1 << pb)) | (bitXc << pb);
        outX = (outX & ~(1 << pc)) | (bitXa << pc);

        // Layered diffraction luminance
        float zInt = 0.5f + 0.25f * bitXa + 0.25f * bitYb;

        Point10 wet = { outX, outY, zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // Block D: Carry-Propagated Cross-Modulation (Shear Wrapping)
    inline Point10 processBlockD(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        uint16_t mask = (uint16_t)std::floor(p.p1 + 0.5f) & MASK_10BIT;
        int shift = clamp((int)std::floor(p.p2 + 0.5f), 0, 4);
        float borrow = clamp(p.p3, 0.0f, 1.0f);

        uint16_t carry = ((inX & inY & mask) << 1) & MASK_10BIT;
        uint16_t cascade = (carry << shift) & MASK_10BIT;

        uint16_t outX = (inX ^ (inY & mask)) & MASK_10BIT;
        uint16_t outY = 0;

        if (borrow <= 0.5f) {
            outY = (inY + cascade) & MASK_10BIT;
        } else {
            outY = (inY - cascade) & MASK_10BIT;
        }

        // Avalanche seam illumination
        float carryPop = (float)popcount10(carry) / 10.0f;
        float zInt = clamp(0.4f + carryPop * 0.6f, 0.1f, 1.0f);

        Point10 wet = { outX, outY, zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // Block E: Circular Permutation Matrix
    inline Point10 processBlockE(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        int mode = clamp((int)std::floor(p.p1 + 0.5f), 0, 3);
        int rot = clamp((int)std::floor(p.p2 + 0.5f), 0, 19);
        int strideIdx = clamp((int)std::floor(p.p3 + 0.5f), 0, 5);

        static constexpr int STRIDES[6] = {1, 3, 5, 7, 9, 11};
        int stride = STRIDES[strideIdx];

        uint32_t u = (((uint32_t)inX << 10) | inY) & MASK_20BIT;
        uint32_t perm = 0;

        if (mode == 0) { // Odd / Even Split
            for (int i = 0; i < 10; ++i) {
                perm |= ((u >> (2 * i)) & 1) << i;
                perm |= ((u >> (2 * i + 1)) & 1) << (10 + i);
            }
        } else if (mode == 1) { // Perfect Shuffle
            for (int i = 0; i < 10; ++i) {
                perm |= ((u >> i) & 1) << (2 * i);
                perm |= ((u >> (10 + i)) & 1) << (2 * i + 1);
            }
        } else if (mode == 2) { // Inversion Shuffling
            uint32_t lo = u & 0x3FF;
            uint32_t hi = (u >> 10) & 0x3FF;
            perm = ((uint32_t)bitReverseW((uint16_t)hi, 10) << 10) | bitReverseW((uint16_t)lo, 10);
        } else { // Quadrant Rotation (5-bit nibble cycle)
            perm = ((u >> 5) | (u << 15)) & MASK_20BIT;
        }

        // Rotate by rot * stride mod 20
        int effectiveRot = (rot * stride) % 20;
        uint32_t shifted = ((perm >> effectiveRot) | (perm << (20 - effectiveRot))) & MASK_20BIT;

        uint16_t outX = (uint16_t)((shifted >> 10) & MASK_10BIT);
        uint16_t outY = (uint16_t)(shifted & MASK_10BIT);

        // Virtual 4D hypercube depth shading (middle bits)
        float depth = (float)((shifted >> 5) & 0x3FF) / 1023.0f;
        float zInt = clamp(0.2f + 0.8f * depth, 0.1f, 1.0f);

        Point10 wet = { outX, outY, zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // Block F: Gray Code Dyadic Folding
    inline Point10 processBlockF(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        int depth = clamp((int)std::floor(p.p1 + 0.5f), 1, 10);
        int mode = clamp((int)std::floor(p.p2 + 0.5f), 0, 2);
        int tap = clamp((int)std::floor(p.p3 + 0.5f), 1, 9);

        uint16_t mask = (1 << depth) - 1;
        uint16_t outX = inX;
        uint16_t outY = inY;

        auto binToGray = [](uint16_t val, int k, uint16_t m) -> uint16_t {
            uint16_t target = val & m;
            uint16_t g = target ^ (target >> k);
            return (val & ~m) | (g & m);
        };

        auto grayToBin = [](uint16_t val, uint16_t m) -> uint16_t {
            uint16_t target = val & m;
            uint16_t b = 0;
            for (uint16_t t = target; t > 0; t >>= 1) {
                b ^= t;
            }
            return (val & ~m) | (b & m);
        };

        if (mode == 0) { // Binary to Gray
            outX = binToGray(inX, tap, mask);
            outY = binToGray(inY, tap, mask);
        } else if (mode == 1) { // Gray to Binary
            outX = grayToBin(inX, mask);
            outY = grayToBin(inY, mask);
        } else { // Dual Reflected Cascade
            outX = binToGray(inX, tap, mask);
            outY = grayToBin(inY, mask);
        }

        // Parity zebra grading
        float zInt = 0.6f + 0.4f * (parity10(outX) ^ parity10(outY));

        Point10 wet = { (uint16_t)(outX & MASK_10BIT), (uint16_t)(outY & MASK_10BIT), zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // Block G: Galois Field GF(2^10) Polynomial Scramble
    inline Point10 processBlockG(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        int polyIdx = clamp((int)std::floor(p.p1 + 0.5f), 0, 7);
        uint16_t poly = GF10_POLYS[polyIdx];

        uint16_t alpha = clamp((uint16_t)std::floor(p.p2 + 0.5f), (uint16_t)1, (uint16_t)MASK_10BIT);
        int powerMode = clamp((int)std::floor(p.p3 + 0.5f), 0, 3);

        uint16_t outX = inX;
        uint16_t outY = inY;

        if (powerMode == 0) { // Linear multiplication
            outX = gf10_multiply(inX, alpha, poly);
            outY = gf10_multiply(inY, alpha, poly);
        } else if (powerMode == 1) { // Inversion
            outX = gf10_multiply(gf10_inverse(inX, poly), alpha, poly);
            outY = gf10_multiply(gf10_inverse(inY, poly), alpha, poly);
        } else if (powerMode == 2) { // Cube X^3
            outX = gf10_multiply(gf10_pow(inX, 3, poly), alpha, poly);
            outY = gf10_multiply(gf10_pow(inY, 3, poly), alpha, poly);
        } else { // S-Box Quintic X^5
            outX = gf10_multiply(gf10_pow(inX, 5, poly), alpha, poly);
            outY = gf10_multiply(gf10_pow(inY, 5, poly), alpha, poly);
        }

        // Cryptographic hop blanking
        float leap = (std::abs((int)outX - (int)inX) + std::abs((int)outY - (int)inY)) / 1024.0f;
        float zInt = clamp(1.0f - leap * 0.95f, 0.05f, 1.0f);

        Point10 wet = { (uint16_t)(outX & MASK_10BIT), (uint16_t)(outY & MASK_10BIT), zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // Block H: 1D Elementary Cellular Automata Mesh
    inline Point10 processBlockH(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        uint8_t rule = (uint8_t)clamp((int)std::floor(p.p1 + 0.5f), 0, 255);
        int steps = clamp((int)std::floor(p.p2 + 0.5f), 1, 4);
        int inject = clamp((int)std::floor(p.p3 + 0.5f), 0, 2);

        uint16_t curX = inX & MASK_10BIT;
        uint16_t curY = inY & MASK_10BIT;

        for (int s = 0; s < steps; ++s) {
            // Apply cross-axis injection
            if (inject == 0) { // Edge seeding
                curX = (curX & ~0x201) | (((curY >> 9) & 1) << 9) | (curY & 1);
                curY = (curY & ~0x201) | (((curX >> 9) & 1) << 9) | (curX & 1);
            } else if (inject == 1) { // Center seeding
                curX = (curX & ~0x030) | (((curY >> 4) & 3) << 4);
                curY = (curY & ~0x030) | (((curX >> 4) & 3) << 4);
            } else { // Full XOR superposition
                curX = (curX ^ ((curY >> 1) | (curY << 9))) & MASK_10BIT;
                curY = (curY ^ ((curX >> 1) | (curX << 9))) & MASK_10BIT;
            }

            auto evolveCA = [rule](uint16_t state) -> uint16_t {
                uint16_t next = 0;
                for (int i = 0; i < 10; ++i) {
                    int left = (state >> ((i + 9) % 10)) & 1;
                    int mid = (state >> i) & 1;
                    int right = (state >> ((i + 1) % 10)) & 1;
                    int neighborhood = (left << 2) | (mid << 1) | right;
                    if ((rule >> neighborhood) & 1) {
                        next |= (1 << i);
                    }
                }
                return next;
            };

            curX = evolveCA(curX);
            curY = evolveCA(curY);
        }

        // CA living state contrast
        float aliveFraction = (float)(popcount10(curX) + popcount10(curY)) / 20.0f;
        float zInt = clamp(0.2f + 0.8f * aliveFraction, 0.1f, 1.0f);

        Point10 wet = { curX, curY, zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // Block I: Popcount / Hamming Dispersion
    inline Point10 processBlockI(uint16_t inX, uint16_t inY, const BlockParams& p) const {
        if (!p.active || p.mix <= 0.0001f) return {inX, inY, 1.0f};

        int gain = clamp((int)std::floor(p.p1 + 0.5f), 0, 32);
        int mode = clamp((int)std::floor(p.p2 + 0.5f), 0, 2);
        float mutual = clamp(p.p3, 0.0f, 1.0f);

        uint16_t popX = popcount10(inX);
        uint16_t popY = popcount10(inY);
        uint16_t mutPop = popcount10(inX ^ inY);

        int signX = parity10(inY) ? -1 : 1;
        int signY = parity10(inX) ? -1 : 1;

        int shiftX = 0, shiftY = 0;
        if (mode == 0) { // Sign flip displacement
            shiftX = (int)std::round((1.0f - mutual) * (popY * gain * signX) + mutual * (mutPop * gain * signX));
            shiftY = (int)std::round((1.0f - mutual) * (popX * gain * signY) + mutual * (mutPop * gain * signY));
        } else if (mode == 1) { // Hamming shear
            shiftX = (int)std::round((1.0f - mutual) * (popY * gain) + mutual * (mutPop * gain));
            shiftY = 0;
        } else { // Orthogonal jump
            shiftX = (popX > 5 ? gain * 8 : -gain * 8);
            shiftY = (popY > 5 ? gain * 8 : -gain * 8);
        }

        uint16_t outX = (uint16_t)((inX + shiftX) & MASK_10BIT);
        uint16_t outY = (uint16_t)((inY + shiftY) & MASK_10BIT);

        // Topographic energy shading
        float energy = (float)(popX + popY + mutPop) / 30.0f;
        float zInt = clamp(0.2f + 0.8f * energy, 0.1f, 1.0f);

        Point10 wet = { outX, outY, zInt };
        return applyMix(inX, inY, wet, p.mix);
    }

    // -------------------------------------------------------------
    // MAIN DSP PROCESS FUNCTION
    // -------------------------------------------------------------
    EngineOutput process(float inX_V, float inY_V,
                         const BlockParams blocks[9],
                         float scanX, float scanY,
                         float zBlankThreshold,
                         float sampleRate) {
        // Handle decimation
        if (decimationRatio > 1) {
            decimateCounter++;
            if (decimateCounter < (uint32_t)decimationRatio) {
                return lastOutput;
            }
            decimateCounter = 0;
        }

        // 1. Convert incoming voltages to 10-bit integer coordinates
        uint16_t dX = voltageTo10Bit(inX_V);
        uint16_t dY = voltageTo10Bit(inY_V);

        Point10 resPoints[9];
        EngineOutput out;

        if (routeMode == ROUTE_SERIAL) {
            // Mode 0: Serial Pipeline (Morton -> Reverse -> Transpose -> ... -> Hamming)
            Point10 cur = {dX, dY, 1.0f};
            float minBlockZ = 1.0f;
            cur = processBlockA(cur.x, cur.y, blocks[0]); resPoints[0] = cur; if (blocks[0].active) minBlockZ = std::min(minBlockZ, cur.z);
            cur = processBlockB(cur.x, cur.y, blocks[1]); resPoints[1] = cur; if (blocks[1].active) minBlockZ = std::min(minBlockZ, cur.z);
            cur = processBlockC(cur.x, cur.y, blocks[2]); resPoints[2] = cur; if (blocks[2].active) minBlockZ = std::min(minBlockZ, cur.z);
            cur = processBlockD(cur.x, cur.y, blocks[3]); resPoints[3] = cur; if (blocks[3].active) minBlockZ = std::min(minBlockZ, cur.z);
            cur = processBlockE(cur.x, cur.y, blocks[4]); resPoints[4] = cur; if (blocks[4].active) minBlockZ = std::min(minBlockZ, cur.z);
            cur = processBlockF(cur.x, cur.y, blocks[5]); resPoints[5] = cur; if (blocks[5].active) minBlockZ = std::min(minBlockZ, cur.z);
            cur = processBlockG(cur.x, cur.y, blocks[6]); resPoints[6] = cur; if (blocks[6].active) minBlockZ = std::min(minBlockZ, cur.z);
            cur = processBlockH(cur.x, cur.y, blocks[7]); resPoints[7] = cur; if (blocks[7].active) minBlockZ = std::min(minBlockZ, cur.z);
            cur = processBlockI(cur.x, cur.y, blocks[8]); resPoints[8] = cur; if (blocks[8].active) minBlockZ = std::min(minBlockZ, cur.z);

            float targetX_V = tenBitToVoltage(cur.x);
            float targetY_V = tenBitToVoltage(cur.y);

            // True Jump-Blanking: calculate normalized Euclidean hop distance across 10-bit frame
            float hopDx = (float)cur.x - (float)prevX;
            float hopDy = (float)cur.y - (float)prevY;
            float hopDistNorm = std::sqrt(hopDx * hopDx + hopDy * hopDy) / 1023.0f;

            // Retrace Blanking: sensitive to zBlankThreshold
            // At threshold 0.0: no blanking on hops; at 1.0: aggressive blanking on any hop
            float hopBlank = clamp(1.0f - (hopDistNorm * zBlankThreshold * 4.0f), 0.0f, 1.0f);
            float blockBlank = clamp(1.0f - ((1.0f - minBlockZ) * zBlankThreshold), 0.0f, 1.0f);
            float targetZ_V = normZToVoltage(hopBlank * blockBlank);

            // Activity LED tracking: In Serial mode, light up with the percentage that mix is active
            for (int i = 0; i < 9; ++i) {
                out.cellActivity[i] = clamp(blocks[i].mix, 0.0f, 1.0f);
            }

            applySlew(targetX_V, targetY_V, targetZ_V, sampleRate);
            out.outX = currentOutX;
            out.outY = currentOutY;
            out.outZ = currentOutZ;

            prevX = cur.x;
            prevY = cur.y;

        } else {
            // Mode 1: 2D Matrix Crossfade Scanner
            resPoints[0] = processBlockA(dX, dY, blocks[0]);
            resPoints[1] = processBlockB(dX, dY, blocks[1]);
            resPoints[2] = processBlockC(dX, dY, blocks[2]);
            resPoints[3] = processBlockD(dX, dY, blocks[3]);
            resPoints[4] = processBlockE(dX, dY, blocks[4]);
            resPoints[5] = processBlockF(dX, dY, blocks[5]);
            resPoints[6] = processBlockG(dX, dY, blocks[6]);
            resPoints[7] = processBlockH(dX, dY, blocks[7]);
            resPoints[8] = processBlockI(dX, dY, blocks[8]);

            // Grid cell centers: 3x3 plane in [-1.0, 1.0]^2
            static constexpr float CELL_X[9] = {-0.75f, 0.00f, 0.75f, -0.75f, 0.00f, 0.75f, -0.75f, 0.00f, 0.75f};
            static constexpr float CELL_Y[9] = { 0.75f, 0.75f, 0.75f,  0.00f, 0.00f, 0.00f, -0.75f,-0.75f,-0.75f};

            float weights[9];
            float totalWeight = 0.0001f;
            static constexpr float SIGMA2 = 2.0f * 0.45f * 0.45f;

            for (int i = 0; i < 9; ++i) {
                float dx = scanX - CELL_X[i];
                float dy = scanY - CELL_Y[i];
                float dist2 = dx * dx + dy * dy;
                float w = std::exp(-dist2 / SIGMA2);
                weights[i] = w;
                totalWeight += w;
                // In Matrix Mode, LEDs visually trace the 2D scan cursor across the 3x3 plane
                out.cellActivity[i] = clamp(w, 0.0f, 1.0f);
            }

            float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
            for (int i = 0; i < 9; ++i) {
                float normW = weights[i] / totalWeight;
                sumX += (float)resPoints[i].x * normW;
                sumY += (float)resPoints[i].y * normW;
                sumZ += resPoints[i].z * normW;
            }

            uint16_t finalX = (uint16_t)clamp((int)std::round(sumX), 0, (int)MASK_10BIT);
            uint16_t finalY = (uint16_t)clamp((int)std::round(sumY), 0, (int)MASK_10BIT);

            float targetX_V = tenBitToVoltage(finalX);
            float targetY_V = tenBitToVoltage(finalY);

            // True Jump-Blanking on synthesized output coordinates
            float hopDx = (float)finalX - (float)prevX;
            float hopDy = (float)finalY - (float)prevY;
            float hopDistNorm = std::sqrt(hopDx * hopDx + hopDy * hopDy) / 1023.0f;

            float hopBlank = clamp(1.0f - (hopDistNorm * zBlankThreshold * 4.0f), 0.0f, 1.0f);
            float blockBlank = clamp(1.0f - ((1.0f - sumZ) * zBlankThreshold), 0.0f, 1.0f);
            float targetZ_V = normZToVoltage(hopBlank * blockBlank);

            applySlew(targetX_V, targetY_V, targetZ_V, sampleRate);
            out.outX = currentOutX;
            out.outY = currentOutY;
            out.outZ = currentOutZ;

            prevX = finalX;
            prevY = finalY;
        }

        lastOutput = out;
        return out;
    }

private:
    inline void applySlew(float targetX, float targetY, float targetZ, float sampleRate) {
        if (slewMode == SLEW_OFF || sampleRate <= 0.0f) {
            currentOutX = targetX;
            currentOutY = targetY;
            currentOutZ = targetZ;
            return;
        }

        float cutoffHz = 20000.0f;
        if (slewMode == SLEW_SUBTLE) cutoffHz = 16000.0f;
        else if (slewMode == SLEW_MEDIUM) cutoffHz = 8000.0f;
        else if (slewMode == SLEW_HEAVY) cutoffHz = 3000.0f;

        float alpha = 1.0f - std::exp(-2.0f * 3.1415926535f * cutoffHz / sampleRate);
        alpha = clamp(alpha, 0.001f, 1.0f);

        currentOutX += alpha * (targetX - currentOutX);
        currentOutY += alpha * (targetY - currentOutY);
        currentOutZ += alpha * (targetZ - currentOutZ);
    }
};

} // namespace bitterroot
