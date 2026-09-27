#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Chop XL — 6-Channel Alternating Segment Multiplexer & Shape Slicer
//  12HP module modulating 2 pairs of XYRGBI inputs into alternating
//  segments around the trajectory, alternating between Input 1 and
//  Input 2 across the shape.
// ─────────────────────────────────────────────────────────────────────

struct ChopXLModule : Module {
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
		// CV Inputs
		COUNT_CV_INPUT,
		LENGTH_CV_INPUT,
		POSITION_CV_INPUT,
		VARIETY_CV_INPUT,
		// Row 1: Input 1 (X, Y, R, G, B, I)
		X1_INPUT,
		Y1_INPUT,
		R1_INPUT,
		G1_INPUT,
		B1_INPUT,
		I1_INPUT,
		// Row 2: Input 2 (X, Y, R, G, B, I)
		X2_INPUT,
		Y2_INPUT,
		R2_INPUT,
		G2_INPUT,
		B2_INPUT,
		I2_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Row 3: Output (X, Y, R, G, B, I)
		X_OUTPUT,
		Y_OUTPUT,
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		I_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	ChopXLModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Knobs (Count snaps to integer steps 1..64)
		configParam(COUNT_PARAM, 1.f, 64.f, 4.f, "Chop Segment Count", " segments", 0.f, 1.f, 0.f);
		paramQuantities[COUNT_PARAM]->snapEnabled = true;

		configParam(LENGTH_PARAM, 0.f, 1.f, 0.5f, "Segment Duty Cycle (Input 1 Length)", "%", 0.f, 100.f);
		configParam(POSITION_PARAM, 0.f, 1.f, 0.f, "Position Offset / Phase", " turns", 0.f, 1.f);
		configParam(VARIETY_PARAM, 0.f, 1.f, 0.f, "Length Variety Dispersion", "%", 0.f, 100.f);

		// Attenuverters
		configParam(COUNT_TRIM_PARAM, -1.f, 1.f, 0.f, "Count CV Attenuverter", "%", 0.f, 100.f);
		configParam(LENGTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Length CV Attenuverter", "%", 0.f, 100.f);
		configParam(POSITION_TRIM_PARAM, -1.f, 1.f, 0.f, "Position CV Attenuverter", "%", 0.f, 100.f);
		configParam(VARIETY_TRIM_PARAM, -1.f, 1.f, 0.f, "Variety CV Attenuverter", "%", 0.f, 100.f);

		// CV Inputs
		configInput(COUNT_CV_INPUT, "Count CV");
		configInput(LENGTH_CV_INPUT, "Length CV");
		configInput(POSITION_CV_INPUT, "Position CV");
		configInput(VARIETY_CV_INPUT, "Variety CV");

		// Row 1: Input 1
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(R1_INPUT, "R 1");
		configInput(G1_INPUT, "G 1");
		configInput(B1_INPUT, "B 1");
		configInput(I1_INPUT, "I 1");

		// Row 2: Input 2
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");
		configInput(R2_INPUT, "R 2");
		configInput(G2_INPUT, "G 2");
		configInput(B2_INPUT, "B 2");
		configInput(I2_INPUT, "I 2");

		// Row 3: Output
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(R_OUTPUT, "R");
		configOutput(G_OUTPUT, "G");
		configOutput(B_OUTPUT, "B");
		configOutput(I_OUTPUT, "I");
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
		int channels = 1;
		for (int i = 0; i < INPUTS_LEN; i++) {
			channels = std::max(channels, inputs[i].getChannels());
		}

		for (int o = 0; o < OUTPUTS_LEN; o++) {
			outputs[o].setChannels(channels);
		}

		float countParam = params[COUNT_PARAM].getValue();
		float lengthParam = params[LENGTH_PARAM].getValue();
		float posParam = params[POSITION_PARAM].getValue();
		float varParam = params[VARIETY_PARAM].getValue();

		float countTrim = params[COUNT_TRIM_PARAM].getValue();
		float lengthTrim = params[LENGTH_TRIM_PARAM].getValue();
		float posTrim = params[POSITION_TRIM_PARAM].getValue();
		float varTrim = params[VARIETY_TRIM_PARAM].getValue();

		bool in1Connected = inputs[X1_INPUT].isConnected() || inputs[Y1_INPUT].isConnected();

		for (int c = 0; c < channels; c++) {
			float inX1 = inputs[X1_INPUT].getPolyVoltage(c);
			float inY1 = inputs[Y1_INPUT].getPolyVoltage(c);
			float inX2 = inputs[X2_INPUT].getPolyVoltage(c);
			float inY2 = inputs[Y2_INPUT].getPolyVoltage(c);

			// Track phase primarily from Input 1 trajectory, fallback to Input 2
			float trackX = in1Connected ? inX1 : inX2;
			float trackY = in1Connected ? inY1 : inY2;

			float countCV = inputs[COUNT_CV_INPUT].getPolyVoltage(c) / 5.f;
			float lengthCV = inputs[LENGTH_CV_INPUT].getPolyVoltage(c) / 5.f;
			float posCV = inputs[POSITION_CV_INPUT].getPolyVoltage(c) / 5.f;
			float varCV = inputs[VARIETY_CV_INPUT].getPolyVoltage(c) / 5.f;

			// Snap Count to integer values (1 to 64)
			float rawN = countParam + countCV * countTrim * 32.f;
			int N = (int)clamp(std::round(rawN), 1.f, 64.f);

			float masterL = clamp(lengthParam + lengthCV * lengthTrim, 0.f, 1.f);
			float P = posParam + posCV * posTrim;
			float V = clamp(varParam + varCV * varTrim, 0.f, 1.f);

			// Polar trajectory phase [0, 1)
			float theta = std::atan2(trackY, trackX);
			float phase = (theta + (float)M_PI) / (2.f * (float)M_PI);

			// Segment index & local normalized phase
			float scaledPhase = std::fmod((float)N * phase + P, (float)N);
			if (scaledPhase < 0.f)
				scaledPhase += (float)N;

			int k = (int)std::floor(scaledPhase);
			float dashPhase = scaledPhase - (float)k;

			// Per-segment length with variety variance
			float variance = hashSegment(k) * V * 0.5f;
			float segL = clamp(masterL + variance, 0.f, 1.f);

			// Anti-aliased transition between Input 1 and Input 2
			// w = 1.0 (Input 1 fully active), w = 0.0 (Input 2 fully active)
			float slew = 0.02f * std::max(0.1f, segL);
			float w = 1.f;
			if (segL <= 0.001f) {
				w = 0.f;
			} else if (segL >= 0.999f) {
				w = 1.f;
			} else {
				float fadeIn = smoothstep(0.f, slew, dashPhase);
				float fadeOut = smoothstep(segL, std::max(0.f, segL - slew), dashPhase);
				w = fadeIn * fadeOut;
			}

			// Alternate each channel between Input 1 and Input 2
			for (int kChan = 0; kChan < 6; kChan++) {
				float v1 = inputs[X1_INPUT + kChan].getPolyVoltage(c);
				float v2 = inputs[X2_INPUT + kChan].getPolyVoltage(c);
				float out = w * v1 + (1.f - w) * v2;
				outputs[X_OUTPUT + kChan].setVoltage(clamp(out, -12.f, 12.f), c);
			}
		}
	}
};

struct ChopXLWidget : ModuleWidget {
	ChopXLWidget(ChopXLModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ChopXL.svg")));

		// 12HP Screws (width 60.96 mm = 12 * RACK_GRID_WIDTH)
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 4 Control Columns (x = 7.62, 22.86, 38.10, 53.34 mm)
		const double ctrlX[4] = {7.62, 22.86, 38.10, 53.34};

		// Row 1: Knobs (center Y = 23.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ctrlX[0], 23.50)), module, ChopXLModule::COUNT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ctrlX[1], 23.50)), module, ChopXLModule::LENGTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ctrlX[2], 23.50)), module, ChopXLModule::POSITION_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(ctrlX[3], 23.50)), module, ChopXLModule::VARIETY_PARAM));

		// Row 2: Attenuverters (center Y = 45.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrlX[0], 45.50)), module, ChopXLModule::COUNT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrlX[1], 45.50)), module, ChopXLModule::LENGTH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrlX[2], 45.50)), module, ChopXLModule::POSITION_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(ctrlX[3], 45.50)), module, ChopXLModule::VARIETY_TRIM_PARAM));

		// Row 3: CV Inputs (center Y = 63.00 mm, aligns with XORXY Row 4)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(ctrlX[0], 63.00)), module, ChopXLModule::COUNT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(ctrlX[1], 63.00)), module, ChopXLModule::LENGTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(ctrlX[2], 63.00)), module, ChopXLModule::POSITION_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(ctrlX[3], 63.00)), module, ChopXLModule::VARIETY_CV_INPUT));

		// 6 Signal Columns (pitch = 9.50 mm, left margin = 6.73 mm)
		// x = 6.73, 16.23, 25.73, 35.23, 44.73, 54.23 mm
		const double colX[6] = {6.73, 16.23, 25.73, 35.23, 44.73, 54.23};

		// Row 4: Input 1 (center Y = 90.50 mm, aligns with XORXY Row 6)
		for (int k = 0; k < 6; k++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[k], 90.50)), module, ChopXLModule::X1_INPUT + k));
		}

		// Row 5: Input 2 (center Y = 104.50 mm, aligns with XORXY Row 7)
		for (int k = 0; k < 6; k++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[k], 104.50)), module, ChopXLModule::X2_INPUT + k));
		}

		// Row 6: Output (center Y = 118.00 mm, aligns with XORXY Row 8)
		for (int k = 0; k < 6; k++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX[k], 118.00)), module, ChopXLModule::X_OUTPUT + k));
		}
	}
};

Model* modelChopXL = createModel<ChopXLModule, ChopXLWidget>("ChopXL");
