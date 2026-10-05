#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
        // Pitch = 0.75: dx between adjacent cells is 0.75
        static constexpr float cell_x[9] = {-0.75f, 0.00f, 0.75f, -0.75f, 0.00f, 0.75f, -0.75f, 0.00f, 0.75f};
        static constexpr float cell_y[9] = { 0.75f, 0.75f, 0.75f,  0.00f, 0.00f, 0.00f, -0.75f,-0.75f,-0.75f};

        // scanX, scanY are in [-1.0, 1.0]
        // bleed is [0.0, 1.0]: controls gaussian variance (spread / diffusion)
        // At bleed = 0.0: sharp focus, radius strictly truncated at cell boundary (no crosstalk to adjacent cells)
        // As bleed increases: smoothly widens dispersion across the matrix
        bleed = clamp(bleed, 0.0f, 1.0f);
        float sigma = 0.18f + bleed * 0.90f;
        float sigma2 = 2.0f * sigma * sigma;

        // Cutoff radius: at bleed = 0.0, cutoff = 0.50 (less than 0.75 distance to adjacent cells)
        // At bleed = 1.0, cutoff = 3.0 (encompasses entire matrix)
        float rCutoff = 0.50f + bleed * 2.50f;
        float rCutoff2 = rCutoff * rCutoff;

        float maxV = getMaxVoltage();
        EngineOutput out;

        for (int i = 0; i < 9; ++i) {
            float dx = scanX - cell_x[i];
            float dy = scanY - cell_y[i];
            float dist2 = dx * dx + dy * dy;

            float w = 0.0f;
            if (dist2 < rCutoff2) {
                float rawW = std::exp(-dist2 / sigma2);
                // Smooth Hann window taper to exactly 0 at rCutoff
                float r = std::sqrt(dist2);
                float taper = 0.5f * (1.0f + std::cos((float)M_PI * (r / rCutoff)));
                w = rawW * taper;
            }
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
