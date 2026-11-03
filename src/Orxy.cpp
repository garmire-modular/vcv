#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Orxy — Dual X/Y Boolean OR / NOR Logic Module (3HP)
//  Evaluates boolean OR and inverted NOR logic on dual X/Y channels.
//  Comparator uses Schmitt trigger hysteresis: HIGH >= 1.5V, LOW <= 0.8V.
//  Outputs standard Eurorack 10V gate voltages.
// ─────────────────────────────────────────────────────────────────────

struct OrxyModule : Module {
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
		OR_X_OUTPUT,
		OR_Y_OUTPUT,
		NOR_X_OUTPUT,
		NOR_Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bool highX1[16] = {false};
	bool highY1[16] = {false};
	bool highX2[16] = {false};
	bool highY2[16] = {false};

	OrxyModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");

		// Outputs
		configOutput(OR_X_OUTPUT, "OR X");
		configOutput(OR_Y_OUTPUT, "OR Y");
		configOutput(NOR_X_OUTPUT, "NOR X");
		configOutput(NOR_Y_OUTPUT, "NOR Y");
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

		outputs[OR_X_OUTPUT].setChannels(channels);
		outputs[OR_Y_OUTPUT].setChannels(channels);
		outputs[NOR_X_OUTPUT].setChannels(channels);
		outputs[NOR_Y_OUTPUT].setChannels(channels);

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

			bool orX = highX1[c] || highX2[c];
			bool orY = highY1[c] || highY2[c];

			// Rapid switching: Condition met -> Input 2; Condition not met -> Input 1
			float outOrX = orX ? vX2 : vX1;
			float outOrY = orY ? vY2 : vY1;
			float outNorX = !orX ? vX2 : vX1;
			float outNorY = !orY ? vY2 : vY1;

			outputs[OR_X_OUTPUT].setVoltage(clamp(outOrX, -12.f, 12.f), c);
			outputs[OR_Y_OUTPUT].setVoltage(clamp(outOrY, -12.f, 12.f), c);
			outputs[NOR_X_OUTPUT].setVoltage(clamp(outNorX, -12.f, 12.f), c);
			outputs[NOR_Y_OUTPUT].setVoltage(clamp(outNorY, -12.f, 12.f), c);
		}
	}
};

struct OrxyWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	OrxyModule::SkinTheme currentTheme = OrxyModule::THEME_MEDIUM;

	void setTheme(OrxyModule::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case OrxyModule::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Orxy_Light.svg");
					break;
				case OrxyModule::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Orxy_Dark.svg");
					break;
				case OrxyModule::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Orxy.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<OrxyModule*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		OrxyModule* mod = dynamic_cast<OrxyModule*>(this->module);
		if (!mod) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			OrxyModule::SkinTheme theme = (OrxyModule::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	OrxyWidget(OrxyModule* module) {
		setModule(module);
		panel = new SvgPanel();
		OrxyModule::SkinTheme initTheme = module ? module->skinTheme : OrxyModule::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case OrxyModule::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Orxy_Light.svg");
				break;
			case OrxyModule::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Orxy_Dark.svg");
				break;
			case OrxyModule::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Orxy.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Center column of jacks (x = 7.62 mm)
		// Pair 1: Input 1 (X1, Y1)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 23.00)), module, OrxyModule::X1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.50)), module, OrxyModule::Y1_INPUT));

		// Pair 2: Input 2 (X2, Y2)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 50.50)), module, OrxyModule::X2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 63.00)), module, OrxyModule::Y2_INPUT));

		// Pair 3: OR Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 78.00)), module, OrxyModule::OR_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.50)), module, OrxyModule::OR_Y_OUTPUT));

		// Pair 4: NOR Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.50)), module, OrxyModule::NOR_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, OrxyModule::NOR_Y_OUTPUT));
	}
};

Model* modelOrxy = createModel<OrxyModule, OrxyWidget>("Orxy");
