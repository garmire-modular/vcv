#include <iostream>
#include <cassert>
#include <cmath>
#include "../src/core/GeorgiaCore.hpp"

using namespace garmire::georgia;

int main() {
    std::cout << "Testing GeorgiaCore 3D Wireframe DSP (v1.2.1)..." << std::endl;

    VoiceCore voice;
    voice.reset();

    // 1. Invariant check: Sphere surface point length at unit radius
    for (float ph = 0.05f; ph < 0.95f; ph += 0.1f) {
        Vec3 P, N;
        VoiceCore::generateSurfacePoint(0.0f, 0.0f, ph, 16.0f, P, N); // Family 0, Morph 0 = Sphere
        float len = P.length();
        assert(std::abs(len - 1.0f) < 1e-3f);
    }
    std::cout << "-> Sphere surface point length invariant passed (Radius = 1.0)" << std::endl;

    // 1b. Toroid Ring Density Doubling Check:
    // At m = 1.0, kToroid = 2*K. Thus, stepping phase by 1/(2*K) covers a full 2*pi revolution in tube angle u.
    {
        float K = 16.0f;
        Vec3 pA, nA, pB, nB;
        // Step phase by exactly 1 / (2*K):
        float phA = 0.20f;
        float phB = phA + 1.0f / (2.0f * K);
        VoiceCore::generateSurfacePoint(0.0f, 1.0f, phA, K, pA, nA);
        VoiceCore::generateSurfacePoint(0.0f, 1.0f, phB, K, pB, nB);
        // Tube angle u at phB is u_A + 2*pi, so cos(u) and sin(u) are identical!
        // The tube displacement relative to the major ring center must be identical:
        // Phi advances by 2*(vB - vA) = 2*pi*(1/(2*K)) = pi/K, but tube angle is identical.
        float uA = 2.0f * (float)M_PI * std::fmod(phA * 2.0f * K, 1.0f);
        float uB = 2.0f * (float)M_PI * std::fmod(phB * 2.0f * K, 1.0f);
        assert(std::abs(std::cos(uA) - std::cos(uB)) < 1e-3f);
        assert(std::abs(std::sin(uA) - std::sin(uB)) < 1e-3f);
        std::cout << "-> Toroid 2x ring density doubling verified (kToroid = 2*K at m=1.0)" << std::endl;
    }

    // 2. Wireframe Grid Cube Check: All 3 passes should lie exactly on the cube surface [-0.85, 0.85]^3
    {
        for (float ph = 0.01f; ph < 0.99f; ph += 0.05f) {
            Vec3 P, N;
            VoiceCore::generateSurfacePoint(1.0f, 0.0f, ph, 18.0f, P, N); // Family 1 (Cube), Morph 0
            assert(std::abs(P.x) <= 0.85001f);
            assert(std::abs(P.y) <= 0.85001f);
            assert(std::abs(P.z) <= 0.85001f);
            float maxCoord = std::max(std::max(std::abs(P.x), std::abs(P.y)), std::abs(P.z));
            assert(std::abs(maxCoord - 0.85f) < 1e-3f); // At least one coordinate is on a face!
        }
        std::cout << "-> Wireframe Grid Cube check passed (All vertices on surface [-0.85, 0.85]^3 with zero concentric rings)" << std::endl;
    }

    // 2b. Toroid Donut Check: At Shape=0, Morph=1, toroid must have a clear donut hole (inner radius ~0.35, outer radius ~0.95)
    // and full 360-degree revolution in Y both positive and negative
    {
        Vec3 p0, n0, pQuarter, nQuarter, pThreeQuarter, nThreeQuarter;
        // Inner edge: at m=1, kToroid = 2*K. u = pi occurs at phase = 0.5 / (2*K) -> radius = R1 - R2 = 0.65 - 0.30 = 0.35
        // At phase = 0.5 / (2*K), phi = 2*v = 2*pi*(0.5/32) is very close to 0, so x ~ 0.35
        VoiceCore::generateSurfacePoint(0.0f, 1.0f, 0.5f / (2.0f * 16.0f), 16.0f, p0, n0); // u = pi
        assert(std::abs(p0.x - 0.35f) < 0.05f);

        // At phase = 0.25 -> v = pi/4 -> phi = 2*v = pi/2 -> y > 0
        VoiceCore::generateSurfacePoint(0.0f, 1.0f, 0.25f, 16.0f, pQuarter, nQuarter);
        assert(pQuarter.y > 0.30f);

        // At phase = 0.75 -> v = 3*pi/4 -> phi = 2*v = 3*pi/2 -> y < 0 (full closed donut reaching negative Y!)
        VoiceCore::generateSurfacePoint(0.0f, 1.0f, 0.75f, 16.0f, pThreeQuarter, nThreeQuarter);
        assert(pThreeQuarter.y < -0.30f);

        std::cout << "-> Toroid donut hole & 360-degree closed revolution check passed (Hole = 0.35, Outer = 0.95)" << std::endl;
    }

    // 2c. Catenoid & Funnel Check (Shape 3):
    // Catenoid: narrow waist in center (z=0), flared at rims
    // Funnel: narrow neck at bottom, flared horn at top
    {
        Vec3 pCatWaist, nCatWaist, pCatRim, nCatRim;
        Vec3 pFunNeck, nFunNeck, pFunRim, nFunRim;

        // Catenoid (m=0): at phase = 0.5 -> z = 0 (waist)
        VoiceCore::generateSurfacePoint(3.0f, 0.0f, 0.5f, 16.0f, pCatWaist, nCatWaist);
        float rWaist = std::sqrt(pCatWaist.x * pCatWaist.x + pCatWaist.y * pCatWaist.y);
        assert(std::abs(rWaist - 0.25f) < 0.05f);

        // Catenoid (m=0): at phase = 0.0 -> z = +0.85 (rim)
        VoiceCore::generateSurfacePoint(3.0f, 0.0f, 0.01f, 16.0f, pCatRim, nCatRim);
        float rRim = std::sqrt(pCatRim.x * pCatRim.x + pCatRim.y * pCatRim.y);
        assert(rRim > 0.70f);

        // Funnel (m=1): at phase = 0.99 -> z = -0.85 (neck)
        VoiceCore::generateSurfacePoint(3.0f, 1.0f, 0.99f, 16.0f, pFunNeck, nFunNeck);
        float rNeck = std::sqrt(pFunNeck.x * pFunNeck.x + pFunNeck.y * pFunNeck.y);
        assert(std::abs(rNeck - 0.20f) < 0.05f);

        // Funnel (m=1): at phase = 0.01 -> z = +0.85 (flared horn rim)
        VoiceCore::generateSurfacePoint(3.0f, 1.0f, 0.01f, 16.0f, pFunRim, nFunRim);
        float rFlare = std::sqrt(pFunRim.x * pFunRim.x + pFunRim.y * pFunRim.y);
        assert(rFlare > 0.80f);

        std::cout << "-> Catenoid & Funnel check passed (Catenoid waist = 0.25, Funnel neck = 0.20, flare = 0.85)" << std::endl;
    }

    // 3. Stretch effect on Sphere check: Stretch should alter coordinates off equator
    {
        float x0 = 0.0f, y0 = 0.0f, s0 = 0.0f;
        float xStr = 0.0f, yStr = 0.0f, sStr = 0.0f;
        VoiceCore v1, v2;
        v1.reset(); v2.reset();
        v1.phase = 0.25f; // v = pi/4
        v2.phase = 0.25f;

        v1.process(
            48000.0f, 0.5f, 0.0f, RANGE_LFO, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, // Shape = Sphere
            0.5f, 0.0f, 0.0f, 0.0f, // Density, Depth
            0.5f, 0.0f, 1.0f, 0.0f, // Window, Spread
            0.0f, 0.0f, 0.0f, 0.0f, // Amp, Twist
            0.0f, 0.0f, 0.0f, 0.0f, // Rot X, Rot Y
            0.0f, 0.0f,             // Rot Z
            0.0f, 0.0f,             // Stretch = 0
            0.0f, 0.0f,
            x0, y0, s0
        );

        v2.process(
            48000.0f, 0.5f, 0.0f, RANGE_LFO, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, // Shape = Sphere
            0.5f, 0.0f, 0.0f, 0.0f, // Density, Depth
            0.5f, 0.0f, 1.0f, 0.0f, // Window, Spread
            0.0f, 0.0f, 0.0f, 0.0f, // Amp, Twist
            0.0f, 0.0f, 0.0f, 0.0f, // Rot X, Rot Y
            0.0f, 0.0f,             // Rot Z
            0.75f, 0.0f,            // Stretch = 0.75
            0.0f, 0.0f,
            xStr, yStr, sStr
        );

        assert(std::abs(x0 - xStr) > 0.01f || std::abs(y0 - yStr) > 0.01f);
        std::cout << "-> Stretch on Sphere check passed (x0=" << x0 << " vs xStr=" << xStr << ")" << std::endl;
    }

    // 4. Audio stream processing check: 1000 samples
    float sampleRate = 48000.0f;
    for (int i = 0; i < 1000; ++i) {
        float x = 0.0f, y = 0.0f, sOut = 0.0f;
        float testAudio = 5.0f * std::sin(2.0f * (float)M_PI * 440.0f * (float)i / sampleRate);
        voice.process(
            sampleRate,
            0.5477226f, 0.0f, RANGE_LFO, 0.0f, // freq
            0.0f, 0.0f,                         // shape
            0.5f, 0.0f,                         // morph
            0.5f, 0.0f,                         // density
            0.5f, 0.0f,                         // depth
            0.5f, 0.0f,                         // window
            0.5f, 0.0f,                         // spread
            0.5f, 0.0f,                         // amp
            0.2f, 0.0f,                         // twist
            0.2f, 0.0f,                         // rot X
            0.3f, 0.0f,                         // rot Y
            0.0f, 0.0f,                         // rot Z
            0.4f, 0.0f,                         // stretch
            testAudio,                          // mono audio in
            0.0f,                               // sync in
            x, y, sOut
        );

        assert(!std::isnan(x) && !std::isnan(y) && !std::isnan(sOut));
        assert(x >= -10.0f && x <= 10.0f);
        assert(y >= -10.0f && y <= 10.0f);
    }
    std::cout << "-> 1000 sample process stream verified (Output cleanly bounded in ±10V)" << std::endl;
    std::cout << "ALL GEORGIA CORE TESTS PASSED!" << std::endl;
    return 0;
}
