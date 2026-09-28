#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Bleed — Dual X/Y Pair Cross-Bleed & Phase Feedback Recursion Engine
//  8 HP module processing two independent pairs of X/Y signals with
//  bidirectional per-channel bleed crosstalk and all-pass phase-delayed
//  recursive feedback loops.
// ─────────────────────────────────────────────────────────────────────

struct Bleed : Module {
	enum ParamId {
		X_BLEED_PARAM,
		Y_BLEED_PARAM,
		X_FEEDBACK_PARAM,
		Y_FEEDBACK_PARAM,
		X_PHASE_PARAM,
		Y_PHASE_PARAM,
		X_BLEED_TRIM_PARAM,
		Y_BLEED_TRIM_PARAM,
		X_FEEDBACK_TRIM_PARAM,
		Y_FEEDBACK_TRIM_PARAM,
		X_PHASE_TRIM_PARAM,
		Y_PHASE_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X1_INPUT,
		Y1_INPUT,
		X2_INPUT,
		Y2_INPUT,
		X_BLEED_CV_INPUT,
		Y_BLEED_CV_INPUT,
		X_FEEDBACK_CV_INPUT,
		Y_FEEDBACK_CV_INPUT,
		X_PHASE_CV_INPUT,
		Y_PHASE_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		X1_OUTPUT,
		Y1_OUTPUT,
		X2_OUTPUT,
		Y2_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	// First-order all-pass filter for frequency-dependent phase delay
	struct AllPassFilter {
		float x1 = 0.0f;
		float y1 = 0.0f;

		void reset() {
			x1 = 0.0f;
			y1 = 0.0f;
		}

		float process(float in, float fc, float sampleRate) {
			float wc = clamp(fc / sampleRate, 0.0001f, 0.49f);
			float tanW = std::tan((float)M_PI * wc);
			float a = (tanW - 1.0f) / (tanW + 1.0f);
			float out = a * in + x1 - a * y1;
			x1 = in;
			y1 = out;
			return out;
		}
	};

	AllPassFilter apX1[16];
	AllPassFilter apY1[16];
	AllPassFilter apX2[16];
	AllPassFilter apY2[16];

	float fbStateX1[16] = {0};
	float fbStateY1[16] = {0};
	float fbStateX2[16] = {0};
	float fbStateY2[16] = {0};

	Bleed() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Bipolar Bleed Knobs (-100% to +100%)
		configParam(X_BLEED_PARAM, -1.f, 1.f, 0.f, "X bleed", "%", 0.f, 100.f);
		configParam(Y_BLEED_PARAM, -1.f, 1.f, 0.f, "Y bleed", "%", 0.f, 100.f);

		// Row 2: Feedback Knobs (0% to 125%)
		configParam(X_FEEDBACK_PARAM, 0.f, 1.25f, 0.f, "X feedback", "%", 0.f, 100.f);
		configParam(Y_FEEDBACK_PARAM, 0.f, 1.25f, 0.f, "Y feedback", "%", 0.f, 100.f);

		// Row 3: Phase Knobs (0% to 100%)
		configParam(X_PHASE_PARAM, 0.f, 1.f, 0.5f, "X phase", "%", 0.f, 100.f);
		configParam(Y_PHASE_PARAM, 0.f, 1.f, 0.5f, "Y phase", "%", 0.f, 100.f);

		// Trimpots (CV Attenuverters) — AGENTS.md Standard Tooltip Naming
		configParam(X_BLEED_TRIM_PARAM, -1.f, 1.f, 0.f, "X bleed CV depth", "%", 0.f, 100.f);
		configParam(Y_BLEED_TRIM_PARAM, -1.f, 1.f, 0.f, "Y bleed CV depth", "%", 0.f, 100.f);
		configParam(X_FEEDBACK_TRIM_PARAM, -1.f, 1.f, 0.f, "X feedback CV depth", "%", 0.f, 100.f);
		configParam(Y_FEEDBACK_TRIM_PARAM, -1.f, 1.f, 0.f, "Y feedback CV depth", "%", 0.f, 100.f);
		configParam(X_PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "X phase CV depth", "%", 0.f, 100.f);
		configParam(Y_PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Y phase CV depth", "%", 0.f, 100.f);

		// Inputs (Rack automatically appends "input" to tooltips)
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");

		configInput(X_BLEED_CV_INPUT, "X bleed CV");
		configInput(Y_BLEED_CV_INPUT, "Y bleed CV");
		configInput(X_FEEDBACK_CV_INPUT, "X feedback CV");
		configInput(Y_FEEDBACK_CV_INPUT, "Y feedback CV");
		configInput(X_PHASE_CV_INPUT, "X phase CV");
		configInput(Y_PHASE_CV_INPUT, "Y phase CV");

		// Outputs (Rack automatically appends "output" to tooltips)
		configOutput(X1_OUTPUT, "X 1");
		configOutput(Y1_OUTPUT, "Y 1");
		configOutput(X2_OUTPUT, "X 2");
		configOutput(Y2_OUTPUT, "Y 2");
	}

	void onReset() override {
		for (int i = 0; i < 16; i++) {
			apX1[i].reset();
			apY1[i].reset();
			apX2[i].reset();
			apY2[i].reset();
			fbStateX1[i] = 0.0f;
			fbStateY1[i] = 0.0f;
			fbStateX2[i] = 0.0f;
			fbStateY2[i] = 0.0f;
		}
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max({
			inputs[X1_INPUT].getChannels(),
			inputs[Y1_INPUT].getChannels(),
			inputs[X2_INPUT].getChannels(),
			inputs[Y2_INPUT].getChannels(),
			1
		});

		outputs[X1_OUTPUT].setChannels(channels);
		outputs[Y1_OUTPUT].setChannels(channels);
		outputs[X2_OUTPUT].setChannels(channels);
		outputs[Y2_OUTPUT].setChannels(channels);

		float xBleedVal = params[X_BLEED_PARAM].getValue();
		float yBleedVal = params[Y_BLEED_PARAM].getValue();
		float xFbVal = params[X_FEEDBACK_PARAM].getValue();
		float yFbVal = params[Y_FEEDBACK_PARAM].getValue();
		float xPhaseVal = params[X_PHASE_PARAM].getValue();
		float yPhaseVal = params[Y_PHASE_PARAM].getValue();

		float xBleedTrim = params[X_BLEED_TRIM_PARAM].getValue();
		float yBleedTrim = params[Y_BLEED_TRIM_PARAM].getValue();
		float xFbTrim = params[X_FEEDBACK_TRIM_PARAM].getValue();
		float yFbTrim = params[Y_FEEDBACK_TRIM_PARAM].getValue();
		float xPhaseTrim = params[X_PHASE_TRIM_PARAM].getValue();
		float yPhaseTrim = params[Y_PHASE_TRIM_PARAM].getValue();

		bool y1Connected = inputs[Y1_INPUT].isConnected();
		bool x2Connected = inputs[X2_INPUT].isConnected();
		bool y2Connected = inputs[Y2_INPUT].isConnected();

		bool cvYBleedConnected = inputs[Y_BLEED_CV_INPUT].isConnected();
		bool cvYFbConnected = inputs[Y_FEEDBACK_CV_INPUT].isConnected();
		bool cvYPhaseConnected = inputs[Y_PHASE_CV_INPUT].isConnected();

		float sampleRate = args.sampleRate;

		for (int c = 0; c < channels; c++) {
			float inX1 = inputs[X1_INPUT].getPolyVoltage(c);
			float inY1 = y1Connected ? inputs[Y1_INPUT].getPolyVoltage(c) : inX1;
			float inX2 = x2Connected ? inputs[X2_INPUT].getPolyVoltage(c) : inX1;
			float inY2 = y2Connected ? inputs[Y2_INPUT].getPolyVoltage(c) : (x2Connected ? inX2 : inY1);

			// CV Modulation (0.2x scaling -> 5V = 100%)
			float cvXBleed = inputs[X_BLEED_CV_INPUT].getPolyVoltage(c) * 0.2f;
			float cvYBleed = cvYBleedConnected ? inputs[Y_BLEED_CV_INPUT].getPolyVoltage(c) * 0.2f : cvXBleed;

			float cvXFb = inputs[X_FEEDBACK_CV_INPUT].getPolyVoltage(c) * 0.2f;
			float cvYFb = cvYFbConnected ? inputs[Y_FEEDBACK_CV_INPUT].getPolyVoltage(c) * 0.2f : cvXFb;

			float cvXPhase = inputs[X_PHASE_CV_INPUT].getPolyVoltage(c) * 0.2f;
			float cvYPhase = cvYPhaseConnected ? inputs[Y_PHASE_CV_INPUT].getPolyVoltage(c) * 0.2f : cvXPhase;

			float kBleedX = clamp(xBleedVal + cvXBleed * xBleedTrim, -1.0f, 1.0f);
			float kBleedY = clamp(yBleedVal + cvYBleed * yBleedTrim, -1.0f, 1.0f);

			float fbX = clamp(xFbVal + cvXFb * xFbTrim, 0.0f, 1.25f);
			float fbY = clamp(yFbVal + cvYFb * yFbTrim, 0.0f, 1.25f);

			float mPhaseX = clamp(xPhaseVal + cvXPhase * xPhaseTrim, 0.0f, 1.0f);
			float mPhaseY = clamp(yPhaseVal + cvYPhase * yPhaseTrim, 0.0f, 1.0f);

			// Exponential frequency sweep for all-pass filters (30 Hz to 12 kHz)
			float fcX = 30.0f * std::pow(400.0f, mPhaseX);
			float fcY = 30.0f * std::pow(400.0f, mPhaseY);

			// All-pass filter phase delay on feedback signal
			float apSigX1 = apX1[c].process(fbStateX1[c], fcX, sampleRate);
			float apSigY1 = apY1[c].process(fbStateY1[c], fcY, sampleRate);
			float apSigX2 = apX2[c].process(fbStateX2[c], fcX, sampleRate);
			float apSigY2 = apY2[c].process(fbStateY2[c], fcY, sampleRate);

			// Feedback recursion injection into each independent pair
			float injX1 = inX1 + fbX * apSigX1;
			float injY1 = inY1 + fbY * apSigY1;
			float injX2 = inX2 + fbX * apSigX2;
			float injY2 = inY2 + fbY * apSigY2;

			// Cross-channel bleed between Pair 1 and Pair 2:
			// Out1 = Inj1 + Bleed * Inj2
			// Out2 = Inj2 + Bleed * Inj1
			float outX1 = injX1 + kBleedX * injX2;
			float outX2 = injX2 + kBleedX * injX1;
			float outY1 = injY1 + kBleedY * injY2;
			float outY2 = injY2 + kBleedY * injY1;

			// Soft saturation in feedback loop to maintain bounded oscillation
			fbStateX1[c] = 5.0f * std::tanh(outX1 / 5.0f);
			fbStateY1[c] = 5.0f * std::tanh(outY1 / 5.0f);
			fbStateX2[c] = 5.0f * std::tanh(outX2 / 5.0f);
			fbStateY2[c] = 5.0f * std::tanh(outY2 / 5.0f);

			// Galvo safety bounds (-12V to +12V)
			outputs[X1_OUTPUT].setVoltage(clamp(outX1, -12.0f, 12.0f), c);
			outputs[Y1_OUTPUT].setVoltage(clamp(outY1, -12.0f, 12.0f), c);
			outputs[X2_OUTPUT].setVoltage(clamp(outX2, -12.0f, 12.0f), c);
			outputs[Y2_OUTPUT].setVoltage(clamp(outY2, -12.0f, 12.0f), c);
		}
	}
};

struct BleedWidget : ModuleWidget {
	BleedWidget(Bleed* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bleed.svg")));

		// 8 HP Standard Eurorack Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Knob Columns: X = 10.82 mm, Y = 29.82 mm
		// Row 1: Bleed Knobs (Center Y = 20.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 20.00)), module, Bleed::X_BLEED_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 20.00)), module, Bleed::Y_BLEED_PARAM));

		// Row 2: Feedback Knobs (Center Y = 34.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 34.50)), module, Bleed::X_FEEDBACK_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 34.50)), module, Bleed::Y_FEEDBACK_PARAM));

		// Row 3: Phase Knobs (Center Y = 49.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 49.00)), module, Bleed::X_PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 49.00)), module, Bleed::Y_PHASE_PARAM));

		// Zone 3: CV Attenuverter Trimpots
		// Row 1: Bleed CV Depth (Center Y = 61.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(10.82, 61.50)), module, Bleed::X_BLEED_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(29.82, 61.50)), module, Bleed::Y_BLEED_TRIM_PARAM));

		// Row 2: Feedback CV Depth (Center Y = 69.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(10.82, 69.50)), module, Bleed::X_FEEDBACK_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(29.82, 69.50)), module, Bleed::Y_FEEDBACK_TRIM_PARAM));

		// Row 3: Phase CV Depth (Center Y = 77.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(10.82, 77.50)), module, Bleed::X_PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(29.82, 77.50)), module, Bleed::Y_PHASE_TRIM_PARAM));

		// Zone 4: I/O Jacks (4 Columns: 6.07, 15.57, 25.07, 34.57 mm)
		// Row 1: Signal Inputs (Center Y = 89.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.07, 89.50)), module, Bleed::X1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.57, 89.50)), module, Bleed::Y1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.07, 89.50)), module, Bleed::X2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34.57, 89.50)), module, Bleed::Y2_INPUT));

		// Row 2: Bleed & Feedback CV Inputs (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.07, 99.00)), module, Bleed::X_BLEED_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.57, 99.00)), module, Bleed::Y_BLEED_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.07, 99.00)), module, Bleed::X_FEEDBACK_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34.57, 99.00)), module, Bleed::Y_FEEDBACK_CV_INPUT));

		// Row 3: Phase CV Inputs (Center Y = 108.50 mm, at 10.82 and 29.82 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.82, 108.50)), module, Bleed::X_PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.82, 108.50)), module, Bleed::Y_PHASE_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.07, 118.00)), module, Bleed::X1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.57, 118.00)), module, Bleed::Y1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.07, 118.00)), module, Bleed::X2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(34.57, 118.00)), module, Bleed::Y2_OUTPUT));
	}
};

Model* modelBleed = createModel<Bleed, BleedWidget>("Bleed");
