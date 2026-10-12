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

    // 3D Geometric Surface Generator with 4 Discrete Specialized Algorithms
    // Shape 0: Sphere <-> Toroid
    // Shape 1: Cube (Sharp 3D Grid) <-> Cylinder
    // Shape 2: Pyramid <-> Cone
    // Shape 3: Catenoid (Hourglass/Wormhole) <-> Funnel (Gravitational Well/Flared Horn)
    // Evaluates 3D surface point, outward surface normal, and 2D middle cross-section point
    static void generateSurfacePoint(float shapeParam, float morphParam, float phase, float K,
                                     Vec3& outP, Vec3& outN, Vec3& outP2D) {
        int shapeFamily = (int)std::floor(shapeParam + 0.5f);
        if (shapeFamily < 0) shapeFamily = 0;
        if (shapeFamily > 3) shapeFamily = 3;

        float m = clampf(morphParam, 0.0f, 1.0f); // 0 = 1st variation, 1 = 2nd variation

        // Precompute standard scanning coordinates
        float u = 2.0f * (float)M_PI * std::fmod(phase * K, 1.0f);
        float v = (float)M_PI * phase;

        float cu = std::cos(u);
        float su = std::sin(u);
        float cv = std::cos(v);
        float sv = std::sin(v);

        if (shapeFamily == 0) {
            // ── Shape 0: Sphere (m=0) <-> Toroid (m=1) ──
            // Sphere: Unit radius 1.0 (swept over latitude v in [0, pi] with K rings)
            Vec3 pSphere(cu * sv, su * sv, cv);
            Vec3 nSphere = pSphere.normalized();

            // Toroid (Donut with hollow central hole):
            // phi = 2*v completes a full 360-degree closed revolution around the donut.
            // Since the major loop covers 2*pi (double the angular span of the sphere's pi latitude),
            // we dynamically scale the effective ring frequency by (1 + m) so that at 100% Toroid,
            // the number of tube rings is doubled (2*K), fully restoring spatial resolution.
            float kToroid = K * (1.0f + m);
            float uToroid = 2.0f * (float)M_PI * std::fmod(phase * kToroid, 1.0f);
            float cuTor = std::cos(uToroid);
            float suTor = std::sin(uToroid);

            float phi = 2.0f * v;
            float cphi = std::cos(phi);
            float sphi = std::sin(phi);
            float R1 = 0.65f;
            float R2 = 0.30f;

            Vec3 pToroid((R1 + R2 * cuTor) * cphi, (R1 + R2 * cuTor) * sphi, R2 * suTor);
            Vec3 nToroid(cuTor * cphi, cuTor * sphi, suTor);

            outP = pSphere * (1.0f - m) + pToroid * m;
            outN = (nSphere * (1.0f - m) + nToroid * m).normalized();

            // 2D middle cross-section: equatorial circle (Sphere) <-> donut face ring (Toroid)
            Vec3 p2DSphere(cu, su, 0.0f);
            Vec3 p2DToroid((R1 + R2 * cuTor) * cphi, (R1 + R2 * cuTor) * sphi, 0.0f);
            outP2D = p2DSphere * (1.0f - m) + p2DToroid * m;
        } else if (shapeFamily == 1) {
            // ── Shape 1: Cube (m=0) <-> Cylinder (m=1) ──
            // Authentic 3-pass orthogonal wireframe grid across all 6 faces without concentric circles:
            // Pass 0 (0 <= phase < 1/3): Z-axis loops (XY cross-sections stepping along Z)
            // Pass 1 (1/3 <= phase < 2/3): X-axis loops (YZ cross-sections stepping along X)
            // Pass 2 (2/3 <= phase < 1): Y-axis loops (XZ cross-sections stepping along Y)
            // Cube scale reduced by 2.5% (0.82875) to prevent corner clipping at init settings; Cylinder scale remains 0.85
            float cubeSize = 0.85f * (0.975f + 0.025f * m);
            float cubeSpan = 2.0f * cubeSize;

            float ph3 = phase * 3.0f;
            int sec = (int)ph3;
            if (sec > 2) sec = 2;
            float sub = ph3 - (float)sec;

            float kSec = std::max(2.0f, K * (1.0f / 3.0f));
            float uSec = 2.0f * (float)M_PI * (sub * kSec);
            float cuSec = std::cos(uSec);
            float suSec = std::sin(uSec);

            float denSq = std::max(std::abs(cuSec), std::abs(suSec));
            if (denSq < 1e-6f) denSq = 1e-6f;

            // Denominator blends from square (m=0) to circle (m=1)
            float den = (1.0f - m) * denSq + m * 1.0f;

            if (sec == 0) {
                // Pass 0: XY cross-sections stepping along Z from +cubeSize to -cubeSize
                float z = cubeSize - sub * cubeSpan;
                float x = cubeSize * (cuSec / den);
                float y = cubeSize * (suSec / den);
                outP = Vec3(x, y, z);
                outN = Vec3(cuSec / den, suSec / den, 0.0f).normalized();
            } else if (sec == 1) {
                // Pass 1: YZ cross-sections stepping along X from +cubeSize to -cubeSize
                float x = cubeSize - sub * cubeSpan;
                float rCyl = std::sqrt(std::max(0.0f, 0.85f * 0.85f - x * x));
                float yCube = cubeSize * (cuSec / denSq);
                float yCyl = (std::abs(cuSec) > 1e-5f) ? (rCyl * ((cuSec >= 0.0f) ? 1.0f : -1.0f)) : 0.0f;
                float y = (1.0f - m) * yCube + m * yCyl;
                float z = cubeSize * (suSec / denSq);
                outP = Vec3(x, y, z);
                outN = Vec3(0.0f, cuSec, suSec).normalized();
            } else {
                // Pass 2: XZ cross-sections stepping along Y from -cubeSize to +cubeSize
                float y = -cubeSize + sub * cubeSpan;
                float rCyl = std::sqrt(std::max(0.0f, 0.85f * 0.85f - y * y));
                float xCube = cubeSize * (cuSec / denSq);
                float xCyl = (std::abs(cuSec) > 1e-5f) ? (rCyl * ((cuSec >= 0.0f) ? 1.0f : -1.0f)) : 0.0f;
                float x = (1.0f - m) * xCube + m * xCyl;
                float z = cubeSize * (suSec / denSq);
                outP = Vec3(x, y, z);
                outN = Vec3(cuSec, 0.0f, suSec).normalized();
            }

            // 2D middle cross-section: square (Cube) <-> circle (Cylinder)
            float xMid = cubeSize * (cu / den);
            float yMid = cubeSize * (su / den);
            outP2D = Vec3(xMid, yMid, 0.0f);
        } else if (shapeFamily == 2) {
            // ── Shape 2: Pyramid (m=0) <-> Cone (m=1) ──
            // Small point (apex) is oriented at the top of the screen:
            // Apex (h=0) at z=-0.85, Base (h=1) at z=+0.85.
            // Under base 90-degree X-rotation (y1 = -z), apex maps to y1 = +0.85 (top).
            float h = (v / (float)M_PI); // Height fraction [0, 1] from apex to base
            float z = -0.85f + h * 1.70f;
            float rH = h * 0.85f; // Radius expands linearly from apex to base

            float denomSquare = std::max(std::abs(cu), std::abs(su));
            if (denomSquare < 1e-6f) denomSquare = 1e-6f;

            // Denominator blends from exact square (m=0) to circle (m=1)
            float denom = (1.0f - m) * denomSquare + m * 1.0f;
            float x = rH * (cu / denom);
            float y = rH * (su / denom);

            outP = Vec3(x, y, z);
            outN = Vec3(x, y, -0.4f).normalized();

            // 2D middle cross-section: mid-height square (Pyramid) <-> circle (Cone)
            float rMid = 0.425f;
            float xMid = rMid * (cu / denom);
            float yMid = rMid * (su / denom);
            outP2D = Vec3(xMid, yMid, 0.0f);
        } else {
            // ── Shape 3: Funnel (m=0) <-> Catenoid (m=1) ──
            // Funnel (m=0): Flared horn / gravitational well with wide flare at top and narrow neck at bottom
            // Catenoid (m=1): Symmetrical double-flanged spool / wormhole with narrow central waist
            float t = (v / (float)M_PI); // [0, 1] from top to bottom
            float z = -0.85f + t * 1.70f; // z in [-0.85, +0.85]: top of screen at t=0, bottom at t=1

            // Funnel (m=0): wide flare (0.85) at top (t=0, z=-0.85 -> y1=+0.85), narrow neck (0.20) at bottom (t=1, z=+0.85 -> y1=-0.85)
            float flareHeight = std::max(0.0f, 1.0f - t);
            float rFun = 0.20f + 0.65f * std::pow(flareHeight, 2.2f);

            // Catenoid (m=1): waist radius = 0.25 at z=0, flaring to ~0.84 at rims z=±0.85
            float cCat = 0.25f;
            float rCat = cCat * std::cosh(z / 0.45f);

            float r = (1.0f - m) * rFun + m * rCat;
            float x = r * cu;
            float y = r * su;

            outP = Vec3(x, y, z);
            outN = Vec3(cu, su, (0.5f - t) * 0.5f).normalized();

            // 2D middle cross-section: mid-height circle (Funnel r~0.34) <-> waist circle (Catenoid r=0.25)
            float rFunMid = 0.20f + 0.65f * std::pow(0.5f, 2.2f);
            float rMid = (1.0f - m) * rFunMid + m * 0.25f;
            outP2D = Vec3(rMid * cu, rMid * su, 0.0f);
        }
    }

    // Overload for callers only needing 3D position and normal
    static void generateSurfacePoint(float shapeParam, float morphParam, float phase, float K,
                                     Vec3& outP, Vec3& outN) {
        Vec3 dummy2D;
        generateSurfacePoint(shapeParam, morphParam, phase, K, outP, outN, dummy2D);
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

        // 5. Wireframe Scanning Coordinates
        // Density K in [2.0, 64.0] (upper ceiling increased 2x)
        float density = clampf(densityParam + densityCV * 0.1f, 0.0f, 1.0f);
        float K = 2.0f + 62.0f * (density * density);

        // Latitude progression parameter v for spread belt modulation
        float v = (float)M_PI * phase;

        // 6. Generate 3D Surface Point, Normal & 2D Middle Cross-Section Point
        float shapeVal = clampf(shapeParam + shapeCV * 0.2f, 0.0f, 3.0f);
        float morphVal = clampf(morphParam + morphCV * 0.1f, 0.0f, 1.0f);

        Vec3 P, N, P2D;
        generateSurfacePoint(shapeVal, morphVal, phase, K, P, N, P2D);

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
        // All rotation angles span 0 to 360 degrees (0 to 2*pi).
        // Initial state (rotXParam = 0) has a base 90° (pi/2) offset for an upright initial view.
        float rotX = (float)M_PI * 0.5f + (rotXParam + rotXCV * 0.1f) * 2.0f * (float)M_PI;
        float rotY = (rotYParam + rotYCV * 0.1f) * 2.0f * (float)M_PI;
        float rotZ = (rotZParam + rotZCV * 0.1f) * 2.0f * (float)M_PI;

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

        // In-plane rotation for 2D middle cross-section (controlled by Rot Z)
        float x2D = P2D.x * cz - P2D.y * sz;
        float y2D = P2D.x * sz + P2D.y * cz;

        // 11. Perspective Camera Projection & Depth Control
        float depthKnob = clampf(depthParam + depthCV * 0.1f, 0.0f, 1.0f);
        float dCam = 3.0f;
        float zEff = 1.0f + depthKnob * (z2 / dCam);
        if (zEff < 0.2f) zEff = 0.2f;

        float x3D = x3 / zEff;
        float y3D = y3 / zEff;

        // At Depth 0%, present a pure 2D version (middle cross section).
        // Increasing Depth blooms into the full 3D rotated perspective wireframe.
        float projX = (1.0f - depthKnob) * x2D + depthKnob * x3D;
        float projY = (1.0f - depthKnob) * y2D + depthKnob * y3D;

        // 12. Output Deflection (Standard Eurorack ±5V, clamped safely at ±10V)
        outX = clampf(projX * 5.0f, -10.0f, 10.0f);
        outY = clampf(projY * 5.0f, -10.0f, 10.0f);
    }
};

} // namespace georgia
} // namespace garmire
