#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  SumMult — Dual X/Y Precision Adder and Four-Quadrant Multiplier
//  Provides simultaneous summed (X1 + X2, Y1 + Y2) and four-quadrant
//  multiplied ((X1 * X2)/5V, (Y1 * Y2)/5V) outputs for dual-channel
//  laser trajectory modulation and cross-axis vector synthesis.
// ─────────────────────────────────────────────────────────────────────

struct SumMultModule : Module {
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
		SUM_X_OUTPUT,
		SUM_Y_OUTPUT,
		MULT_X_OUTPUT,
		MULT_Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	SumMultModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs: Rack automatically appends "input" to tooltips
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");

		// Outputs: Rack automatically appends "output" to tooltips
		configOutput(SUM_X_OUTPUT, "X sum");
		configOutput(SUM_Y_OUTPUT, "Y sum");
		configOutput(MULT_X_OUTPUT, "X mult");
		configOutput(MULT_Y_OUTPUT, "Y mult");
	}

	void process(const ProcessArgs& args) override {
		int x1Channels = inputs[X1_INPUT].getChannels();
		int y1Channels = inputs[Y1_INPUT].getChannels();
		int x2Channels = inputs[X2_INPUT].getChannels();
		int y2Channels = inputs[Y2_INPUT].getChannels();

		int channels = std::max({x1Channels, y1Channels, x2Channels, y2Channels, 1});

		outputs[SUM_X_OUTPUT].setChannels(channels);
		outputs[SUM_Y_OUTPUT].setChannels(channels);
		outputs[MULT_X_OUTPUT].setChannels(channels);
		outputs[MULT_Y_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			// Inputs are distinct: no normalization between X and Y
			// Disconnected inputs return 0.0V
			float inX1 = inputs[X1_INPUT].getPolyVoltage(c);
			float inY1 = inputs[Y1_INPUT].getPolyVoltage(c);
			float inX2 = inputs[X2_INPUT].getPolyVoltage(c);
			float inY2 = inputs[Y2_INPUT].getPolyVoltage(c);

			// Precision Addition: X1 + X2, Y1 + Y2
			float sumX = inX1 + inX2;
			float sumY = inY1 + inY2;

			// Four-Quadrant Multiplication scaled by 5V standard:
			// (V1 * V2) / 5.0f
			float multX = (inX1 * inX2) * 0.2f;
			float multY = (inY1 * inY2) * 0.2f;

			// Laser galvanometer safety limits (-12V to +12V)
			outputs[SUM_X_OUTPUT].setVoltage(clamp(sumX, -12.f, 12.f), c);
			outputs[SUM_Y_OUTPUT].setVoltage(clamp(sumY, -12.f, 12.f), c);
			outputs[MULT_X_OUTPUT].setVoltage(clamp(multX, -12.f, 12.f), c);
			outputs[MULT_Y_OUTPUT].setVoltage(clamp(multY, -12.f, 12.f), c);
		}
	}
};

struct SumMultWidget : ModuleWidget {
	SumMultWidget(SumMultModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SumMult.svg")));

		// 3HP Screws (Centered at x = 7.62 mm)
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Center column of jacks (x = 7.62 mm)
		// Pair 1: Input 1 (X1, Y1)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 23.00)), module, SumMultModule::X1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.50)), module, SumMultModule::Y1_INPUT));

		// Pair 2: Input 2 (X2, Y2)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 50.50)), module, SumMultModule::X2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 63.00)), module, SumMultModule::Y2_INPUT));

		// Pair 3: Sum Outputs (X1 + X2, Y1 + Y2)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 78.00)), module, SumMultModule::SUM_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.50)), module, SumMultModule::SUM_Y_OUTPUT));

		// Pair 4: Mult Outputs ((X1 * X2)/5V, (Y1 * Y2)/5V)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.50)), module, SumMultModule::MULT_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, SumMultModule::MULT_Y_OUTPUT));
	}
};

Model* modelSumMult = createModel<SumMultModule, SumMultWidget>("SumMult");
