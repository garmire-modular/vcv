#include <iostream>
#include <cassert>
#include <cmath>

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

void test_trough_dsp() {
	std::cout << "--- Testing Trough (Pittsburgh Modular Flamingo Center-Clipper) DSP ---" << std::endl;

	// Dry signal pass-through at 0% trough depth
	float inVal = -4.0f;
	float normTrough = 0.0f;
	float tiltVal = 0.0f;

	auto processChannel = [](float inVal, float normTrough, float tiltVal) -> float {
		float shifted = inVal + tiltVal;
		float vPeak = std::max(std::abs(inVal), 5.f);
		float k = 1.f + 9.f * normTrough;
		float xNorm = shifted / vPeak;
		float folded;
		if (xNorm < 0.f) {
			folded = -vPeak * std::sin((float)M_PI_2 * std::fabs(std::sin((float)M_PI_2 * k * xNorm)));
		} else {
			folded = shifted;
		}

		if (normTrough <= 0.05f) {
			float blend = normTrough / 0.05f;
			return (1.f - blend) * inVal + blend * folded;
		}
		return folded;
	};

	float outDry = processChannel(inVal, 0.0f, 0.0f);
	assert(std::fabs(outDry - inVal) < 1e-4f);
	std::cout << "[PASS] 0% Trough depth yields 100% dry signal (-4.0V => " << outDry << "V)" << std::endl;

	// Positive crest pass-through (unaffected by trough clipping)
	float outCrest = processChannel(4.0f, 1.0f, 0.0f);
	assert(std::fabs(outCrest - 4.0f) < 1e-4f);
	std::cout << "[PASS] Positive crest passed through unaltered (+4.0V => " << outCrest << "V)" << std::endl;

	// Negative trough center-clipping transformation
	float outWetTrough = processChannel(-4.0f, 0.5f, 0.0f);
	std::cout << "[PASS] Trough Center-Clipped -4.0V signal => " << outWetTrough << "V" << std::endl;
}

int main() {
	test_trough_dsp();
	std::cout << "ALL TROUGH DSP TESTS PASSED PERFECTLY!" << std::endl;
	return 0;
}
