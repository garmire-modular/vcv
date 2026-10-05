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
		float v = getValue();
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
		// Row 1: Scanning & Dispersion Controls
		SCAN_X_PARAM,
		SCAN_Y_PARAM,
		BLEED_PARAM,

		// Rows 4-6: 3x3 Attenuverters (Cells 1 to 9)
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
		// Row 3: CV Inputs
		SCAN_X_CV_INPUT,
		SCAN_Y_CV_INPUT,
		BLEED_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Rows 4-6: 3x3 Outputs (Cells 1 to 9)
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
		// Row 2: 3x3 Activity LED Matrix (Cells 1 to 9)
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

		// Row 1 Knobs: scaled 0% to 100% directly for natural text entry and tooltip
		configParam(SCAN_X_PARAM, 0.f, 100.f, 50.f, "Scan X", "%");
		configParam(SCAN_Y_PARAM, 0.f, 100.f, 50.f, "Scan Y", "%");
		configParam(BLEED_PARAM, 0.f, 100.f, 0.f, "Bleed / crosstalk", "%");

		// Row 3 CV Inputs
		configInput(SCAN_X_CV_INPUT, "Scan X CV input");
		configInput(SCAN_Y_CV_INPUT, "Scan Y CV input");
		configInput(BLEED_CV_INPUT, "Bleed CV input");

		// Lower 3x3 Attenuverters (Cells 1 to 9)
		const char* const cellNames[9] = {
			"Cell 1", "Cell 2", "Cell 3",
			"Cell 4", "Cell 5", "Cell 6",
			"Cell 7", "Cell 8", "Cell 9"
		};

		for (int i = 0; i < 9; ++i) {
			char attLabel[32];
			snprintf(attLabel, sizeof(attLabel), "%s level", cellNames[i]);
			configParam(ATTEN_1_PARAM + i, 0.f, 100.f, 100.f, attLabel, "%");

			char outLabel[32];
			snprintf(outLabel, sizeof(outLabel), "%s voltage output", cellNames[i]);
			configOutput(OUT_1_OUTPUT + i, outLabel);
		}
	}

	void process(const ProcessArgs& args) override {
		// Scan coordinates: param is 0% to 100% -> normalized [0.0, 1.0]
		float scanX_norm = params[SCAN_X_PARAM].getValue() * 0.01f;
		if (inputs[SCAN_X_CV_INPUT].isConnected()) {
			// +/-5V provides full +/-50% sweep
			scanX_norm += inputs[SCAN_X_CV_INPUT].getVoltage() * 0.1f;
		}
		scanX_norm = rack::math::clamp(scanX_norm, 0.0f, 1.0f);

		float scanY_norm = params[SCAN_Y_PARAM].getValue() * 0.01f;
		if (inputs[SCAN_Y_CV_INPUT].isConnected()) {
			scanY_norm += inputs[SCAN_Y_CV_INPUT].getVoltage() * 0.1f;
		}
		scanY_norm = rack::math::clamp(scanY_norm, 0.0f, 1.0f);

		float bleed = params[BLEED_PARAM].getValue() * 0.01f;
		if (inputs[BLEED_CV_INPUT].isConnected()) {
			bleed += inputs[BLEED_CV_INPUT].getVoltage() * 0.1f;
		}
		bleed = rack::math::clamp(bleed, 0.0f, 1.0f);

		// Read attenuverters [0.0, 1.0]
		float atts[9];
		for (int i = 0; i < 9; ++i) {
			atts[i] = params[ATTEN_1_PARAM + i].getValue() * 0.01f;
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

		// Upper Section Columns (col 1 = 12.00 mm, col 2 = 30.48 mm, col 3 = 48.96 mm)
		float colX[3] = {12.00f, 30.48f, 48.96f};

		// Row 1: Three Knobs (Center Y = 21.00 mm)
		float knobY = 21.00f;
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX[0], knobY)), module, Matrix3x3::SCAN_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX[1], knobY)), module, Matrix3x3::SCAN_Y_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX[2], knobY)), module, Matrix3x3::BLEED_PARAM));

		// Row 2: Square Shaped Matrix with 5mm LEDs (Center Y = 42.00 mm)
		// 3x3 LED centers: X = [22.48, 30.48, 38.48], Y = [34.00, 42.00, 50.00]
		float ledX[3] = {22.48f, 30.48f, 38.48f};
		float ledY[3] = {34.00f, 42.00f, 50.00f};
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int idx = r * 3 + c;
				addChild(createLightCentered<LargeSimpleLight<GreenLight>>(mm2px(Vec(ledX[c], ledY[r])), module, Matrix3x3::GRID_LED_1 + idx));
			}
		}

		// Row 3: Three CV Depth Input Jacks (Center Y = 63.50 mm)
		float cvY = 63.50f;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[0], cvY)), module, Matrix3x3::SCAN_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[1], cvY)), module, Matrix3x3::SCAN_Y_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[2], cvY)), module, Matrix3x3::BLEED_CV_INPUT));

		// Lower Section: Left Attenuverters & Right Outputs (Y = 84.00, 97.50, 111.00 mm)
		float trimX[3] = {9.50f, 18.50f, 27.50f};
		float outX[3] = {37.50f, 46.50f, 55.50f};
		float cellY[3] = {84.00f, 97.50f, 111.00f};

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
			"0-1V",
			"0-5V",
			"0-10V"
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
