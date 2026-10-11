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
    SCOPE_SURFACE_NORMAL = 0, // Displace along 3D surface normal (modulated by Spread)
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
    float ringBuffer[BUFFER_SIZE] = {};
    size_t writeHead = 0;
    
    // Sync trigger state
    float lastSync = 0.0f;
    bool syncPulse = false;

    ScopeMode scopeMode = SCOPE_SURFACE_NORMAL;

    void reset() {
        phase = 0.0f;
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
        float s = clampf(shapeParam, 0.0f, 2.0f); // 0 = Sphere/Toroid, 1 = Cube/Cylinder, 2 = Pyramid/Cone
        float m = clampf(morphParam, 0.0f, 1.0f); // 0 = 1st variation, 1 = 2nd variation

        // Precompute trigonometric functions
        float cu = std::cos(u);
        float su = std::sin(u);
        float cv = std::cos(v);
        float sv = std::sin(v);

        // ── Family 1: Sphere (m=0) <-> Toroid (m=1) ──
        auto evalSphereToroid = [&](float morph, Vec3& p, Vec3& n) {
            // Sphere: unit radius 1.0
            Vec3 pSphere(cu * sv, su * sv, cv);
            Vec3 nSphere = pSphere.normalized();

            // Toroid: major radius R1 = 0.70, tube radius R2 = 0.35
            float R1 = 0.70f;
            float R2 = 0.35f;
            Vec3 pToroid((R1 + R2 * cu) * cv, (R1 + R2 * cu) * sv, R2 * su);
            Vec3 nToroid(cu * cv, cu * sv, su);

            p = pSphere * (1.0f - morph) + pToroid * morph;
            n = (nSphere * (1.0f - morph) + nToroid * morph).normalized();
        };

        // ── Family 2: Cube (m=0) <-> Cylinder (m=1) with Sharp Corners ──
        auto evalCubeCylinder = [&](float morph, Vec3& p, Vec3& n) {
            // Ray projection onto unit cube [-1, 1]^3: scale = 1 / max(|xs|, |ys|, |zs|)
            float xs = cu * sv;
            float ys = su * sv;
            float zs = cv;

            float mCube = std::max(std::max(std::abs(xs), std::abs(ys)), std::abs(zs));
            if (mCube < 1e-6f) mCube = 1e-6f;
            Vec3 pCube(xs / mCube * 0.85f, ys / mCube * 0.85f, zs / mCube * 0.85f);

            // Ray projection onto unit cylinder (circle in xy, flat caps in z): scale = 1 / max(sv, |cv|)
            float mCyl = std::max(sv, std::abs(zs));
            if (mCyl < 1e-6f) mCyl = 1e-6f;
            Vec3 pCyl(xs / mCyl * 0.85f, ys / mCyl * 0.85f, zs / mCyl * 0.85f);

            p = pCube * (1.0f - morph) + pCyl * morph;
            
            // Surface normal
            Vec3 nCube(xs / mCube, ys / mCube, zs / mCube);
            Vec3 nCyl(cu, su, (std::abs(zs) > sv) ? ((zs >= 0.0f) ? 1.0f : -1.0f) : 0.0f);
            n = (nCube * (1.0f - morph) + nCyl * morph).normalized();
        };

        // ── Family 3: Pyramid (m=0) <-> Cone (m=1) ──
        auto evalPyramidCone = [&](float morph, Vec3& p, Vec3& n) {
            float h = (v / (float)M_PI); // Height fraction [0, 1] from apex to base
            float z = (1.0f - h) * 1.7f - 0.85f; // Apex at +0.85, Base at -0.85
            float rH = h * 0.85f; // Radius expands linearly

            float denomSquare = std::max(std::abs(cu), std::abs(su));
            if (denomSquare < 1e-6f) denomSquare = 1e-6f;

            // Denominator blends smoothly from exact square (m=0) to exact circle (m=1)
            float denom = (1.0f - morph) * denomSquare + morph * 1.0f;
            float x = rH * (cu / denom);
            float y = rH * (su / denom);

            p = Vec3(x, y, z);
            n = Vec3(x, y, 0.4f).normalized();
        };

        if (s <= 1.0f) {
            Vec3 p1, n1, p2, n2;
            evalSphereToroid(m, p1, n1);
            evalCubeCylinder(m, p2, n2);
            float frac = s;
            outP = p1 * (1.0f - frac) + p2 * frac;
            outN = (n1 * (1.0f - frac) + n2 * frac).normalized();
        } else {
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
                 float spreadParam, float spreadCV,
                 float ampParam, float ampCV,
                 float twistParam, float twistCV,
                 float rotXParam, float rotXCV,
                 float rotYParam, float rotYCV,
                 float rotZParam, float rotZCV,
                 float stretchParam, float stretchCV,
                 float audioIn,
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

        // 3. Audio Ring Buffer (Mono input)
        ringBuffer[writeHead] = audioIn;
        size_t currentHead = writeHead;
        writeHead = (writeHead + 1) & BUFFER_MASK;

        // 4. Buffer Read Time Window (0.5 ms to 500 ms)
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

        // 5. Wireframe Scanning Coordinates (u, v)
        // Density K in [2.0, 64.0] (upper ceiling increased 2x)
        float density = clampf(densityParam + densityCV * 0.1f, 0.0f, 1.0f);
        float K = 2.0f + 62.0f * (density * density);

        // Continuous parametric meridian ribbon
        float u = 2.0f * (float)M_PI * std::fmod(phase * K, 1.0f);
        float v = (float)M_PI * phase;

        // 6. Generate 3D Surface Point & Normal
        float shapeVal = clampf(shapeParam + shapeCV * 0.2f, 0.0f, 2.0f);
        float morphVal = clampf(morphParam + morphCV * 0.1f, 0.0f, 1.0f);

        Vec3 P, N;
        generateSurfacePoint(shapeVal, morphVal, u, v, P, N);

        // 7. 3D Twist Transformation
        float twistVal = clampf(twistParam + twistCV * 0.2f, -1.0f, 1.0f);
        if (std::abs(twistVal) > 1e-4f) {
            float twistAngle = twistVal * (2.0f * (float)M_PI) * P.z;
            float ctw = std::cos(twistAngle), stw = std::sin(twistAngle);
            float tx = P.x * ctw - P.y * stw;
            float ty = P.x * stw + P.y * ctw;
            P.x = tx;
            P.y = ty;

            float nx = N.x * ctw - N.y * stw;
            float ny = N.x * stw + N.y * ctw;
            N.x = nx;
            N.y = ny;
        }

        // 8. 3D Stretch Transformation (Polly Logarithmic Spiral & Polar Stretch)
        // Works dynamically on Sphere, Toroid, Cube, Cylinder, Pyramid, and Cone
        float stretchVal = clampf(stretchParam + stretchCV * 0.2f, -1.0f, 1.0f);
        if (std::abs(stretchVal) > 1e-4f) {
            float rCyl = std::sqrt(P.x * P.x + P.y * P.y);
            float dampFactor = 1.0f + stretchVal * 2.5f * (1.0f - rCyl);
            P.x *= dampFactor;
            P.y *= dampFactor;
            P.z *= (1.0f + stretchVal * 0.75f);

            // Logarithmic spiral twist along Z
            float thetaSpiral = stretchVal * 1.5f * (float)M_PI * P.z;
            float csp = std::cos(thetaSpiral), ssp = std::sin(thetaSpiral);
            float sx = P.x * csp - P.y * ssp;
            float sy = P.x * ssp + P.y * csp;
            P.x = sx;
            P.y = sy;
        }

        // 9. Amplitude Displacement & Spread (0% to 100%)
        float ampKnob = clampf(ampParam + ampCV * 0.1f, 0.0f, 1.0f); // Max 100%
        float ampGain = ampKnob * 1.0f;
        float geomScale = 1.0f / (1.0f + 0.35f * ampGain);
        P = P * geomScale;

        float finalDisp = (waveDisp * 0.2f) * ampGain;

        // Spread parameter: 0% = tight equatorial belt, 100% = full surface normal
        float spreadVal = clampf(spreadParam + spreadCV * 0.1f, 0.0f, 1.0f);
        float kSpread = 80.0f * ((1.0f - spreadVal) * (1.0f - spreadVal));
        float spreadFactor = std::exp(-kSpread * std::pow(v - (float)M_PI * 0.5f, 2.0f));

        // Apply audio displacement according to ScopeMode
        if (scopeMode == SCOPE_SURFACE_NORMAL) {
            P = P + N * (finalDisp * spreadFactor);
        } else if (scopeMode == SCOPE_EQUATORIAL_BELT) {
            // Dedicated equatorial belt
            float beltFactor = std::exp(-80.0f * std::pow(v - (float)M_PI * 0.5f, 2.0f));
            P = P + N * (finalDisp * beltFactor);
        } else if (scopeMode == SCOPE_Z_DEPTH_RELIEF) {
            P.z += finalDisp * spreadFactor;
        }

        // 10. 3D Euler Rotations: Pitch (X), Yaw (Y), Roll (Z)
        float rotX = (clampf(rotXParam + rotXCV * 0.1f, -1.0f, 1.0f) * (float)M_PI);
        float rotY = (clampf(rotYParam + rotYCV * 0.1f, -1.0f, 1.0f) * (float)M_PI);
        float rotZ = (clampf(rotZParam + rotZCV * 0.1f, 0.0f, 1.0f) * 2.0f * (float)M_PI);

        // Rotate X (Pitch)
        float cx = std::cos(rotX), sx = std::sin(rotX);
        float y1 = P.y * cx - P.z * sx;
        float z1 = P.y * sx + P.z * cx;

        // Rotate Y (Yaw)
        float cy = std::cos(rotY), sy = std::sin(rotY);
        float x2 = P.x * cy + z1 * sy;
        float z2 = -P.x * sy + z1 * cy;

        // Rotate Z (Roll)
        float cz = std::cos(rotZ), sz = std::sin(rotZ);
        float x3 = x2 * cz - y1 * sz;
        float y3 = x2 * sz + y1 * cz;

        // 11. Perspective Camera Projection
        float depthKnob = clampf(depthParam + depthCV * 0.1f, 0.0f, 1.0f);
        float dCam = 3.0f;
        float zEff = 1.0f + depthKnob * (z2 / dCam);
        if (zEff < 0.2f) zEff = 0.2f;

        float projX = x3 / zEff;
        float projY = y3 / zEff;

        // 12. Output Deflection (Standard Eurorack ±5V, clamped safely at ±10V)
        outX = clampf(projX * 5.0f, -10.0f, 10.0f);
        outY = clampf(projY * 5.0f, -10.0f, 10.0f);
    }
};

} // namespace georgia
} // namespace garmire
