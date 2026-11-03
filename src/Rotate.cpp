#include "plugin.hpp"
#include <cmath>

struct Rotate : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum ParamId {
		ROTATE_PARAM,
		AUTO_ROTATE_PARAM,
		ROTATE_CV_ATTEN_PARAM,
		AUTO_CV_ATTEN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		ROTATE_CV_INPUT,
		AUTO_CV_INPUT,
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

	float autoPhase[16] = {};

	Rotate() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters
		configParam(ROTATE_PARAM, -1.f, 1.f, 0.f, "Manual rotation", " turns");
		configParam(AUTO_ROTATE_PARAM, -10.f, 10.f, 0.f, "Auto-rotation speed", " Hz");
		configParam(ROTATE_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Rotation CV depth", "%", 0.f, 100.f);
		configParam(AUTO_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Auto-rotation CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(ROTATE_CV_INPUT, "Rotation CV");
		configInput(AUTO_CV_INPUT, "Auto-rotation CV");

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
		int channels = std::max({inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels(), inputs[ROTATE_CV_INPUT].getChannels(), inputs[AUTO_CV_INPUT].getChannels()});
		if (channels == 0) {
			channels = 1;
		}

		for (int c = 0; c < channels; c++) {
			// Normalisation: Y input normalises to X input if unpatched
			float xIn = inputs[X_INPUT].getPolyVoltage(c);
			float yIn = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : xIn;

			// Independent CV modulation inputs (Rotate CV & Auto-Rotate CV are distinct)
			float rotCv = inputs[ROTATE_CV_INPUT].getPolyVoltage(c);
			float autoCv = inputs[AUTO_CV_INPUT].getPolyVoltage(c);

			// Auto-rotation speed in Hz (modulated independently by Auto CV)
			float autoSpeed = params[AUTO_ROTATE_PARAM].getValue() + (autoCv / 5.f) * 10.f * params[AUTO_CV_ATTEN_PARAM].getValue();
			autoPhase[c] += autoSpeed * args.sampleTime;
			if (autoPhase[c] > 1.f) autoPhase[c] -= 1.f;
			if (autoPhase[c] < -1.f) autoPhase[c] += 1.f;

			// Total angle calculation (Rotate CV modulates static turn angle)
			float totalTurns = params[ROTATE_PARAM].getValue() + (rotCv / 5.f) * params[ROTATE_CV_ATTEN_PARAM].getValue() + autoPhase[c];
			float angle = totalTurns * 2.f * (float)M_PI;

			float cosA = std::cos(angle);
			float sinA = std::sin(angle);

			// 2D Rotation matrix: X' = cos(A)*X - sin(A)*Y, Y' = sin(A)*X + cos(A)*Y
			float xOut = cosA * xIn - sinA * yIn;
			float yOut = sinA * xIn + cosA * yIn;

			// Laser trajectory signal output (-12V to +12V galvo safety clamp)
			xOut = clamp(xOut, -12.f, 12.f);
			yOut = clamp(yOut, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(xOut, c);
			outputs[Y_OUTPUT].setVoltage(yOut, c);
		}

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
	}
};

struct RotateWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Rotate::SkinTheme currentTheme = Rotate::THEME_MEDIUM;

	void setTheme(Rotate::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Rotate::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Rotate_Light.svg");
					break;
				case Rotate::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Rotate_Dark.svg");
					break;
				case Rotate::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Rotate.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Rotate*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	RotateWidget(Rotate* module) {
		setModule(module);
				panel = new SvgPanel();
		Rotate::SkinTheme initTheme = module ? module->skinTheme : Rotate::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Rotate::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Rotate_Light.svg");
				break;
			case Rotate::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Rotate_Dark.svg");
				break;
			case Rotate::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Rotate.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: Rotation & Auto Knobs (Center Y = 0.85 in = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Rotate::ROTATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Rotate::AUTO_ROTATE_PARAM));

		// Row 2: CV Depth Trimpots (Center Y = 3.15 in = 80.01 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 80.01)), module, Rotate::ROTATE_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 80.01)), module, Rotate::AUTO_CV_ATTEN_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 3.70 in = 93.98 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 93.98)), module, Rotate::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 93.98)), module, Rotate::Y_INPUT));

		// Row 2: CV Inputs (Center Y = 4.15 in = 105.41 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.41)), module, Rotate::ROTATE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 105.41)), module, Rotate::AUTO_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 4.60 in = 116.84 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 116.84)), module, Rotate::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 116.84)), module, Rotate::Y_OUTPUT));
	}

	void appendContextMenu(ui::Menu* menu) override {
		Rotate* module = dynamic_cast<Rotate*>(this->module);
		if (!module) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			Rotate::SkinTheme theme = (Rotate::SkinTheme)i;
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

Model* modelRotate = createModel<Rotate, RotateWidget>("Rotate");
