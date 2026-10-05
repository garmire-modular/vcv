#include "plugin.hpp"
#include "core/Matrix3x3Core.hpp"
#include <cmath>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────────────
// Custom ParamQuantity Subclasses
// ─────────────────────────────────────────────────────────────────────

struct MatrixPercentParamQuantity : ParamQuantity {
	std::string prefix;
	MatrixPercentParamQuantity() = default;
	MatrixPercentParamQuantity(const std::string& p) : prefix(p) {}
	std::string getDisplayValueString() override {
		float v = getValue() * 100.0f;
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%.1f%%", prefix.c_str(), v);
		return std::string(buf);
	}
};

// ─────────────────────────────────────────────────────────────────────
//  Matrix 3x3 Module
//  12 HP 2D Spatial Crossfade Scanner & Voltage Distributor
// ─────────────────────────────────────────────────────────────────────

struct Matrix3x3 : Module {
	enum ParamId {
		// Upper Section: Scanning & Dispersion Controls
		SCAN_X_PARAM,
		SCAN_Y_PARAM,
		BLEED_PARAM,

		// Lower Section: 3x3 Attenuverters (Cells 1 to 9)
		ATTEN_1_PARAM,
		ATTEN_2_PARAM,
		ATTEN_3_PARAM,
		ATTEN_4_PARAM,
		ATTEN_5_PARAM,
		ATTEN_6_PARAM,
		ATTEN_7_PARAM,
		ATTEN_8_PARAM,
		ATTEN_9_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Upper Section: CV Inputs
		SCAN_X_CV_INPUT,
		SCAN_Y_CV_INPUT,
		BLEED_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Lower Section: 3x3 Outputs (Cells 1 to 9)
		OUT_1_OUTPUT,
		OUT_2_OUTPUT,
		OUT_3_OUTPUT,
		OUT_4_OUTPUT,
		OUT_5_OUTPUT,
		OUT_6_OUTPUT,
		OUT_7_OUTPUT,
		OUT_8_OUTPUT,
		OUT_9_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		// 3x3 Activity LED Matrix (Cells 1 to 9)
		GRID_LED_1,
		GRID_LED_2,
		GRID_LED_3,
		GRID_LED_4,
		GRID_LED_5,
		GRID_LED_6,
		GRID_LED_7,
		GRID_LED_8,
		GRID_LED_9,

		LIGHTS_LEN
	};

	matrix3x3::Matrix3x3Engine engine;

	Matrix3x3() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Upper Knobs
		configParam<MatrixPercentParamQuantity>(SCAN_X_PARAM, 0.f, 1.f, 0.5f, "Scan X focus");
		configParam<MatrixPercentParamQuantity>(SCAN_Y_PARAM, 0.f, 1.f, 0.5f, "Scan Y focus");
		configParam<MatrixPercentParamQuantity>(BLEED_PARAM, 0.f, 1.f, 0.2f, "Bleed dispersion");

		// Upper CV Inputs (Standard format: "<Parameter> CV depth")
		configInput(SCAN_X_CV_INPUT, "Scan X CV depth");
		configInput(SCAN_Y_CV_INPUT, "Scan Y CV depth");
		configInput(BLEED_CV_INPUT, "Bleed CV depth");

		// Lower 3x3 Attenuverters (Cells 1 to 9)
		const char* const cellNames[9] = {
			"Cell 1", "Cell 2", "Cell 3",
			"Cell 4", "Cell 5", "Cell 6",
			"Cell 7", "Cell 8", "Cell 9"
		};

		for (int i = 0; i < 9; ++i) {
			char attLabel[32];
			snprintf(attLabel, sizeof(attLabel), "%s level", cellNames[i]);
			configParam<MatrixPercentParamQuantity>(ATTEN_1_PARAM + i, 0.f, 1.f, 1.0f, attLabel);

			char outLabel[32];
			snprintf(outLabel, sizeof(outLabel), "%s voltage output", cellNames[i]);
			configOutput(OUT_1_OUTPUT + i, outLabel);
		}
	}

	void process(const ProcessArgs& args) override {
		// Scan coordinates: param is 0% to 100% (50% = center 0.0)
		float scanX_norm = (params[SCAN_X_PARAM].getValue() - 0.5f) * 2.0f;
		if (inputs[SCAN_X_CV_INPUT].isConnected()) {
			scanX_norm += inputs[SCAN_X_CV_INPUT].getVoltage() * 0.1f; // 10V = 1.0 full scan sweep
		}
		scanX_norm = rack::math::clamp(scanX_norm, -1.0f, 1.0f);

		float scanY_norm = (params[SCAN_Y_PARAM].getValue() - 0.5f) * 2.0f;
		if (inputs[SCAN_Y_CV_INPUT].isConnected()) {
			scanY_norm += inputs[SCAN_Y_CV_INPUT].getVoltage() * 0.1f;
		}
		scanY_norm = rack::math::clamp(scanY_norm, -1.0f, 1.0f);

		float bleed = params[BLEED_PARAM].getValue();
		if (inputs[BLEED_CV_INPUT].isConnected()) {
			bleed += inputs[BLEED_CV_INPUT].getVoltage() * 0.1f;
		}
		bleed = rack::math::clamp(bleed, 0.0f, 1.0f);

		// Read attenuverters
		float atts[9];
		for (int i = 0; i < 9; ++i) {
			atts[i] = params[ATTEN_1_PARAM + i].getValue();
		}

		// Process engine
		matrix3x3::EngineOutput out = engine.process(scanX_norm, scanY_norm, bleed, atts);

		// Update outputs and 3x3 LEDs
		for (int i = 0; i < 9; ++i) {
			outputs[OUT_1_OUTPUT + i].setVoltage(out.outVolts[i]);
			lights[GRID_LED_1 + i].setBrightness(out.cellWeights[i]);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "voltageRange", json_integer((int)engine.voltageRange));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) {
			int vr = json_integer_value(vrJ);
			if (vr >= 0 && vr <= 2) {
				engine.voltageRange = (matrix3x3::VoltageRange)vr;
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────
//  Matrix 3x3 Widget
// ─────────────────────────────────────────────────────────────────────

struct Matrix3x3Widget : ModuleWidget {
	Matrix3x3Widget(Matrix3x3* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Matrix3x3.svg")));

		// 12HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Upper Section: Left Knobs & Right CV Jacks (Y = 22.00, 36.50, 51.00 mm)
		float colKnobs = 11.50f;
		float colCv = 49.46f;
		float rowY[3] = {22.00f, 36.50f, 51.00f};

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colKnobs, rowY[0])), module, Matrix3x3::SCAN_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colKnobs, rowY[1])), module, Matrix3x3::SCAN_Y_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colKnobs, rowY[2])), module, Matrix3x3::BLEED_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colCv, rowY[0])), module, Matrix3x3::SCAN_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colCv, rowY[1])), module, Matrix3x3::SCAN_Y_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colCv, rowY[2])), module, Matrix3x3::BLEED_CV_INPUT));

		// Center 3x3 LED Matrix (X = 21.98, 30.48, 38.98 mm; Y = 22.00, 36.50, 51.00 mm)
		float ledX[3] = {21.98f, 30.48f, 38.98f};
		float ledY[3] = {22.00f, 36.50f, 51.00f};
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int idx = r * 3 + c;
				addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(ledX[c], ledY[r])), module, Matrix3x3::GRID_LED_1 + idx));
			}
		}

		// Lower Section: Left Attenuverters & Right Outputs (Y = 76.00, 95.00, 114.00 mm)
		float trimX[3] = {9.50f, 18.50f, 27.50f};
		float outX[3] = {37.50f, 46.50f, 55.50f};
		float cellY[3] = {76.00f, 95.00f, 114.00f};

		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int idx = r * 3 + c;
				addParam(createParamCentered<Trimpot>(mm2px(Vec(trimX[c], cellY[r])), module, Matrix3x3::ATTEN_1_PARAM + idx));
				addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outX[c], cellY[r])), module, Matrix3x3::OUT_1_OUTPUT + idx));
			}
		}
	}

	void appendContextMenu(Menu* menu) override {
		Matrix3x3* module = dynamic_cast<Matrix3x3*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Output Voltage Range"));

		const char* rangeLabels[] = {
			"0–1.0V (Laser Diode / Logic Standard)",
			"0–5.0V (Eurorack Nominal Standard)",
			"0–10.0V (Full Video / VCA Standard)"
		};

		for (int i = 0; i < 3; ++i) {
			matrix3x3::VoltageRange vr = (matrix3x3::VoltageRange)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->engine.voltageRange == vr; },
				[=]() { module->engine.voltageRange = vr; }
			));
		}
	}
};

Model* modelMatrix3x3 = createModel<Matrix3x3, Matrix3x3Widget>("Matrix3x3");
