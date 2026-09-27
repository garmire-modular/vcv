#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Orxy — Dual X/Y Boolean OR / NOR Logic Module (3HP)
//  Evaluates boolean OR and inverted NOR logic on dual X/Y channels.
//  Comparator uses Schmitt trigger hysteresis: HIGH >= 1.5V, LOW <= 0.8V.
//  Outputs standard Eurorack 10V gate voltages.
// ─────────────────────────────────────────────────────────────────────

struct OrxyModule : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		X1_INPUT,
		Y1_INPUT,
		X2_INPUT,
		Y2_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OR_X_OUTPUT,
		OR_Y_OUTPUT,
		NOR_X_OUTPUT,
		NOR_Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bool highX1[16] = {false};
	bool highY1[16] = {false};
	bool highX2[16] = {false};
	bool highY2[16] = {false};

	OrxyModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");

		// Outputs
		configOutput(OR_X_OUTPUT, "OR X");
		configOutput(OR_Y_OUTPUT, "OR Y");
		configOutput(NOR_X_OUTPUT, "NOR X");
		configOutput(NOR_Y_OUTPUT, "NOR Y");
	}

	void onReset() override {
		for (int i = 0; i < 16; i++) {
			highX1[i] = false;
			highY1[i] = false;
			highX2[i] = false;
			highY2[i] = false;
		}
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max({
			inputs[X1_INPUT].getChannels(),
			inputs[Y1_INPUT].getChannels(),
			inputs[X2_INPUT].getChannels(),
			inputs[Y2_INPUT].getChannels(),
			1
		});

		outputs[OR_X_OUTPUT].setChannels(channels);
		outputs[OR_Y_OUTPUT].setChannels(channels);
		outputs[NOR_X_OUTPUT].setChannels(channels);
		outputs[NOR_Y_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			float vX1 = inputs[X1_INPUT].getPolyVoltage(c);
			float vY1 = inputs[Y1_INPUT].getPolyVoltage(c);
			float vX2 = inputs[X2_INPUT].getPolyVoltage(c);
			float vY2 = inputs[Y2_INPUT].getPolyVoltage(c);

			// Schmitt trigger comparator with hysteresis
			if (!highX1[c] && vX1 >= 1.5f) highX1[c] = true;
			else if (highX1[c] && vX1 <= 0.8f) highX1[c] = false;

			if (!highY1[c] && vY1 >= 1.5f) highY1[c] = true;
			else if (highY1[c] && vY1 <= 0.8f) highY1[c] = false;

			if (!highX2[c] && vX2 >= 1.5f) highX2[c] = true;
			else if (highX2[c] && vX2 <= 0.8f) highX2[c] = false;

			if (!highY2[c] && vY2 >= 1.5f) highY2[c] = true;
			else if (highY2[c] && vY2 <= 0.8f) highY2[c] = false;

			bool orX = highX1[c] || highX2[c];
			bool orY = highY1[c] || highY2[c];

			outputs[OR_X_OUTPUT].setVoltage(orX ? 10.f : 0.f, c);
			outputs[OR_Y_OUTPUT].setVoltage(orY ? 10.f : 0.f, c);
			outputs[NOR_X_OUTPUT].setVoltage(!orX ? 10.f : 0.f, c);
			outputs[NOR_Y_OUTPUT].setVoltage(!orY ? 10.f : 0.f, c);
		}
	}
};

struct OrxyWidget : ModuleWidget {
	OrxyWidget(OrxyModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Orxy.svg")));

		// 3HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Center column of jacks (x = 7.62 mm)
		// Pair 1: Input 1 (X1, Y1)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 23.00)), module, OrxyModule::X1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.50)), module, OrxyModule::Y1_INPUT));

		// Pair 2: Input 2 (X2, Y2)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 50.50)), module, OrxyModule::X2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 63.00)), module, OrxyModule::Y2_INPUT));

		// Pair 3: OR Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 78.00)), module, OrxyModule::OR_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.50)), module, OrxyModule::OR_Y_OUTPUT));

		// Pair 4: NOR Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.50)), module, OrxyModule::NOR_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, OrxyModule::NOR_Y_OUTPUT));
	}
};

Model* modelOrxy = createModel<OrxyModule, OrxyWidget>("Orxy");
