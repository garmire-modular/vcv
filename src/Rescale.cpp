#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Rescale — Multi-Range Precision Voltage Scaler, Normalizer & Converter
//  Module providing bidirectional conversion and summing between
//  standard Eurorack and laser voltage ranges:
//  - 0-1V   and ±1V
//  - 0-5V   and ±5V
//  - 0-10V  and ±10V
// ─────────────────────────────────────────────────────────────────────

struct RescaleModule : Module {
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
		// Row 1: 1V inputs
		UNI_1V_INPUT,
		BI_1V_INPUT,
		// Row 2: 5V inputs
		UNI_5V_INPUT,
		BI_5V_INPUT,
		// Row 3: 10V inputs
		UNI_10V_INPUT,
		BI_10V_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Row 4: 1V outputs
		UNI_1V_OUTPUT,
		BI_1V_OUTPUT,
		// Row 5: 5V outputs
		UNI_5V_OUTPUT,
		BI_5V_OUTPUT,
		// Row 6: 10V outputs
		UNI_10V_OUTPUT,
		BI_10V_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	RescaleModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs (Rack automatically appends "input" to tooltips)
		configInput(UNI_1V_INPUT, "0-1V");
		configInput(BI_1V_INPUT, "+/-1V");
		configInput(UNI_5V_INPUT, "0-5V");
		configInput(BI_5V_INPUT, "+/-5V");
		configInput(UNI_10V_INPUT, "0-10V");
		configInput(BI_10V_INPUT, "+/-10V");

		// Outputs (Rack automatically appends "output" to tooltips)
		configOutput(UNI_1V_OUTPUT, "0-1V");
		configOutput(BI_1V_OUTPUT, "+/-1V");
		configOutput(UNI_5V_OUTPUT, "0-5V");
		configOutput(BI_5V_OUTPUT, "+/-5V");
		configOutput(UNI_10V_OUTPUT, "0-10V");
		configOutput(BI_10V_OUTPUT, "+/-10V");
	}

	void process(const ProcessArgs& args) override {
		bool connUni1  = inputs[UNI_1V_INPUT].isConnected();
		bool connBi1   = inputs[BI_1V_INPUT].isConnected();
		bool connUni5  = inputs[UNI_5V_INPUT].isConnected();
		bool connBi5   = inputs[BI_5V_INPUT].isConnected();
		bool connUni10 = inputs[UNI_10V_INPUT].isConnected();
		bool connBi10  = inputs[BI_10V_INPUT].isConnected();

		bool hasAny = connUni1 || connBi1 || connUni5 || connBi5 || connUni10 || connBi10;
		bool hasUni = connUni1 || connUni5 || connUni10;
		bool hasBi  = connBi1  || connBi5  || connBi10;

		int channels = 1;
		if (hasAny) {
			for (int i = 0; i < INPUTS_LEN; i++) {
				if (inputs[i].isConnected()) {
					channels = std::max(channels, inputs[i].getChannels());
				}
			}
		}

		for (int i = 0; i < OUTPUTS_LEN; i++) {
			outputs[i].setChannels(channels);
		}

		if (!hasAny) {
			for (int c = 0; c < channels; c++) {
				outputs[UNI_1V_OUTPUT].setVoltage(0.0f, c);
				outputs[BI_1V_OUTPUT].setVoltage(0.0f, c);
				outputs[UNI_5V_OUTPUT].setVoltage(0.0f, c);
				outputs[BI_5V_OUTPUT].setVoltage(0.0f, c);
				outputs[UNI_10V_OUTPUT].setVoltage(0.0f, c);
				outputs[BI_10V_OUTPUT].setVoltage(0.0f, c);
			}
			return;
		}

		for (int c = 0; c < channels; c++) {
			// Sum unipolar normalized contributions (0V -> 0.0, Vmax -> 1.0)
			float sumUni = 0.0f;
			if (connUni1)  sumUni += inputs[UNI_1V_INPUT].getPolyVoltage(c) / 1.0f;
			if (connUni5)  sumUni += inputs[UNI_5V_INPUT].getPolyVoltage(c) / 5.0f;
			if (connUni10) sumUni += inputs[UNI_10V_INPUT].getPolyVoltage(c) / 10.0f;

			// Sum bipolar normalized contributions (0V -> 0.0, +Vmax -> +1.0, -Vmax -> -1.0)
			float sumBi = 0.0f;
			if (connBi1)  sumBi += inputs[BI_1V_INPUT].getPolyVoltage(c) / 1.0f;
			if (connBi5)  sumBi += inputs[BI_5V_INPUT].getPolyVoltage(c) / 5.0f;
			if (connBi10) sumBi += inputs[BI_10V_INPUT].getPolyVoltage(c) / 10.0f;

			float normUni = 0.0f;
			float normBi  = 0.0f;

			if (hasUni && !hasBi) {
				// Pure unipolar conversion
				normUni = sumUni;
				normBi  = 2.0f * sumUni - 1.0f;
			}
			else if (!hasUni && hasBi) {
				// Pure bipolar conversion
				normBi  = sumBi;
				normUni = (sumBi + 1.0f) * 0.5f;
			}
			else {
				// Combined unipolar carrier with bipolar modulation
				normUni = sumUni + sumBi;
				normBi  = (2.0f * sumUni - 1.0f) + sumBi;
			}

			// Drive all 6 outputs with laser galvanometer / Eurorack safety clamping (-12V to +12V)
			outputs[UNI_1V_OUTPUT].setVoltage(clamp(normUni * 1.0f, -12.0f, 12.0f), c);
			outputs[BI_1V_OUTPUT].setVoltage(clamp(normBi * 1.0f, -12.0f, 12.0f), c);

			outputs[UNI_5V_OUTPUT].setVoltage(clamp(normUni * 5.0f, -12.0f, 12.0f), c);
			outputs[BI_5V_OUTPUT].setVoltage(clamp(normBi * 5.0f, -12.0f, 12.0f), c);

			outputs[UNI_10V_OUTPUT].setVoltage(clamp(normUni * 10.0f, -12.0f, 12.0f), c);
			outputs[BI_10V_OUTPUT].setVoltage(clamp(normBi * 10.0f, -12.0f, 12.0f), c);
		}
	}
};

struct RescaleWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	RescaleModule::SkinTheme currentTheme = RescaleModule::THEME_MEDIUM;

	void setTheme(RescaleModule::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case RescaleModule::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Rescale_Light.svg");
					break;
				case RescaleModule::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Rescale_Dark.svg");
					break;
				case RescaleModule::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Rescale.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<RescaleModule*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		RescaleModule* mod = dynamic_cast<RescaleModule*>(this->module);
		if (!mod) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			RescaleModule::SkinTheme theme = (RescaleModule::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	RescaleWidget(RescaleModule* module) {
		setModule(module);
		panel = new SvgPanel();
		RescaleModule::SkinTheme initTheme = module ? module->skinTheme : RescaleModule::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case RescaleModule::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Rescale_Light.svg");
				break;
			case RescaleModule::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Rescale_Dark.svg");
				break;
			case RescaleModule::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Rescale.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Left Column: X = 7.62 mm | Right Column: X = 22.86 mm

		// Row 1 (Inputs 1V): Y = 32.00 mm
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 32.00)), module, RescaleModule::UNI_1V_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 32.00)), module, RescaleModule::BI_1V_INPUT));

		// Row 2 (Inputs 5V): Y = 47.00 mm
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 47.00)), module, RescaleModule::UNI_5V_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 47.00)), module, RescaleModule::BI_5V_INPUT));

		// Row 3 (Inputs 10V): Y = 62.00 mm
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 62.00)), module, RescaleModule::UNI_10V_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 62.00)), module, RescaleModule::BI_10V_INPUT));

		// Row 4 (Outputs 1V): Y = 88.00 mm
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 88.00)), module, RescaleModule::UNI_1V_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 88.00)), module, RescaleModule::BI_1V_OUTPUT));

		// Row 5 (Outputs 5V): Y = 103.00 mm
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 103.00)), module, RescaleModule::UNI_5V_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 103.00)), module, RescaleModule::BI_5V_OUTPUT));

		// Row 6 (Outputs 10V): Y = 118.00 mm (Fixed bottom anchor)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, RescaleModule::UNI_10V_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, RescaleModule::BI_10V_OUTPUT));
	}
};

Model* modelRescale = createModel<RescaleModule, RescaleWidget>("Rescale");
