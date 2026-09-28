#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi ? hi : v);
}

enum WarpMode {
	WARP_BIFURCATION = 0,
	WARP_WAVEFOLD = 1,
	WARP_SKEW = 2,
	WARP_MODES_LEN
};

struct ChannelState {
	float prevInX = 0.f;
	int samplesSinceZeroCross = 0;
	float currentPeriod = 367.f;
	float smoothedPeriod = 367.f;
	float subPhase = 0.f;
	int cycleIndex = 0;
	float internalPhase = 0.f;

	void reset() {
		prevInX = 0.f;
		samplesSinceZeroCross = 0;
		currentPeriod = 367.f;
		smoothedPeriod = 367.f;
		subPhase = 0.f;
		cycleIndex = 0;
		internalPhase = 0.f;
	}
};

struct Maude : Module {
	enum ParamId {
		DIV_PARAM,
		RATIO_PARAM,
		DEPTH_PARAM,
		WARP_PARAM,

		DIV_TRIM_PARAM,
		RATIO_TRIM_PARAM,
		DEPTH_TRIM_PARAM,
		WARP_TRIM_PARAM,

		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		DIV_CV_INPUT,
		RATIO_CV_INPUT,
		DEPTH_CV_INPUT,
		WARP_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		X_OUTPUT,
		Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	WarpMode warpMode = WARP_BIFURCATION;
	ChannelState channels[16];

	struct IntegerParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int val = (int)std::round(getValue());
			return std::to_string(val);
		}
		void setDisplayValueString(std::string s) override {
			char* endPtr = nullptr;
			float val = std::strtof(s.c_str(), &endPtr);
			if (endPtr != s.c_str()) {
				setValue(clampf(std::round(val), getMinValue(), getMaxValue()));
			}
		}
	};

	struct WarpParamQuantity : ParamQuantity {
		Maude* getMaude() {
			return dynamic_cast<Maude*>(module);
		}

		std::string getLabel() override {
			Maude* m = getMaude();
			if (!m) return "Warp";
			switch (m->warpMode) {
				case WARP_BIFURCATION: return "Bifurcation";
				case WARP_WAVEFOLD: return "Wavefold";
				case WARP_SKEW: return "Quadrature Skew";
				default: return "Warp";
			}
		}

		std::string getDisplayValueString() override {
			float val = getValue() * 100.f;
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", val);
			return std::string(buf);
		}

		std::string getUnit() override {
			return "%";
		}
	};

	Maude() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// DIV Param (1 to 12 integer subharmonic divisions, default 4)
		configParam<IntegerParamQuantity>(DIV_PARAM, 1.f, 12.f, 4.f, "Subharmonic Divisions", "");
		paramQuantities[DIV_PARAM]->snapEnabled = true;

		// RATIO Param (1 to 12 ring modulation harmonic ratio multiplier, default 1)
		configParam<IntegerParamQuantity>(RATIO_PARAM, 1.f, 12.f, 1.f, "Ring Mod Ratio", "");
		paramQuantities[RATIO_PARAM]->snapEnabled = true;

		// DEPTH Param (0% to 100% wet/dry crossfade, default 100%)
		configParam(DEPTH_PARAM, 0.f, 1.f, 1.f, "Modulation Depth", "%", 0.f, 100.f);

		// WARP Param (0% to 100%, dynamic label per active mode)
		configParam<WarpParamQuantity>(WARP_PARAM, 0.f, 1.f, 0.f, "Warp", "%", 0.f, 100.f);

		// Attenuverter Trimpots (Strict adherence to AGENTS.md 6.5.4)
		configParam(DIV_TRIM_PARAM, -1.f, 1.f, 0.f, "Divisions CV depth", "%", 0.f, 100.f);
		configParam(RATIO_TRIM_PARAM, -1.f, 1.f, 0.f, "Ratio CV depth", "%", 0.f, 100.f);
		configParam(DEPTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Depth CV depth", "%", 0.f, 100.f);
		configParam(WARP_TRIM_PARAM, -1.f, 1.f, 0.f, "Warp CV depth", "%", 0.f, 100.f);

		// Port Labels
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(DIV_CV_INPUT, "Divisions CV");
		configInput(RATIO_CV_INPUT, "Ratio CV");
		configInput(DEPTH_CV_INPUT, "Depth CV");
		configInput(WARP_CV_INPUT, "Warp CV");

		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");

		for (int c = 0; c < 16; c++) {
			channels[c].reset();
		}
	}

	void onReset() override {
		for (int c = 0; c < 16; c++) {
			channels[c].reset();
		}
	}

	void process(const ProcessArgs& args) override {
		int xCh = inputs[X_INPUT].getChannels();
		int yCh = inputs[Y_INPUT].getChannels();
		int divCvCh = inputs[DIV_CV_INPUT].getChannels();
		int ratCvCh = inputs[RATIO_CV_INPUT].getChannels();
		int depCvCh = inputs[DEPTH_CV_INPUT].getChannels();
		int warpCvCh = inputs[WARP_CV_INPUT].getChannels();

		bool xConnected = inputs[X_INPUT].isConnected();
		bool yConnected = inputs[Y_INPUT].isConnected();

		int numChannels = std::max({xCh, yCh, divCvCh, ratCvCh, depCvCh, warpCvCh, 1});
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float divParam = params[DIV_PARAM].getValue();
		float ratioParam = params[RATIO_PARAM].getValue();
		float depthParam = params[DEPTH_PARAM].getValue();
		float warpParam = params[WARP_PARAM].getValue();

		float divTrim = params[DIV_TRIM_PARAM].getValue();
		float ratioTrim = params[RATIO_TRIM_PARAM].getValue();
		float depthTrim = params[DEPTH_TRIM_PARAM].getValue();
		float warpTrim = params[WARP_TRIM_PARAM].getValue();

		// Reference oscillator for unpatched normalled state (130.81278 Hz = C3)
		const float refFreq = 130.81278f;
		const float refDeltaPhase = refFreq * args.sampleTime;

		for (int c = 0; c < numChannels; c++) {
			auto& chan = channels[c];

			// CV modulation inputs
			float divCv = inputs[DIV_CV_INPUT].getPolyVoltage(c) / 5.f;
			float ratCv = inputs[RATIO_CV_INPUT].getPolyVoltage(c) / 5.f;
			float depCv = inputs[DEPTH_CV_INPUT].getPolyVoltage(c) / 5.f;
			float warpCv = inputs[WARP_CV_INPUT].getPolyVoltage(c) / 5.f;

			int numDiv = (int)clampf(std::round(divParam + divCv * divTrim * 11.f), 1.f, 12.f);
			int ratio = (int)clampf(std::round(ratioParam + ratCv * ratioTrim * 11.f), 1.f, 12.f);
			float depth = clampf(depthParam + depCv * depthTrim, 0.f, 1.f);
			float warp = clampf(warpParam + warpCv * warpTrim, 0.f, 1.f);

			// Period doubling base span: 2 * numDiv input cycles ensures continuous subharmonic undertones
			int totalCycles = 2 * numDiv;

			float inX = 0.f;
			float inY = 0.f;

			if (!xConnected && !yConnected) {
				// Normalled internal quadrature reference circle
				chan.internalPhase += refDeltaPhase;
				if (chan.internalPhase >= 1.f) {
					chan.internalPhase -= 1.f;
				}
				float angle = 2.f * (float)M_PI * chan.internalPhase;
				inX = 5.f * std::cos(angle);
				inY = 5.f * std::sin(angle);

				// Advance subharmonic phase locked to internal reference
				chan.subPhase += refDeltaPhase / (float)totalCycles;
				if (chan.subPhase >= 1.f) {
					chan.subPhase -= 1.f;
				}
			} else {
				inX = inputs[X_INPUT].getPolyVoltage(c);
				inY = yConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

				// Cycle detector with hysteresis Schmitt trigger (+/-0.02V)
				if (chan.prevInX <= -0.02f && inX > 0.02f && chan.samplesSinceZeroCross > 10) {
					chan.currentPeriod = (float)chan.samplesSinceZeroCross;
					// Exponential smoothing of tracked fundamental period
					chan.smoothedPeriod += 0.1f * (chan.currentPeriod - chan.smoothedPeriod);
					chan.samplesSinceZeroCross = 0;
					chan.cycleIndex = (chan.cycleIndex + 1) % totalCycles;

					// Soft phase alignment towards cycle boundary
					float targetSubPhase = (float)chan.cycleIndex / (float)totalCycles;
					float phaseDiff = targetSubPhase - chan.subPhase;
					while (phaseDiff > 0.5f) phaseDiff -= 1.0f;
					while (phaseDiff < -0.5f) phaseDiff += 1.0f;
					chan.subPhase += 0.25f * phaseDiff;
				}
				chan.prevInX = inX;
				chan.samplesSinceZeroCross = std::min(chan.samplesSinceZeroCross + 1, 480000);

				float clampedPeriod = clampf(chan.smoothedPeriod, 8.f, 480000.f);
				float dPhase = 1.0f / (clampedPeriod * (float)totalCycles);
				chan.subPhase += dPhase;
				if (chan.subPhase >= 1.0f) chan.subPhase -= 1.0f;
				if (chan.subPhase < 0.0f) chan.subPhase += 1.0f;
			}

			// Subharmonic angle over the ratio multiplier
			float theta = 4.f * (float)M_PI * chan.subPhase * (float)ratio;
			float modX = 0.f;
			float modY = 0.f;

			switch (warpMode) {
				case WARP_BIFURCATION: {
					// Subharmonic period doubling cascade (sub-octave undertone)
					float baseMod = std::cos(theta);
					float subOctave = std::cos(0.5f * theta);
					float combined = (1.0f - warp) * baseMod + warp * subOctave;
					modX = combined;
					modY = combined;
					break;
				}
				case WARP_WAVEFOLD: {
					// Multi-tier rosette wavefolding into concentric layers
					float drive = 1.0f + 2.5f * warp;
					float folded = std::sin(drive * std::cos(theta) * (float)(M_PI * 0.5));
					modX = folded;
					modY = folded;
					break;
				}
				case WARP_SKEW: {
					// Quadrature phase skew twisting rosettes into spinning pinwheels
					float skewAngle = warp * (float)M_PI;
					modX = std::cos(theta);
					modY = std::cos(theta + skewAngle);
					break;
				}
				default: {
					modX = std::cos(theta);
					modY = std::cos(theta);
					break;
				}
			}

			// Bipolar ring modulation wet/dry crossfade
			float wetX = inX * modX;
			float wetY = inY * modY;

			float outX = (1.0f - depth) * inX + depth * wetX;
			float outY = (1.0f - depth) * inY + depth * wetY;

			// Galvo-safe signal output clamping
			outputs[X_OUTPUT].setVoltage(clampf(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(outY, -12.f, 12.f), c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "warpMode", json_integer((int)warpMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* wmJ = json_object_get(rootJ, "warpMode");
		if (wmJ) {
			warpMode = (WarpMode)json_integer_value(wmJ);
		}
	}
};

struct MaudeWidget : ModuleWidget {
	MaudeWidget(Maude* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Maude.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Zone 1 & 2: Main Parameter Knobs
		// Row 1: DIV (X = 7.62 mm) & RATIO (X = 22.86 mm) (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Maude::DIV_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Maude::RATIO_PARAM));

		// Row 2: DEPTH (X = 7.62 mm) & WARP (X = 22.86 mm) (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Maude::DEPTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Maude::WARP_PARAM));

		// Zone 3: CV Attenuverter Trimpots
		// Row 1 Trimpots: DIV (7.62) & RATIO (22.86) (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Maude::DIV_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Maude::RATIO_TRIM_PARAM));

		// Row 2 Trimpots: DEPTH (7.62) & WARP (22.86) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Maude::DEPTH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Maude::WARP_TRIM_PARAM));

		// Zone 4: Bottom I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Maude::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Maude::Y_INPUT));

		// Row 2: DIV CV (X = 7.62 mm), RATIO CV (X = 22.86 mm) (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Maude::DIV_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Maude::RATIO_CV_INPUT));

		// Row 3: DEPTH CV (X = 7.62 mm), WARP CV (X = 22.86 mm) (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Maude::DEPTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Maude::WARP_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Maude::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Maude::Y_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Maude* module = dynamic_cast<Maude*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Warp Parameter Mode"));

		const char* modeLabels[] = {
			"Bifurcation (Subharmonic Period Doubling)",
			"Wavefold (Multi-Tier Concentric Rosettes)",
			"Quadrature Skew (Pinwheel Twist & Spiral)"
		};

		for (int i = 0; i < WARP_MODES_LEN; i++) {
			WarpMode m = (WarpMode)i;
			menu->addChild(createCheckMenuItem(modeLabels[i], "",
				[=]() { return module->warpMode == m; },
				[=]() { module->warpMode = m; }
			));
		}
	}
};

Model* modelMaude = createModel<Maude, MaudeWidget>("Maude");
