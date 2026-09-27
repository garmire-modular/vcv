#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Xorxy — Dual X/Y Boolean XOR / XNOR Logic Module (3HP)
//  Evaluates boolean XOR and inverted XNOR logic on dual X/Y channels.
//  Comparator uses Schmitt trigger hysteresis: HIGH >= 1.5V, LOW <= 0.8V.
//  Outputs standard Eurorack 10V gate voltages.
// ─────────────────────────────────────────────────────────────────────

struct XorxyModule : Module {
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
		XOR_X_OUTPUT,
		XOR_Y_OUTPUT,
		XNOR_X_OUTPUT,
		XNOR_Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bool highX1[16] = {false};
	bool highY1[16] = {false};
	bool highX2[16] = {false};
	bool highY2[16] = {false};

	XorxyModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");

		// Outputs
		configOutput(XOR_X_OUTPUT, "XOR X");
		configOutput(XOR_Y_OUTPUT, "XOR Y");
		configOutput(XNOR_X_OUTPUT, "XNOR X");
		configOutput(XNOR_Y_OUTPUT, "XNOR Y");
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

		outputs[XOR_X_OUTPUT].setChannels(channels);
		outputs[XOR_Y_OUTPUT].setChannels(channels);
		outputs[XNOR_X_OUTPUT].setChannels(channels);
		outputs[XNOR_Y_OUTPUT].setChannels(channels);

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

			bool xorX = highX1[c] != highX2[c];
			bool xorY = highY1[c] != highY2[c];

			outputs[XOR_X_OUTPUT].setVoltage(xorX ? 10.f : 0.f, c);
			outputs[XOR_Y_OUTPUT].setVoltage(xorY ? 10.f : 0.f, c);
			outputs[XNOR_X_OUTPUT].setVoltage(!xorX ? 10.f : 0.f, c);
			outputs[XNOR_Y_OUTPUT].setVoltage(!xorY ? 10.f : 0.f, c);
		}
	}
};

struct XorxyWidget : ModuleWidget {
	XorxyWidget(XorxyModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Xorxy.svg")));

		// 3HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Center column of jacks (x = 7.62 mm)
		// Pair 1: Input 1 (X1, Y1)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 23.00)), module, XorxyModule::X1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.50)), module, XorxyModule::Y1_INPUT));

		// Pair 2: Input 2 (X2, Y2)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 50.50)), module, XorxyModule::X2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 63.00)), module, XorxyModule::Y2_INPUT));

		// Pair 3: XOR Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 78.00)), module, XorxyModule::XOR_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.50)), module, XorxyModule::XOR_Y_OUTPUT));

		// Pair 4: XNOR Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.50)), module, XorxyModule::XNOR_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, XorxyModule::XNOR_Y_OUTPUT));
	}
};

Model* modelXorxy = createModel<XorxyModule, XorxyWidget>("Xorxy");
