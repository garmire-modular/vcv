#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace eunice {

enum DistributionMode {
    DIST_POWER_LAW = 0,
    DIST_SIGMOID = 1,
    DIST_DIODE = 2
};

enum SlewProfile {
    SLEW_LINEAR = 0,
    SLEW_EXPONENTIAL = 1
};

enum SlewDestination {
    SLEW_DEST_SH = 0,
    SLEW_DEST_BOTH = 1,
    SLEW_DEST_TH = 2
};

// Fast xorshift32 PRNG for zero-heap, deterministic audio-rate noise
inline float xorshift32_float(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    // Map to [-1.0f, +1.0f]
    return (static_cast<float>(state) / 2147483648.0f) - 1.0f;
}

struct Engine {
    static constexpr int MAX_CHANNELS = 16;

    // Per-channel states
    struct ChannelState {
        uint32_t prngState = 0x12345678;
        float triPhase = 0.0f;
        float lpfState = 0.0f;
        float heldSH = 0.0f;
        float heldTH = 0.0f;
        float slewedSH = 0.0f;
        float slewedTH = 0.0f;
        float lastClock = 0.0f;
        bool clockHigh = false;
    };

    ChannelState channels[MAX_CHANNELS];

    // Global internal clock phase
    float clockPhase = 0.0f;
    float sampleRate = 44100.0f;

    // Modes
    DistributionMode distMode = DIST_POWER_LAW;
    SlewProfile slewProfile = SLEW_LINEAR;

    void setSampleRate(float sr) {
        if (sr > 1000.0f) {
            sampleRate = sr;
        }
    }

    void reset() {
        clockPhase = 0.0f;
        for (int c = 0; c < MAX_CHANNELS; c++) {
            channels[c].prngState = 0x12345678u + static_cast<uint32_t>(c * 199999 + 17);
            channels[c].triPhase = static_cast<float>(c) / static_cast<float>(MAX_CHANNELS);
            channels[c].lpfState = 0.0f;
            channels[c].heldSH = 0.0f;
            channels[c].heldTH = 0.0f;
            channels[c].slewedSH = 0.0f;
            channels[c].slewedTH = 0.0f;
            channels[c].lastClock = 0.0f;
            channels[c].clockHigh = false;
        }
    }

    // Mathematical transfer curve for Distribution Tilt
    static float applyDistribution(float vIn, float distParam, DistributionMode mode) {
        // distParam is normalized [0, 1], with 0.5 = neutral
        distParam = std::max(0.0f, std::min(1.0f, distParam));

        switch (mode) {
            case DIST_POWER_LAW: {
                // Map [-5V, +5V] to [0, 1]
                float norm = std::max(-5.0f, std::min(5.0f, vIn));
                float p = 0.5f * (norm / 5.0f + 1.0f);
                p = std::max(0.0f, std::min(1.0f, p));

                // Gamma: D=0.5 -> gamma=1.0; D=1.0 -> gamma=0.25 (skew up); D=0.0 -> gamma=4.0 (skew down)
                float gamma = std::pow(2.0f, 4.0f * (0.5f - distParam));
                float pSkew = std::pow(p, gamma);
                return 5.0f * (2.0f * pSkew - 1.0f);
            }
            case DIST_SIGMOID: {
                // Bias shift
                float delta = 3.0f * (2.0f * distParam - 1.0f);
                float x = (vIn + delta) / 5.0f;
                float normalizer = std::tanh(1.0f + std::abs(delta) / 5.0f);
                if (normalizer < 1e-4f) normalizer = 1.0f;
                return 5.0f * (std::tanh(x) / normalizer);
            }
            case DIST_DIODE: {
                if (distParam >= 0.5f) {
                    // Compress negative, allow positive
                    float kNeg = 1.0f - 1.8f * (distParam - 0.5f);
                    return (vIn >= 0.0f) ? vIn : (vIn * kNeg);
                } else {
                    // Compress positive, allow negative
                    float kPos = 1.0f - 1.8f * (0.5f - distParam);
                    return (vIn <= 0.0f) ? vIn : (vIn * kPos);
                }
            }
            default:
                return vIn;
        }
    }

    // Step a single slew limiter state
    static float stepSlew(float current, float target, float riseTime, float fallTime, float dt, SlewProfile profile) {
        float delta = target - current;
        if (std::abs(delta) < 1e-7f) {
            return target;
        }

        bool isRising = (delta > 0.0f);
        float t = isRising ? riseTime : fallTime;
        if (t < 0.0001f) {
            return target;
        }

        if (profile == SLEW_LINEAR) {
            // Maximum rate of change per second based on standard 10V span
            float maxDeltaPerSec = 10.0f / t;
            float maxStep = maxDeltaPerSec * dt;
            if (isRising) {
                return current + std::min(delta, maxStep);
            } else {
                return current - std::min(-delta, maxStep);
            }
        } else {
            // Exponential RC: y[n] = y[n-1] + alpha * (x[n] - y[n-1])
            float alpha = 1.0f - std::exp(-dt / t);
            alpha = std::max(0.0f, std::min(1.0f, alpha));
            return current + alpha * delta;
        }
    }
};

} // namespace eunice
