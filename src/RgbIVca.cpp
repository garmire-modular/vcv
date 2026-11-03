#include "plugin.hpp"
#include "core/RgbIVcaEngine.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  RGB/I/VCA — 3HP Triple VCA for Laser Z / Intensity Modulation &
//              RGB Signal Scaling
// ─────────────────────────────────────────────────────────────────────

struct RgbIVca : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum ParamId {
		I_ATTV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		I_INPUT,
		R_INPUT,
		G_INPUT,
		B_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	rgbivca::Engine engine;

	RgbIVca() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// I Attenuverter (-100% to +100%, defaults to +100% unity gain)
		configParam(I_ATTV_PARAM, -1.f, 1.f, 1.f, "Intensity CV depth", "%", 0.f, 100.f);

		// Inputs: Rack automatically appends "input" in tooltips
		configInput(I_INPUT, "Intensity CV");
		configInput(R_INPUT, "Red");
		configInput(G_INPUT, "Green");
		configInput(B_INPUT, "Blue");

		// Outputs: Rack automatically appends "output" in tooltips
		configOutput(R_OUTPUT, "Red");
		configOutput(G_OUTPUT, "Green");
		configOutput(B_OUTPUT, "Blue");
	}

	void onReset() override {
		engine.scaleMode = rgbivca::SCALE_UNSCALED;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "scaleMode", json_integer((int)engine.scaleMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);
		json_t* modeJ = json_object_get(rootJ, "scaleMode");
		if (modeJ) {
			int m = json_integer_value(modeJ);
			if (m >= 0 && m <= 3) {
				engine.scaleMode = (rgbivca::ScaleMode)m;
			}
		}
	}

	void process(const ProcessArgs& args) override {
		int iCh = inputs[I_INPUT].getChannels();
		int rCh = inputs[R_INPUT].getChannels();
		int gCh = inputs[G_INPUT].getChannels();
		int bCh = inputs[B_INPUT].getChannels();

		int channels = std::max({iCh, rCh, gCh, bCh, 1});

		outputs[R_OUTPUT].setChannels(channels);
		outputs[G_OUTPUT].setChannels(channels);
		outputs[B_OUTPUT].setChannels(channels);

		bool iConnected = inputs[I_INPUT].isConnected();
		float attv = params[I_ATTV_PARAM].getValue();

		for (int c = 0; c < channels; c++) {
			float inI = iConnected ? inputs[I_INPUT].getPolyVoltage(iCh > 1 ? c : 0) : 5.0f;
			float inR = inputs[R_INPUT].getPolyVoltage(c);
			float inG = inputs[G_INPUT].getPolyVoltage(c);
			float inB = inputs[B_INPUT].getPolyVoltage(c);

			auto res = engine.process(inR, inG, inB, inI, iConnected, attv);

			outputs[R_OUTPUT].setVoltage(res.r, c);
			outputs[G_OUTPUT].setVoltage(res.g, c);
			outputs[B_OUTPUT].setVoltage(res.b, c);
		}
	}
};

struct RgbIVcaWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	RgbIVca::SkinTheme currentTheme = RgbIVca::THEME_MEDIUM;

	void setTheme(RgbIVca::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case RgbIVca::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/RgbIVca_Light.svg");
					break;
				case RgbIVca::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/RgbIVca_Dark.svg");
					break;
				case RgbIVca::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/RgbIVca.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<RgbIVca*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	RgbIVcaWidget(RgbIVca* module) {
		setModule(module);
				panel = new SvgPanel();
		RgbIVca::SkinTheme initTheme = module ? module->skinTheme : RgbIVca::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case RgbIVca::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/RgbIVca_Light.svg");
				break;
			case RgbIVca::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/RgbIVca_Dark.svg");
				break;
			case RgbIVca::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/RgbIVca.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Section 1: Intensity / I Modulation
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, RgbIVca::I_ATTV_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.00)), module, RgbIVca::I_INPUT));

		// Section 2: RGB Inputs
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 49.00)), module, RgbIVca::R_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 61.50)), module, RgbIVca::G_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 74.00)), module, RgbIVca::B_INPUT));

		// Section 3: RGB Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 95.00)), module, RgbIVca::R_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 106.50)), module, RgbIVca::G_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, RgbIVca::B_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		RgbIVca* module = dynamic_cast<RgbIVca*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("RGB Signal Scaling"));

		const char* const scaleLabels[] = {
			"Unscaled",
			"Scale to +/-1V",
			"Scale to +/-5V",
			"Scale to +/-10V"
		};

		for (int i = 0; i < 4; ++i) {
			rgbivca::ScaleMode mode = (rgbivca::ScaleMode)i;
			menu->addChild(createCheckMenuItem(scaleLabels[i], "",
				[=]() { return module->engine.scaleMode == mode; },
				[=]() { module->engine.scaleMode = mode; }
			));
		}

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			RgbIVca::SkinTheme theme = (RgbIVca::SkinTheme)i;
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

Model* modelRgbIVca = createModel<RgbIVca, RgbIVcaWidget>("RgbIVca");
