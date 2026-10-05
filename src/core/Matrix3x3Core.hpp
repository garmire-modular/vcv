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
        // Grid cell centers spanning [0.0, 1.0] in normalized Cartesian space:
        // Left column: 0.0, Middle column: 0.5, Right column: 1.0
        // Bottom row: 0.0, Middle row: 0.5, Top row: 1.0
        //
        // Row 1 (Top / Cells 1-3):    Y = 1.0; X = 0.0 (Cell 1), 0.5 (Cell 2), 1.0 (Cell 3)
        // Row 2 (Middle / Cells 4-6): Y = 0.5; X = 0.0 (Cell 4), 0.5 (Cell 5), 1.0 (Cell 6)
        // Row 3 (Bottom / Cells 7-9): Y = 0.0; X = 0.0 (Cell 7), 0.5 (Cell 8), 1.0 (Cell 9)
        static constexpr float cell_x[9] = {
            0.0f, 0.5f, 1.0f,
            0.0f, 0.5f, 1.0f,
            0.0f, 0.5f, 1.0f
        };
        static constexpr float cell_y[9] = {
            1.0f, 1.0f, 1.0f,
            0.5f, 0.5f, 0.5f,
            0.0f, 0.0f, 0.0f
        };

        // scanX, scanY are in [0.0, 1.0] (0% to 100%)
        // At scanX = 0%, scanY = 0% -> exactly on Cell 7 (bottom-left) -> distance 0.0 -> full brightness (1.0)
        // bleed is [0.0, 1.0]: controls gaussian variance (spread / diffusion)
        bleed = clamp(bleed, 0.0f, 1.0f);
        float sigma = 0.12f + bleed * 0.60f;
        float sigma2 = 2.0f * sigma * sigma;

        // Cutoff radius: adjacent cells are at distance 0.50
        // At bleed = 0.0, cutoff = 0.35 (less than 0.50 distance to adjacent cells) -> strictly ZERO crosstalk
        // At bleed = 1.0, cutoff = 2.0 (encompasses entire matrix)
        float rCutoff = 0.35f + bleed * 1.65f;
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
