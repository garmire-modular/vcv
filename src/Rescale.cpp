#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Rescale — Multi-Range Precision Voltage Scaler, Normalizer & Converter
//  6 HP module providing bidirectional conversion and summing between
//  standard Eurorack and laser voltage ranges:
//  - 0-1V   and ±1V
//  - 0-5V   and ±5V
//  - 0-10V  and ±10V
// ─────────────────────────────────────────────────────────────────────

struct RescaleModule : Module {
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
	RescaleWidget(RescaleModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Rescale.svg")));

		// Screws for 6 HP
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

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
