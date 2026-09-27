#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Sum/Mix XL — 6-Channel Precision Adder & Four-Quadrant Multiplier
//  8HP utility providing simultaneous summed (Input 1 + Input 2) and
//  four-quadrant multiplied ((Input 1 * Input 2)/5V) outputs for
//  full Laser Trajectory (X, Y), Color (R, G, B), and Intensity/Z (I).
// ─────────────────────────────────────────────────────────────────────

struct SumMixXLModule : Module {
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
	SumMixXLWidget(SumMixXLModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SumMixXL.svg")));

		// 8HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

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
