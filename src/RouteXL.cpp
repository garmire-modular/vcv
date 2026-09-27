#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Route XL — 6-Channel 1:2 Alternating Toggle Router / Bernoulli Gate
//  8HP router for full Laser Trajectory (X, Y), Color (R, G, B),
//  and Intensity/Z (I).
//  Routes incoming trajectory and color signals alternating between
//  Output 1 and Output 2 on each clock/switch pulse.
//  Switch inputs feature downward cascading normalization from X to I.
//  Inactive outputs output 0V (blanking).
// ─────────────────────────────────────────────────────────────────────

struct RouteXLModule : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		// Column 1: Input
		X_INPUT,
		Y_INPUT,
		R_INPUT,
		G_INPUT,
		B_INPUT,
		I_INPUT,
		// Column 2: Switch CV
		SW_X_INPUT,
		SW_Y_INPUT,
		SW_R_INPUT,
		SW_G_INPUT,
		SW_B_INPUT,
		SW_I_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Column 3: Output 1
		X1_OUTPUT,
		Y1_OUTPUT,
		R1_OUTPUT,
		G1_OUTPUT,
		B1_OUTPUT,
		I1_OUTPUT,
		// Column 4: Output 2
		X2_OUTPUT,
		Y2_OUTPUT,
		R2_OUTPUT,
		G2_OUTPUT,
		B2_OUTPUT,
		I2_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::SchmittTrigger trigger[6][16];
	bool routeState[6][16] = {{false}};

	RouteXLModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Column 1: Input
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(R_INPUT, "R");
		configInput(G_INPUT, "G");
		configInput(B_INPUT, "B");
		configInput(I_INPUT, "I");

		// Column 2: Switch
		configInput(SW_X_INPUT, "X switch");
		configInput(SW_Y_INPUT, "Y switch");
		configInput(SW_R_INPUT, "R switch");
		configInput(SW_G_INPUT, "G switch");
		configInput(SW_B_INPUT, "B switch");
		configInput(SW_I_INPUT, "I switch");

		// Column 3: Output 1
		configOutput(X1_OUTPUT, "X 1");
		configOutput(Y1_OUTPUT, "Y 1");
		configOutput(R1_OUTPUT, "R 1");
		configOutput(G1_OUTPUT, "G 1");
		configOutput(B1_OUTPUT, "B 1");
		configOutput(I1_OUTPUT, "I 1");

		// Column 4: Output 2
		configOutput(X2_OUTPUT, "X 2");
		configOutput(Y2_OUTPUT, "Y 2");
		configOutput(R2_OUTPUT, "R 2");
		configOutput(G2_OUTPUT, "G 2");
		configOutput(B2_OUTPUT, "B 2");
		configOutput(I2_OUTPUT, "I 2");
	}

	void onReset() override {
		for (int ch = 0; ch < 6; ch++) {
			for (int i = 0; i < 16; i++) {
				trigger[ch][i].reset();
				routeState[ch][i] = false;
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
			// Trigger processing and cascading normalization
			for (int k = 0; k < 6; k++) {
				if (swConn[k]) {
					float swVolt = inputs[SW_X_INPUT + k].getPolyVoltage(c);
					if (trigger[k][c].process(swVolt)) {
						routeState[k][c] = !routeState[k][c];
					}
				} else if (k > 0) {
					// Normalize state from preceding channel when unpatched
					routeState[k][c] = routeState[k - 1][c];
				}
			}

			// Route inputs: false = Output 1, true = Output 2
			for (int k = 0; k < 6; k++) {
				float in = inputs[X_INPUT + k].getPolyVoltage(c);
				float clamped = clamp(in, -12.f, 12.f);

				float out1 = !routeState[k][c] ? clamped : 0.f;
				float out2 = routeState[k][c] ? clamped : 0.f;

				outputs[X1_OUTPUT + k].setVoltage(out1, c);
				outputs[X2_OUTPUT + k].setVoltage(out2, c);
			}
		}
	}
};

struct RouteXLWidget : ModuleWidget {
	RouteXLWidget(RouteXLModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/RouteXL.svg")));

		// 8HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 4 Columns: [6.07, 15.57, 25.07, 34.57] mm
		// 6 Rows: [27.00, 45.00, 63.00, 81.00, 99.00, 117.00] mm
		const double colX[4] = {6.07, 15.57, 25.07, 34.57};
		const double rowY[6] = {27.00, 45.00, 63.00, 81.00, 99.00, 117.00};

		// Column 1: Input
		for (int r = 0; r < 6; r++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[0], rowY[r])), module, RouteXLModule::X_INPUT + r));
		}

		// Column 2: Switch CV
		for (int r = 0; r < 6; r++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[1], rowY[r])), module, RouteXLModule::SW_X_INPUT + r));
		}

		// Column 3: Output 1
		for (int r = 0; r < 6; r++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX[2], rowY[r])), module, RouteXLModule::X1_OUTPUT + r));
		}

		// Column 4: Output 2
		for (int r = 0; r < 6; r++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX[3], rowY[r])), module, RouteXLModule::X2_OUTPUT + r));
		}
	}
};

Model* modelRouteXL = createModel<RouteXLModule, RouteXLWidget>("RouteXL");
