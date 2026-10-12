#include "plugin.hpp"
#include "core/GeorgiaCore.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Georgia — 3D Rotating Wireframe Vector Synthesizer (18 HP)
//  Generate class module producing animated 3D wireframe surfaces
//  (Sphere/Toroid, Cube/Cylinder, Pyramid/Cone) with perspective camera
//  depth, 3D Euler rotations, Polly logarithmic stretch, 3D twist,
//  and mono audio waveform displacement with equatorial spread control.
// ─────────────────────────────────────────────────────────────────────

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Georgia : Module {
	using RangeMode = garmire::georgia::RangeMode;
	using ScopeMode = garmire::georgia::ScopeMode;

	enum ParamId {
		// Row 1 Knobs: FREQ, RANGE, FINE, SHAPE, MORPH
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,
		SHAPE_PARAM,
		MORPH_PARAM,

		// Row 2 Knobs: DENSITY, DEPTH, WINDOW, SPREAD, AMP
		DENSITY_PARAM,
		DEPTH_PARAM,
		WINDOW_PARAM,
		SPREAD_PARAM,
		AMP_PARAM,

		// Row 3 Knobs: TWIST, ROT X, ROT Y, ROT Z, STRETCH
		TWIST_PARAM,
		ROT_X_PARAM,
		ROT_Y_PARAM,
		ROT_Z_PARAM,
		STRETCH_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 1
		// IN, FREQ, FINE, SHAPE, MORPH
		IN_TRIM_PARAM,
		FREQ_TRIM_PARAM,
		FINE_TRIM_PARAM,
		SHAPE_TRIM_PARAM,
		MORPH_TRIM_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 2
		// DENSITY, DEPTH, WINDOW, SPREAD, AMP
		DENSITY_TRIM_PARAM,
		DEPTH_TRIM_PARAM,
		WINDOW_TRIM_PARAM,
		SPREAD_TRIM_PARAM,
		AMP_TRIM_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 3
		// TWIST, ROT X, ROT Y, ROT Z, STRETCH
		TWIST_TRIM_PARAM,
		ROT_X_TRIM_PARAM,
		ROT_Y_TRIM_PARAM,
		ROT_Z_TRIM_PARAM,
		STRETCH_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Jack Row 1: IN, FREQ, FINE, SHAPE, MORPH
		IN_INPUT,
		FREQ_CV_INPUT,
		FINE_CV_INPUT,
		SHAPE_CV_INPUT,
		MORPH_CV_INPUT,

		// Jack Row 2: DENSITY, DEPTH, WINDOW, SPREAD, AMP
		DENSITY_CV_INPUT,
		DEPTH_CV_INPUT,
		WINDOW_CV_INPUT,
		SPREAD_CV_INPUT,
		AMP_CV_INPUT,

		// Jack Row 3: TWIST, ROT X, ROT Y, ROT Z, STRETCH
		TWIST_CV_INPUT,
		ROT_X_CV_INPUT,
		ROT_Y_CV_INPUT,
		ROT_Z_CV_INPUT,
		STRETCH_CV_INPUT,

		// Jack Row 4: SYNC IN
		SYNC_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Jack Row 4: SYNC IN, X, Y, SYNC OUT
		X_OUTPUT,
		Y_OUTPUT,
		SYNC_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_TEAL,
		RANGE_LIGHT_MAGENTA,

		LIGHTS_LEN
	};

	RangeMode rangeMode = garmire::georgia::RANGE_LFO;
	ScopeMode scopeMode = garmire::georgia::SCOPE_SURFACE_NORMAL;
	dsp::BooleanTrigger rangeTrigger;

	// Real-time safe 16-voice polyphonic core DSP engines
	garmire::georgia::VoiceCore voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Georgia* getGeorgia() {
			return dynamic_cast<Georgia*>(module);
		}

		float getDisplayValue() override {
			Georgia* georgia = getGeorgia();
			if (!georgia) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (georgia->paramQuantities.size() > Georgia::FINE_PARAM) ? georgia->params[Georgia::FINE_PARAM].getValue() : 0.f;

			if (georgia->rangeMode == garmire::georgia::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (georgia->rangeMode == garmire::georgia::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 50.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Georgia* georgia = getGeorgia();
			if (!georgia) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (georgia->rangeMode == garmire::georgia::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Georgia* georgia = getGeorgia();
			if (!georgia) return;

			float fine = (georgia->paramQuantities.size() > Georgia::FINE_PARAM) ? georgia->params[Georgia::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (georgia->rangeMode == garmire::georgia::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (georgia->rangeMode == garmire::georgia::RANGE_LFO) {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 0.01f);
				float fCoarse = targetHz - fine * 20.0f;
				if (fCoarse <= 0.01f) {
					newCoarse = 0.0f;
				} else {
					newCoarse = std::sqrt((fCoarse - 0.01f) / 199.99f);
				}
			} else {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 100.f);
				float fineFactor = 1.0f + fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float fCoarse = targetHz / fineFactor;
				if (fCoarse <= 1.0f) fCoarse = 1.0f;

				float ratio = fCoarse / 150.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(2000.0f / 150.0f);
			}

			setValue(clampf(newCoarse, 0.0f, 1.0f));
		}

		void setDisplayValue(float displayValue) override {
			Georgia* georgia = getGeorgia();
			if (!georgia) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (georgia->rangeMode == garmire::georgia::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Georgia* georgia = getGeorgia();
			if (!georgia) {
				ParamQuantity::setDisplayValueString(s);
				return;
			}

			std::string str = s;
			std::string lowerStr = s;
			for (char& c : lowerStr) c = (char)std::tolower((unsigned char)c);

			enum UnitType { UNIT_NONE, UNIT_HZ, UNIT_KHZ, UNIT_MHZ, UNIT_SEC, UNIT_MS };
			UnitType unitType = UNIT_NONE;

			if (lowerStr.find("khz") != std::string::npos) {
				unitType = UNIT_KHZ;
			} else if (lowerStr.find("mhz") != std::string::npos) {
				unitType = UNIT_MHZ;
			} else if (lowerStr.find("hz") != std::string::npos) {
				unitType = UNIT_HZ;
			} else if (lowerStr.find("ms") != std::string::npos) {
				unitType = UNIT_MS;
			} else if (lowerStr.find("s") != std::string::npos || lowerStr.find("sec") != std::string::npos) {
				unitType = UNIT_SEC;
			}

			char* endPtr = nullptr;
			float rawVal = std::strtof(str.c_str(), &endPtr);
			if (endPtr == str.c_str()) return;

			bool isPeriod;
			float finalVal;

			if (unitType == UNIT_SEC) {
				isPeriod = true;
				finalVal = rawVal;
			} else if (unitType == UNIT_MS) {
				isPeriod = true;
				finalVal = rawVal * 0.001f;
			} else if (unitType == UNIT_HZ) {
				isPeriod = false;
				finalVal = rawVal;
			} else if (unitType == UNIT_KHZ) {
				isPeriod = false;
				finalVal = rawVal * 1000.0f;
			} else if (unitType == UNIT_MHZ) {
				isPeriod = false;
				finalVal = rawVal * 1000000.0f;
			} else {
				isPeriod = (georgia->rangeMode == garmire::georgia::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Georgia* georgia = getGeorgia();
			if (!georgia) return "";
			return (georgia->rangeMode == garmire::georgia::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	struct ShapeParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int fam = (int)std::floor(getValue() + 0.5f);
			if (fam < 0) fam = 0;
			if (fam > 3) fam = 3;
			switch (fam) {
				case 0: return "Sphere / Toroid";
				case 1: return "Cube / Cylinder";
				case 2: return "Pyramid / Cone";
				case 3: return "Funnel / Catenoid";
				default: return "Sphere / Toroid";
			}
		}
	};

	struct WindowParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			return 0.5f * std::pow(1000.0f, getValue());
		}
		std::string getDisplayValueString() override {
			float v = getDisplayValue();
			char buf[32];
			if (v < 100.0f) {
				std::snprintf(buf, sizeof(buf), "%.1f", v);
			} else {
				std::snprintf(buf, sizeof(buf), "%.0f", v);
			}
			return std::string(buf);
		}
		void setDisplayValue(float displayValue) override {
			float ms = clampf(displayValue, 0.5f, 500.0f);
			float raw = std::log10(2.0f * ms) / 3.0f;
			setValue(clampf(raw, 0.0f, 1.0f));
		}
		void setDisplayValueString(std::string s) override {
			std::string str = s;
			std::string lowerStr = s;
			for (char& c : lowerStr) c = (char)std::tolower((unsigned char)c);

			char* endPtr = nullptr;
			float rawVal = std::strtof(str.c_str(), &endPtr);
			if (endPtr == str.c_str()) return;

			float ms = rawVal;
			if (lowerStr.find("ms") != std::string::npos) {
				ms = rawVal;
			} else if (lowerStr.find("s") != std::string::npos || lowerStr.find("sec") != std::string::npos) {
				ms = rawVal * 1000.0f;
			}
			setDisplayValue(ms);
		}
		std::string getUnit() override {
			return " ms";
		}
	};

	struct RotAngleParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			return getValue() * 360.0f;
		}
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", getDisplayValue());
			return std::string(buf);
		}
		void setDisplayValue(float displayValue) override {
			float deg = std::fmod(displayValue, 360.0f);
			if (deg < 0.0f) deg += 360.0f;
			if (displayValue >= 360.0f && deg == 0.0f) {
				setValue(1.0f);
			} else {
				setValue(clampf(deg / 360.0f, 0.0f, 1.0f));
			}
		}
		void setDisplayValueString(std::string s) override {
			char* endPtr = nullptr;
			float rawVal = std::strtof(s.c_str(), &endPtr);
			if (endPtr == s.c_str()) return;
			setDisplayValue(rawVal);
		}
		std::string getUnit() override {
			return "°";
		}
	};

	Georgia() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Knobs: Row 1
		// FREQ, RANGE, FINE, SHAPE, MORPH
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477226f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine tune", "%", 0.f, 100.f);
		configParam<ShapeParamQuantity>(SHAPE_PARAM, 0.f, 3.f, 0.f, "Shape", "");
		paramQuantities[SHAPE_PARAM]->snapEnabled = true;
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph", "%", 0.f, 100.f);

		// Knobs: Row 2
		// DENSITY, DEPTH, WINDOW, SPREAD, AMP
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density", "%", 0.f, 100.f);
		configParam(DEPTH_PARAM, 0.f, 1.f, 1.0f, "Depth", "%", 0.f, 100.f);
		configParam<WindowParamQuantity>(WINDOW_PARAM, 0.f, 1.f, 0.5f, "Buffer window", " ms");
		configParam(SPREAD_PARAM, 0.f, 1.f, 1.0f, "Spread", "%", 0.f, 100.f);
		configParam(AMP_PARAM, 0.f, 1.f, 0.0f, "Amplitude", "%", 0.f, 100.f); // Default 0%, Max 100%

		// Knobs: Row 3
		// TWIST, ROT X, ROT Y, ROT Z, STRETCH
		configParam(TWIST_PARAM, -1.f, 1.f, 0.f, "Twist", "%", 0.f, 100.f);
		configParam<RotAngleParamQuantity>(ROT_X_PARAM, 0.f, 1.f, 0.f, "X rotation", "°");
		configParam<RotAngleParamQuantity>(ROT_Y_PARAM, 0.f, 1.f, 0.f, "Y rotation", "°");
		configParam<RotAngleParamQuantity>(ROT_Z_PARAM, 0.f, 1.f, 0.f, "Z rotation", "°");
		configParam(STRETCH_PARAM, -1.f, 1.f, 0.f, "Stretch", "%", 0.f, 100.f);

		// Zone 3 Attenuverter Trimpots: Row 1
		// IN, FREQ, FINE, SHAPE, MORPH
		configParam(IN_TRIM_PARAM, -1.f, 1.f, 1.f, "Input level", "%", 0.f, 100.f);
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(FINE_TRIM_PARAM, -1.f, 1.f, 0.f, "Fine tune CV depth", "%", 0.f, 100.f);
		configParam(SHAPE_TRIM_PARAM, -1.f, 1.f, 0.f, "Shape CV depth", "%", 0.f, 100.f);
		configParam(MORPH_TRIM_PARAM, -1.f, 1.f, 0.f, "Morph CV depth", "%", 0.f, 100.f);

		// Zone 3 Attenuverter Trimpots: Row 2
		// DENSITY, DEPTH, WINDOW, SPREAD, AMP
		configParam(DENSITY_TRIM_PARAM, -1.f, 1.f, 0.f, "Density CV depth", "%", 0.f, 100.f);
		configParam(DEPTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Depth CV depth", "%", 0.f, 100.f);
		configParam(WINDOW_TRIM_PARAM, -1.f, 1.f, 0.f, "Window CV depth", "%", 0.f, 100.f);
		configParam(SPREAD_TRIM_PARAM, -1.f, 1.f, 0.f, "Spread CV depth", "%", 0.f, 100.f);
		configParam(AMP_TRIM_PARAM, -1.f, 1.f, 0.f, "Amplitude CV depth", "%", 0.f, 100.f);

		// Zone 3 Attenuverter Trimpots: Row 3
		// TWIST, ROT X, ROT Y, ROT Z, STRETCH
		configParam(TWIST_TRIM_PARAM, -1.f, 1.f, 0.f, "Twist CV depth", "%", 0.f, 100.f);
		configParam(ROT_X_TRIM_PARAM, -1.f, 1.f, 0.f, "X rotation CV depth", "%", 0.f, 100.f);
		configParam(ROT_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Y rotation CV depth", "%", 0.f, 100.f);
		configParam(ROT_Z_TRIM_PARAM, -1.f, 1.f, 0.f, "Z rotation CV depth", "%", 0.f, 100.f);
		configParam(STRETCH_TRIM_PARAM, -1.f, 1.f, 0.f, "Stretch CV depth", "%", 0.f, 100.f);

		// Jacks: Row 1
		// IN, FREQ, FINE, SHAPE, MORPH
		configInput(IN_INPUT, "Audio");
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(FINE_CV_INPUT, "Fine tune CV");
		configInput(SHAPE_CV_INPUT, "Shape CV");
		configInput(MORPH_CV_INPUT, "Morph CV");

		// Jacks: Row 2
		// DENSITY, DEPTH, WINDOW, SPREAD, AMP
		configInput(DENSITY_CV_INPUT, "Density CV");
		configInput(DEPTH_CV_INPUT, "Depth CV");
		configInput(WINDOW_CV_INPUT, "Window CV");
		configInput(SPREAD_CV_INPUT, "Spread CV");
		configInput(AMP_CV_INPUT, "Amplitude CV");

		// Jacks: Row 3
		// TWIST, ROT X, ROT Y, ROT Z, STRETCH
		configInput(TWIST_CV_INPUT, "Twist CV");
		configInput(ROT_X_CV_INPUT, "X rotation CV");
		configInput(ROT_Y_CV_INPUT, "Y rotation CV");
		configInput(ROT_Z_CV_INPUT, "Z rotation CV");
		configInput(STRETCH_CV_INPUT, "Stretch CV");

		// Jacks: Row 4
		// SYNC IN, X, Y, SYNC OUT
		configInput(SYNC_INPUT, "Sync");
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(SYNC_OUTPUT, "Sync");

		for (int c = 0; c < 16; ++c) {
			voices[c].reset();
		}
	}

	void onReset() override {
		rangeMode = garmire::georgia::RANGE_LFO;
		scopeMode = garmire::georgia::SCOPE_SURFACE_NORMAL;
		for (int c = 0; c < 16; ++c) {
			voices[c].reset();
		}
	}

	void process(const ProcessArgs& args) override {
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)(((int)rangeMode + 1) % 3);
		}

		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == garmire::georgia::RANGE_VERY_SLOW ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_TEAL].setBrightness(rangeMode == garmire::georgia::RANGE_LFO ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_MAGENTA].setBrightness(rangeMode == garmire::georgia::RANGE_VCO ? 1.0f : 0.0f);

		int channels = 1;
		channels = std::max(channels, inputs[IN_INPUT].getChannels());
		channels = std::max(channels, inputs[FREQ_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[FINE_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[SHAPE_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[MORPH_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[DENSITY_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[DEPTH_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[WINDOW_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[SPREAD_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[AMP_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[TWIST_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[ROT_X_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[ROT_Y_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[ROT_Z_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[STRETCH_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[SYNC_INPUT].getChannels());
		channels = std::min(channels, 16);

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
		outputs[SYNC_OUTPUT].setChannels(channels);

		float coarseFreq = params[FREQ_PARAM].getValue();
		float fineFreq = params[FINE_PARAM].getValue();
		float shapeVal = params[SHAPE_PARAM].getValue();
		float morphVal = params[MORPH_PARAM].getValue();

		float densityVal = params[DENSITY_PARAM].getValue();
		float depthVal = params[DEPTH_PARAM].getValue();
		float windowVal = params[WINDOW_PARAM].getValue();
		float spreadVal = params[SPREAD_PARAM].getValue();
		float ampVal = params[AMP_PARAM].getValue();

		float twistVal = params[TWIST_PARAM].getValue();
		float rotXVal = params[ROT_X_PARAM].getValue();
		float rotYVal = params[ROT_Y_PARAM].getValue();
		float rotZVal = params[ROT_Z_PARAM].getValue();
		float stretchVal = params[STRETCH_PARAM].getValue();

		float inTrim = params[IN_TRIM_PARAM].getValue();
		float freqTrim = params[FREQ_TRIM_PARAM].getValue();
		float fineTrim = params[FINE_TRIM_PARAM].getValue();
		float shapeTrim = params[SHAPE_TRIM_PARAM].getValue();
		float morphTrim = params[MORPH_TRIM_PARAM].getValue();

		float densityTrim = params[DENSITY_TRIM_PARAM].getValue();
		float depthTrim = params[DEPTH_TRIM_PARAM].getValue();
		float windowTrim = params[WINDOW_TRIM_PARAM].getValue();
		float spreadTrim = params[SPREAD_TRIM_PARAM].getValue();
		float ampTrim = params[AMP_TRIM_PARAM].getValue();

		float twistTrim = params[TWIST_TRIM_PARAM].getValue();
		float rotXTrim = params[ROT_X_TRIM_PARAM].getValue();
		float rotYTrim = params[ROT_Y_TRIM_PARAM].getValue();
		float rotZTrim = params[ROT_Z_TRIM_PARAM].getValue();
		float stretchTrim = params[STRETCH_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float audioIn = inputs[IN_INPUT].getPolyVoltage(c) * inTrim;

			float freqCV = inputs[FREQ_CV_INPUT].getPolyVoltage(c) * freqTrim;
			float fineCV = inputs[FINE_CV_INPUT].getPolyVoltage(c) * fineTrim;
			float shapeCV = inputs[SHAPE_CV_INPUT].getPolyVoltage(c) * shapeTrim;
			float morphCV = inputs[MORPH_CV_INPUT].getPolyVoltage(c) * morphTrim;

			float densityCV = inputs[DENSITY_CV_INPUT].getPolyVoltage(c) * densityTrim;
			float depthCV = inputs[DEPTH_CV_INPUT].getPolyVoltage(c) * depthTrim;
			float windowCV = inputs[WINDOW_CV_INPUT].getPolyVoltage(c) * windowTrim;
			float spreadCV = inputs[SPREAD_CV_INPUT].getPolyVoltage(c) * spreadTrim;
			float ampCV = inputs[AMP_CV_INPUT].getPolyVoltage(c) * ampTrim;

			float twistCV = inputs[TWIST_CV_INPUT].getPolyVoltage(c) * twistTrim;
			float rotXCV = inputs[ROT_X_CV_INPUT].getPolyVoltage(c) * rotXTrim;
			float rotYCV = inputs[ROT_Y_CV_INPUT].getPolyVoltage(c) * rotYTrim;
			float rotZCV = inputs[ROT_Z_CV_INPUT].getPolyVoltage(c) * rotZTrim;
			float stretchCV = inputs[STRETCH_CV_INPUT].getPolyVoltage(c) * stretchTrim;

			float syncIn = inputs[SYNC_INPUT].getPolyVoltage(c);

			float outX = 0.0f;
			float outY = 0.0f;
			float syncOut = 0.0f;

			voices[c].scopeMode = scopeMode;
			voices[c].process(
				args.sampleRate,
				coarseFreq, fineFreq + fineCV * 0.1f, rangeMode, freqCV,
				shapeVal, shapeCV,
				morphVal, morphCV,
				densityVal, densityCV,
				depthVal, depthCV,
				windowVal, windowCV,
				spreadVal, spreadCV,
				ampVal, ampCV,
				twistVal, twistCV,
				rotXVal, rotXCV,
				rotYVal, rotYCV,
				rotZVal, rotZCV,
				stretchVal, stretchCV,
				audioIn,
				syncIn,
				outX, outY, syncOut
			);

			outputs[X_OUTPUT].setVoltage(outX, c);
			outputs[Y_OUTPUT].setVoltage(outY, c);
			outputs[SYNC_OUTPUT].setVoltage(syncOut, c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "scopeMode", json_integer((int)scopeMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) {
			rangeMode = (RangeMode)json_integer_value(rmJ);
		}
		json_t* smJ = json_object_get(rootJ, "scopeMode");
		if (smJ) {
			scopeMode = (ScopeMode)json_integer_value(smJ);
		}
	}
};

// ── Custom 3-Color Range Light Widget (Palette: #e1be6a, #40b0a6, #d35fb7) ──
template <typename TBase = GrayModuleLightWidget>
struct TGeorgiaRangeLight : TBase {
	TGeorgiaRangeLight() {
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff)); // Very Slow (Warm Gold)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // LFO (Teal)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff)); // VCO (Magenta)
	}
};
struct GeorgiaRangeLightWidget : SmallLight<TGeorgiaRangeLight<>> {};

struct GeorgiaWidget : ModuleWidget {
	GeorgiaWidget(Georgia* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Georgia.svg")));

		// 5 Columns across 91.44 mm: cols = [13.52, 29.62, 45.72, 61.82, 77.92] mm
		const float cols[5] = {13.52f, 29.62f, 45.72f, 61.82f, 77.92f};

		// Row 1 Knobs (Center Y = 21.59 mm):
		// FREQ, RANGE, FINE, SHAPE, MORPH
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[0], 21.59f)), module, Georgia::FREQ_PARAM));
		
		// Range button and LED centered at cols[1] (directly above DEPTH)
		addChild(createLightCentered<GeorgiaRangeLightWidget>(mm2px(Vec(cols[1], 15.50f)), module, Georgia::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<TL1105>(mm2px(Vec(cols[1], 21.59f)), module, Georgia::RANGE_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[2], 21.59f)), module, Georgia::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[3], 21.59f)), module, Georgia::SHAPE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[4], 21.59f)), module, Georgia::MORPH_PARAM));

		// Row 2 Knobs (Center Y = 34.50 mm):
		// DENSITY, DEPTH, WINDOW, SPREAD, AMP
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[0], 34.50f)), module, Georgia::DENSITY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[1], 34.50f)), module, Georgia::DEPTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[2], 34.50f)), module, Georgia::WINDOW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[3], 34.50f)), module, Georgia::SPREAD_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[4], 34.50f)), module, Georgia::AMP_PARAM));

		// Row 3 Knobs (Center Y = 47.50 mm):
		// TWIST, ROT X, ROT Y, ROT Z, STRETCH
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[0], 47.50f)), module, Georgia::TWIST_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[1], 47.50f)), module, Georgia::ROT_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[2], 47.50f)), module, Georgia::ROT_Y_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[3], 47.50f)), module, Georgia::ROT_Z_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[4], 47.50f)), module, Georgia::STRETCH_PARAM));

		// Zone 3: CV Attenuverter Trimpots (3 rows of 5 columns)
		// Row 1 (Center Y = 59.50 mm): IN, FREQ, FINE, SHAPE, MORPH
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[0], 59.50f)), module, Georgia::IN_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[1], 59.50f)), module, Georgia::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[2], 59.50f)), module, Georgia::FINE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[3], 59.50f)), module, Georgia::SHAPE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[4], 59.50f)), module, Georgia::MORPH_TRIM_PARAM));

		// Row 2 (Center Y = 68.50 mm): DENSITY, DEPTH, WINDOW, SPREAD, AMP
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[0], 68.50f)), module, Georgia::DENSITY_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[1], 68.50f)), module, Georgia::DEPTH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[2], 68.50f)), module, Georgia::WINDOW_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[3], 68.50f)), module, Georgia::SPREAD_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[4], 68.50f)), module, Georgia::AMP_TRIM_PARAM));

		// Row 3 (Center Y = 77.50 mm): TWIST, ROT X, ROT Y, ROT Z, STRETCH
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[0], 77.50f)), module, Georgia::TWIST_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[1], 77.50f)), module, Georgia::ROT_X_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[2], 77.50f)), module, Georgia::ROT_Y_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[3], 77.50f)), module, Georgia::ROT_Z_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[4], 77.50f)), module, Georgia::STRETCH_TRIM_PARAM));

		// Zone 4: Jacks
		// Row 1 (Center Y = 89.50 mm): IN, FREQ, FINE, SHAPE, MORPH
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[0], 89.50f)), module, Georgia::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[1], 89.50f)), module, Georgia::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[2], 89.50f)), module, Georgia::FINE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[3], 89.50f)), module, Georgia::SHAPE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[4], 89.50f)), module, Georgia::MORPH_CV_INPUT));

		// Row 2 (Center Y = 99.00 mm): DENSITY, DEPTH, WINDOW, SPREAD, AMP
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[0], 99.00f)), module, Georgia::DENSITY_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[1], 99.00f)), module, Georgia::DEPTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[2], 99.00f)), module, Georgia::WINDOW_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[3], 99.00f)), module, Georgia::SPREAD_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[4], 99.00f)), module, Georgia::AMP_CV_INPUT));

		// Row 3 (Center Y = 108.50 mm): TWIST, ROT X, ROT Y, ROT Z, STRETCH
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[0], 108.50f)), module, Georgia::TWIST_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[1], 108.50f)), module, Georgia::ROT_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[2], 108.50f)), module, Georgia::ROT_Y_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[3], 108.50f)), module, Georgia::ROT_Z_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[4], 108.50f)), module, Georgia::STRETCH_CV_INPUT));

		// Row 4 (Fixed Center Y = 118.00 mm): SYNC IN, X, Y, SYNC OUT
		const float bottom_jacks[4] = {17.50f, 36.31f, 55.13f, 73.94f};
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(bottom_jacks[0], 118.00f)), module, Georgia::SYNC_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(bottom_jacks[1], 118.00f)), module, Georgia::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(bottom_jacks[2], 118.00f)), module, Georgia::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(bottom_jacks[3], 118.00f)), module, Georgia::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Georgia* georgia = dynamic_cast<Georgia*>(module);
		if (!georgia) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Frequency range time-scale"));

		const char* rangeLabels[] = {
			"Very slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; i++) {
			Georgia::RangeMode mode = (Georgia::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return georgia->rangeMode == mode; },
				[=]() { georgia->rangeMode = mode; }
			));
		}
	}
};

Model* modelGeorgia = createModel<Georgia, GeorgiaWidget>("Georgia");
