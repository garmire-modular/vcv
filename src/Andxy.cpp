#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Andxy — Dual X/Y Boolean AND / NAND Logic Module (3HP)
//  Evaluates boolean AND and inverted NAND logic on dual X/Y channels.
//  Comparator uses Schmitt trigger hysteresis: HIGH >= 1.5V, LOW <= 0.8V.
//  Outputs standard Eurorack 10V gate voltages.
// ─────────────────────────────────────────────────────────────────────

struct AndxyModule : Module {
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
		AND_X_OUTPUT,
		AND_Y_OUTPUT,
		NAND_X_OUTPUT,
		NAND_Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bool highX1[16] = {false};
	bool highY1[16] = {false};
	bool highX2[16] = {false};
	bool highY2[16] = {false};

	AndxyModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");

		// Outputs
		configOutput(AND_X_OUTPUT, "AND X");
		configOutput(AND_Y_OUTPUT, "AND Y");
		configOutput(NAND_X_OUTPUT, "NAND X");
		configOutput(NAND_Y_OUTPUT, "NAND Y");
	}

	void onReset() override {
		for (int i = 0; i < 16; i++) {
			highX1[i] = false;
			highY1[i] = false;
			highX2[i] = false;
			highY2[i] = false;
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

		outputs[AND_X_OUTPUT].setChannels(channels);
		outputs[AND_Y_OUTPUT].setChannels(channels);
		outputs[NAND_X_OUTPUT].setChannels(channels);
		outputs[NAND_Y_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			float vX1 = inputs[X1_INPUT].getPolyVoltage(c);
			float vY1 = inputs[Y1_INPUT].getPolyVoltage(c);
			float vX2 = inputs[X2_INPUT].getPolyVoltage(c);
			float vY2 = inputs[Y2_INPUT].getPolyVoltage(c);

			// Schmitt trigger comparator with hysteresis
			if (!highX1[c] && vX1 >= 1.5f) highX1[c] = true;
			else if (highX1[c] && vX1 <= 0.8f) highX1[c] = false;

			if (!highY1[c] && vY1 >= 1.5f) highY1[c] = true;
			else if (highY1[c] && vY1 <= 0.8f) highY1[c] = false;

			if (!highX2[c] && vX2 >= 1.5f) highX2[c] = true;
			else if (highX2[c] && vX2 <= 0.8f) highX2[c] = false;

			if (!highY2[c] && vY2 >= 1.5f) highY2[c] = true;
			else if (highY2[c] && vY2 <= 0.8f) highY2[c] = false;

			bool andX = highX1[c] && highX2[c];
			bool andY = highY1[c] && highY2[c];

			// Rapid switching: Condition met -> Input 2; Condition not met -> Input 1
			float outAndX = andX ? vX2 : vX1;
			float outAndY = andY ? vY2 : vY1;
			float outNandX = !andX ? vX2 : vX1;
			float outNandY = !andY ? vY2 : vY1;

			outputs[AND_X_OUTPUT].setVoltage(clamp(outAndX, -12.f, 12.f), c);
			outputs[AND_Y_OUTPUT].setVoltage(clamp(outAndY, -12.f, 12.f), c);
			outputs[NAND_X_OUTPUT].setVoltage(clamp(outNandX, -12.f, 12.f), c);
			outputs[NAND_Y_OUTPUT].setVoltage(clamp(outNandY, -12.f, 12.f), c);
		}
	}
};

struct AndxyWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	AndxyModule::SkinTheme currentTheme = AndxyModule::THEME_MEDIUM;

	void setTheme(AndxyModule::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case AndxyModule::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Andxy_Light.svg");
					break;
				case AndxyModule::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Andxy_Dark.svg");
					break;
				case AndxyModule::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Andxy.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<AndxyModule*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		AndxyModule* mod = dynamic_cast<AndxyModule*>(this->module);
		if (!mod) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			AndxyModule::SkinTheme theme = (AndxyModule::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	AndxyWidget(AndxyModule* module) {
		setModule(module);
		panel = new SvgPanel();
		AndxyModule::SkinTheme initTheme = module ? module->skinTheme : AndxyModule::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case AndxyModule::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Andxy_Light.svg");
				break;
			case AndxyModule::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Andxy_Dark.svg");
				break;
			case AndxyModule::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Andxy.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Center column of jacks (x = 7.62 mm)
		// Pair 1: Input 1 (X1, Y1)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 23.00)), module, AndxyModule::X1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.50)), module, AndxyModule::Y1_INPUT));

		// Pair 2: Input 2 (X2, Y2)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 50.50)), module, AndxyModule::X2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 63.00)), module, AndxyModule::Y2_INPUT));

		// Pair 3: AND Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 78.00)), module, AndxyModule::AND_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.50)), module, AndxyModule::AND_Y_OUTPUT));

		// Pair 4: NAND Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.50)), module, AndxyModule::NAND_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, AndxyModule::NAND_Y_OUTPUT));
	}
};

Model* modelAndxy = createModel<AndxyModule, AndxyWidget>("Andxy");
