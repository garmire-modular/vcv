#include "plugin.hpp"
#include <cmath>
#include <algorithm>

struct Ants : Module {
	enum ParamId {
		COUNT_PARAM,
		LENGTH_PARAM,
		POSITION_PARAM,
		VARIETY_PARAM,
		COUNT_TRIM_PARAM,
		LENGTH_TRIM_PARAM,
		POSITION_TRIM_PARAM,
		VARIETY_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		COUNT_CV_INPUT,
		LENGTH_CV_INPUT,
		POSITION_CV_INPUT,
		VARIETY_CV_INPUT,
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

	Ants() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Controls (Count snaps to integer steps 1..64)
		configParam(COUNT_PARAM, 1.f, 64.f, 1.f, "Dash Count", " segments", 0.f, 1.f, 0.f);
		paramQuantities[COUNT_PARAM]->snapEnabled = true;

		configParam(LENGTH_PARAM, 0.f, 1.f, 0.5f, "Master Dash Length", "%", 0.f, 100.f);
		configParam(POSITION_PARAM, 0.f, 1.f, 0.f, "Dash Position Offset", " turns", 0.f, 1.f);
		configParam(VARIETY_PARAM, 0.f, 1.f, 0.f, "Length Variety Dispersion", "%", 0.f, 100.f);

		// Attenuverters (default 0%)
		configParam(COUNT_TRIM_PARAM, -1.f, 1.f, 0.f, "Count CV depth", "%", 0.f, 100.f);
		configParam(LENGTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Length CV depth", "%", 0.f, 100.f);
		configParam(POSITION_TRIM_PARAM, -1.f, 1.f, 0.f, "Position CV depth", "%", 0.f, 100.f);
		configParam(VARIETY_TRIM_PARAM, -1.f, 1.f, 0.f, "Variety CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X Signal");
		configInput(Y_INPUT, "Y Signal (Normalizes from X)");
		configInput(COUNT_CV_INPUT, "Count CV");
		configInput(LENGTH_CV_INPUT, "Length CV");
		configInput(POSITION_CV_INPUT, "Position CV");
		configInput(VARIETY_CV_INPUT, "Variety CV");

		// Outputs
		configOutput(X_OUTPUT, "X Signal");
		configOutput(Y_OUTPUT, "Y Signal");
	}

	float smoothstep(float edge0, float edge1, float x) {
		float t = clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
		return t * t * (3.f - 2.f * t);
	}

	// Deterministic pseudo-random hash for per-segment length variance
	float hashSegment(int k) {
		uint32_t x = (uint32_t)(k * 0x45d9f3b + 0x12345);
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = (x >> 16) ^ x;
		return (float)(x & 0x00ffffff) / (float)0x00ffffff * 2.f - 1.f; // -1.0 to +1.0
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int countCvChannels = inputs[COUNT_CV_INPUT].getChannels();
		int lengthCvChannels = inputs[LENGTH_CV_INPUT].getChannels();
		int posCvChannels = inputs[POSITION_CV_INPUT].getChannels();
		int varCvChannels = inputs[VARIETY_CV_INPUT].getChannels();

		int numChannels = std::max({xChannels, yChannels, countCvChannels, lengthCvChannels, posCvChannels, varCvChannels, 1});

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float countParam = params[COUNT_PARAM].getValue();
		float lengthParam = params[LENGTH_PARAM].getValue();
		float posParam = params[POSITION_PARAM].getValue();
		float varParam = params[VARIETY_PARAM].getValue();

		float countTrim = params[COUNT_TRIM_PARAM].getValue();
		float lengthTrim = params[LENGTH_TRIM_PARAM].getValue();
		float posTrim = params[POSITION_TRIM_PARAM].getValue();
		float varTrim = params[VARIETY_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float countCV = inputs[COUNT_CV_INPUT].getPolyVoltage(c) / 5.f;
			float lengthCV = inputs[LENGTH_CV_INPUT].getPolyVoltage(c) / 5.f;
			float posCV = inputs[POSITION_CV_INPUT].getPolyVoltage(c) / 5.f;
			float varCV = inputs[VARIETY_CV_INPUT].getPolyVoltage(c) / 5.f;

			// Snap Count to integer values (1 to 64)
			float rawN = countParam + countCV * countTrim * 32.f;
			int N = (int)clamp(std::round(rawN), 1.f, 64.f);

			// At Count = 1: 100% dry pass-through!
			if (N <= 1) {
				outputs[X_OUTPUT].setVoltage(inX, c);
				outputs[Y_OUTPUT].setVoltage(inY, c);
				continue;
			}

			float masterL = clamp(lengthParam + lengthCV * lengthTrim, 0.f, 1.f);
			float P = posParam + posCV * posTrim;
			float V = clamp(varParam + varCV * varTrim, 0.f, 1.f);

			// Polar trajectory phase
			float theta = std::atan2(inY, inX);
			float phase = (theta + (float)M_PI) / (2.f * (float)M_PI);

			// Dashed segment index & local phase
			float scaledPhase = std::fmod((float)N * phase + P, (float)N);
			if (scaledPhase < 0.f)
				scaledPhase += (float)N;

			int k = (int)std::floor(scaledPhase);
			float dashPhase = scaledPhase - (float)k;

			// Per-segment length with Variety variance
			float variance = hashSegment(k) * V * 0.5f;
			float segL = clamp(masterL + variance, 0.f, 1.f);

			// Anti-aliased smoothstep laser blanking window
			float slew = 0.05f * std::max(0.1f, segL);
			float gain = 1.f;
			if (segL <= 0.001f) {
				gain = 0.f;
			} else if (segL >= 0.999f) {
				gain = 1.f;
			} else {
				float fadeIn = smoothstep(0.f, slew, dashPhase);
				float fadeOut = smoothstep(segL, std::max(0.f, segL - slew), dashPhase);
				gain = fadeIn * fadeOut;
			}

			float outX = inX * gain;
			float outY = inY * gain;

			// Laser safety voltage bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.f, 12.f), c);
		}
	}
};

struct AntsWidget : ModuleWidget {
	AntsWidget(Ants* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ants.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: COUNT (7.62 mm) & LENGTH (22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Ants::COUNT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Ants::LENGTH_PARAM));

		// Row 2: POSITION (7.62 mm) & VARIETY (22.86 mm) Knobs (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Ants::POSITION_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Ants::VARIETY_PARAM));

		// Attenuverter Trimpots
		// Trimpot Row 1: COUNT (7.62 mm) & LENGTH (22.86 mm) (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Ants::COUNT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Ants::LENGTH_TRIM_PARAM));

		// Trimpot Row 2: POSITION (7.62 mm) & VARIETY (22.86 mm) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Ants::POSITION_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Ants::VARIETY_TRIM_PARAM));

		// Bottom I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Ants::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Ants::Y_INPUT));

		// Row 2: COUNT CV (7.62 mm), LENGTH CV (22.86 mm) (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Ants::COUNT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Ants::LENGTH_CV_INPUT));

		// Row 3: POSITION CV (7.62 mm), VARIETY CV (22.86 mm) (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Ants::POSITION_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Ants::VARIETY_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Ants::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Ants::Y_OUTPUT));
	}
};

Model* modelAnts = createModel<Ants, AntsWidget>("Ants");
