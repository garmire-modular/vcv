#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace garmire {
namespace georgia {

inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

constexpr size_t BUFFER_SIZE = 65536;
constexpr size_t BUFFER_MASK = BUFFER_SIZE - 1;

enum RangeMode {
    RANGE_VERY_SLOW = 0, // 600s down to 0.1s
    RANGE_LFO,            // 0.01 - 200 Hz
    RANGE_VCO             // 150 Hz - 2 kHz
};

enum ScopeMode {
    SCOPE_SURFACE_NORMAL = 0, // Displace along 3D surface normal
    SCOPE_EQUATORIAL_BELT,    // Dedicated equatorial oscilloscope ribbon
    SCOPE_Z_DEPTH_RELIEF      // Modulate z-depth extrusion
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vec3 normalized() const {
        float l = length();
        if (l < 1e-6f) return Vec3(0.0f, 0.0f, 1.0f);
        return Vec3(x / l, y / l, z / l);
    }
};

// Pure C++ Core DSP Engine for Georgia 3D wireframe voice
// Hardware-agnostic, zero heap allocations, real-time safe
struct VoiceCore {
    float phase = 0.0f;           // Carrier orbit phase [0, 1)
    float envL = 0.0f;            // Envelope detector Left
    float envR = 0.0f;            // Envelope detector Right
    float ringBuffer[BUFFER_SIZE] = {};
    size_t writeHead = 0;
    
    // Sync trigger state
    float lastSync = 0.0f;
    bool syncPulse = false;

    ScopeMode scopeMode = SCOPE_SURFACE_NORMAL;

    void reset() {
        phase = 0.0f;
        envL = 0.0f;
        envR = 0.0f;
        writeHead = 0;
        lastSync = 0.0f;
        syncPulse = false;
        scopeMode = SCOPE_SURFACE_NORMAL;
        for (size_t i = 0; i < BUFFER_SIZE; ++i) {
            ringBuffer[i] = 0.0f;
        }
    }

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

    // 3D Geometric Surface Generator with Continuous Morphing
    // Returns 3D position P and 3D surface unit normal N
    static void generateSurfacePoint(float shapeParam, float morphParam,
                                     float u, float v,
                                     Vec3& outP, Vec3& outN) {
        // Clamp parameters
        float s = clampf(shapeParam, 0.0f, 2.0f); // 0 = Sphere/Torus, 1 = Cube/Cylinder, 2 = Pyramid/Cone
        float m = clampf(morphParam, 0.0f, 1.0f); // 0 = 1st variation, 1 = 2nd variation

        // Precompute trigonometric functions
        float cu = std::cos(u);
        float su = std::sin(u);
        float cv = std::cos(v);
        float sv = std::sin(v);

        // ── Family 1: Sphere (m=0) <-> Torus (m=1) ──
        auto evalSphereTorus = [&](float morph, Vec3& p, Vec3& n) {
            // Sphere: unit radius 1.0
            Vec3 pSphere(cu * sv, su * sv, cv);
            Vec3 nSphere = pSphere.normalized();

            // Toroid: major radius R1 = 0.7, tube radius R2 = 0.35
            float R1 = 0.70f;
            float R2 = 0.35f;
            Vec3 pTorus((R1 + R2 * cu) * cv, (R1 + R2 * cu) * sv, R2 * su);
            Vec3 nTorus(cu * cv, cu * sv, su);

            p = pSphere * (1.0f - morph) + pTorus * morph;
            n = (nSphere * (1.0f - morph) + nTorus * morph).normalized();
        };

        // ── Family 2: Cube (m=0) <-> Cylinder (m=1) ──
        auto evalCubeCylinder = [&](float morph, Vec3& p, Vec3& n) {
            // Square cross-section superellipse exponent
            // Cube: exp = 8.0 (sharp square), Cylinder: exp = 2.0 (round circle)
            float pExp = 8.0f * (1.0f - morph) + 2.0f * morph;
            
            // Normalized circle/square cross section
            auto superCos = [](float angle, float exp) {
                float c = std::cos(angle);
                float sgn = (c >= 0.0f) ? 1.0f : -1.0f;
                return sgn * std::pow(std::abs(c), 2.0f / exp);
            };
            auto superSin = [](float angle, float exp) {
                float s = std::sin(angle);
                float sgn = (s >= 0.0f) ? 1.0f : -1.0f;
                return sgn * std::pow(std::abs(s), 2.0f / exp);
            };

            float rx = superCos(u, pExp);
            float ry = superSin(u, pExp);
            float rz = (v / (float)M_PI) * 2.0f - 1.0f; // Height [-1, 1]

            // Endcap shaping: smooth cube vs open/closed cylinder
            p = Vec3(rx * 0.85f, ry * 0.85f, rz * 0.85f);
            n = Vec3(rx, ry, 0.0f).normalized();
        };

        // ── Family 3: Pyramid (m=0) <-> Cone (m=1) ──
        auto evalPyramidCone = [&](float morph, Vec3& p, Vec3& n) {
            // Height parameter z in [-0.85, 0.85]
            float h = (v / (float)M_PI); // [0, 1]
            float z = (1.0f - h) * 1.7f - 0.85f; // Base at -0.85, Apex at +0.85
            float radiusAtH = h * 0.85f; // Radius expands from apex (h=0) to base (h=1)

            // Cross section: Square (m=0) to Circle (m=1)
            float pExp = 8.0f * (1.0f - morph) + 2.0f * morph;
            auto superCos = [](float angle, float exp) {
                float c = std::cos(angle);
                float sgn = (c >= 0.0f) ? 1.0f : -1.0f;
                return sgn * std::pow(std::abs(c), 2.0f / exp);
            };
            auto superSin = [](float angle, float exp) {
                float s = std::sin(angle);
                float sgn = (s >= 0.0f) ? 1.0f : -1.0f;
                return sgn * std::pow(std::abs(s), 2.0f / exp);
            };

            float x = radiusAtH * superCos(u, pExp);
            float y = radiusAtH * superSin(u, pExp);

            p = Vec3(x, y, z);
            n = Vec3(x, y, 0.4f).normalized();
        };

        if (s <= 1.0f) {
            // Blend from Family 1 to Family 2
            Vec3 p1, n1, p2, n2;
            evalSphereTorus(m, p1, n1);
            evalCubeCylinder(m, p2, n2);
            float frac = s;
            outP = p1 * (1.0f - frac) + p2 * frac;
            outN = (n1 * (1.0f - frac) + n2 * frac).normalized();
        } else {
            // Blend from Family 2 to Family 3
            Vec3 p2, n2, p3, n3;
            evalCubeCylinder(m, p2, n2);
            evalPyramidCone(m, p3, n3);
            float frac = s - 1.0f;
            outP = p2 * (1.0f - frac) + p3 * frac;
            outN = (n2 * (1.0f - frac) + n3 * frac).normalized();
        }
    }

    // Process a single sample
    void process(float sampleRate,
                 float coarseFreq, float fineFreq, RangeMode rangeMode, float freqCV,
                 float shapeParam, float shapeCV,
                 float morphParam, float morphCV,
                 float densityParam, float densityCV,
                 float depthParam, float depthCV,
                 float windowParam, float windowCV,
                 float routingParam, float routingCV,
                 float ampParam, float ampCV,
                 float fovParam, float fovCV,
                 float rotXParam, float rotXCV,
                 float rotYParam, float rotYCV,
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
        float baseFreq = calculateBaseFrequency(rangeMode, coarseFreq, fineFreq);
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

        // 3. Envelope Tracking (Fast instant tracking with 2000ms release)
        float absL = std::abs(audioInL) * 0.2f;
        float absR = std::abs(audioInR) * 0.2f;
        float alpha = 1.0f - std::exp(-1.0f / (sampleRate * 0.05f));
        envL += alpha * (absL - envL);
        envR += alpha * (absR - envR);

        // 4. Stereo Audio Routing
        float routing = clampf(routingParam + routingCV * 0.1f, 0.0f, 1.0f);
        float lWeight = (1.0f - routing) * 0.5f + routing * 1.0f;
        float rWeight = (1.0f - routing) * 0.5f;

        float inputAudio = audioInL * lWeight + audioInR * rWeight;
        ringBuffer[writeHead] = inputAudio;
        size_t currentHead = writeHead;
        writeHead = (writeHead + 1) & BUFFER_MASK;

        // 5. Buffer Read Time Window (0.5 ms to 500 ms)
        float windowKnob = clampf(windowParam + windowCV * 0.1f, 0.0f, 1.0f);
        float windowMs = 0.5f * std::pow(1000.0f, windowKnob);
        float windowSamples = (windowMs * 0.001f) * sampleRate;
        if (windowSamples < 1.0f) windowSamples = 1.0f;
        if (windowSamples > (float)(BUFFER_SIZE - 2)) windowSamples = (float)(BUFFER_SIZE - 2);

        float delaySamples = (1.0f - phase) * windowSamples;
        float readIndex = (float)currentHead - delaySamples;
        while (readIndex < 0.0f) readIndex += (float)BUFFER_SIZE;
        size_t idx0 = ((size_t)readIndex) & BUFFER_MASK;
        size_t idx1 = (idx0 + 1) & BUFFER_MASK;
        float frac = readIndex - (float)((size_t)readIndex);
        float waveDisp = ringBuffer[idx0] + frac * (ringBuffer[idx1] - ringBuffer[idx0]);

        // 6. Wireframe Scanning Coordinates (u, v)
        // Density K in [2.0, 32.0]
        float density = clampf(densityParam + densityCV * 0.1f, 0.0f, 1.0f);
        float K = 2.0f + 30.0f * (density * density);

        // Continuous parametric meridian ribbon
        float u = 2.0f * (float)M_PI * std::fmod(phase * K, 1.0f);
        float v = (float)M_PI * phase;

        // 7. Generate 3D Surface Point & Normal
        float shapeVal = clampf(shapeParam + shapeCV * 0.2f, 0.0f, 2.0f);
        float morphVal = clampf(morphParam + morphCV * 0.1f, 0.0f, 1.0f);

        Vec3 P, N;
        generateSurfacePoint(shapeVal, morphVal, u, v, P, N);

        // 8. Pre-Amplitude Stretch (Polly Logarithmic Spiral Stretch)
        float stretchVal = clampf(stretchParam + stretchCV * 0.2f, -1.0f, 1.0f);
        if (std::abs(stretchVal) > 1e-4f) {
            float rNorm = P.length();
            float dampFactor = 1.0f + stretchVal * 4.0f * (1.0f - rNorm);
            P = P * dampFactor;
        }

        // 9. Amplitude Displacement & Balanced Headroom Scaling
        float ampKnob = clampf(ampParam + ampCV * 0.1f, 0.0f, 2.0f);
        float ampGain = ampKnob * (1.75f / 2.0f); // Capped at 175%
        float geomScale = 1.0f / (1.0f + 0.35f * ampGain);
        P = P * geomScale;

        float finalDisp = (waveDisp * 0.2f) * ampGain;

        // Apply audio displacement according to ScopeMode
        if (scopeMode == SCOPE_SURFACE_NORMAL) {
            P = P + N * finalDisp;
        } else if (scopeMode == SCOPE_EQUATORIAL_BELT) {
            // Equatorial scope belt focused along v = pi/2
            float beltFactor = std::exp(-16.0f * std::pow(v - (float)M_PI * 0.5f, 2.0f));
            P = P + N * (finalDisp * beltFactor);
        } else if (scopeMode == SCOPE_Z_DEPTH_RELIEF) {
            P.z += finalDisp;
        }

        // 10. Modulation from Routing Channel R (Macro scale & extra rotation)
        float scaleMod = 1.0f + (routing * envR * 0.7f);
        float rotMod = routing * envR * (float)M_PI;
        P = P * scaleMod;

        // 11. 3D Euler Rotations: Pitch (X), Yaw (Y), Roll/Phase (Z)
        float rotX = (clampf(rotXParam + rotXCV * 0.1f, -1.0f, 1.0f) * (float)M_PI);
        float rotY = (clampf(rotYParam + rotYCV * 0.1f, -1.0f, 1.0f) * (float)M_PI);
        float rotZ = (clampf(phaseParam + phaseCV * 0.1f, 0.0f, 1.0f) * 2.0f * (float)M_PI) + rotMod;

        // Rotate X (Pitch)
        float cx = std::cos(rotX), sx = std::sin(rotX);
        float y1 = P.y * cx - P.z * sx;
        float z1 = P.y * sx + P.z * cx;

        // Rotate Y (Yaw)
        float cy = std::cos(rotY), sy = std::sin(rotY);
        float x2 = P.x * cy + z1 * sy;
        float z2 = -P.x * sy + z1 * cy;

        // Rotate Z (Roll / Phase)
        float cz = std::cos(rotZ), sz = std::sin(rotZ);
        float x3 = x2 * cz - y1 * sz;
        float y3 = x2 * sz + y1 * cz;

        // 12. Perspective Camera Projection
        float fov = clampf(fovParam + fovCV * 0.1f, 0.0f, 1.0f);
        float dCam = 2.0f + 3.0f * fov; // Camera distance [2.0, 5.0]

        float depthKnob = clampf(depthParam + depthCV * 0.1f, 0.0f, 1.0f);
        float zEff = 1.0f + depthKnob * (z2 / dCam);
        if (zEff < 0.2f) zEff = 0.2f;

        float projX = x3 / zEff;
        float projY = y3 / zEff;

        // 13. Output Deflection (Standard Eurorack ±5V, clamped safely at ±10V)
        outX = clampf(projX * 5.0f, -10.0f, 10.0f);
        outY = clampf(projY * 5.0f, -10.0f, 10.0f);
    }
};

} // namespace georgia
} // namespace garmire
