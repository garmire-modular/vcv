#include <iostream>
#include <cassert>
#include <cmath>
#include "../src/core/GeorgiaCore.hpp"

using namespace garmire::georgia;

int main() {
    std::cout << "Testing GeorgiaCore 3D Wireframe DSP..." << std::endl;

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

    // 2. Continuous morph check: Sphere to Torus, Cube, Cylinder, Pyramid, Cone
    for (float s = 0.0f; s <= 2.0f; s += 0.5f) {
        for (float m = 0.0f; m <= 1.0f; m += 0.5f) {
            Vec3 P, N;
            VoiceCore::generateSurfacePoint(s, m, 1.0f, 1.0f, P, N);
            assert(!std::isnan(P.x) && !std::isnan(P.y) && !std::isnan(P.z));
            assert(!std::isnan(N.x) && !std::isnan(N.y) && !std::isnan(N.z));
        }
    }
    std::cout << "-> Continuous 3D morphing invariants passed" << std::endl;

    // 3. Audio stream & 3D Euler rotation processing check: 1000 samples
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
            0.5f, 0.0f,                         // routing
            1.0f, 0.0f,                         // amp
            0.5f, 0.0f,                         // fov
            0.2f, 0.0f,                         // rot X
            0.3f, 0.0f,                         // rot Y
            0.0f, 0.0f,                         // phase
            0.0f, 0.0f,                         // stretch
            testAudio, testAudio,               // audio In
            0.0f,                               // sync in
            x, y, sOut
        );

        assert(!std::isnan(x) && !std::isinf(x));
        assert(!std::isnan(y) && !std::isinf(y));
        assert(std::abs(x) <= 10.0f);
        assert(std::abs(y) <= 10.0f);
    }
    std::cout << "-> Real-time 3D projection & audio stability passed" << std::endl;
    std::cout << "ALL GEORGIA CORE TESTS PASSED!" << std::endl;
    return 0;
}
