#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Knots — X/Y Polar Angle Harmonic Multiplier & Lissajous Engine
//  Module multiplying polar angle velocities independently on X
//  and Y axes to generate intertwined Lissajous knots, kaleidoscopic
//  radial sectors, and harmonic loop trajectories.
// ─────────────────────────────────────────────────────────────────────

struct Knots : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum ParamId {
		X_KNOTS_PARAM,
		Y_KNOTS_PARAM,
		X_KNOTS_TRIM_PARAM,
		Y_KNOTS_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_KNOTS_CV_INPUT,
		Y_KNOTS_CV_INPUT,
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

	Knots() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Knots Multipliers (1.0 to 16.0 harmonics, default 1.0 for 1:1 bypass)
		configParam(X_KNOTS_PARAM, 1.f, 16.f, 1.f, "X knots", "", 0.f, 1.f, 0.f);
		configParam(Y_KNOTS_PARAM, 1.f, 16.f, 1.f, "Y knots", "", 0.f, 1.f, 0.f);

		// Attenuverters (default 0%)
		configParam(X_KNOTS_TRIM_PARAM, -1.f, 1.f, 0.f, "X knots CV depth", "%", 0.f, 100.f);
		configParam(Y_KNOTS_TRIM_PARAM, -1.f, 1.f, 0.f, "Y knots CV depth", "%", 0.f, 100.f);

		// Inputs (Rack automatically appends "input" to tooltips)
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_KNOTS_CV_INPUT, "X knots CV");
		configInput(Y_KNOTS_CV_INPUT, "Y knots CV");

		// Outputs (Rack automatically appends "output" to tooltips)
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
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int xCvChannels = inputs[X_KNOTS_CV_INPUT].getChannels();
		int yCvChannels = inputs[Y_KNOTS_CV_INPUT].getChannels();

		int numChannels = std::max({xChannels, yChannels, xCvChannels, yCvChannels, 1});

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float xKnotsParam = params[X_KNOTS_PARAM].getValue();
		float yKnotsParam = params[Y_KNOTS_PARAM].getValue();
		float xKnotsTrim = params[X_KNOTS_TRIM_PARAM].getValue();
		float yKnotsTrim = params[Y_KNOTS_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool cvYConnected = inputs[Y_KNOTS_CV_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float xKnotsCV = inputs[X_KNOTS_CV_INPUT].getPolyVoltage(c) / 5.f;
			float yKnotsCV = cvYConnected ? inputs[Y_KNOTS_CV_INPUT].getPolyVoltage(c) : xKnotsCV;

			float kX = clamp(xKnotsParam + xKnotsCV * xKnotsTrim * 8.f, 1.f, 16.f);
			float kY = clamp(yKnotsParam + yKnotsCV * yKnotsTrim * 8.f, 1.f, 16.f);

			// Polar transformation: distance r is preserved, angle theta is multiplied
			float r = std::hypot(inX, inY);
			float theta = std::atan2(inY, inX);

			// Polar angle harmonic multiplication & sector wrapping
			float thetaX = std::fmod((theta + (float)M_PI) * kX, 2.f * (float)M_PI);
			if (thetaX < 0.f) thetaX += 2.f * (float)M_PI;
			thetaX -= (float)M_PI;

			float thetaY = std::fmod((theta + (float)M_PI) * kY, 2.f * (float)M_PI);
			if (thetaY < 0.f) thetaY += 2.f * (float)M_PI;
			thetaY -= (float)M_PI;

			float outX = r * std::cos(thetaX);
			float outY = r * std::sin(thetaY);

			// Laser safety voltage bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.f, 12.f), c);
		}
	}
};

struct KnotsWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Knots::SkinTheme currentTheme = Knots::THEME_MEDIUM;

	void setTheme(Knots::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Knots::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Knots_Light.svg");
					break;
				case Knots::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Knots_Dark.svg");
					break;
				case Knots::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Knots.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Knots*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	KnotsWidget(Knots* module) {
		setModule(module);
				panel = new SvgPanel();
		Knots::SkinTheme initTheme = module ? module->skinTheme : Knots::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Knots::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Knots_Light.svg");
				break;
			case Knots::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Knots_Dark.svg");
				break;
			case Knots::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Knots.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Main Controls
		// Row 1: X Knots (7.62 mm) & Y Knots (22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Knots::X_KNOTS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Knots::Y_KNOTS_PARAM));

		// Attenuverter Trimpots
		// Trimpot Row 1: X Knots CV (7.62 mm), Y Knots CV (22.86 mm) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Knots::X_KNOTS_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Knots::Y_KNOTS_TRIM_PARAM));

		// Bottom I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Knots::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Knots::Y_INPUT));

		// Row 2: X Knots CV (X = 7.62 mm), Y Knots CV (X = 22.86 mm) (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Knots::X_KNOTS_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Knots::Y_KNOTS_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Knots::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Knots::Y_OUTPUT));
	}

	void appendContextMenu(ui::Menu* menu) override {
		Knots* module = dynamic_cast<Knots*>(this->module);
		if (!module) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			Knots::SkinTheme theme = (Knots::SkinTheme)i;
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

Model* modelKnots = createModel<Knots, KnotsWidget>("Knots");
