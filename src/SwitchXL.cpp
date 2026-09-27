#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Switch XL — 6-Channel 2x2:1 CV Switch (A/B Router)
//  8HP router for full Laser Trajectory (X, Y), Color (R, G, B),
//  and Intensity/Z (I).
//  Passes Input 1 when track pulse is LOW and Input 2 when HIGH.
//  Switch CV features downward cascading normalization from X down to I.
// ─────────────────────────────────────────────────────────────────────

struct SwitchXLModule : Module {
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
		// Column 3: Switch CV
		SW_X_INPUT,
		SW_Y_INPUT,
		SW_R_INPUT,
		SW_G_INPUT,
		SW_B_INPUT,
		SW_I_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Column 4: Outputs
		X_OUTPUT,
		Y_OUTPUT,
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		I_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bool highState[6][16] = {{false}};

	SwitchXLModule() {
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

		// Column 3: Switch
		configInput(SW_X_INPUT, "X switch");
		configInput(SW_Y_INPUT, "Y switch");
		configInput(SW_R_INPUT, "R switch");
		configInput(SW_G_INPUT, "G switch");
		configInput(SW_B_INPUT, "B switch");
		configInput(SW_I_INPUT, "I switch");

		// Column 4: Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(R_OUTPUT, "R");
		configOutput(G_OUTPUT, "G");
		configOutput(B_OUTPUT, "B");
		configOutput(I_OUTPUT, "I");
	}

	void onReset() override {
		for (int ch = 0; ch < 6; ch++) {
			for (int i = 0; i < 16; i++) {
				highState[ch][i] = false;
			}
		}
	}

	void process(const ProcessArgs& args) override {
		int channels = 1;
		for (int i = 0; i < INPUTS_LEN; i++) {
			channels = std::max(channels, inputs[i].getChannels());
		}

		for (int o = 0; o < OUTPUTS_LEN; o++) {
			outputs[o].setChannels(channels);
		}

		bool swConn[6] = {
			inputs[SW_X_INPUT].isConnected(),
			inputs[SW_Y_INPUT].isConnected(),
			inputs[SW_R_INPUT].isConnected(),
			inputs[SW_G_INPUT].isConnected(),
			inputs[SW_B_INPUT].isConnected(),
			inputs[SW_I_INPUT].isConnected()
		};

		for (int c = 0; c < channels; c++) {
			// Switch CV Cascading Normalization: X -> Y -> R -> G -> B -> I
			float swVolt[6];
			float currentNorm = 0.f;
			for (int k = 0; k < 6; k++) {
				if (swConn[k]) {
					currentNorm = inputs[SW_X_INPUT + k].getPolyVoltage(c);
				}
				swVolt[k] = currentNorm;
			}

			// Comparator with hysteresis: Trip at >= 1.5V, reset at <= 0.8V
			for (int k = 0; k < 6; k++) {
				if (!highState[k][c] && swVolt[k] >= 1.5f) {
					highState[k][c] = true;
				} else if (highState[k][c] && swVolt[k] <= 0.8f) {
					highState[k][c] = false;
				}
			}

			// Route inputs: LOW = Input 1, HIGH = Input 2
			for (int k = 0; k < 6; k++) {
				float in1 = inputs[X1_INPUT + k].getPolyVoltage(c);
				float in2 = inputs[X2_INPUT + k].getPolyVoltage(c);
				float out = highState[k][c] ? in2 : in1;
				outputs[X_OUTPUT + k].setVoltage(clamp(out, -12.f, 12.f), c);
			}
		}
	}
};

struct SwitchXLWidget : ModuleWidget {
	SwitchXLWidget(SwitchXLModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SwitchXL.svg")));

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
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[0], rowY[r])), module, SwitchXLModule::X1_INPUT + r));
		}

		// Column 2: Input 2
		for (int r = 0; r < 6; r++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[1], rowY[r])), module, SwitchXLModule::X2_INPUT + r));
		}

		// Column 3: Switch CV
		for (int r = 0; r < 6; r++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[2], rowY[r])), module, SwitchXLModule::SW_X_INPUT + r));
		}

		// Column 4: Output
		for (int r = 0; r < 6; r++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX[3], rowY[r])), module, SwitchXLModule::X_OUTPUT + r));
		}
	}
};

Model* modelSwitchXL = createModel<SwitchXLModule, SwitchXLWidget>("SwitchXL");
