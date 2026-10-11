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
//  (Sphere/Torus, Cube/Cylinder, Pyramid/Cone) with perspective camera
//  depth, 3D Euler rotations, Polly logarithmic stretch, and dual-channel
//  audio waveform displacement.
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

		// Row 2 Knobs: DENSITY, DEPTH, WINDOW, ROUTING, AMP
		DENSITY_PARAM,
		DEPTH_PARAM,
		WINDOW_PARAM,
		ROUTING_PARAM,
		AMP_PARAM,

		// Row 3 Knobs: FOV, ROT X, ROT Y, PHASE, STRETCH
		FOV_PARAM,
		ROT_X_PARAM,
		ROT_Y_PARAM,
		PHASE_PARAM,
		STRETCH_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 1
		// LEFT IN, RIGHT IN, FREQ, SHAPE, MORPH
		L_TRIM_PARAM,
		R_TRIM_PARAM,
		FREQ_TRIM_PARAM,
		SHAPE_TRIM_PARAM,
		MORPH_TRIM_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 2
		// DENSITY, DEPTH, WINDOW, ROUTING, AMP
		DENSITY_TRIM_PARAM,
		DEPTH_TRIM_PARAM,
		WINDOW_TRIM_PARAM,
		ROUTING_TRIM_PARAM,
		AMP_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Jack Row 1: LEFT IN, RIGHT IN, FREQ, SHAPE, MORPH
		L_INPUT,
		R_INPUT,
		FREQ_CV_INPUT,
		SHAPE_CV_INPUT,
		MORPH_CV_INPUT,

		// Jack Row 2: DENSITY, DEPTH, WINDOW, ROUTING, AMP
		DENSITY_CV_INPUT,
		DEPTH_CV_INPUT,
		WINDOW_CV_INPUT,
		ROUTING_CV_INPUT,
		AMP_CV_INPUT,

		// Jack Row 3: FOV, ROT X, ROT Y, PHASE, STRETCH
		FOV_CV_INPUT,
		ROT_X_CV_INPUT,
		ROT_Y_CV_INPUT,
		PHASE_CV_INPUT,
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
			float val = getValue();
			char buf[64];
			if (val < 0.5f) {
				std::snprintf(buf, sizeof(buf), "Sphere / Torus (%.2f)", val);
			} else if (val < 1.5f) {
				std::snprintf(buf, sizeof(buf), "Cube / Cylinder (%.2f)", val);
			} else {
				std::snprintf(buf, sizeof(buf), "Pyramid / Cone (%.2f)", val);
			}
			return std::string(buf);
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
		std::string getUnit() override {
			return " ms";
		}
	};

	struct RoutingParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			int avgPct = (int)std::round((1.0f - v) * 100.0f);
			int sepPct = 100 - avgPct;
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%d%% L+R Average : %d%% L R Separate", avgPct, sepPct);
			return std::string(buf);
		}
	};

	struct AngleParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			return getValue() * 180.0f;
		}
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", getDisplayValue());
			return std::string(buf);
		}
		std::string getUnit() override {
			return "°";
		}
	};

	struct PhaseParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			return getValue() * 360.0f;
		}
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", getDisplayValue());
			return std::string(buf);
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
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine Tune", "%", 0.f, 100.f);
		configParam<ShapeParamQuantity>(SHAPE_PARAM, 0.f, 2.f, 0.f, "Shape", "");
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph", "%", 0.f, 100.f);

		// Knobs: Row 2
		// DENSITY, DEPTH, WINDOW, ROUTING, AMP
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density", "%", 0.f, 100.f);
		configParam(DEPTH_PARAM, 0.f, 1.f, 0.7f, "Depth", "%", 0.f, 100.f);
		configParam<WindowParamQuantity>(WINDOW_PARAM, 0.f, 1.f, 0.5f, "Buffer Window", " ms");
		configParam<RoutingParamQuantity>(ROUTING_PARAM, 0.f, 1.f, 0.f, "Routing", "");
		configParam(AMP_PARAM, 0.f, 2.f, 1.f, "Amplitude", "%", 0.f, 100.f);

		// Knobs: Row 3
		// FOV, ROT X, ROT Y, PHASE, STRETCH
		configParam(FOV_PARAM, 0.f, 1.f, 0.5f, "Field of view", "%", 0.f, 100.f);
		configParam<AngleParamQuantity>(ROT_X_PARAM, -1.f, 1.f, 0.15f, "X rotation", "°");
		configParam<AngleParamQuantity>(ROT_Y_PARAM, -1.f, 1.f, 0.25f, "Y rotation", "°");
		configParam<PhaseParamQuantity>(PHASE_PARAM, 0.f, 1.f, 0.f, "Phase", "°");
		configParam(STRETCH_PARAM, -1.f, 1.f, 0.f, "Stretch", "%", 0.f, 100.f);

		// Zone 3 Attenuverter Trimpots: Row 1
		// LEFT IN, RIGHT IN, FREQ, SHAPE, MORPH
		configParam(L_TRIM_PARAM, -1.f, 1.f, 1.f, "Left input level", "%", 0.f, 100.f);
		configParam(R_TRIM_PARAM, -1.f, 1.f, 1.f, "Right input level", "%", 0.f, 100.f);
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(SHAPE_TRIM_PARAM, -1.f, 1.f, 0.f, "Shape CV depth", "%", 0.f, 100.f);
		configParam(MORPH_TRIM_PARAM, -1.f, 1.f, 0.f, "Morph CV depth", "%", 0.f, 100.f);

		// Zone 3 Attenuverter Trimpots: Row 2
		// DENSITY, DEPTH, WINDOW, ROUTING, AMP
		configParam(DENSITY_TRIM_PARAM, -1.f, 1.f, 0.f, "Density CV depth", "%", 0.f, 100.f);
		configParam(DEPTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Depth CV depth", "%", 0.f, 100.f);
		configParam(WINDOW_TRIM_PARAM, -1.f, 1.f, 0.f, "Window CV depth", "%", 0.f, 100.f);
		configParam(ROUTING_TRIM_PARAM, -1.f, 1.f, 0.f, "Routing CV depth", "%", 0.f, 100.f);
		configParam(AMP_TRIM_PARAM, -1.f, 1.f, 0.f, "Amplitude CV depth", "%", 0.f, 100.f);

		// Jacks: Row 1
		// LEFT IN, RIGHT IN, FREQ, SHAPE, MORPH
		configInput(L_INPUT, "Left audio input");
		configInput(R_INPUT, "Right audio input");
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(SHAPE_CV_INPUT, "Shape CV");
		configInput(MORPH_CV_INPUT, "Morph CV");

		// Jacks: Row 2
		// DENSITY, DEPTH, WINDOW, ROUTING, AMP
		configInput(DENSITY_CV_INPUT, "Density CV");
		configInput(DEPTH_CV_INPUT, "Depth CV");
		configInput(WINDOW_CV_INPUT, "Window CV");
		configInput(ROUTING_CV_INPUT, "Routing CV");
		configInput(AMP_CV_INPUT, "Amplitude CV");

		// Jacks: Row 3
		// FOV, ROT X, ROT Y, PHASE, STRETCH
		configInput(FOV_CV_INPUT, "Field of view CV");
		configInput(ROT_X_CV_INPUT, "X rotation CV");
		configInput(ROT_Y_CV_INPUT, "Y rotation CV");
		configInput(PHASE_CV_INPUT, "Phase CV");
		configInput(STRETCH_CV_INPUT, "Stretch CV");

		// Jacks: Row 4
		// SYNC IN, X, Y, SYNC OUT
		configInput(SYNC_INPUT, "Sync In");
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(SYNC_OUTPUT, "Sync Out");

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
		channels = std::max(channels, inputs[L_INPUT].getChannels());
		channels = std::max(channels, inputs[R_INPUT].getChannels());
		channels = std::max(channels, inputs[FREQ_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[SHAPE_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[MORPH_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[DENSITY_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[DEPTH_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[WINDOW_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[ROUTING_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[AMP_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[FOV_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[ROT_X_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[ROT_Y_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[PHASE_CV_INPUT].getChannels());
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
		float routingVal = params[ROUTING_PARAM].getValue();
		float ampVal = params[AMP_PARAM].getValue();

		float fovVal = params[FOV_PARAM].getValue();
		float rotXVal = params[ROT_X_PARAM].getValue();
		float rotYVal = params[ROT_Y_PARAM].getValue();
		float phaseVal = params[PHASE_PARAM].getValue();
		float stretchVal = params[STRETCH_PARAM].getValue();

		float lTrim = params[L_TRIM_PARAM].getValue();
		float rTrim = params[R_TRIM_PARAM].getValue();
		float freqTrim = params[FREQ_TRIM_PARAM].getValue();
		float shapeTrim = params[SHAPE_TRIM_PARAM].getValue();
		float morphTrim = params[MORPH_TRIM_PARAM].getValue();

		float densityTrim = params[DENSITY_TRIM_PARAM].getValue();
		float depthTrim = params[DEPTH_TRIM_PARAM].getValue();
		float windowTrim = params[WINDOW_TRIM_PARAM].getValue();
		float routingTrim = params[ROUTING_TRIM_PARAM].getValue();
		float ampTrim = params[AMP_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float audioL = inputs[L_INPUT].getPolyVoltage(c) * lTrim;
			float audioR = inputs[R_INPUT].isConnected() ? (inputs[R_INPUT].getPolyVoltage(c) * rTrim) : audioL;

			float freqCV = inputs[FREQ_CV_INPUT].getPolyVoltage(c) * freqTrim;
			float shapeCV = inputs[SHAPE_CV_INPUT].getPolyVoltage(c) * shapeTrim;
			float morphCV = inputs[MORPH_CV_INPUT].getPolyVoltage(c) * morphTrim;

			float densityCV = inputs[DENSITY_CV_INPUT].getPolyVoltage(c) * densityTrim;
			float depthCV = inputs[DEPTH_CV_INPUT].getPolyVoltage(c) * depthTrim;
			float windowCV = inputs[WINDOW_CV_INPUT].getPolyVoltage(c) * windowTrim;
			float routingCV = inputs[ROUTING_CV_INPUT].getPolyVoltage(c) * routingTrim;
			float ampCV = inputs[AMP_CV_INPUT].getPolyVoltage(c) * ampTrim;

			float fovCV = inputs[FOV_CV_INPUT].getPolyVoltage(c);
			float rotXCV = inputs[ROT_X_CV_INPUT].getPolyVoltage(c);
			float rotYCV = inputs[ROT_Y_CV_INPUT].getPolyVoltage(c);
			float phaseCV = inputs[PHASE_CV_INPUT].getPolyVoltage(c);
			float stretchCV = inputs[STRETCH_CV_INPUT].getPolyVoltage(c);

			float syncIn = inputs[SYNC_INPUT].getPolyVoltage(c);

			float outX = 0.0f;
			float outY = 0.0f;
			float syncOut = 0.0f;

			voices[c].scopeMode = scopeMode;
			voices[c].process(
				args.sampleRate,
				coarseFreq, fineFreq, rangeMode, freqCV,
				shapeVal, shapeCV,
				morphVal, morphCV,
				densityVal, densityCV,
				depthVal, depthCV,
				windowVal, windowCV,
				routingVal, routingCV,
				ampVal, ampCV,
				fovVal, fovCV,
				rotXVal, rotXCV,
				rotYVal, rotYCV,
				phaseVal, phaseCV,
				stretchVal, stretchCV,
				audioL, audioR,
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

		// Row 2 Knobs (Center Y = 36.50 mm):
		// DENSITY, DEPTH, WINDOW, ROUTING, AMP
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[0], 36.50f)), module, Georgia::DENSITY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[1], 36.50f)), module, Georgia::DEPTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[2], 36.50f)), module, Georgia::WINDOW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[3], 36.50f)), module, Georgia::ROUTING_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[4], 36.50f)), module, Georgia::AMP_PARAM));

		// Row 3 Knobs (Center Y = 51.50 mm):
		// FOV, ROT X, ROT Y, PHASE, STRETCH
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[0], 51.50f)), module, Georgia::FOV_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[1], 51.50f)), module, Georgia::ROT_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[2], 51.50f)), module, Georgia::ROT_Y_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[3], 51.50f)), module, Georgia::PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cols[4], 51.50f)), module, Georgia::STRETCH_PARAM));

		// Zone 3: CV Attenuverter Trimpots (5 columns)
		// Row 1 (Center Y = 66.00 mm): LEFT IN, RIGHT IN, FREQ, SHAPE, MORPH
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[0], 66.00f)), module, Georgia::L_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[1], 66.00f)), module, Georgia::R_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[2], 66.00f)), module, Georgia::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[3], 66.00f)), module, Georgia::SHAPE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[4], 66.00f)), module, Georgia::MORPH_TRIM_PARAM));

		// Row 2 (Center Y = 76.00 mm): DENSITY, DEPTH, WINDOW, ROUTING, AMP
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[0], 76.00f)), module, Georgia::DENSITY_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[1], 76.00f)), module, Georgia::DEPTH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[2], 76.00f)), module, Georgia::WINDOW_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[3], 76.00f)), module, Georgia::ROUTING_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(cols[4], 76.00f)), module, Georgia::AMP_TRIM_PARAM));

		// Zone 4: Jacks
		// Row 1 (Center Y = 89.50 mm): LEFT IN, RIGHT IN, FREQ, SHAPE, MORPH
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[0], 89.50f)), module, Georgia::L_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[1], 89.50f)), module, Georgia::R_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[2], 89.50f)), module, Georgia::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[3], 89.50f)), module, Georgia::SHAPE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[4], 89.50f)), module, Georgia::MORPH_CV_INPUT));

		// Row 2 (Center Y = 99.00 mm): DENSITY, DEPTH, WINDOW, ROUTING, AMP
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[0], 99.00f)), module, Georgia::DENSITY_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[1], 99.00f)), module, Georgia::DEPTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[2], 99.00f)), module, Georgia::WINDOW_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[3], 99.00f)), module, Georgia::ROUTING_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[4], 99.00f)), module, Georgia::AMP_CV_INPUT));

		// Row 3 (Center Y = 108.50 mm): FOV, ROT X, ROT Y, PHASE, STRETCH
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[0], 108.50f)), module, Georgia::FOV_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[1], 108.50f)), module, Georgia::ROT_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[2], 108.50f)), module, Georgia::ROT_Y_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(cols[3], 108.50f)), module, Georgia::PHASE_CV_INPUT));
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

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscilloscope 3D Mapping"));

		const char* scopeLabels[] = {
			"Surface normal displacement (3D hull)",
			"Equatorial scope belt",
			"Z-depth relief extrusion"
		};

		for (int i = 0; i < 3; i++) {
			Georgia::ScopeMode mode = (Georgia::ScopeMode)i;
			menu->addChild(createCheckMenuItem(scopeLabels[i], "",
				[=]() { return georgia->scopeMode == mode; },
				[=]() { georgia->scopeMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Shape Family Presets"));

		const char* shapeLabels[] = {
			"Sphere / Torus (Family 1)",
			"Cube / Cylinder (Family 2)",
			"Pyramid / Cone (Family 3)"
		};

		for (int i = 0; i < 3; i++) {
			float targetVal = (float)i;
			menu->addChild(createCheckMenuItem(shapeLabels[i], "",
				[=]() { return std::round(georgia->params[Georgia::SHAPE_PARAM].getValue()) == targetVal; },
				[=]() { georgia->params[Georgia::SHAPE_PARAM].setValue(targetVal); }
			));
		}
	}
};

Model* modelGeorgia = createModel<Georgia, GeorgiaWidget>("Georgia");
