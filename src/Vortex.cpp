#include "plugin.hpp"
#include <cmath>
#include <algorithm>

struct Vortex : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum ParamId {
		X_DEPTH_PARAM,
		Y_DEPTH_PARAM,
		X_COUNT_PARAM,
		Y_COUNT_PARAM,
		X_DEPTH_TRIM_PARAM,
		Y_DEPTH_TRIM_PARAM,
		X_COUNT_TRIM_PARAM,
		Y_COUNT_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_DEPTH_CV_INPUT,
		Y_DEPTH_CV_INPUT,
		X_COUNT_CV_INPUT,
		Y_COUNT_CV_INPUT,
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

	Vortex() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Vortex Depth (0.0 to 1.0 strength, default 0.0)
		configParam(X_DEPTH_PARAM, 0.f, 1.f, 0.f, "X Vortex Depth", "%", 0.f, 100.f);
		configParam(Y_DEPTH_PARAM, 0.f, 1.f, 0.f, "Y Vortex Depth", "%", 0.f, 100.f);

		// Vortex Count (1.0 to 8.0 harmonic multiplier, default 1.0)
		configParam(X_COUNT_PARAM, 1.f, 8.f, 1.f, "X Vortex Count", "", 0.f, 1.f);
		configParam(Y_COUNT_PARAM, 1.f, 8.f, 1.f, "Y Vortex Count", "", 0.f, 1.f);

		// CV Attenuverters (-1.0 to +1.0)
		configParam(X_DEPTH_TRIM_PARAM, -1.f, 1.f, 0.f, "X depth CV depth", "%", 0.f, 100.f);
		configParam(Y_DEPTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Y depth CV depth", "%", 0.f, 100.f);
		configParam(X_COUNT_TRIM_PARAM, -1.f, 1.f, 0.f, "X count CV depth", "%", 0.f, 100.f);
		configParam(Y_COUNT_TRIM_PARAM, -1.f, 1.f, 0.f, "Y count CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X Signal");
		configInput(Y_INPUT, "Y Signal (Normalizes from X)");
		configInput(X_DEPTH_CV_INPUT, "X Depth CV");
		configInput(Y_DEPTH_CV_INPUT, "Y Depth CV (Normalizes from X Depth CV)");
		configInput(X_COUNT_CV_INPUT, "X Count CV");
		configInput(Y_COUNT_CV_INPUT, "Y Count CV (Normalizes from X Count CV)");

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
		int xDepthCvChannels = inputs[X_DEPTH_CV_INPUT].getChannels();
		int yDepthCvChannels = inputs[Y_DEPTH_CV_INPUT].getChannels();
		int xCountCvChannels = inputs[X_COUNT_CV_INPUT].getChannels();
		int yCountCvChannels = inputs[Y_COUNT_CV_INPUT].getChannels();

		int numChannels = std::max({xChannels, yChannels, xDepthCvChannels, yDepthCvChannels, xCountCvChannels, yCountCvChannels, 1});

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float xDepthParam = params[X_DEPTH_PARAM].getValue();
		float yDepthParam = params[Y_DEPTH_PARAM].getValue();
		float xCountParam = params[X_COUNT_PARAM].getValue();
		float yCountParam = params[Y_COUNT_PARAM].getValue();

		float xDepthTrim = params[X_DEPTH_TRIM_PARAM].getValue();
		float yDepthTrim = params[Y_DEPTH_TRIM_PARAM].getValue();
		float xCountTrim = params[X_COUNT_TRIM_PARAM].getValue();
		float yCountTrim = params[Y_COUNT_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool cvYDepthConnected = inputs[Y_DEPTH_CV_INPUT].isConnected();
		bool cvYCountConnected = inputs[Y_COUNT_CV_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float xDepthCV = inputs[X_DEPTH_CV_INPUT].getPolyVoltage(c) / 5.f;
			float yDepthCV = cvYDepthConnected ? inputs[Y_DEPTH_CV_INPUT].getPolyVoltage(c) : xDepthCV;

			float xCountCV = inputs[X_COUNT_CV_INPUT].getPolyVoltage(c) / 5.f;
			float yCountCV = cvYCountConnected ? inputs[Y_COUNT_CV_INPUT].getPolyVoltage(c) : xCountCV;

			float depthX = clamp(xDepthParam + xDepthCV * xDepthTrim, 0.f, 1.f);
			float depthY = clamp(yDepthParam + yDepthCV * yDepthTrim, 0.f, 1.f);

			float countX = clamp(xCountParam + xCountCV * xCountTrim * 4.f, 1.f, 8.f);
			float countY = clamp(yCountParam + yCountCV * yCountTrim * 4.f, 1.f, 8.f);

			// Polar transformation
			float r = std::sqrt(inX * inX + inY * inY);
			float theta = std::atan2(inY, inX);

			// Vortex Phase Shift Modulation
			float phaseX = theta * countX;
			float phaseY = theta * countY;

			float deltaThetaX = depthX * std::sin(phaseX);
			float deltaThetaY = depthY * std::sin(phaseY);

			// Radial/Angular trajectory deformation
			float rX = r * (1.f + 0.2f * depthX * std::cos(phaseX));
			float rY = r * (1.f + 0.2f * depthY * std::cos(phaseY));

			float outX = rX * std::cos(theta + deltaThetaX);
			float outY = rY * std::sin(theta + deltaThetaY);

			// Laser safety voltage bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.f, 12.f), c);
		}
	}
};

struct VortexWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Vortex::SkinTheme currentTheme = Vortex::THEME_MEDIUM;

	void setTheme(Vortex::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Vortex::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Vortex_Light.svg");
					break;
				case Vortex::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Vortex_Dark.svg");
					break;
				case Vortex::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Vortex.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Vortex*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	VortexWidget(Vortex* module) {
		setModule(module);
				panel = new SvgPanel();
		Vortex::SkinTheme initTheme = module ? module->skinTheme : Vortex::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Vortex::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Vortex_Light.svg");
				break;
			case Vortex::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Vortex_Dark.svg");
				break;
			case Vortex::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Vortex.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Main Controls
		// Row 1: X DEPTH (7.62 mm) & Y DEPTH (22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Vortex::X_DEPTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Vortex::Y_DEPTH_PARAM));

		// Row 2: X COUNT (7.62 mm) & Y COUNT (22.86 mm) Knobs (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Vortex::X_COUNT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Vortex::Y_COUNT_PARAM));

		// Attenuverter Trimpots
		// Trimpot Row 1: X DEPTH CV (7.62 mm), Y DEPTH CV (22.86 mm) (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Vortex::X_DEPTH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Vortex::Y_DEPTH_TRIM_PARAM));

		// Trimpot Row 2: X COUNT CV (7.62 mm), Y COUNT CV (22.86 mm) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Vortex::X_COUNT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Vortex::Y_COUNT_TRIM_PARAM));

		// Bottom I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Vortex::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Vortex::Y_INPUT));

		// Row 2: X DEPTH CV (X = 7.62 mm), Y DEPTH CV (X = 22.86 mm) (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Vortex::X_DEPTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Vortex::Y_DEPTH_CV_INPUT));

		// Row 3: X COUNT CV (X = 7.62 mm), Y COUNT CV (X = 22.86 mm) (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Vortex::X_COUNT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Vortex::Y_COUNT_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Vortex::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Vortex::Y_OUTPUT));
	}

	void appendContextMenu(ui::Menu* menu) override {
		Vortex* module = dynamic_cast<Vortex*>(this->module);
		if (!module) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			Vortex::SkinTheme theme = (Vortex::SkinTheme)i;
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

Model* modelVortex = createModel<Vortex, VortexWidget>("Vortex");
