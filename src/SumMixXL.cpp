#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Sum/Mix XL — 6-Channel Precision Adder & Four-Quadrant Multiplier
//  8HP utility providing simultaneous summed (Input 1 + Input 2) and
//  four-quadrant multiplied ((Input 1 * Input 2)/5V) outputs for
//  full Laser Trajectory (X, Y), Color (R, G, B), and Intensity/Z (I).
// ─────────────────────────────────────────────────────────────────────

struct SumMixXLModule : Module {
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
		// Column 1: Input 1
		X1_INPUT,
		Y1_INPUT,
		R1_INPUT,
		G1_INPUT,
		B1_INPUT,
		I1_INPUT,
		// Column 2: Input 2
		X2_INPUT,
		Y2_INPUT,
		R2_INPUT,
		G2_INPUT,
		B2_INPUT,
		I2_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Column 3: Sum Outputs
		SUM_X_OUTPUT,
		SUM_Y_OUTPUT,
		SUM_R_OUTPUT,
		SUM_G_OUTPUT,
		SUM_B_OUTPUT,
		SUM_I_OUTPUT,
		// Column 4: Mult Outputs
		MULT_X_OUTPUT,
		MULT_Y_OUTPUT,
		MULT_R_OUTPUT,
		MULT_G_OUTPUT,
		MULT_B_OUTPUT,
		MULT_I_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	SumMixXLModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Column 1: Input 1
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(R1_INPUT, "R 1");
		configInput(G1_INPUT, "G 1");
		configInput(B1_INPUT, "B 1");
		configInput(I1_INPUT, "I 1");

		// Column 2: Input 2
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");
		configInput(R2_INPUT, "R 2");
		configInput(G2_INPUT, "G 2");
		configInput(B2_INPUT, "B 2");
		configInput(I2_INPUT, "I 2");

		// Column 3: Sum Outputs
		configOutput(SUM_X_OUTPUT, "X sum");
		configOutput(SUM_Y_OUTPUT, "Y sum");
		configOutput(SUM_R_OUTPUT, "R sum");
		configOutput(SUM_G_OUTPUT, "G sum");
		configOutput(SUM_B_OUTPUT, "B sum");
		configOutput(SUM_I_OUTPUT, "I sum");

		// Column 4: Mult Outputs
		configOutput(MULT_X_OUTPUT, "X mult");
		configOutput(MULT_Y_OUTPUT, "Y mult");
		configOutput(MULT_R_OUTPUT, "R mult");
		configOutput(MULT_G_OUTPUT, "G mult");
		configOutput(MULT_B_OUTPUT, "B mult");
		configOutput(MULT_I_OUTPUT, "I mult");
	}

	void process(const ProcessArgs& args) override {
		int channels = 1;
		for (int i = 0; i < INPUTS_LEN; i++) {
			channels = std::max(channels, inputs[i].getChannels());
		}

		for (int o = 0; o < OUTPUTS_LEN; o++) {
			outputs[o].setChannels(channels);
		}

		for (int c = 0; c < channels; c++) {
			for (int k = 0; k < 6; k++) {
				float in1 = inputs[X1_INPUT + k].getPolyVoltage(c);
				float in2 = inputs[X2_INPUT + k].getPolyVoltage(c);

				// Precision Addition
				float sum = in1 + in2;

				// Four-Quadrant Multiplication scaled by 5V
				float mult = (in1 * in2) * 0.2f;

				outputs[SUM_X_OUTPUT + k].setVoltage(clamp(sum, -12.f, 12.f), c);
				outputs[MULT_X_OUTPUT + k].setVoltage(clamp(mult, -12.f, 12.f), c);
			}
		}
	}
};

struct SumMixXLWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	SumMixXLModule::SkinTheme currentTheme = SumMixXLModule::THEME_MEDIUM;

	void setTheme(SumMixXLModule::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case SumMixXLModule::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/SumMixXL_Light.svg");
					break;
				case SumMixXLModule::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/SumMixXL_Dark.svg");
					break;
				case SumMixXLModule::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/SumMixXL.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<SumMixXLModule*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		SumMixXLModule* mod = dynamic_cast<SumMixXLModule*>(this->module);
		if (!mod) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			SumMixXLModule::SkinTheme theme = (SumMixXLModule::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	SumMixXLWidget(SumMixXLModule* module) {
		setModule(module);
		panel = new SvgPanel();
		SumMixXLModule::SkinTheme initTheme = module ? module->skinTheme : SumMixXLModule::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case SumMixXLModule::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/SumMixXL_Light.svg");
				break;
			case SumMixXLModule::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/SumMixXL_Dark.svg");
				break;
			case SumMixXLModule::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/SumMixXL.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// 4 Columns: [6.07, 15.57, 25.07, 34.57] mm
		// 6 Rows: [27.00, 45.00, 63.00, 81.00, 99.00, 117.00] mm
		const double colX[4] = {6.07, 15.57, 25.07, 34.57};
		const double rowY[6] = {27.00, 45.00, 63.00, 81.00, 99.00, 117.00};

		// Column 1: Input 1
		for (int r = 0; r < 6; r++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[0], rowY[r])), module, SumMixXLModule::X1_INPUT + r));
		}

		// Column 2: Input 2
		for (int r = 0; r < 6; r++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[1], rowY[r])), module, SumMixXLModule::X2_INPUT + r));
		}

		// Column 3: Sum Output
		for (int r = 0; r < 6; r++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX[2], rowY[r])), module, SumMixXLModule::SUM_X_OUTPUT + r));
		}

		// Column 4: Mult Output
		for (int r = 0; r < 6; r++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX[3], rowY[r])), module, SumMixXLModule::MULT_X_OUTPUT + r));
		}
	}
};

Model* modelSumMixXL = createModel<SumMixXLModule, SumMixXLWidget>("SumMixXL");
