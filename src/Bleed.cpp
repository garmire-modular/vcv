#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Bleed — Dual X/Y Pair Cross-Bleed & Phase Feedback Recursion Engine
//  8 HP module processing two independent pairs of X/Y signals with
//  bidirectional per-channel bleed crosstalk, ±180° phase-rotated
//  feedback recursion, and radial Automatic Gain Control (AGC).
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

	bool agcEnabled = true;

	// First-order all-pass filter for frequency-dependent quadrature phase delay
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

	// AGC radial envelope trackers
	float envIn1[16] = {0};
	float envIn2[16] = {0};
	float envOut1[16] = {0};
	float envOut2[16] = {0};

	Bleed() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Bipolar Bleed Knobs (-100% to +100%)
		configParam(X_BLEED_PARAM, -1.f, 1.f, 0.f, "X bleed", "%", 0.f, 100.f);
		configParam(Y_BLEED_PARAM, -1.f, 1.f, 0.f, "Y bleed", "%", 0.f, 100.f);

		// Row 2: Feedback Knobs (0% to 125%)
		configParam(X_FEEDBACK_PARAM, 0.f, 1.25f, 0.f, "X feedback", "%", 0.f, 100.f);
		configParam(Y_FEEDBACK_PARAM, 0.f, 1.25f, 0.f, "Y feedback", "%", 0.f, 100.f);

		// Row 3: Phase Knobs (-180° to +180°)
		configParam(X_PHASE_PARAM, -180.f, 180.f, 0.f, "X phase", "°");
		configParam(Y_PHASE_PARAM, -180.f, 180.f, 0.f, "Y phase", "°");

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

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "agcEnabled", json_boolean(agcEnabled));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* agcJ = json_object_get(rootJ, "agcEnabled");
		if (agcJ) {
			agcEnabled = json_is_true(agcJ);
		}
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
			envIn1[i] = 0.0f;
			envIn2[i] = 0.0f;
			envOut1[i] = 0.0f;
			envOut2[i] = 0.0f;
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
		float alphaRel = 1.0f - std::exp(-1.0f / (sampleRate * 0.040f)); // ~40 ms AGC release

		for (int c = 0; c < channels; c++) {
			float inX1 = inputs[X1_INPUT].getPolyVoltage(c);
			float inY1 = y1Connected ? inputs[Y1_INPUT].getPolyVoltage(c) : inX1;
			float inX2 = x2Connected ? inputs[X2_INPUT].getPolyVoltage(c) : inX1;
			float inY2 = y2Connected ? inputs[Y2_INPUT].getPolyVoltage(c) : (x2Connected ? inX2 : inY1);

			// Track input envelope (2D Euclidean radial distance) for AGC reference target
			float rIn1 = std::sqrt(inX1 * inX1 + inY1 * inY1);
			float rIn2 = std::sqrt(inX2 * inX2 + inY2 * inY2);

			if (rIn1 > envIn1[c]) {
				envIn1[c] = rIn1;
			} else {
				envIn1[c] += alphaRel * (rIn1 - envIn1[c]);
			}

			if (rIn2 > envIn2[c]) {
				envIn2[c] = rIn2;
			} else {
				envIn2[c] += alphaRel * (rIn2 - envIn2[c]);
			}

			// AGC target ceiling: preserves original input scale, nominal floor of 5V, safe ceiling of 10V
			float target1 = clamp(std::max(5.0f, envIn1[c]), 5.0f, 10.0f);
			float target2 = clamp(std::max(5.0f, envIn2[c]), 5.0f, 10.0f);

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

			// Phase modulation (-180° to +180°)
			float phaseDegX = clamp(xPhaseVal + cvXPhase * xPhaseTrim * 180.0f, -180.0f, 180.0f);
			float phaseDegY = clamp(yPhaseVal + cvYPhase * yPhaseTrim * 180.0f, -180.0f, 180.0f);

			float radX = phaseDegX * (float)(M_PI / 180.0);
			float radY = phaseDegY * (float)(M_PI / 180.0);

			float cosX = std::cos(radX);
			float sinX = std::sin(radX);
			float cosY = std::cos(radY);
			float sinY = std::sin(radY);

			// All-pass filter phase delay on feedback signal for quadrature component
			float apSigX1 = apX1[c].process(fbStateX1[c], 400.0f, sampleRate);
			float apSigY1 = apY1[c].process(fbStateY1[c], 600.0f, sampleRate);
			float apSigX2 = apX2[c].process(fbStateX2[c], 400.0f, sampleRate);
			float apSigY2 = apY2[c].process(fbStateY2[c], 600.0f, sampleRate);

			// Continuous ±180° phase rotation of feedback loop:
			// 0° = in-phase (+1.0)
			// ±180° = inverted phase (-1.0)
			// ±90° = quadrature phase shift
			float rotFbX1 = cosX * fbStateX1[c] + sinX * apSigX1;
			float rotFbY1 = cosY * fbStateY1[c] + sinY * apSigY1;
			float rotFbX2 = cosX * fbStateX2[c] + sinX * apSigX2;
			float rotFbY2 = cosY * fbStateY2[c] + sinY * apSigY2;

			// Feedback recursion injection into each independent pair
			float injX1 = inX1 + fbX * rotFbX1;
			float injY1 = inY1 + fbY * rotFbY1;
			float injX2 = inX2 + fbX * rotFbX2;
			float injY2 = inY2 + fbY * rotFbY2;

			// Cross-channel bleed between Pair 1 and Pair 2:
			// Out1 = Inj1 + Bleed * Inj2
			// Out2 = Inj2 + Bleed * Inj1
			float outX1 = injX1 + kBleedX * injX2;
			float outX2 = injX2 + kBleedX * injX1;
			float outY1 = injY1 + kBleedY * injY2;
			float outY2 = injY2 + kBleedY * injY1;

			// Automatic Gain Control (AGC):
			// Measure composite output radius and scale (X, Y) uniformly
			// so the combined shape never clips or warps.
			if (agcEnabled) {
				float rOut1 = std::sqrt(outX1 * outX1 + outY1 * outY1);
				float rOut2 = std::sqrt(outX2 * outX2 + outY2 * outY2);

				if (rOut1 > envOut1[c]) {
					envOut1[c] = rOut1;
				} else {
					envOut1[c] += alphaRel * (rOut1 - envOut1[c]);
				}

				if (rOut2 > envOut2[c]) {
					envOut2[c] = rOut2;
				} else {
					envOut2[c] += alphaRel * (rOut2 - envOut2[c]);
				}

				float g1 = (envOut1[c] > target1 && envOut1[c] > 1e-4f) ? (target1 / envOut1[c]) : 1.0f;
				float g2 = (envOut2[c] > target2 && envOut2[c] > 1e-4f) ? (target2 / envOut2[c]) : 1.0f;

				outX1 *= g1;
				outY1 *= g1;
				outX2 *= g2;
				outY2 *= g2;
			}

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

	void appendContextMenu(Menu* menu) override {
		Bleed* module = dynamic_cast<Bleed*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Dynamics"));

		struct AgcItem : MenuItem {
			Bleed* module;
			void onAction(const event::Action& e) override {
				module->agcEnabled = !module->agcEnabled;
			}
		};

		AgcItem* agcItem = createMenuItem<AgcItem>("Automatic Gain Control (AGC)");
		agcItem->module = module;
		agcItem->rightText = module->agcEnabled ? "✔" : "";
		menu->addChild(agcItem);
	}
};

Model* modelBleed = createModel<Bleed, BleedWidget>("Bleed");
