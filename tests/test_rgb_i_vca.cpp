#include "../src/core/RgbIVcaEngine.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void testUnpatchedNormalization() {
	std::cout << "[Test] Running unpatched I normalization tests..." << std::endl;
	rgbivca::Engine engine;
	engine.scaleMode = rgbivca::SCALE_UNSCALED;

	// When I is unpatched, knob at 1.0 -> gain is 1.0
	auto out1 = engine.process(5.0f, 2.5f, 1.0f, 0.0f, false, 1.0f);
	assert(std::abs(out1.r - 5.0f) < 1e-5f);
	assert(std::abs(out1.g - 2.5f) < 1e-5f);
	assert(std::abs(out1.b - 1.0f) < 1e-5f);

	// Knob at 0.0 -> muted
	auto out0 = engine.process(5.0f, 2.5f, 1.0f, 0.0f, false, 0.0f);
	assert(std::abs(out0.r - 0.0f) < 1e-5f);
	assert(std::abs(out0.g - 0.0f) < 1e-5f);
	assert(std::abs(out0.b - 0.0f) < 1e-5f);

	// Knob at -1.0 -> inverted
	auto outNeg = engine.process(5.0f, 2.5f, 1.0f, 0.0f, false, -1.0f);
	assert(std::abs(outNeg.r - (-5.0f)) < 1e-5f);
	assert(std::abs(outNeg.g - (-2.5f)) < 1e-5f);
	assert(std::abs(outNeg.b - (-1.0f)) < 1e-5f);

	// Knob at 0.5 -> 50%
	auto outHalf = engine.process(5.0f, 2.5f, 1.0f, 0.0f, false, 0.5f);
	assert(std::abs(outHalf.r - 2.5f) < 1e-5f);
	assert(std::abs(outHalf.g - 1.25f) < 1e-5f);
	assert(std::abs(outHalf.b - 0.5f) < 1e-5f);

	std::cout << "  -> Unpatched I normalization tests passed." << std::endl;
}

void testPatchedModulation() {
	std::cout << "[Test] Running patched I modulation tests..." << std::endl;
	rgbivca::Engine engine;
	engine.scaleMode = rgbivca::SCALE_UNSCALED;

	// I = 5.0V, knob = 1.0 -> unity gain
	auto out5V = engine.process(5.0f, 4.0f, 3.0f, 5.0f, true, 1.0f);
	assert(std::abs(out5V.r - 5.0f) < 1e-5f);
	assert(std::abs(out5V.g - 4.0f) < 1e-5f);
	assert(std::abs(out5V.b - 3.0f) < 1e-5f);

	// I = 2.5V, knob = 1.0 -> 50% gain
	auto out2_5V = engine.process(5.0f, 4.0f, 3.0f, 2.5f, true, 1.0f);
	assert(std::abs(out2_5V.r - 2.5f) < 1e-5f);
	assert(std::abs(out2_5V.g - 2.0f) < 1e-5f);
	assert(std::abs(out2_5V.b - 1.5f) < 1e-5f);

	// I = 0.0V -> muted
	auto out0V = engine.process(5.0f, 4.0f, 3.0f, 0.0f, true, 1.0f);
	assert(std::abs(out0V.r - 0.0f) < 1e-5f);
	assert(std::abs(out0V.g - 0.0f) < 1e-5f);
	assert(std::abs(out0V.b - 0.0f) < 1e-5f);

	// I = 5.0V, knob = -1.0 -> inverted
	auto outNegKnob = engine.process(5.0f, 4.0f, 3.0f, 5.0f, true, -1.0f);
	assert(std::abs(outNegKnob.r - (-5.0f)) < 1e-5f);

	// I = -5.0V, knob = 1.0 -> inverted
	auto outNegCV = engine.process(5.0f, 4.0f, 3.0f, -5.0f, true, 1.0f);
	assert(std::abs(outNegCV.r - (-5.0f)) < 1e-5f);

	// I = -5.0V, knob = -1.0 -> positive (four-quadrant)
	auto outDoubleNeg = engine.process(5.0f, 4.0f, 3.0f, -5.0f, true, -1.0f);
	assert(std::abs(outDoubleNeg.r - 5.0f) < 1e-5f);

	std::cout << "  -> Patched I modulation tests passed." << std::endl;
}

void testScalingModes() {
	std::cout << "[Test] Running scaling modes tests..." << std::endl;
	rgbivca::Engine engine;

	float inR = 5.0f;
	float inG = -5.0f;
	float inB = 2.5f;

	// Mode 0: Unscaled (5V -> 5V, clamp +-12V)
	engine.scaleMode = rgbivca::SCALE_UNSCALED;
	auto out0 = engine.process(inR, inG, inB, 5.0f, false, 1.0f);
	assert(std::abs(out0.r - 5.0f) < 1e-5f);
	assert(std::abs(out0.g - (-5.0f)) < 1e-5f);
	assert(std::abs(out0.b - 2.5f) < 1e-5f);

	// Mode 1: Scale to +/-1V (5V -> 1V, clamp +-1V)
	engine.scaleMode = rgbivca::SCALE_BI_1V;
	auto out1 = engine.process(inR, inG, inB, 5.0f, false, 1.0f);
	assert(std::abs(out1.r - 1.0f) < 1e-5f);
	assert(std::abs(out1.g - (-1.0f)) < 1e-5f);
	assert(std::abs(out1.b - 0.5f) < 1e-5f);

	// Clamping check for Mode 1: 10V input with gain 1.0 should clamp to 1.0V
	auto out1Clamp = engine.process(10.0f, -10.0f, 0.0f, 5.0f, false, 1.0f);
	assert(std::abs(out1Clamp.r - 1.0f) < 1e-5f);
	assert(std::abs(out1Clamp.g - (-1.0f)) < 1e-5f);

	// Mode 2: Scale to +/-5V (5V -> 5V, clamp +-5V)
	engine.scaleMode = rgbivca::SCALE_BI_5V;
	auto out5 = engine.process(inR, inG, inB, 5.0f, false, 1.0f);
	assert(std::abs(out5.r - 5.0f) < 1e-5f);
	assert(std::abs(out5.g - (-5.0f)) < 1e-5f);
	assert(std::abs(out5.b - 2.5f) < 1e-5f);

	// Clamping check for Mode 2: 10V input with gain 1.0 should clamp to 5.0V
	auto out5Clamp = engine.process(10.0f, -10.0f, 0.0f, 5.0f, false, 1.0f);
	assert(std::abs(out5Clamp.r - 5.0f) < 1e-5f);
	assert(std::abs(out5Clamp.g - (-5.0f)) < 1e-5f);

	// Mode 3: Scale to +/-10V (5V -> 10V, clamp +-10V)
	engine.scaleMode = rgbivca::SCALE_BI_10V;
	auto out10 = engine.process(inR, inG, inB, 5.0f, false, 1.0f);
	assert(std::abs(out10.r - 10.0f) < 1e-5f);
	assert(std::abs(out10.g - (-10.0f)) < 1e-5f);
	assert(std::abs(out10.b - 5.0f) < 1e-5f);

	// Clamping check for Mode 3: 10V input with gain 1.0 -> 20V clamped to 10.0V
	auto out10Clamp = engine.process(10.0f, -10.0f, 0.0f, 5.0f, false, 1.0f);
	assert(std::abs(out10Clamp.r - 10.0f) < 1e-5f);
	assert(std::abs(out10Clamp.g - (-10.0f)) < 1e-5f);

	std::cout << "  -> Scaling modes tests passed." << std::endl;
}

int main() {
	std::cout << "=== RGB/I/VCA DSP Engine Test Suite ===" << std::endl;
	testUnpatchedNormalization();
	testPatchedModulation();
	testScalingModes();
	std::cout << "=== All RGB/I/VCA DSP Engine Tests Passed! ===" << std::endl;
	return 0;
}
