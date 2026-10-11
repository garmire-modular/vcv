#include <iostream>
#include <cassert>
#include <cmath>
#include "../src/core/GeorgiaCore.hpp"

using namespace garmire::georgia;

int main() {
    std::cout << "Testing GeorgiaCore 3D Wireframe DSP (v1.1.0)..." << std::endl;

    VoiceCore voice;
    voice.reset();

    // 1. Invariant check: Sphere surface point length at unit radius
    for (float u = 0.0f; u < 6.28f; u += 0.5f) {
        for (float v = 0.2f; v < 3.0f; v += 0.5f) {
            Vec3 P, N;
            VoiceCore::generateSurfacePoint(0.0f, 0.0f, u, v, P, N); // Family 0, Morph 0 = Sphere
            float len = P.length();
            assert(std::abs(len - 1.0f) < 1e-3f);
        }
    }
    std::cout << "-> Sphere surface point length invariant passed (Radius = 1.0)" << std::endl;

    // 2. Sharp Cube Corner Check: At u = pi/4, v = pi/2, Cube corner should be at |x|=|y|=0.85
    {
        Vec3 P, N;
        VoiceCore::generateSurfacePoint(1.0f, 0.0f, (float)M_PI * 0.25f, (float)M_PI * 0.5f, P, N);
        assert(std::abs(P.x - 0.85f) < 1e-3f);
        assert(std::abs(P.y - 0.85f) < 1e-3f);
        std::cout << "-> Sharp cube corner check passed (P = " << P.x << ", " << P.y << ", " << P.z << ")" << std::endl;
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
