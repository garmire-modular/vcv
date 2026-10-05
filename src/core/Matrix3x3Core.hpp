#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace matrix3x3 {

template <typename T>
inline T clamp(T val, T lo, T hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
}

enum VoltageRange {
    RANGE_0_1V = 0,
    RANGE_0_5V = 1,
    RANGE_0_10V = 2
};

struct EngineOutput {
    float outVolts[9];
    float cellWeights[9];
};

class Matrix3x3Engine {
public:
    VoltageRange voltageRange = RANGE_0_5V;

    inline float getMaxVoltage() const {
        switch (voltageRange) {
            case RANGE_0_1V: return 1.0f;
            case RANGE_0_10V: return 10.0f;
            case RANGE_0_5V:
            default: return 5.0f;
        }
    }

    EngineOutput process(float scanX, float scanY, float bleed, const float attenuverters[9]) const {
        // Grid cell centers: 3x3 plane in [-1.0, 1.0]^2
        static constexpr float cell_x[9] = {-0.75f, 0.00f, 0.75f, -0.75f, 0.00f, 0.75f, -0.75f, 0.00f, 0.75f};
        static constexpr float cell_y[9] = { 0.75f, 0.75f, 0.75f,  0.00f, 0.00f, 0.00f, -0.75f,-0.75f,-0.75f};

        // scanX, scanY are in [-1.0, 1.0]
        // bleed is [0.0, 1.0]: controls gaussian variance (spread / diffusion)
        // at bleed = 0.0: sharp focus (sigma ~ 0.35)
        // at bleed = 1.0: wide dispersion across grid (sigma ~ 1.20)
        float sigma = 0.35f + bleed * 0.85f;
        float sigma2 = 2.0f * sigma * sigma;

        float maxV = getMaxVoltage();
        EngineOutput out;

        for (int i = 0; i < 9; ++i) {
            float dx = scanX - cell_x[i];
            float dy = scanY - cell_y[i];
            float dist2 = dx * dx + dy * dy;
            float w = std::exp(-dist2 / sigma2);
            w = clamp(w, 0.0f, 1.0f);

            out.cellWeights[i] = w;

            // attenuverters are [0.0, 1.0]
            float att = clamp(attenuverters[i], 0.0f, 1.0f);
            out.outVolts[i] = w * att * maxV;
        }

        return out;
    }
};

} // namespace matrix3x3
