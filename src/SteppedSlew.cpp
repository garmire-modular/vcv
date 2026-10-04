#include "plugin.hpp"
#include "core/SteppedSlewEngine.hpp"
#include <cmath>

struct SlewTimeParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float val = getValue(); // 0.0 to 1.0
		float timeSec = 0.0005f * std::pow(20000.0f, val);
		char buf[32];
		if (timeSec < 0.01f) {
			snprintf(buf, sizeof(buf), "%.2f ms", timeSec * 1000.f);
		} else if (timeSec < 1.0f) {
			snprintf(buf, sizeof(buf), "%.1f ms", timeSec * 1000.f);
		} else {
			snprintf(buf, sizeof(buf), "%.2f s", timeSec);
		}
		return std::string(buf);
	}
	std::string getUnit() override { return ""; }
};

struct SlewShapeParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float v = getValue(); // -1.0 to +1.0
		char buf[32];
		if (std::abs(v) < 0.01f) {
			return "Linear";
		} else if (v < 0.f) {
			snprintf(buf, sizeof(buf), "Log (%.2f)", v);
			return std::string(buf);
		} else {
			snprintf(buf, sizeof(buf), "Exp (%.2f)", v);
			return std::string(buf);
		}
	}
	std::string getUnit() override { return ""; }
};

struct StepsParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int steps = static_cast<int>(std::round(getValue()));
		if (steps == 0) {
			return "Bypass (Continuous)";
		}
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", steps);
		return std::string(buf);
	}
	std::string getUnit() override {
		int steps = static_cast<int>(std::round(getValue()));
		return (steps == 0) ? "" : " steps";
	}
};

struct AttenParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float v = getValue(); // -1.0 to 1.0
		char buf[32];
		if (std::abs(v) < 0.005f) {
			return "0.0%";
		}
		snprintf(buf, sizeof(buf), "%.1f%%", v * 100.f);
		return std::string(buf);
	}
	std::string getUnit() override { return ""; }
};

struct SteppedSlew : Module {
	enum ParamId {
		TIME_UP_PARAM,
		SHAPE_UP_PARAM,
		STEPS_UP_PARAM,
		TIME_DOWN_PARAM,
		SHAPE_DOWN_PARAM,
		STEPS_DOWN_PARAM,
		TIME_UP_ATTEN_PARAM,
		SHAPE_UP_ATTEN_PARAM,
		STEPS_UP_ATTEN_PARAM,
		TIME_DOWN_ATTEN_PARAM,
		SHAPE_DOWN_ATTEN_PARAM,
		STEPS_DOWN_ATTEN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TIME_UP_CV_INPUT,
		SHAPE_UP_CV_INPUT,
		STEPS_UP_CV_INPUT,
		TIME_DOWN_CV_INPUT,
		SHAPE_DOWN_CV_INPUT,
		STEPS_DOWN_CV_INPUT,
		SIGNAL_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		UP_GATE_OUTPUT,
		EOU_TRIG_OUTPUT,
		DOWN_GATE_OUTPUT,
		EOD_TRIG_OUTPUT,
		SLEW_OUTPUT,
		STEP_OUTPUT,
		STEP_TRIG_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		STEP_UP_LIGHT,
		STEP_DOWN_LIGHT,
		LIGHTS_LEN
	};

	stepped_slew::Engine engine;

	SteppedSlew() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Up Knobs
		configParam<SlewTimeParamQuantity>(TIME_UP_PARAM, 0.f, 1.f, 0.45f, "Up slew time");
		configParam<SlewShapeParamQuantity>(SHAPE_UP_PARAM, -1.f, 1.f, 0.f, "Up slew shape");
		configParam<StepsParamQuantity>(STEPS_UP_PARAM, 0.f, 96.f, 12.f, "Up steps");

		// Row 2: Down Knobs
		configParam<SlewTimeParamQuantity>(TIME_DOWN_PARAM, 0.f, 1.f, 0.45f, "Down slew time");
		configParam<SlewShapeParamQuantity>(SHAPE_DOWN_PARAM, -1.f, 1.f, 0.f, "Down slew shape");
		configParam<StepsParamQuantity>(STEPS_DOWN_PARAM, 0.f, 96.f, 12.f, "Down steps");

		// Attenuverters Row 1: Up CV Depth
		configParam<AttenParamQuantity>(TIME_UP_ATTEN_PARAM, -1.f, 1.f, 0.f, "Up time CV depth");
		configParam<AttenParamQuantity>(SHAPE_UP_ATTEN_PARAM, -1.f, 1.f, 0.f, "Up shape CV depth");
		configParam<AttenParamQuantity>(STEPS_UP_ATTEN_PARAM, -1.f, 1.f, 0.f, "Up steps CV depth");

		// Attenuverters Row 2: Down CV Depth
		configParam<AttenParamQuantity>(TIME_DOWN_ATTEN_PARAM, -1.f, 1.f, 0.f, "Down time CV depth");
		configParam<AttenParamQuantity>(SHAPE_DOWN_ATTEN_PARAM, -1.f, 1.f, 0.f, "Down shape CV depth");
		configParam<AttenParamQuantity>(STEPS_DOWN_ATTEN_PARAM, -1.f, 1.f, 0.f, "Down steps CV depth");

		// Inputs
		configInput(TIME_UP_CV_INPUT, "Up time CV");
		configInput(SHAPE_UP_CV_INPUT, "Up shape CV");
		configInput(STEPS_UP_CV_INPUT, "Up steps CV");
		configInput(TIME_DOWN_CV_INPUT, "Down time CV");
		configInput(SHAPE_DOWN_CV_INPUT, "Down shape CV");
		configInput(STEPS_DOWN_CV_INPUT, "Down steps CV");
		configInput(SIGNAL_INPUT, "Signal");

		// Outputs
		configOutput(UP_GATE_OUTPUT, "Up gate");
		configOutput(EOU_TRIG_OUTPUT, "End of up trigger");
		configOutput(DOWN_GATE_OUTPUT, "Down gate");
		configOutput(EOD_TRIG_OUTPUT, "End of down trigger");
		configOutput(SLEW_OUTPUT, "Continuous slew");
		configOutput(STEP_OUTPUT, "Stepped slew");
		configOutput(STEP_TRIG_OUTPUT, "Step trigger");

		// Lights
		configLight(STEP_UP_LIGHT, "Up step indicator");
		configLight(STEP_DOWN_LIGHT, "Down step indicator");

		engine.reset();
	}

	void onReset() override {
		engine.reset();
	}

	void process(const ProcessArgs& args) override {
		// Read CV inputs with normaling from Up to Down
		float timeUpCv = inputs[TIME_UP_CV_INPUT].getVoltage();
		float shapeUpCv = inputs[SHAPE_UP_CV_INPUT].getVoltage();
		float stepsUpCv = inputs[STEPS_UP_CV_INPUT].getVoltage();

		float timeDownCv = inputs[TIME_DOWN_CV_INPUT].isConnected() ? inputs[TIME_DOWN_CV_INPUT].getVoltage() : timeUpCv;
		float shapeDownCv = inputs[SHAPE_DOWN_CV_INPUT].isConnected() ? inputs[SHAPE_DOWN_CV_INPUT].getVoltage() : shapeUpCv;
		float stepsDownCv = inputs[STEPS_DOWN_CV_INPUT].isConnected() ? inputs[STEPS_DOWN_CV_INPUT].getVoltage() : stepsUpCv;

		// Compute modulated parameters
		float tUp = stepped_slew::calcTimeSeconds(params[TIME_UP_PARAM].getValue(), timeUpCv, params[TIME_UP_ATTEN_PARAM].getValue());
		float sUp = std::max(-1.f, std::min(1.f, params[SHAPE_UP_PARAM].getValue() + params[SHAPE_UP_ATTEN_PARAM].getValue() * (shapeUpCv / 10.f)));
		int stepsUp = stepped_slew::calcSteps(params[STEPS_UP_PARAM].getValue(), stepsUpCv, params[STEPS_UP_ATTEN_PARAM].getValue());

		float tDown = stepped_slew::calcTimeSeconds(params[TIME_DOWN_PARAM].getValue(), timeDownCv, params[TIME_DOWN_ATTEN_PARAM].getValue());
		float sDown = std::max(-1.f, std::min(1.f, params[SHAPE_DOWN_PARAM].getValue() + params[SHAPE_DOWN_ATTEN_PARAM].getValue() * (shapeDownCv / 10.f)));
		int stepsDown = stepped_slew::calcSteps(params[STEPS_DOWN_PARAM].getValue(), stepsDownCv, params[STEPS_DOWN_ATTEN_PARAM].getValue());

		// Input voltage normaled to 0.0V
		float inV = inputs[SIGNAL_INPUT].isConnected() ? inputs[SIGNAL_INPUT].getVoltage() : 0.0f;

		// Core DSP Process
		auto out = engine.process(inV, tUp, sUp, stepsUp, tDown, sDown, stepsDown, args.sampleRate);

		// Set outputs
		outputs[UP_GATE_OUTPUT].setVoltage(out.upGate);
		outputs[EOU_TRIG_OUTPUT].setVoltage(out.eouTrig);
		outputs[DOWN_GATE_OUTPUT].setVoltage(out.downGate);
		outputs[EOD_TRIG_OUTPUT].setVoltage(out.eodTrig);
		outputs[SLEW_OUTPUT].setVoltage(out.slewOut);
		outputs[STEP_OUTPUT].setVoltage(out.stepOut);
		outputs[STEP_TRIG_OUTPUT].setVoltage(out.stepGate);

		// Lights
		lights[STEP_UP_LIGHT].setBrightness(out.ledUpBrightness);
		lights[STEP_DOWN_LIGHT].setBrightness(out.ledDownBrightness);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "steppingMode", json_integer(engine.steppingMode));
		json_object_set_new(rootJ, "quantizeStrategy", json_integer(engine.quantizeStrategy));
		json_object_set_new(rootJ, "clockMode", json_integer(engine.clockMode));
		json_object_set_new(rootJ, "rootKey", json_integer(engine.rootKey));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* smJ = json_object_get(rootJ, "steppingMode");
		if (smJ) {
			engine.steppingMode = static_cast<stepped_slew::SteppingMode>(json_integer_value(smJ));
		}
		json_t* qsJ = json_object_get(rootJ, "quantizeStrategy");
		if (qsJ) {
			engine.quantizeStrategy = static_cast<stepped_slew::QuantizeStrategy>(json_integer_value(qsJ));
		}
		json_t* cmJ = json_object_get(rootJ, "clockMode");
		if (cmJ) {
			engine.clockMode = static_cast<stepped_slew::ClockMode>(json_integer_value(cmJ));
		}
		json_t* rkJ = json_object_get(rootJ, "rootKey");
		if (rkJ) {
			engine.rootKey = json_integer_value(rkJ);
		}
	}
};

struct SteppedSlewWidget : ModuleWidget {
	SteppedSlewWidget(SteppedSlew* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SteppedSlew.svg")));

		// 10HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 3 Primary Columns: 9.40 mm, 25.40 mm, 41.40 mm
		const double col_1 = 9.400;
		const double col_2 = 25.400;
		const double col_3 = 41.400;

		// 4 Port Columns: 7.900 mm, 19.567 mm, 31.233 mm, 42.900 mm
		const double p_col_1 = 7.900;
		const double p_col_2 = 19.567;
		const double p_col_3 = 31.233;
		const double p_col_4 = 42.900;

		// ---------------- Row 1: Primary Knobs (UP) (Center Y = 21.59 mm) ----------------
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_1, 21.59)), module, SteppedSlew::TIME_UP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_2, 21.59)), module, SteppedSlew::SHAPE_UP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_3, 21.59)), module, SteppedSlew::STEPS_UP_PARAM));

		// ---------------- Row 2: Primary Knobs (DOWN) (Center Y = 43.00 mm) ----------------
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_1, 43.00)), module, SteppedSlew::TIME_DOWN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_2, 43.00)), module, SteppedSlew::SHAPE_DOWN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_3, 43.00)), module, SteppedSlew::STEPS_DOWN_PARAM));

		// ---------------- Row 3: Attenuverters (UP CV) (Center Y = 58.00 mm) ----------------
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_1, 58.00)), module, SteppedSlew::TIME_UP_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_2, 58.00)), module, SteppedSlew::SHAPE_UP_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_3, 58.00)), module, SteppedSlew::STEPS_UP_ATTEN_PARAM));

		// ---------------- Row 4: Attenuverters (DOWN CV) (Center Y = 70.00 mm) ----------------
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_1, 70.00)), module, SteppedSlew::TIME_DOWN_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_2, 70.00)), module, SteppedSlew::SHAPE_DOWN_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_3, 70.00)), module, SteppedSlew::STEPS_DOWN_ATTEN_PARAM));

		// ---------------- Row 5: Jacks (UP CV Inputs) (Center Y = 89.50 mm) ----------------
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_1, 89.50)), module, SteppedSlew::TIME_UP_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_2, 89.50)), module, SteppedSlew::SHAPE_UP_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_3, 89.50)), module, SteppedSlew::STEPS_UP_CV_INPUT));

		// Up Step LED: Above & to right of Steps Up Jack (col_3 + 4.10 = 45.50, 85.50)
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(45.50, 85.50)), module, SteppedSlew::STEP_UP_LIGHT));

		// ---------------- Row 6: Jacks (DOWN CV Inputs) (Center Y = 99.00 mm) ----------------
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_1, 99.00)), module, SteppedSlew::TIME_DOWN_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_2, 99.00)), module, SteppedSlew::SHAPE_DOWN_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_3, 99.00)), module, SteppedSlew::STEPS_DOWN_CV_INPUT));

		// Down Step LED: Above & to right of Steps Down Jack (45.50, 95.00)
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(45.50, 95.00)), module, SteppedSlew::STEP_DOWN_LIGHT));

		// ---------------- Row 7: Jacks (Gates / Trigs) (Center Y = 108.50 mm) ----------------
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(p_col_1, 108.50)), module, SteppedSlew::UP_GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(p_col_2, 108.50)), module, SteppedSlew::EOU_TRIG_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(p_col_3, 108.50)), module, SteppedSlew::DOWN_GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(p_col_4, 108.50)), module, SteppedSlew::EOD_TRIG_OUTPUT));

		// ---------------- Row 8: Jacks (Main Signal I/O) (Fixed Bottom Center Y = 118.00 mm) ----------------
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(p_col_1, 118.00)), module, SteppedSlew::SIGNAL_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(p_col_2, 118.00)), module, SteppedSlew::SLEW_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(p_col_3, 118.00)), module, SteppedSlew::STEP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(p_col_4, 118.00)), module, SteppedSlew::STEP_TRIG_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		SteppedSlew* module = dynamic_cast<SteppedSlew*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Stepping Mode"));

		struct ModeItem : MenuItem {
			SteppedSlew* module;
			stepped_slew::SteppingMode mode;
			void onAction(const event::Action& e) override {
				module->engine.steppingMode = mode;
			}
		};

		const char* modeLabels[] = {
			"Equal (Transition Sub-division)",
			"Semitone (12-EDO)",
			"Quarter-tone (24-EDO)",
			"19-EDO (19-TET)",
			"22-EDO (22-TET)",
			"31-EDO (31-TET)",
			"Just Intonation (5-limit)",
			"Quarter-comma Meantone"
		};

		for (int m = 0; m <= 7; ++m) {
			auto item = createMenuItem<ModeItem>(modeLabels[m]);
			item->module = module;
			item->mode = static_cast<stepped_slew::SteppingMode>(m);
			item->rightText = (module->engine.steppingMode == m) ? "\u2713" : "";
			menu->addChild(item);
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Quantization Strategy"));

		struct StrategyItem : MenuItem {
			SteppedSlew* module;
			stepped_slew::QuantizeStrategy strategy;
			void onAction(const event::Action& e) override {
				module->engine.quantizeStrategy = strategy;
			}
		};

		const char* stratLabels[] = {
			"Scale Degree Traversal",
			"Subdivided & Nearest Scale Snap"
		};

		for (int s = 0; s <= 1; ++s) {
			auto item = createMenuItem<StrategyItem>(stratLabels[s]);
			item->module = module;
			item->strategy = static_cast<stepped_slew::QuantizeStrategy>(s);
			item->rightText = (module->engine.quantizeStrategy == s) ? "\u2713" : "";
			menu->addChild(item);
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Step Clock Output"));

		struct ClockModeItem : MenuItem {
			SteppedSlew* module;
			stepped_slew::ClockMode clockMode;
			void onAction(const event::Action& e) override {
				module->engine.clockMode = clockMode;
			}
		};

		const char* clockLabels[] = {
			"1ms Trigger Pulse (Default)",
			"Alternating Toggle (Flip-Flop)"
		};

		for (int c = 0; c <= 1; ++c) {
			auto item = createMenuItem<ClockModeItem>(clockLabels[c]);
			item->module = module;
			item->clockMode = static_cast<stepped_slew::ClockMode>(c);
			item->rightText = (module->engine.clockMode == c) ? "\u2713" : "";
			menu->addChild(item);
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Tuning Root Key"));

		struct RootKeyItem : MenuItem {
			SteppedSlew* module;
			int root;
			void onAction(const event::Action& e) override {
				module->engine.rootKey = root;
			}
		};

		const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		for (int r = 0; r < 12; ++r) {
			auto item = createMenuItem<RootKeyItem>(notes[r]);
			item->module = module;
			item->root = r;
			item->rightText = (module->engine.rootKey == r) ? "\u2713" : "";
			menu->addChild(item);
		}
	}
};

Model* modelSteppedSlew = createModel<SteppedSlew, SteppedSlewWidget>("SteppedSlew");
