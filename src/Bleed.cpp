#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Bleed — Cross-Channel X/Y Mix, Crosstalk & Phase Feedback Engine
//  6 HP module with bidirectional per-channel bleed crosstalk and
//  quadrature all-pass phase-delayed feedback recursion.
// ─────────────────────────────────────────────────────────────────────

struct Bleed : Module {
	enum ParamId {
		X_BLEED_PARAM,
		Y_BLEED_PARAM,
		MOD1_PARAM,
		MOD2_PARAM,
		X_BLEED_TRIM_PARAM,
		Y_BLEED_TRIM_PARAM,
		MOD1_TRIM_PARAM,
		MOD2_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_BLEED_CV_INPUT,
		Y_BLEED_CV_INPUT,
		MOD1_CV_INPUT,
		MOD2_CV_INPUT,
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

	enum ModMode {
		MODE_DUAL_FEEDBACK = 0,
		MODE_DUAL_PHASE = 1,
		MODE_MASTER_FB_PHASE = 2
	};

	int modMode = MODE_DUAL_FEEDBACK;

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

	AllPassFilter apX[16];
	AllPassFilter apY[16];
	float fbStateX[16] = {0};
	float fbStateY[16] = {0};

	struct Mod1ParamQuantity : ParamQuantity {
		std::string getLabel() override {
			Bleed* m = dynamic_cast<Bleed*>(module);
			if (!m) return "Mod 1";
			switch (m->modMode) {
				case MODE_DUAL_FEEDBACK: return "X feedback";
				case MODE_DUAL_PHASE: return "X phase";
				case MODE_MASTER_FB_PHASE: return "Feedback";
				default: return "Mod 1";
			}
		}
	};

	struct Mod2ParamQuantity : ParamQuantity {
		std::string getLabel() override {
			Bleed* m = dynamic_cast<Bleed*>(module);
			if (!m) return "Mod 2";
			switch (m->modMode) {
				case MODE_DUAL_FEEDBACK: return "Y feedback";
				case MODE_DUAL_PHASE: return "Y phase";
				case MODE_MASTER_FB_PHASE: return "Phase";
				default: return "Mod 2";
			}
		}
	};

	struct Mod1TrimParamQuantity : ParamQuantity {
		std::string getLabel() override {
			Bleed* m = dynamic_cast<Bleed*>(module);
			if (!m) return "Mod 1 CV depth";
			switch (m->modMode) {
				case MODE_DUAL_FEEDBACK: return "X feedback CV depth";
				case MODE_DUAL_PHASE: return "X phase CV depth";
				case MODE_MASTER_FB_PHASE: return "Feedback CV depth";
				default: return "Mod 1 CV depth";
			}
		}
	};

	struct Mod2TrimParamQuantity : ParamQuantity {
		std::string getLabel() override {
			Bleed* m = dynamic_cast<Bleed*>(module);
			if (!m) return "Mod 2 CV depth";
			switch (m->modMode) {
				case MODE_DUAL_FEEDBACK: return "Y feedback CV depth";
				case MODE_DUAL_PHASE: return "Y phase CV depth";
				case MODE_MASTER_FB_PHASE: return "Phase CV depth";
				default: return "Mod 2 CV depth";
			}
		}
	};

	Bleed() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Bipolar Bleed Knobs (-100% to +100%)
		configParam(X_BLEED_PARAM, -1.f, 1.f, 0.f, "X bleed", "%", 0.f, 100.f);
		configParam(Y_BLEED_PARAM, -1.f, 1.f, 0.f, "Y bleed", "%", 0.f, 100.f);

		// Row 2: Mode-dependent MOD 1 & MOD 2 Knobs (0.0 to 1.0)
		configParam<Mod1ParamQuantity>(MOD1_PARAM, 0.f, 1.f, 0.f, "Mod 1", "%", 0.f, 100.f);
		configParam<Mod2ParamQuantity>(MOD2_PARAM, 0.f, 1.f, 0.f, "Mod 2", "%", 0.f, 100.f);

		// Trimpots (CV Attenuverters)
		configParam(X_BLEED_TRIM_PARAM, -1.f, 1.f, 0.f, "X bleed CV depth", "%", 0.f, 100.f);
		configParam(Y_BLEED_TRIM_PARAM, -1.f, 1.f, 0.f, "Y bleed CV depth", "%", 0.f, 100.f);
		configParam<Mod1TrimParamQuantity>(MOD1_TRIM_PARAM, -1.f, 1.f, 0.f, "Mod 1 CV depth", "%", 0.f, 100.f);
		configParam<Mod2TrimParamQuantity>(MOD2_TRIM_PARAM, -1.f, 1.f, 0.f, "Mod 2 CV depth", "%", 0.f, 100.f);

		// Inputs (Rack automatically appends "input" to tooltips)
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_BLEED_CV_INPUT, "X bleed CV");
		configInput(Y_BLEED_CV_INPUT, "Y bleed CV");
		configInput(MOD1_CV_INPUT, "Mod 1 CV");
		configInput(MOD2_CV_INPUT, "Mod 2 CV");

		// Outputs (Rack automatically appends "output" to tooltips)
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "modMode", json_integer(modMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* modeJ = json_object_get(rootJ, "modMode");
		if (modeJ) {
			modMode = json_integer_value(modeJ);
		}
	}

	void onReset() override {
		for (int i = 0; i < 16; i++) {
			apX[i].reset();
			apY[i].reset();
			fbStateX[i] = 0.0f;
			fbStateY[i] = 0.0f;
		}
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int numChannels = std::max({xChannels, yChannels, 1});

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float xBleedParam = params[X_BLEED_PARAM].getValue();
		float yBleedParam = params[Y_BLEED_PARAM].getValue();
		float mod1Param = params[MOD1_PARAM].getValue();
		float mod2Param = params[MOD2_PARAM].getValue();

		float xBleedTrim = params[X_BLEED_TRIM_PARAM].getValue();
		float yBleedTrim = params[Y_BLEED_TRIM_PARAM].getValue();
		float mod1Trim = params[MOD1_TRIM_PARAM].getValue();
		float mod2Trim = params[MOD2_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool cvYBleedConnected = inputs[Y_BLEED_CV_INPUT].isConnected();
		bool cvMod2Connected = inputs[MOD2_CV_INPUT].isConnected();

		float sampleRate = args.sampleRate;

		for (int c = 0; c < numChannels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			// Bleed CV modulation
			float cvXBleed = inputs[X_BLEED_CV_INPUT].getPolyVoltage(c) / 5.0f;
			float cvYBleed = cvYBleedConnected ? inputs[Y_BLEED_CV_INPUT].getPolyVoltage(c) / 5.0f : cvXBleed;
			float kYX = clamp(xBleedParam + cvXBleed * xBleedTrim, -1.0f, 1.0f);
			float kXY = clamp(yBleedParam + cvYBleed * yBleedTrim, -1.0f, 1.0f);

			// Mod CV modulation
			float cvMod1 = inputs[MOD1_CV_INPUT].getPolyVoltage(c) / 5.0f;
			float cvMod2 = cvMod2Connected ? inputs[MOD2_CV_INPUT].getPolyVoltage(c) / 5.0f : cvMod1;
			float m1 = clamp(mod1Param + cvMod1 * mod1Trim, 0.0f, 1.0f);
			float m2 = clamp(mod2Param + cvMod2 * mod2Trim, 0.0f, 1.0f);

			float fbGainX = 0.0f;
			float fbGainY = 0.0f;
			float fcX = 500.0f;
			float fcY = 500.0f;

			if (modMode == MODE_DUAL_FEEDBACK) {
				// MOD 1 = X feedback (0.0 to 1.25), MOD 2 = Y feedback (0.0 to 1.25)
				fbGainX = m1 * 1.25f;
				fbGainY = m2 * 1.25f;
				fcX = 400.0f;
				fcY = 600.0f; // quadrature all-pass offset
			} else if (modMode == MODE_DUAL_PHASE) {
				// MOD 1 = X phase, MOD 2 = Y phase (30 Hz to 12 kHz)
				fbGainX = 0.85f;
				fbGainY = 0.85f;
				fcX = 30.0f * std::pow(400.0f, m1);
				fcY = 30.0f * std::pow(400.0f, m2);
			} else { // MODE_MASTER_FB_PHASE
				// MOD 1 = Master feedback (0.0 to 1.25), MOD 2 = Master phase (30 Hz to 12 kHz)
				fbGainX = m1 * 1.25f;
				fbGainY = m1 * 1.25f;
				float fcMaster = 30.0f * std::pow(400.0f, m2);
				fcX = fcMaster;
				fcY = fcMaster * 1.414f; // 90° quadrature offset
			}

			// Process feedback through all-pass delay filters
			float apSigX = apX[c].process(fbStateX[c], fcX, sampleRate);
			float apSigY = apY[c].process(fbStateY[c], fcY, sampleRate);

			// Inject feedback
			float injX = inX + fbGainX * apSigX;
			float injY = inY + fbGainY * apSigY;

			// Apply bidirectional cross-channel bleed
			float outX = injX + kYX * injY;
			float outY = injY + kXY * injX;

			// Soft saturation (tanh) in feedback state to keep recursive gain self-stabilizing
			fbStateX[c] = 5.0f * std::tanh(outX / 5.0f);
			fbStateY[c] = 5.0f * std::tanh(outY / 5.0f);

			// Laser safety bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.0f, 12.0f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.0f, 12.0f), c);
		}
	}
};

struct BleedWidget : ModuleWidget {
	BleedWidget(Bleed* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bleed.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: X Bleed (7.62 mm) & Y Bleed (22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Bleed::X_BLEED_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Bleed::Y_BLEED_PARAM));

		// Row 2: MOD 1 (7.62 mm) & MOD 2 (22.86 mm) Knobs (Center Y = 40.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 40.00)), module, Bleed::MOD1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 40.00)), module, Bleed::MOD2_PARAM));

		// Attenuverter Trimpots (Zone 3)
		// Row 1: X Bleed CV (7.62 mm), Y Bleed CV (22.86 mm) (Center Y = 61.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.50)), module, Bleed::X_BLEED_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.50)), module, Bleed::Y_BLEED_TRIM_PARAM));

		// Row 2: MOD 1 CV (7.62 mm), MOD 2 CV (22.86 mm) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Bleed::MOD1_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Bleed::MOD2_TRIM_PARAM));

		// Bottom I/O Jacks (Zone 4)
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Bleed::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Bleed::Y_INPUT));

		// Row 2: Bleed CV Inputs (Center Y = 99.00 mm): X Bleed CV (7.62), Y Bleed CV (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Bleed::X_BLEED_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Bleed::Y_BLEED_CV_INPUT));

		// Row 3: Mod CV Inputs (Center Y = 108.50 mm): MOD 1 CV (7.62), MOD 2 CV (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Bleed::MOD1_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Bleed::MOD2_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Bleed::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Bleed::Y_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Bleed* module = dynamic_cast<Bleed*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Modulation Mode"));

		struct ModeItem : MenuItem {
			Bleed* module;
			int mode;
			void onAction(const event::Action& e) override {
				module->modMode = mode;
			}
		};

		auto addModeItem = [&](const std::string& label, int m) {
			ModeItem* item = createMenuItem<ModeItem>(label);
			item->module = module;
			item->mode = m;
			item->rightText = (module->modMode == m) ? "✔" : "";
			menu->addChild(item);
		};

		addModeItem("Dual Feedback (X Feedback / Y Feedback)", Bleed::MODE_DUAL_FEEDBACK);
		addModeItem("Dual Phase (X Phase / Y Phase)", Bleed::MODE_DUAL_PHASE);
		addModeItem("Master Feedback & Phase", Bleed::MODE_MASTER_FB_PHASE);
	}
};

Model* modelBleed = createModel<Bleed, BleedWidget>("Bleed");
