#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace bitdsp {

template <typename T>
inline T clamp(T val, T lo, T hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
}

static constexpr uint16_t MASK_10BIT = 0x03FF; // 1023
static constexpr uint32_t MASK_20BIT = 0x000FFFFF; // 1048575

enum VoltageRange {
    RANGE_BIPOLAR_5V = 0,
    RANGE_UNIPOLAR_10V = 1
};

inline uint16_t voltageTo10Bit(float v, VoltageRange range) {
    float norm = (range == RANGE_BIPOLAR_5V) ? (v + 5.0f) * 0.1f : v * 0.1f;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return (uint16_t)std::floor(norm * 1023.0f + 0.5f);
}

inline float tenBitToVoltage(uint16_t d, VoltageRange range) {
    float norm = (float)(d & MASK_10BIT) / 1023.0f;
    return (range == RANGE_BIPOLAR_5V) ? (norm * 10.0f - 5.0f) : (norm * 10.0f);
}

inline uint16_t bitReverseW(uint16_t val, int width) {
    width = clamp(width, 1, 10);
    uint16_t rev = 0;
    for (int i = 0; i < width; ++i) {
        if ((val >> i) & 1) {
            rev |= (1 << (width - 1 - i));
        }
    }
    uint16_t mask = (1 << width) - 1;
    return ((val & ~mask) | rev) & MASK_10BIT;
}

inline uint16_t popcount10(uint16_t v) {
    v &= MASK_10BIT;
    uint16_t c = 0;
    while (v) {
        c += (v & 1);
        v >>= 1;
    }
    return c;
}

inline bool parity10(uint16_t v) {
    return (popcount10(v) % 2) != 0;
}

static constexpr uint16_t GF10_POLYS[8] = {
    0x0409, 0x0481, 0x0611, 0x050D, 0x046F, 0x0425, 0x0679, 0x04D5
};

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

inline uint16_t gf10_inverse(uint16_t a, uint16_t poly) {
    if ((a & MASK_10BIT) == 0) return 0;
    return gf10_pow(a, 1022, poly);
}

struct PointXY {
    float x;
    float y;
};

inline PointXY applyMix(float inX, float inY, float wetX, float wetY, float mix) {
    mix = clamp(mix, 0.0f, 1.0f);
    if (mix <= 0.0001f) return {inX, inY};
    if (mix >= 0.9999f) return {wetX, wetY};
    return {
        (1.0f - mix) * inX + mix * wetX,
        (1.0f - mix) * inY + mix * wetY
    };
}

// 1. BitMorton
inline PointXY processMorton(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    int shift = clamp((int)std::floor(p1 + 0.5f), 0, 19);
    int strideMode = clamp((int)std::floor(p2 + 0.5f), 0, 2);
    float hilbertMorph = clamp(p3, 0.0f, 1.0f);

    uint32_t z = 0;
    if (strideMode == 1) {
        for (int i = 0; i < 10; ++i) {
            z |= (((inY >> i) & 1) << (2 * i)) | (((inX >> i) & 1) << (2 * i + 1));
        }
    } else if (strideMode == 2) {
        for (int i = 0; i < 5; ++i) {
            uint32_t bx = (inX >> (2 * i)) & 3;
            uint32_t by = (inY >> (2 * i)) & 3;
            z |= (bx << (4 * i)) | (by << (4 * i + 2));
        }
    } else {
        for (int i = 0; i < 10; ++i) {
            z |= (((inX >> i) & 1) << (2 * i)) | (((inY >> i) & 1) << (2 * i + 1));
        }
    }

    uint32_t zRot = ((z >> shift) | (z << (20 - shift))) & MASK_20BIT;
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

    if (hilbertMorph > 0.01f) {
        uint16_t qx = outX, qy = outY;
        if ((qx & 0x200) != 0) qy ^= 0x1FF;
        if ((qy & 0x200) != 0) qx ^= 0x1FF;
        outX = (uint16_t)std::round((1.0f - hilbertMorph) * outX + hilbertMorph * qx);
        outY = (uint16_t)std::round((1.0f - hilbertMorph) * outY + hilbertMorph * qy);
    }

    float wetX_V = tenBitToVoltage(outX, range);
    float wetY_V = tenBitToVoltage(outY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

// 2. BitReverse
inline PointXY processReverse(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    int width = clamp((int)std::floor(p1 + 0.5f), 1, 10);
    uint16_t offset = (uint16_t)std::floor(p2 + 0.5f) & MASK_10BIT;
    int skew = clamp((int)std::floor(p3 + 0.5f), -5, 5);

    int wX = clamp(width + skew, 1, 10);
    int wY = clamp(width - skew, 1, 10);

    uint16_t preX = (inX ^ offset) & MASK_10BIT;
    uint16_t preY = (inY ^ offset) & MASK_10BIT;

    uint16_t outX = bitReverseW(preX, wX);
    uint16_t outY = bitReverseW(preY, wY);

    float wetX_V = tenBitToVoltage(outX, range);
    float wetY_V = tenBitToVoltage(outY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

// 3. BitTranspose
inline PointXY processTranspose(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    int pa = clamp((int)std::floor(p1 + 0.5f), 0, 9);
    int pb = clamp((int)std::floor(p2 + 0.5f), 0, 9);
    int pc = clamp((int)std::floor(p3 + 0.5f), 0, 9);

    uint16_t outX = inX;
    uint16_t outY = inY;

    uint16_t bitXa = (inX >> pa) & 1;
    uint16_t bitYb = (inY >> pb) & 1;
    uint16_t bitXc = (inX >> pc) & 1;

    outX = (outX & ~(1 << pa)) | (bitYb << pa);
    outY = (outY & ~(1 << pb)) | (bitXc << pb);
    outX = (outX & ~(1 << pc)) | (bitXa << pc);

    float wetX_V = tenBitToVoltage(outX, range);
    float wetY_V = tenBitToVoltage(outY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

// 4. BitValanche
inline PointXY processValanche(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    uint16_t mask = (uint16_t)std::floor(p1 + 0.5f) & MASK_10BIT;
    int shift = clamp((int)std::floor(p2 + 0.5f), 0, 4);
    float borrow = clamp(p3, 0.0f, 1.0f);

    uint16_t carry = ((inX & inY & mask) << 1) & MASK_10BIT;
    uint16_t cascade = (carry << shift) & MASK_10BIT;

    uint16_t outX = (inX ^ (inY & mask)) & MASK_10BIT;
    uint16_t outY = (borrow <= 0.5f) ? ((inY + cascade) & MASK_10BIT) : ((inY - cascade) & MASK_10BIT);

    float wetX_V = tenBitToVoltage(outX, range);
    float wetY_V = tenBitToVoltage(outY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

// 5. BitPermute
inline PointXY processPermute(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    int mode = clamp((int)std::floor(p1 + 0.5f), 0, 3);
    int rot = clamp((int)std::floor(p2 + 0.5f), 0, 19);
    int strideIdx = clamp((int)std::floor(p3 + 0.5f), 0, 5);

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
    } else { // Quadrant Rotation
        perm = ((u >> 5) | (u << 15)) & MASK_20BIT;
    }

    int effectiveRot = (rot * stride) % 20;
    uint32_t shifted = ((perm >> effectiveRot) | (perm << (20 - effectiveRot))) & MASK_20BIT;

    uint16_t outX = (uint16_t)((shifted >> 10) & MASK_10BIT);
    uint16_t outY = (uint16_t)(shifted & MASK_10BIT);

    float wetX_V = tenBitToVoltage(outX, range);
    float wetY_V = tenBitToVoltage(outY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

// 6. BitGrayBin
inline PointXY processGrayBin(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    int depth = clamp((int)std::floor(p1 + 0.5f), 1, 10);
    int mode = clamp((int)std::floor(p2 + 0.5f), 0, 2);
    int tap = clamp((int)std::floor(p3 + 0.5f), 1, 9);

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

    if (mode == 0) {
        outX = binToGray(inX, tap, mask);
        outY = binToGray(inY, tap, mask);
    } else if (mode == 1) {
        outX = grayToBin(inX, mask);
        outY = grayToBin(inY, mask);
    } else {
        outX = binToGray(inX, tap, mask);
        outY = grayToBin(inY, mask);
    }

    float wetX_V = tenBitToVoltage(outX, range);
    float wetY_V = tenBitToVoltage(outY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

// 7. BitGalois
inline PointXY processGalois(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    int polyIdx = clamp((int)std::floor(p1 + 0.5f), 0, 7);
    uint16_t poly = GF10_POLYS[polyIdx];
    uint16_t alpha = clamp((uint16_t)std::floor(p2 + 0.5f), (uint16_t)1, (uint16_t)MASK_10BIT);
    int powerMode = clamp((int)std::floor(p3 + 0.5f), 0, 3);

    uint16_t outX = inX;
    uint16_t outY = inY;

    if (powerMode == 0) {
        outX = gf10_multiply(inX, alpha, poly);
        outY = gf10_multiply(inY, alpha, poly);
    } else if (powerMode == 1) {
        outX = gf10_multiply(gf10_inverse(inX, poly), alpha, poly);
        outY = gf10_multiply(gf10_inverse(inY, poly), alpha, poly);
    } else if (powerMode == 2) {
        outX = gf10_multiply(gf10_pow(inX, 3, poly), alpha, poly);
        outY = gf10_multiply(gf10_pow(inY, 3, poly), alpha, poly);
    } else {
        outX = gf10_multiply(gf10_pow(inX, 5, poly), alpha, poly);
        outY = gf10_multiply(gf10_pow(inY, 5, poly), alpha, poly);
    }

    float wetX_V = tenBitToVoltage(outX, range);
    float wetY_V = tenBitToVoltage(outY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

// 8. Bitomata
inline PointXY processBitomata(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    uint8_t rule = (uint8_t)clamp((int)std::floor(p1 + 0.5f), 0, 255);
    int steps = clamp((int)std::floor(p2 + 0.5f), 1, 4);
    int inject = clamp((int)std::floor(p3 + 0.5f), 0, 2);

    uint16_t curX = inX & MASK_10BIT;
    uint16_t curY = inY & MASK_10BIT;

    for (int s = 0; s < steps; ++s) {
        if (inject == 0) {
            curX = (curX & ~0x201) | (((curY >> 9) & 1) << 9) | (curY & 1);
            curY = (curY & ~0x201) | (((curX >> 9) & 1) << 9) | (curX & 1);
        } else if (inject == 1) {
            curX = (curX & ~0x030) | (((curY >> 4) & 3) << 4);
            curY = (curY & ~0x030) | (((curX >> 4) & 3) << 4);
        } else {
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

    float wetX_V = tenBitToVoltage(curX, range);
    float wetY_V = tenBitToVoltage(curY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

// 9. BitHamming
inline PointXY processHamming(float inX_V, float inY_V, float p1, float p2, float p3, float mix, VoltageRange range) {
    if (mix <= 0.0001f) return {inX_V, inY_V};
    uint16_t inX = voltageTo10Bit(inX_V, range);
    uint16_t inY = voltageTo10Bit(inY_V, range);

    int gain = clamp((int)std::floor(p1 + 0.5f), 0, 32);
    int mode = clamp((int)std::floor(p2 + 0.5f), 0, 2);
    float mutual = clamp(p3, 0.0f, 1.0f);

    uint16_t popX = popcount10(inX);
    uint16_t popY = popcount10(inY);
    uint16_t mutPop = popcount10(inX ^ inY);

    int signX = parity10(inY) ? -1 : 1;
    int signY = parity10(inX) ? -1 : 1;

    int shiftX = 0, shiftY = 0;
    if (mode == 0) {
        shiftX = (int)std::round((1.0f - mutual) * (popY * gain * signX) + mutual * (mutPop * gain * signX));
        shiftY = (int)std::round((1.0f - mutual) * (popX * gain * signY) + mutual * (mutPop * gain * signY));
    } else if (mode == 1) {
        shiftX = (int)std::round((1.0f - mutual) * (popY * gain) + mutual * (mutPop * gain));
        shiftY = 0;
    } else {
        shiftX = (popX > 5 ? gain * 8 : -gain * 8);
        shiftY = (popY > 5 ? gain * 8 : -gain * 8);
    }

    uint16_t outX = (uint16_t)((inX + shiftX) & MASK_10BIT);
    uint16_t outY = (uint16_t)((inY + shiftY) & MASK_10BIT);

    float wetX_V = tenBitToVoltage(outX, range);
    float wetY_V = tenBitToVoltage(outY, range);
    return applyMix(inX_V, inY_V, wetX_V, wetY_V, mix);
}

} // namespace bitdsp
