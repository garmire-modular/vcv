#include "plugin.hpp"

struct Crest : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum ParamId {
		CREST_X_PARAM,
		CREST_Y_PARAM,
		TILT_X_PARAM,
		TILT_Y_PARAM,
		CREST_X_TRIM_PARAM,
		CREST_Y_TRIM_PARAM,
		TILT_X_TRIM_PARAM,
		TILT_Y_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		CREST_X_CV_INPUT,
		CREST_Y_CV_INPUT,
		TILT_X_CV_INPUT,
		TILT_Y_CV_INPUT,
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

	Crest() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Crest params (0 to 5, default 0.0)
		configParam(CREST_X_PARAM, 0.f, 5.f, 0.f, "X Crest", "%", 0.f, 100.f / 5.f);
		configParam(CREST_Y_PARAM, 0.f, 5.f, 0.f, "Y Crest", "%", 0.f, 100.f / 5.f);

		// Tilt params (-5V to +5V, default 0V)
		configParam(TILT_X_PARAM, -5.f, 5.f, 0.f, "X Tilt", " V");
		configParam(TILT_Y_PARAM, -5.f, 5.f, 0.f, "Y Tilt", " V");

		// Crest CV Attenuverters (-1 to +1, default 0.0)
		configParam(CREST_X_TRIM_PARAM, -1.f, 1.f, 0.f, "X crest CV depth", "%", 0.f, 100.f);
		configParam(CREST_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Y crest CV depth", "%", 0.f, 100.f);

		// Tilt CV Attenuverters (-1 to +1, default 0.0)
		configParam(TILT_X_TRIM_PARAM, -1.f, 1.f, 0.f, "X tilt CV depth", "%", 0.f, 100.f);
		configParam(TILT_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Y tilt CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X Signal");
		configInput(Y_INPUT, "Y Signal (Normalizes from X)");
		configInput(CREST_X_CV_INPUT, "X Crest CV");
		configInput(CREST_Y_CV_INPUT, "Y Crest CV (Normalizes from X)");
		configInput(TILT_X_CV_INPUT, "X Tilt CV");
		configInput(TILT_Y_CV_INPUT, "Y Tilt CV (Normalizes from X)");

		// Outputs
		configOutput(X_OUTPUT, "X Signal");
		configOutput(Y_OUTPUT, "Y Signal");
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
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int crestXChannels = inputs[CREST_X_CV_INPUT].getChannels();
		int crestYChannels = inputs[CREST_Y_CV_INPUT].getChannels();
		int tiltXChannels = inputs[TILT_X_CV_INPUT].getChannels();
		int tiltYChannels = inputs[TILT_Y_CV_INPUT].getChannels();

		int channels = std::max({xChannels, yChannels, crestXChannels, crestYChannels, tiltXChannels, tiltYChannels, 1});

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float crestXParam = params[CREST_X_PARAM].getValue();
		float crestYParam = params[CREST_Y_PARAM].getValue();
		float tiltXParam = params[TILT_X_PARAM].getValue();
		float tiltYParam = params[TILT_Y_PARAM].getValue();

		float crestXTrim = params[CREST_X_TRIM_PARAM].getValue();
		float crestYTrim = params[CREST_Y_TRIM_PARAM].getValue();
		float tiltXTrim = params[TILT_X_TRIM_PARAM].getValue();
		float tiltYTrim = params[TILT_Y_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool crestYCvConnected = inputs[CREST_Y_CV_INPUT].isConnected();
		bool tiltYCvConnected = inputs[TILT_Y_CV_INPUT].isConnected();

		for (int c = 0; c < channels; c++) {
			// X & Y Inputs
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			// Crest CVs
			float crestXCv = inputs[CREST_X_CV_INPUT].getPolyVoltage(c);
			float crestYCv = crestYCvConnected ? inputs[CREST_Y_CV_INPUT].getPolyVoltage(c) : crestXCv;

			// Tilt CVs
			float tiltXCv = inputs[TILT_X_CV_INPUT].getPolyVoltage(c);
			float tiltYCv = tiltYCvConnected ? inputs[TILT_Y_CV_INPUT].getPolyVoltage(c) : tiltXCv;

			// Effective Crest (normalized 0.0 to 1.0+)
			float normCrestX = std::max(0.f, (crestXParam / 5.f) + (crestXCv / 5.f) * crestXTrim);
			float normCrestY = std::max(0.f, (crestYParam / 5.f) + (crestYCv / 5.f) * crestYTrim);

			// Effective Tilt
			float tiltX = tiltXParam + tiltXCv * tiltXTrim;
			float tiltY = tiltYParam + tiltYCv * tiltYTrim;

			// Signal-Aware 2-Cycle Bounded Waveshaper calculation
			auto processChannel = [](float inVal, float normCrest, float tiltVal) -> float {
				// Shift input by Tilt
				float shifted = inVal + tiltVal;

				// Amplitude Boundary Preservation:
				// Pin upper bound to peak amplitude of input signal (minimum 5.0V baseline)
				float vPeak = std::max(std::abs(inVal), 5.f);

				// 3x fold range multiplier: k goes from 1.0 at 0% Crest up to 10.0 (6 full trigonometric cycles) at 100% Crest
				float k = 1.f + 9.f * normCrest;

				// Normalized phase angle inside [-1, +1] range
				float xNorm = shifted / vPeak;

				// Sinusoidal transfer function strictly bounded within [-vPeak, +vPeak]
				float folded = vPeak * std::sin((float)M_PI_2 * k * xNorm);

				// 0% Crest Behavior:
				// 0% is 100% dry signal (inVal). Fades smoothly over first 5% (normCrest 0.0 -> 0.05) into folded signal.
				float outVal;
				if (normCrest <= 0.05f) {
					float blend = normCrest / 0.05f;
					outVal = (1.f - blend) * inVal + blend * folded;
				} else {
					outVal = folded;
				}

				return outVal;
			};

			float outX = processChannel(inX, normCrestX, tiltX);
			float outY = processChannel(inY, normCrestY, tiltY);

			// Safety clamping for laser galvo drives (-12V to +12V)
			outX = clamp(outX, -12.f, 12.f);
			outY = clamp(outY, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(outX, c);
			outputs[Y_OUTPUT].setVoltage(outY, c);
		}
	}
};

struct CrestWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Crest::SkinTheme currentTheme = Crest::THEME_MEDIUM;

	void setTheme(Crest::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Crest::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Crest_Light.svg");
					break;
				case Crest::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Crest_Dark.svg");
					break;
				case Crest::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Crest.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Crest*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	CrestWidget(Crest* module) {
		setModule(module);
				panel = new SvgPanel();
		Crest::SkinTheme initTheme = module ? module->skinTheme : Crest::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Crest::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Crest_Light.svg");
				break;
			case Crest::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Crest_Dark.svg");
				break;
			case Crest::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Crest.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: X & Y Crest Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Crest::CREST_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Crest::CREST_Y_PARAM));

		// Row 2: X & Y Tilt Knobs (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Crest::TILT_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Crest::TILT_Y_PARAM));

		// Row 3: Crest CV Depth Trimpots (Aligned with Smooth/Wiggle: Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Crest::CREST_X_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Crest::CREST_Y_TRIM_PARAM));

		// Row 4: Tilt CV Depth Trimpots (Aligned with Smooth/Wiggle: Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Crest::TILT_X_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Crest::TILT_Y_TRIM_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Crest::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Crest::Y_INPUT));

		// Row 2: Crest CV Inputs (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Crest::CREST_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Crest::CREST_Y_CV_INPUT));

		// Row 3: Tilt CV Inputs (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Crest::TILT_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Crest::TILT_Y_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Crest::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Crest::Y_OUTPUT));
	}

	void appendContextMenu(ui::Menu* menu) override {
		Crest* module = dynamic_cast<Crest*>(this->module);
		if (!module) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			Crest::SkinTheme theme = (Crest::SkinTheme)i;
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

Model* modelCrest = createModel<Crest, CrestWidget>("Crest");
