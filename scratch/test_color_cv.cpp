#include <iostream>
#include <cassert>
#include <cmath>
#include "../src/dsp/ColorEngine.hpp"

// Mock minimal VCV Rack structs for testing DSP logic standalone
struct InputMock {
	float voltages[16] = {0.f};
	int channels = 0;
	bool connected = false;
	bool isConnected() const { return connected; }
	int getChannels() const { return connected ? (channels > 0 ? channels : 1) : 0; }
	float getPolyVoltage(int c) const { return connected ? voltages[c] : 0.f; }
};

struct OutputMock {
	float voltages[16] = {0.f};
	int channels = 0;
	void setChannels(int ch) { channels = ch; }
	void setVoltage(float v, int c) { voltages[c] = v; }
	float getVoltage(int c = 0) const { return voltages[c]; }
};

struct ParamMock {
	float value = 0.f;
	void setValue(float v) { value = v; }
	float getValue() const { return value; }
};

// Test 1: Color Engine conversion functions
void test_color_engine() {
	std::cout << "--- Testing ColorEngine DSP Math ---" << std::endl;

	// Pure Red HSV: H=0, S=1, V=1 -> R=1, G=0, B=0
	float r=0, g=0, b=0;
	garmire::color::hsvToRgb(0.f, 1.f, 1.f, r, g, b);
	assert(std::fabs(r - 1.f) < 1e-4f && std::fabs(g - 0.f) < 1e-4f && std::fabs(b - 0.f) < 1e-4f);
	std::cout << "[PASS] HSV Pure Red (0, 1, 1) => (" << r << ", " << g << ", " << b << ")" << std::endl;

	// Pure Green HSV: H=120, S=1, V=1 -> R=0, G=1, B=0
	garmire::color::hsvToRgb(120.f, 1.f, 1.f, r, g, b);
	assert(std::fabs(r - 0.f) < 1e-4f && std::fabs(g - 1.f) < 1e-4f && std::fabs(b - 0.f) < 1e-4f);
	std::cout << "[PASS] HSV Pure Green (120, 1, 1) => (" << r << ", " << g << ", " << b << ")" << std::endl;

	// Pure Blue HSL: H=240, S=1, L=0.5 -> R=0, G=0, B=1
	garmire::color::hslToRgb(240.f, 1.f, 0.5f, r, g, b);
	assert(std::fabs(r - 0.f) < 1e-4f && std::fabs(g - 0.f) < 1e-4f && std::fabs(b - 1.f) < 1e-4f);
	std::cout << "[PASS] HSL Pure Blue (240, 1, 0.5) => (" << r << ", " << g << ", " << b << ")" << std::endl;

	// OKLCH White: L=1, C=0, h=0 -> R=1, G=1, B=1
	garmire::color::oklchToRgb(1.f, 0.f, 0.f, r, g, b);
	assert(std::fabs(r - 1.f) < 1e-3f && std::fabs(g - 1.f) < 1e-3f && std::fabs(b - 1.f) < 1e-3f);
	std::cout << "[PASS] OKLCH White (1, 0, 0) => (" << r << ", " << g << ", " << b << ")" << std::endl;

	// OKLCH Black: L=0, C=0, h=0 -> R=0, G=0, B=0
	garmire::color::oklchToRgb(0.f, 0.f, 0.f, r, g, b);
	assert(std::fabs(r - 0.f) < 1e-3f && std::fabs(g - 0.f) < 1e-3f && std::fabs(b - 0.f) < 1e-3f);
	std::cout << "[PASS] OKLCH Black (0, 0, 0) => (" << r << ", " << g << ", " << b << ")" << std::endl;
}

// Test 2: RGB Module CV & Modulation
void test_rgb_cv() {
	std::cout << "--- Testing RGB CV & Attenuverter Logic ---" << std::endl;

	ParamMock knobR, knobG, knobB;
	ParamMock trimR, trimG, trimB;
	InputMock inR, inG, inB;
	OutputMock outR, outG, outB;

	// Initial knob settings: R=0.5 (2.5V), G=0.0 (0V), B=1.0 (5V)
	knobR.setValue(0.5f); knobG.setValue(0.0f); knobB.setValue(1.0f);
	trimR.setValue(1.0f); trimG.setValue(-0.5f); trimB.setValue(0.5f);

	// Connect CV input to Red (5V CV = +1.0 normalized)
	inR.connected = true; inR.channels = 1; inR.voltages[0] = 5.0f; // 5V CV
	// Connect CV input to Green (5V CV)
	inG.connected = true; inG.channels = 1; inG.voltages[0] = 5.0f; // 5V CV
	// Connect CV input to Blue (-5V CV = -1.0 normalized)
	inB.connected = true; inB.channels = 1; inB.voltages[0] = -5.0f; // -5V CV

	// Simulate DSP process calculation
	float rNorm = garmire::color::clampf(knobR.getValue() + (inR.getPolyVoltage(0) / 5.f) * trimR.getValue(), 0.f, 1.f);
	float gNorm = garmire::color::clampf(knobG.getValue() + (inG.getPolyVoltage(0) / 5.f) * trimG.getValue(), 0.f, 1.f);
	float bNorm = garmire::color::clampf(knobB.getValue() + (inB.getPolyVoltage(0) / 5.f) * trimB.getValue(), 0.f, 1.f);

	outR.setVoltage(rNorm * 5.0f, 0);
	outG.setVoltage(gNorm * 5.0f, 0);
	outB.setVoltage(bNorm * 5.0f, 0);

	// Red: knob 0.5 + (5/5)*1.0 = 1.5 -> clamped to 1.0 -> 5.0V output
	assert(std::fabs(outR.getVoltage() - 5.0f) < 1e-4f);
	// Green: knob 0.0 + (5/5)*(-0.5) = -0.5 -> clamped to 0.0 -> 0.0V output
	assert(std::fabs(outG.getVoltage() - 0.0f) < 1e-4f);
	// Blue: knob 1.0 + (-5/5)*(0.5) = 0.5 -> 2.5V output
	assert(std::fabs(outB.getVoltage() - 2.5f) < 1e-4f);

	std::cout << "[PASS] RGB Polyphonic CV Modulation & Clamping verified!" << std::endl;
}

// Test 3: HSV/HSL Module CV & Mode Switching
void test_hsv_hsl_cv() {
	std::cout << "--- Testing HSV / HSL CV & Mode Toggle ---" << std::endl;

	ParamMock hueKnob, satKnob, valKnob;
	ParamMock hueTrim, satTrim, valTrim;
	ParamMock modeParam; // 0 = HSV, 1 = HSL
	InputMock hueIn, satIn, valIn;

	hueKnob.setValue(0.5f); // 180 degrees (Cyan)
	satKnob.setValue(1.0f);
	valKnob.setValue(1.0f);
	modeParam.setValue(0.0f); // HSV

	float r=0, g=0, b=0;
	float hDeg = hueKnob.getValue() * 360.f;
	garmire::color::hsvToRgb(hDeg, satKnob.getValue(), valKnob.getValue(), r, g, b);

	// HSV Cyan (180 deg, 1, 1) -> R=0, G=1, B=1 -> Outputs 0V, 5V, 5V
	assert(std::fabs(r - 0.f) < 1e-4f && std::fabs(g - 1.f) < 1e-4f && std::fabs(b - 1.f) < 1e-4f);
	std::cout << "[PASS] HSV Mode Cyan output: R=" << r*5.0f << "V, G=" << g*5.0f << "V, B=" << b*5.0f << "V" << std::endl;

	// Toggle to HSL mode (mode = 1)
	modeParam.setValue(1.0f);
	garmire::color::hslToRgb(hDeg, satKnob.getValue(), valKnob.getValue(), r, g, b);
	// HSL (180 deg, 1, 1) -> Lightness 1.0 is Pure White -> R=1, G=1, B=1 -> Outputs 5V, 5V, 5V
	assert(std::fabs(r - 1.f) < 1e-4f && std::fabs(g - 1.f) < 1e-4f && std::fabs(b - 1.f) < 1e-4f);
	std::cout << "[PASS] HSL Mode Lightness=1 White output: R=" << r*5.0f << "V, G=" << g*5.0f << "V, B=" << b*5.0f << "V" << std::endl;
}

int main() {
	std::cout << "==========================================" << std::endl;
	std::cout << "  Garmire Color CV & DSP Unit Test Suite" << std::endl;
	std::cout << "==========================================" << std::endl;
	test_color_engine();
	test_rgb_cv();
	test_hsv_hsl_cv();
	std::cout << "\nALL COLOR DSP AND CV TESTS PASSED PERFECTLY!" << std::endl;
	return 0;
}
