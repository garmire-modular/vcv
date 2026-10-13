#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>

namespace {
inline float clampf(float v, float lo, float hi) {
	return std::max(lo, std::min(v, hi));
}
} // namespace

struct Panopia : Module {
	enum ParamIds {
		// Row 1 Knobs: X-Axis, Y-Axis, Rotation
		X_AXIS_PARAM,
		Y_AXIS_PARAM,
		ROTATION_PARAM,

		// Row 2 Knobs: Focus, Center, Smooth
		FOCUS_PARAM,
		CENTER_PARAM,
		SMOOTH_PARAM,

		// Attenuverter Row 1: X, Y, Rotation CV Depth
		X_AXIS_TRIM_PARAM,
		Y_AXIS_TRIM_PARAM,
		ROTATION_TRIM_PARAM,

		// Attenuverter Row 2: Focus, Center, Smooth CV Depth
		FOCUS_TRIM_PARAM,
		CENTER_TRIM_PARAM,
		SMOOTH_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputIds {
		// CV Inputs: Row 1
		X_AXIS_CV_INPUT,
		Y_AXIS_CV_INPUT,
		ROTATION_CV_INPUT,

		// CV Inputs: Row 2
		FOCUS_CV_INPUT,
		CENTER_CV_INPUT,
		SMOOTH_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputIds {
		// CV Outputs: Row 1
		X_AXIS_OUTPUT,
		Y_AXIS_OUTPUT,
		ROTATION_OUTPUT,

		// CV Outputs: Row 2
		FOCUS_OUTPUT,
		CENTER_OUTPUT,
		SMOOTH_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightIds {
		LIGHTS_LEN
	};

	enum VoltageRange {
		RANGE_BIPOLAR_5 = 0,   // ±5V (-5V to +5V)
		RANGE_BIPOLAR_10 = 1,  // ±10V (-10V to +10V)
		RANGE_UNIPOLAR_10 = 2  // 0V to +10V
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT = 1,
		THEME_DARK = 2
	};

	VoltageRange voltageRange = RANGE_BIPOLAR_5;
	SkinTheme skinTheme = THEME_MEDIUM;

	// Custom ParamQuantity for X-Axis and Y-Axis: -1.00 to 1.00
	struct BipolarAxisParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", getValue());
			return buf;
		}
	};

	// Custom ParamQuantity for Rotation: -180.00° to +180.00°
	struct RotationParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", getValue());
			return buf;
		}
	};

	// Custom ParamQuantity for Focus & Center: 0.00 to 100.00
	struct PercentParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", getValue());
			return buf;
		}
	};

	// Custom ParamQuantity for Smooth: 0% to 100%
	struct SmoothParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", getValue());
			return buf;
		}
	};

	Panopia() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1 Knobs
		configParam<BipolarAxisParamQuantity>(X_AXIS_PARAM, -1.0f, 1.0f, 0.0f, "X-axis", "");
		configParam<BipolarAxisParamQuantity>(Y_AXIS_PARAM, -1.0f, 1.0f, 0.0f, "Y-axis", "");
		configParam<RotationParamQuantity>(ROTATION_PARAM, -180.0f, 180.0f, 0.0f, "Rotation", "°");

		// Row 2 Knobs (Ableton Live default values: Focus 50.0, Center 50.0, Smooth 20.0)
		configParam<PercentParamQuantity>(FOCUS_PARAM, 0.0f, 100.0f, 50.0f, "Focus", "");
		configParam<PercentParamQuantity>(CENTER_PARAM, 0.0f, 100.0f, 50.0f, "Center", "");
		configParam<SmoothParamQuantity>(SMOOTH_PARAM, 0.0f, 100.0f, 20.0f, "Smooth", "%");

		// Attenuverter Trimpots (Strict AGENTS.md rule: "<Parameter> CV depth", range -1.0 to 1.0)
		configParam(X_AXIS_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "X-axis CV depth", "");
		configParam(Y_AXIS_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Y-axis CV depth", "");
		configParam(ROTATION_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Rotation CV depth", "");
		configParam(FOCUS_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Focus CV depth", "");
		configParam(CENTER_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Center CV depth", "");
		configParam(SMOOTH_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Smooth CV depth", "");

		// CV Inputs (Strict sentence case, no "input" suffix per AGENTS.md rule)
		configInput(X_AXIS_CV_INPUT, "X-axis CV");
		configInput(Y_AXIS_CV_INPUT, "Y-axis CV");
		configInput(ROTATION_CV_INPUT, "Rotation CV");
		configInput(FOCUS_CV_INPUT, "Focus CV");
		configInput(CENTER_CV_INPUT, "Center CV");
		configInput(SMOOTH_CV_INPUT, "Smooth CV");

		// CV Outputs (Strict sentence case, no "output" suffix per AGENTS.md rule)
		configOutput(X_AXIS_OUTPUT, "X-axis");
		configOutput(Y_AXIS_OUTPUT, "Y-axis");
		configOutput(ROTATION_OUTPUT, "Rotation");
		configOutput(FOCUS_OUTPUT, "Focus");
		configOutput(CENTER_OUTPUT, "Center");
		configOutput(SMOOTH_OUTPUT, "Smooth");
	}

	// Helper to scale normalized [-1.0, 1.0] or [0.0, 1.0] to selected output voltage range
	float scaleOutput(float normVal, bool isBipolar) {
		switch (voltageRange) {
			case RANGE_BIPOLAR_10:
				// ±10V range
				if (isBipolar) {
					return normVal * 10.0f; // [-1, 1] -> [-10V, +10V]
				} else {
					return normVal * 10.0f; // [0, 1] -> [0V, +10V]
				}
			case RANGE_UNIPOLAR_10:
				// 0 to 10V unipolar
				if (isBipolar) {
					// map [-1, 1] -> [0V, 10V]
					return (normVal + 1.0f) * 5.0f;
				} else {
					return normVal * 10.0f; // [0, 1] -> [0V, +10V]
				}
			case RANGE_BIPOLAR_5:
			default:
				// ±5V range
				if (isBipolar) {
					return normVal * 5.0f; // [-1, 1] -> [-5V, +5V]
				} else {
					// unipolar 0 to 1 maps to 0 to 10V standard Eurorack control
					return normVal * 10.0f; // [0, 1] -> [0V, +10V]
				}
		}
	}

	void process(const ProcessArgs& args) override {
		// Calculate polyphony from connected inputs (default 1 channel)
		int channels = 1;
		for (int i = 0; i < INPUTS_LEN; i++) {
			int ch = inputs[i].getChannels();
			if (ch > channels) {
				channels = ch;
			}
		}

		for (int c = 0; c < channels; c++) {
			// 1. X-Axis: Knob [-1.0, 1.0] + CV * attenuverter (5V CV sensitivity = full span 1.0)
			float xKnob = params[X_AXIS_PARAM].getValue();
			float xTrim = params[X_AXIS_TRIM_PARAM].getValue();
			float xCv = inputs[X_AXIS_CV_INPUT].getPolyVoltage(c);
			float xEff = clampf(xKnob + (xCv / 5.0f) * xTrim, -1.0f, 1.0f);
			outputs[X_AXIS_OUTPUT].setVoltage(scaleOutput(xEff, true), c);

			// 2. Y-Axis: Knob [-1.0, 1.0] + CV * attenuverter
			float yKnob = params[Y_AXIS_PARAM].getValue();
			float yTrim = params[Y_AXIS_TRIM_PARAM].getValue();
			float yCv = inputs[Y_AXIS_CV_INPUT].getPolyVoltage(c);
			float yEff = clampf(yKnob + (yCv / 5.0f) * yTrim, -1.0f, 1.0f);
			outputs[Y_AXIS_OUTPUT].setVoltage(scaleOutput(yEff, true), c);

			// 3. Rotation: Knob [-180.0, 180.0] + CV * attenuverter (5V CV sensitivity = 180 deg)
			float rotKnob = params[ROTATION_PARAM].getValue();
			float rotTrim = params[ROTATION_TRIM_PARAM].getValue();
			float rotCv = inputs[ROTATION_CV_INPUT].getPolyVoltage(c);
			float rotDeg = clampf(rotKnob + (rotCv / 5.0f) * 180.0f * rotTrim, -180.0f, 180.0f);
			float rotNorm = rotDeg / 180.0f; // normalized [-1.0, 1.0]
			outputs[ROTATION_OUTPUT].setVoltage(scaleOutput(rotNorm, true), c);

			// 4. Focus: Knob [0.0, 100.0] + CV * attenuverter (5V CV sensitivity = full 100)
			float focKnob = params[FOCUS_PARAM].getValue();
			float focTrim = params[FOCUS_TRIM_PARAM].getValue();
			float focCv = inputs[FOCUS_CV_INPUT].getPolyVoltage(c);
			float focEff = clampf(focKnob + (focCv / 5.0f) * 100.0f * focTrim, 0.0f, 100.0f);
			float focNorm = focEff / 100.0f; // normalized [0.0, 1.0]
			outputs[FOCUS_OUTPUT].setVoltage(scaleOutput(focNorm, false), c);

			// 5. Center: Knob [0.0, 100.0] + CV * attenuverter (5V CV sensitivity = full 100)
			float ctrKnob = params[CENTER_PARAM].getValue();
			float ctrTrim = params[CENTER_TRIM_PARAM].getValue();
			float ctrCv = inputs[CENTER_CV_INPUT].getPolyVoltage(c);
			float ctrEff = clampf(ctrKnob + (ctrCv / 5.0f) * 100.0f * ctrTrim, 0.0f, 100.0f);
			float ctrNorm = ctrEff / 100.0f; // normalized [0.0, 1.0]
			outputs[CENTER_OUTPUT].setVoltage(scaleOutput(ctrNorm, false), c);

			// 6. Smooth: Knob [0.0, 100.0] + CV * attenuverter (5V CV sensitivity = full 100)
			float smhKnob = params[SMOOTH_PARAM].getValue();
			float smhTrim = params[SMOOTH_TRIM_PARAM].getValue();
			float smhCv = inputs[SMOOTH_CV_INPUT].getPolyVoltage(c);
			float smhEff = clampf(smhKnob + (smhCv / 5.0f) * 100.0f * smhTrim, 0.0f, 100.0f);
			float smhNorm = smhEff / 100.0f; // normalized [0.0, 1.0]
			outputs[SMOOTH_OUTPUT].setVoltage(scaleOutput(smhNorm, false), c);
		}

		for (int o = 0; o < OUTPUTS_LEN; o++) {
			outputs[o].setChannels(channels);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) {
			voltageRange = (VoltageRange)json_integer_value(vrJ);
		}
		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) {
			skinTheme = (SkinTheme)json_integer_value(stJ);
		}
	}
};

// ── Custom Hardware Knob Widget (Matching Polly Look & Feel) ──
struct PanopiaCustomKnob : RoundKnob {
	PanopiaCustomKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

// ── Custom Hardware Attenuverter Widget (Matching Polly Look & Feel) ──
struct PanopiaCustomTrimpot : RoundKnob {
	PanopiaCustomTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

// ── Custom Hardware Jack Port Widget (3.5mm Eurorack Jack) ──
struct PanopiaCustomPort : app::SvgPort {
	PanopiaCustomPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

struct PanopiaWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Panopia::SkinTheme currentTheme = Panopia::THEME_MEDIUM;

	void setTheme(Panopia::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Panopia::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Panopia_Light.svg");
					break;
				case Panopia::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Panopia_Dark.svg");
					break;
				case Panopia::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Panopia.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		Panopia* module = dynamic_cast<Panopia*>(this->module);
		if (module && module->skinTheme != currentTheme) {
			setTheme(module->skinTheme);
		}
	}

	PanopiaWidget(Panopia* module) {
		setModule(module);
		panel = new SvgPanel();
		Panopia::SkinTheme initTheme = module ? module->skinTheme : Panopia::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Panopia::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Panopia_Light.svg");
				break;
			case Panopia::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Panopia_Dark.svg");
				break;
			case Panopia::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Panopia.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// 3 Primary Columns across 12 HP (60.96 mm):
		// Pitch = 18.48 mm, Left margin = 12.00 mm
		const float col_x[3] = { 12.00f, 30.48f, 48.96f };

		// Row 1 Knobs: X-Axis, Y-Axis, Rotation (Center Y = 21.50 mm)
		addParam(createParamCentered<PanopiaCustomKnob>(mm2px(Vec(col_x[0], 21.50f)), module, Panopia::X_AXIS_PARAM));
		addParam(createParamCentered<PanopiaCustomKnob>(mm2px(Vec(col_x[1], 21.50f)), module, Panopia::Y_AXIS_PARAM));
		addParam(createParamCentered<PanopiaCustomKnob>(mm2px(Vec(col_x[2], 21.50f)), module, Panopia::ROTATION_PARAM));

		// Row 2 Knobs: Focus, Center, Smooth (Center Y = 41.50 mm)
		addParam(createParamCentered<PanopiaCustomKnob>(mm2px(Vec(col_x[0], 41.50f)), module, Panopia::FOCUS_PARAM));
		addParam(createParamCentered<PanopiaCustomKnob>(mm2px(Vec(col_x[1], 41.50f)), module, Panopia::CENTER_PARAM));
		addParam(createParamCentered<PanopiaCustomKnob>(mm2px(Vec(col_x[2], 41.50f)), module, Panopia::SMOOTH_PARAM));

		// Attenuverter Row 1: X, Y, Rotation (Center Y = 63.00 mm)
		addParam(createParamCentered<PanopiaCustomTrimpot>(mm2px(Vec(col_x[0], 63.00f)), module, Panopia::X_AXIS_TRIM_PARAM));
		addParam(createParamCentered<PanopiaCustomTrimpot>(mm2px(Vec(col_x[1], 63.00f)), module, Panopia::Y_AXIS_TRIM_PARAM));
		addParam(createParamCentered<PanopiaCustomTrimpot>(mm2px(Vec(col_x[2], 63.00f)), module, Panopia::ROTATION_TRIM_PARAM));

		// Attenuverter Row 2: Focus, Center, Smooth (Center Y = 74.00 mm)
		addParam(createParamCentered<PanopiaCustomTrimpot>(mm2px(Vec(col_x[0], 74.00f)), module, Panopia::FOCUS_TRIM_PARAM));
		addParam(createParamCentered<PanopiaCustomTrimpot>(mm2px(Vec(col_x[1], 74.00f)), module, Panopia::CENTER_TRIM_PARAM));
		addParam(createParamCentered<PanopiaCustomTrimpot>(mm2px(Vec(col_x[2], 74.00f)), module, Panopia::SMOOTH_TRIM_PARAM));

		// Jack Row 1 (Inputs 1-3): X, Y, Rotation (Center Y = 89.50 mm)
		addInput(createInputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[0], 89.50f)), module, Panopia::X_AXIS_CV_INPUT));
		addInput(createInputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[1], 89.50f)), module, Panopia::Y_AXIS_CV_INPUT));
		addInput(createInputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[2], 89.50f)), module, Panopia::ROTATION_CV_INPUT));

		// Jack Row 2 (Inputs 4-6): Focus, Center, Smooth (Center Y = 99.00 mm)
		addInput(createInputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[0], 99.00f)), module, Panopia::FOCUS_CV_INPUT));
		addInput(createInputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[1], 99.00f)), module, Panopia::CENTER_CV_INPUT));
		addInput(createInputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[2], 99.00f)), module, Panopia::SMOOTH_CV_INPUT));

		// Jack Row 3 (Outputs 1-3): X, Y, Rotation (Center Y = 108.50 mm)
		addOutput(createOutputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[0], 108.50f)), module, Panopia::X_AXIS_OUTPUT));
		addOutput(createOutputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[1], 108.50f)), module, Panopia::Y_AXIS_OUTPUT));
		addOutput(createOutputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[2], 108.50f)), module, Panopia::ROTATION_OUTPUT));

		// Jack Row 4 (Outputs 4-6): Focus, Center, Smooth (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[0], 118.00f)), module, Panopia::FOCUS_OUTPUT));
		addOutput(createOutputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[1], 118.00f)), module, Panopia::CENTER_OUTPUT));
		addOutput(createOutputCentered<PanopiaCustomPort>(mm2px(Vec(col_x[2], 118.00f)), module, Panopia::SMOOTH_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Panopia* module = dynamic_cast<Panopia*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Voltage Range"));

		const char* rangeLabels[] = {
			"Bipolar ±5V",
			"Bipolar ±10V",
			"Unipolar 0 to 10V"
		};

		for (int i = 0; i < 3; i++) {
			Panopia::VoltageRange r = (Panopia::VoltageRange)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->voltageRange == r; },
				[=]() { module->voltageRange = r; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));

		const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; i++) {
			Panopia::SkinTheme theme = (Panopia::SkinTheme)i;
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

Model* modelPanopia = createModel<Panopia, PanopiaWidget>("Panopia");
