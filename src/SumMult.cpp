#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  SumMult — Dual X/Y Precision Adder and Four-Quadrant Multiplier
//  Provides simultaneous summed (X1 + X2, Y1 + Y2) and four-quadrant
//  multiplied ((X1 * X2)/5V, (Y1 * Y2)/5V) outputs for dual-channel
//  laser trajectory modulation and cross-axis vector synthesis.
// ─────────────────────────────────────────────────────────────────────

struct SumMultModule : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);
	}
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		X1_INPUT,
		Y1_INPUT,
		X2_INPUT,
		Y2_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		SUM_X_OUTPUT,
		SUM_Y_OUTPUT,
		MULT_X_OUTPUT,
		MULT_Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	SumMultModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs: Rack automatically appends "input" to tooltips
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");

		// Outputs: Rack automatically appends "output" to tooltips
		configOutput(SUM_X_OUTPUT, "X sum");
		configOutput(SUM_Y_OUTPUT, "Y sum");
		configOutput(MULT_X_OUTPUT, "X mult");
		configOutput(MULT_Y_OUTPUT, "Y mult");
	}

	void process(const ProcessArgs& args) override {
		int x1Channels = inputs[X1_INPUT].getChannels();
		int y1Channels = inputs[Y1_INPUT].getChannels();
		int x2Channels = inputs[X2_INPUT].getChannels();
		int y2Channels = inputs[Y2_INPUT].getChannels();

		int channels = std::max({x1Channels, y1Channels, x2Channels, y2Channels, 1});

		outputs[SUM_X_OUTPUT].setChannels(channels);
		outputs[SUM_Y_OUTPUT].setChannels(channels);
		outputs[MULT_X_OUTPUT].setChannels(channels);
		outputs[MULT_Y_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			// Inputs are distinct: no normalization between X and Y
			// Disconnected inputs return 0.0V
			float inX1 = inputs[X1_INPUT].getPolyVoltage(c);
			float inY1 = inputs[Y1_INPUT].getPolyVoltage(c);
			float inX2 = inputs[X2_INPUT].getPolyVoltage(c);
			float inY2 = inputs[Y2_INPUT].getPolyVoltage(c);

			// Precision Addition: X1 + X2, Y1 + Y2
			float sumX = inX1 + inX2;
			float sumY = inY1 + inY2;

			// Four-Quadrant Multiplication scaled by 5V standard:
			// (V1 * V2) / 5.0f
			float multX = (inX1 * inX2) * 0.2f;
			float multY = (inY1 * inY2) * 0.2f;

			// Laser galvanometer safety limits (-12V to +12V)
			outputs[SUM_X_OUTPUT].setVoltage(clamp(sumX, -12.f, 12.f), c);
			outputs[SUM_Y_OUTPUT].setVoltage(clamp(sumY, -12.f, 12.f), c);
			outputs[MULT_X_OUTPUT].setVoltage(clamp(multX, -12.f, 12.f), c);
			outputs[MULT_Y_OUTPUT].setVoltage(clamp(multY, -12.f, 12.f), c);
		}
	}
};

struct SumMultWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	SumMultModule::SkinTheme currentTheme = SumMultModule::THEME_MEDIUM;

	void setTheme(SumMultModule::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case SumMultModule::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/SumMult_Light.svg");
					break;
				case SumMultModule::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/SumMult_Dark.svg");
					break;
				case SumMultModule::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/SumMult.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<SumMultModule*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		SumMultModule* mod = dynamic_cast<SumMultModule*>(this->module);
		if (!mod) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			SumMultModule::SkinTheme theme = (SumMultModule::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	SumMultWidget(SumMultModule* module) {
		setModule(module);
		panel = new SvgPanel();
		SumMultModule::SkinTheme initTheme = module ? module->skinTheme : SumMultModule::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case SumMultModule::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/SumMult_Light.svg");
				break;
			case SumMultModule::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/SumMult_Dark.svg");
				break;
			case SumMultModule::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/SumMult.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Center column of jacks (x = 7.62 mm)
		// Pair 1: Input 1 (X1, Y1)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 23.00)), module, SumMultModule::X1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.50)), module, SumMultModule::Y1_INPUT));

		// Pair 2: Input 2 (X2, Y2)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 50.50)), module, SumMultModule::X2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 63.00)), module, SumMultModule::Y2_INPUT));

		// Pair 3: Sum Outputs (X1 + X2, Y1 + Y2)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 78.00)), module, SumMultModule::SUM_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.50)), module, SumMultModule::SUM_Y_OUTPUT));

		// Pair 4: Mult Outputs ((X1 * X2)/5V, (Y1 * Y2)/5V)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.50)), module, SumMultModule::MULT_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, SumMultModule::MULT_Y_OUTPUT));
	}
};

Model* modelSumMult = createModel<SumMultModule, SumMultWidget>("SumMult");
