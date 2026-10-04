#pragma once
#include <algorithm>
#include <cmath>

namespace rgbivca {

enum ScaleMode {
	SCALE_UNSCALED = 0,
	SCALE_BI_1V    = 1,
	SCALE_BI_5V    = 2,
	SCALE_BI_10V   = 3,
};

inline float getScaleMultiplier(ScaleMode mode) {
	switch (mode) {
		case SCALE_UNSCALED: return 1.0f;
		case SCALE_BI_1V:    return 0.2f; // 1V / 5V
		case SCALE_BI_5V:    return 1.0f; // 5V / 5V
		case SCALE_BI_10V:   return 2.0f; // 10V / 5V
		default:             return 1.0f;
	}
}

inline float getClampLimit(ScaleMode mode) {
	switch (mode) {
		case SCALE_UNSCALED: return 12.0f; // Eurorack hardware rail limit
		case SCALE_BI_1V:    return 1.0f;
		case SCALE_BI_5V:    return 5.0f;
		case SCALE_BI_10V:   return 10.0f;
		default:             return 12.0f;
	}
}

inline float clampVal(float v, float minV, float maxV) {
	return (v < minV) ? minV : (v > maxV ? maxV : v);
}

struct Engine {
	ScaleMode scaleMode = SCALE_UNSCALED;

	struct FrameOutput {
		float r;
		float g;
		float b;
	};

	inline FrameOutput process(float inR, float inG, float inB, float inI, bool iConnected, float attv) const {
		// Effective intensity voltage: normalizes to +5.0V when I input is unpatched
		float effI = iConnected ? inI : 5.0f;
		// Gain calculation: 5V represents unity (1.0), modulated by bipolar attenuverter knob [-1.0, 1.0]
		float gain = (effI * 0.2f) * attv;

		float mult = getScaleMultiplier(scaleMode);
		float limit = getClampLimit(scaleMode);

		float outR = clampVal(inR * gain * mult, -limit, limit);
		float outG = clampVal(inG * gain * mult, -limit, limit);
		float outB = clampVal(inB * gain * mult, -limit, limit);

		return {outR, outG, outB};
	}
};

} // namespace rgbivca
