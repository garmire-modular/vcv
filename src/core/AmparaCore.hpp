#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace garmire {
namespace ampara {

inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

constexpr size_t BUFFER_SIZE = 65536; // 65536 samples (over 1.36 seconds at 48kHz, 680ms at 96kHz)
constexpr size_t BUFFER_MASK = BUFFER_SIZE - 1;

enum RangeMode {
    RANGE_VERY_SLOW = 0, // 600s down to 0.1s
    RANGE_LFO,            // 0.01 - 200 Hz
    RANGE_VCO             // 150 Hz - 2 kHz
};

// Pure C++ Core DSP Engine for Ampara voice
// Completely hardware-agnostic, zero heap allocations, real-time safe
struct VoiceCore {
    float phase = 0.0f;           // Carrier orbit phase [0, 1)
    float envL = 0.0f;            // Envelope detector Left
    float envR = 0.0f;            // Envelope detector Right
    float ringBuffer[BUFFER_SIZE] = {};
    size_t writeHead = 0;
    
    // Sync trigger state
    float lastSync = 0.0f;
    bool syncPulse = false;

    void reset() {
        phase = 0.0f;
        envL = 0.0f;
        envR = 0.0f;
        writeHead = 0;
        lastSync = 0.0f;
        syncPulse = false;
        for (size_t i = 0; i < BUFFER_SIZE; ++i) {
            ringBuffer[i] = 0.0f;
        }
    }

    // Calculates base carrier frequency from coarse & fine
    static float calculateBaseFrequency(RangeMode mode, float coarse, float fine) {
        coarse = clampf(coarse, 0.0f, 1.0f);
        fine = clampf(fine, -1.0f, 1.0f);

        if (mode == RANGE_VERY_SLOW) {
            float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
            float period = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
            return (period > 1e-5f) ? (1.0f / period) : 10.0f;
        } else if (mode == RANGE_LFO) {
            float fCoarse = 0.01f + 199.99f * coarse * coarse;
            return clampf(fCoarse + fine * 20.0f, 0.001f, 300.0f);
        } else {
            float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
            return clampf(fCoarse * (1.0f + fine * 0.10f), 50.0f, 3000.0f);
        }
    }

    // Regular N-gon radial distance formula at angle theta
    static float regularPolygonRadius(float n, float theta) {
        if (n <= 2.05f) {
            return 1.0f; // Circle
        }
        
        // Ensure angle in [0, 2pi)
        float twoPi = 2.0f * (float)M_PI;
        theta = std::fmod(theta, twoPi);
        if (theta < 0.0f) theta += twoPi;

        // Discrete integer blend between floor(n) and ceil(n)
        int kLo = (int)std::floor(n);
        int kHi = (int)std::ceil(n);
        if (kLo < 3) kLo = 3;
        if (kHi < 3) kHi = 3;

        auto nGonRadius = [twoPi](int k, float th) -> float {
            float sector = twoPi / (float)k;
            float halfSec = (float)M_PI / (float)k;
            float localTh = std::fmod(th, sector);
            if (localTh < 0.0f) localTh += sector;
            float diff = localTh - halfSec;
            float cosDiff = std::cos(diff);
            if (std::abs(cosDiff) < 1e-4f) cosDiff = 1e-4f;
            return std::cos(halfSec) / cosDiff;
        };

        if (n < 3.0f) {
            // Blend from circle (1.0) to triangle (k=3)
            float rTri = nGonRadius(3, theta);
            float frac = n - 2.0f;
            return 1.0f + frac * (rTri - 1.0f);
        }

        if (kLo == kHi) {
            return nGonRadius(kLo, theta);
        }

        float rLo = nGonRadius(kLo, theta);
        float rHi = nGonRadius(kHi, theta);
        float frac = n - (float)kLo;
        return rLo + frac * (rHi - rLo);
    }

    // Process a single sample
    void process(float sampleRate,
                 float coarseFreq, float fineFreq, RangeMode mode, float freqCV,
                 float shapeParam, float shapeCV,
                 float warpParam, float warpCV,
                 float windowParam, float windowCV,
                 float routingParam, float routingCV,
                 float ampParam, float ampCV,
                 float riseParam, float riseCV,
                 float fallParam, float fallCV,
                 float phaseParam, float phaseCV,
                 float stretchParam, float stretchCV,
                 float audioInL, float audioInR,
                 float syncIn,
                 float& outX, float& outY, float& syncOut) {

        // 1. Hard Sync handling
        syncPulse = false;
        if (syncIn >= 1.5f && lastSync < 1.5f) {
            phase = 0.0f;
            syncPulse = true;
        }
        lastSync = syncIn;

        // 2. Carrier Frequency computation
        float baseFreq = calculateBaseFrequency(mode, coarseFreq, fineFreq);
        float totalFreq = baseFreq * std::pow(2.0f, freqCV);
        totalFreq = clampf(totalFreq, 1e-5f, sampleRate * 0.45f);

        // Advance carrier phase
        float phaseInc = totalFreq / sampleRate;
        phase += phaseInc;
        if (phase >= 1.0f) {
            phase -= 1.0f;
            syncPulse = true;
        }
        syncOut = syncPulse ? 10.0f : 0.0f;

        // 3. Envelope Followers (Rise / Fall)
        // Rise and Fall time begin at 0 (instant response) up to 2000 ms
        float riseKnob = clampf(riseParam + riseCV * 0.1f, 0.0f, 1.0f);
        float fallKnob = clampf(fallParam + fallCV * 0.1f, 0.0f, 1.0f);

        float absL = std::abs(audioInL) * 0.2f; // Normalized so 5V -> 1.0
        float absR = std::abs(audioInR) * 0.2f;

        if (riseKnob <= 1e-4f) {
            if (absL >= envL) envL = absL;
            if (absR >= envR) envR = absR;
        } else {
            float riseSec = 2.0f * (riseKnob * riseKnob); // 0 to 2 seconds quadratic
            float alphaRise = 1.0f - std::exp(-1.0f / (sampleRate * riseSec));
            if (absL >= envL) envL += alphaRise * (absL - envL);
            if (absR >= envR) envR += alphaRise * (absR - envR);
        }

        if (fallKnob <= 1e-4f) {
            if (absL < envL) envL = absL;
            if (absR < envR) envR = absR;
        } else {
            float fallSec = 2.0f * (fallKnob * fallKnob); // 0 to 2 seconds quadratic
            float alphaFall = 1.0f - std::exp(-1.0f / (sampleRate * fallSec));
            if (absL < envL) envL += alphaFall * (absL - envL);
            if (absR < envR) envR += alphaFall * (absR - envR);
        }

        // 4. Routing Parameter (0.0 = 100% L+R Average : 0% L R Separate; 1.0 = 0% L+R Average : 100% L R Separate)
        float routing = clampf(routingParam + routingCV * 0.1f, 0.0f, 1.0f);
        float lWeight = (1.0f - routing) * 0.5f + routing * 1.0f;
        float rWeight = (1.0f - routing) * 0.5f;

        // Store composite audio into circular buffer
        float inputAudio = audioInL * lWeight + audioInR * rWeight;
        ringBuffer[writeHead] = inputAudio;
        size_t currentHead = writeHead;
        writeHead = (writeHead + 1) & BUFFER_MASK;

        // 5. Buffer Read Time Window (0.5 ms to 500 ms)
        float windowKnob = clampf(windowParam + windowCV * 0.1f, 0.0f, 1.0f);
        // Exponential scale from 0.5 ms to 500.0 ms
        float windowMs = 0.5f * std::pow(1000.0f, windowKnob);
        float windowSamples = (windowMs * 0.001f) * sampleRate;
        if (windowSamples < 1.0f) windowSamples = 1.0f;
        if (windowSamples > (float)(BUFFER_SIZE - 2)) windowSamples = (float)(BUFFER_SIZE - 2);

        // Read sample along the perimeter corresponding to current orbit phase
        float delaySamples = (1.0f - phase) * windowSamples;
        float readIndex = (float)currentHead - delaySamples;
        while (readIndex < 0.0f) readIndex += (float)BUFFER_SIZE;
        size_t idx0 = ((size_t)readIndex) & BUFFER_MASK;
        size_t idx1 = (idx0 + 1) & BUFFER_MASK;
        float frac = readIndex - (float)((size_t)readIndex);
        float waveDisp = ringBuffer[idx0] + frac * (ringBuffer[idx1] - ringBuffer[idx0]);

        // 6. Shape (2.0 = Circle, 3 = Tri, 4 = Square, 5 = Pent, 6 = Hex, 7 = Hept, 8 = Oct)
        float shapeVal = clampf(shapeParam + shapeCV * 0.6f, 2.0f, 8.0f);
        // Magnetic detent snap around integer shapes
        float nearestInt = std::round(shapeVal);
        if (std::abs(shapeVal - nearestInt) < 0.06f) {
            shapeVal = nearestInt;
        }

        // Base angle along perimeter
        float theta = phase * 2.0f * (float)M_PI;

        // 7. Warp: Bipolar control [-1.0, +1.0] (±100%)
        float warp = clampf(warpParam + warpCV * 0.2f, -1.0f, 1.0f);
        float warpAngle = theta;
        if (std::abs(warp) > 0.001f) {
            float envFactor = (1.0f - routing) * (0.5f * (envL + envR)) + routing * envL;
            warpAngle += warp * (0.8f * envFactor + 0.2f) * std::sin(shapeVal * theta);
        }

        // 8. Base Geometry Radius
        float rGeom = regularPolygonRadius(shapeVal, warpAngle);

        // Radial vortex modification
        if (std::abs(warp) > 0.001f) {
            rGeom *= (1.0f + warp * 0.4f * std::cos(2.0f * theta));
        }

        // 9. Pre-Amplitude Transform: Polly's Stretch Parameter (Bipolar [-1.0, +1.0])
        // Applied directly to geometric base before audio amplitude displacement
        float stretchVal = clampf(stretchParam + stretchCV * 0.2f, -1.0f, 1.0f);
        if (std::abs(stretchVal) > 1e-4f) {
            float dampFactor = 1.0f + stretchVal * 4.0f * (1.0f - rGeom);
            rGeom *= dampFactor;
        }

        // 10. Amplitude (Waveform displacement amplitude)
        float ampDepth = clampf(ampParam + ampCV * 0.1f, 0.0f, 2.0f);
        float finalDisp = (waveDisp * 0.2f) * ampDepth;

        // Intelligent reduction in scale of the shape in proportion to the amplitude amount:
        // When ampDepth = 0, geomScale = 1.0 (100% full scale).
        // As ampDepth increases, the shape shrinks inward to headroom for the waveform ripples:
        // At max ampDepth = 2.0, geomScale reduces down to ~0.33, preventing clipping bounds.
        float geomScale = 1.0f / (1.0f + ampDepth);

        // 11. Scale & Rotation from Routing Channel R
        float scaleMod = 1.0f + (routing * envR * 0.7f);
        float rotMod = routing * envR * (float)M_PI;

        // 12. End-of-line Phase Rotation
        float phaseAngle = clampf(phaseParam + phaseCV * 0.1f, 0.0f, 1.0f) * 2.0f * (float)M_PI;
        float totalAngle = warpAngle + phaseAngle + rotMod;

        float finalRadius = (rGeom * geomScale + finalDisp) * scaleMod;

        // 13. Convert to Cartesian Coordinates
        float curX = finalRadius * std::cos(totalAngle);
        float curY = finalRadius * std::sin(totalAngle);

        // 14. Output Deflection (Standard Eurorack ±5V, clamped cleanly at ±10V to preserve perfect geometry)
        outX = clampf(curX * 5.0f, -10.0f, 10.0f);
        outY = clampf(curY * 5.0f, -10.0f, 10.0f);
    }
};

} // namespace ampara
} // namespace garmire
