#include "plugin.hpp"

struct Scale : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum ParamId {
		X_SCALE_PARAM,
		X_CV_ATTEN_PARAM,
		Y_SCALE_PARAM,
		Y_CV_ATTEN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_CV_INPUT,
		Y_CV_INPUT,
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

	Scale() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters
		configParam(X_SCALE_PARAM, -2.f, 2.f, 1.f, "X scale");
		configParam(X_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "X scale CV depth", "%", 0.f, 100.f);
		configParam(Y_SCALE_PARAM, -2.f, 2.f, 1.f, "Y scale");
		configParam(Y_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Y scale CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_CV_INPUT, "X scale CV");
		configInput(Y_CV_INPUT, "Y scale CV");

		// Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);
	}



	void process(const ProcessArgs& args) override {
		int channels = std::max({inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels(), inputs[X_CV_INPUT].getChannels(), inputs[Y_CV_INPUT].getChannels()});
		if (channels == 0) {
			channels = 1;
		}

		for (int c = 0; c < channels; c++) {
			// Normalisation: Y input normalises to X input if unpatched
			float xIn = inputs[X_INPUT].getPolyVoltage(c);
			float yIn = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : xIn;

			// Normalisation: Y CV normalises to X CV if unpatched (switching jack logic)
			float xCv = inputs[X_CV_INPUT].getPolyVoltage(c);
			float yCv = inputs[Y_CV_INPUT].isConnected() ? inputs[Y_CV_INPUT].getPolyVoltage(c) : xCv;

			float xScale = params[X_SCALE_PARAM].getValue() + (xCv / 5.f) * params[X_CV_ATTEN_PARAM].getValue();
			float yScale = params[Y_SCALE_PARAM].getValue() + (yCv / 5.f) * params[Y_CV_ATTEN_PARAM].getValue();

			// Laser trajectory signal output (-12V to +12V limit)
			float xOut = clamp(xIn * xScale, -12.f, 12.f);
			float yOut = clamp(yIn * yScale, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(xOut, c);
			outputs[Y_OUTPUT].setVoltage(yOut, c);
		}

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
	}
};

struct ScaleWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Scale::SkinTheme currentTheme = Scale::THEME_MEDIUM;

	void setTheme(Scale::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Scale::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Scale_Light.svg");
					break;
				case Scale::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Scale_Dark.svg");
					break;
				case Scale::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Scale.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Scale*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	ScaleWidget(Scale* module) {
		setModule(module);
				panel = new SvgPanel();
		Scale::SkinTheme initTheme = module ? module->skinTheme : Scale::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Scale::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Scale_Light.svg");
				break;
			case Scale::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Scale_Dark.svg");
				break;
			case Scale::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Scale.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: Main Scale Knobs (Center Y = 0.85 in = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Scale::X_SCALE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Scale::Y_SCALE_PARAM));

		// Row 2: CV Depth Trimpots (Center Y = 3.15 in = 80.01 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 80.01)), module, Scale::X_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 80.01)), module, Scale::Y_CV_ATTEN_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 3.70 in = 93.98 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 93.98)), module, Scale::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 93.98)), module, Scale::Y_INPUT));

		// Row 2: CV Inputs (Center Y = 4.15 in = 105.41 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.41)), module, Scale::X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 105.41)), module, Scale::Y_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 4.60 in = 116.84 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 116.84)), module, Scale::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 116.84)), module, Scale::Y_OUTPUT));
	}

	void appendContextMenu(ui::Menu* menu) override {
		Scale* module = dynamic_cast<Scale*>(this->module);
		if (!module) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			Scale::SkinTheme theme = (Scale::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return module->skinTheme == theme; },
				[=]() {
					module->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}
};

Model* modelScale = createModel<Scale, ScaleWidget>("Scale");
