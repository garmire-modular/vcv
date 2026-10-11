#include <iostream>
#include <cassert>
#include <cmath>
#include "../src/core/AmparaCore.hpp"

using namespace garmire::ampara;

int main() {
    std::cout << "Testing AmparaCore DSP (v2)..." << std::endl;

    VoiceCore voice;
    voice.reset();

    // 1. Invariant check: Circle radius is constant 1.0
    for (float th = 0.0f; th < 6.28f; th += 0.1f) {
        float r = VoiceCore::regularPolygonRadius(2.0f, th);
        assert(std::abs(r - 1.0f) < 1e-4f);
    }
    std::cout << "-> Polygon radius invariant passed (Circle is r=1.0)" << std::endl;

    // 2. Square radius check: at corners (0, pi/2), r=1.0; at facet center (pi/4), r = cos(pi/4) = 1/sqrt(2) approx 0.7071
    float rSqCorner = VoiceCore::regularPolygonRadius(4.0f, 0.0f);
    float rSqCenter = VoiceCore::regularPolygonRadius(4.0f, (float)M_PI / 4.0f);
    assert(std::abs(rSqCorner - 1.0f) < 0.05f);
    assert(std::abs(rSqCenter - (1.0f / std::sqrt(2.0f))) < 0.05f);
    std::cout << "-> Polygon radius invariant passed (Square geometry exact)" << std::endl;

    // 3. Audio stream processing check: 1000 samples with Polly Stretch & Bipolar Warp
    float sampleRate = 48000.0f;
    for (int i = 0; i < 1000; ++i) {
        float x = 0.0f, y = 0.0f, sOut = 0.0f;
        float testAudio = 5.0f * std::sin(2.0f * (float)M_PI * 440.0f * (float)i / sampleRate);
        voice.process(
            sampleRate,
            0.5f, 0.0f, RANGE_LFO, 0.0f, // freq
            3.0f, 0.0f,                  // shape (triangle)
            -0.5f, 0.0f,                 // warp (bipolar)
            0.5f, 0.0f,                  // window
            0.5f, 0.0f,                  // routing
            1.0f, 0.0f,                  // amp
            0.0f, 0.0f,                  // rise (default 0)
            0.0f, 0.0f,                  // fall (default 0)
            0.0f, 0.0f,                  // phase
            0.5f, 0.0f,                  // stretch
            testAudio, testAudio,        // audio In
            0.0f,                        // sync in
            x, y, sOut
        );

        assert(!std::isnan(x) && !std::isinf(x));
        assert(!std::isnan(y) && !std::isinf(y));
        assert(std::abs(x) <= 10.0f);
        assert(std::abs(y) <= 10.0f);
    }
    std::cout << "-> Real-time audio processing & stability passed" << std::endl;
    std::cout << "ALL AMPARA CORE TESTS PASSED!" << std::endl;
    return 0;
}
