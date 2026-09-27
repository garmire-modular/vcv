#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Route — Dual X/Y 1:2 Alternating Toggle Router / Bernoulli Gate
//  Routes incoming X/Y trajectory signals alternating between Output 1
//  and Output 2 on each clock/switch pulse. Inactive output outputs 0V.
// ─────────────────────────────────────────────────────────────────────

struct RouteModule : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		SW_X_INPUT,
		SW_Y_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		X1_OUTPUT,
		Y1_OUTPUT,
		X2_OUTPUT,
		Y2_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::SchmittTrigger triggerX[16];
	dsp::SchmittTrigger triggerY[16];
	bool stateX[16] = {false};
	bool stateY[16] = {false};

	RouteModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(SW_X_INPUT, "X switch");
		configInput(SW_Y_INPUT, "Y switch");

		// Outputs
		configOutput(X1_OUTPUT, "X 1");
		configOutput(Y1_OUTPUT, "Y 1");
		configOutput(X2_OUTPUT, "X 2");
		configOutput(Y2_OUTPUT, "Y 2");
	}

	void onReset() override {
		for (int i = 0; i < 16; i++) {
			triggerX[i].reset();
			triggerY[i].reset();
			stateX[i] = false;
			stateY[i] = false;
		}
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int swXChannels = inputs[SW_X_INPUT].getChannels();
		int swYChannels = inputs[SW_Y_INPUT].getChannels();

		int channels = std::max({xChannels, yChannels, swXChannels, swYChannels, 1});

		outputs[X1_OUTPUT].setChannels(channels);
		outputs[Y1_OUTPUT].setChannels(channels);
		outputs[X2_OUTPUT].setChannels(channels);
		outputs[Y2_OUTPUT].setChannels(channels);

		bool swXConnected = inputs[SW_X_INPUT].isConnected();
		bool swYConnected = inputs[SW_Y_INPUT].isConnected();

		for (int c = 0; c < channels; c++) {
			// Inputs are distinct - no normalization between X and Y
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].getPolyVoltage(c);

			// Switch Gate/Clock CV: Y switch normalizes from X switch
			float swX = swXConnected ? inputs[SW_X_INPUT].getPolyVoltage(c) : 0.f;
			float swY = swYConnected ? inputs[SW_Y_INPUT].getPolyVoltage(c) : swX;

			// Trigger edge detection for alternating toggle
			if (triggerX[c].process(swX)) {
				stateX[c] = !stateX[c];
			}

			if (swYConnected) {
				if (triggerY[c].process(swY)) {
					stateY[c] = !stateY[c];
				}
			} else {
				stateY[c] = stateX[c];
			}

			// Routing: false = Output 1, true = Output 2
			// Inactive outputs output 0 V (blanking)
			float clampedX = clamp(inX, -12.f, 12.f);
			float clampedY = clamp(inY, -12.f, 12.f);

			float outX1 = !stateX[c] ? clampedX : 0.f;
			float outX2 = stateX[c] ? clampedX : 0.f;

			float outY1 = !stateY[c] ? clampedY : 0.f;
			float outY2 = stateY[c] ? clampedY : 0.f;

			outputs[X1_OUTPUT].setVoltage(outX1, c);
			outputs[Y1_OUTPUT].setVoltage(outY1, c);
			outputs[X2_OUTPUT].setVoltage(outX2, c);
			outputs[Y2_OUTPUT].setVoltage(outY2, c);
		}
	}
};

struct RouteWidget : ModuleWidget {
	RouteWidget(RouteModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Route.svg")));

		// 3HP Screws (Centered at x = 7.62 mm)
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Center column of jacks (x = 7.62 mm)
		// Group 1: Inputs (X, Y)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 23.00)), module, RouteModule::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.50)), module, RouteModule::Y_INPUT));

		// Group 2: Switch Clock / Trigger (SW X, SW Y)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 50.50)), module, RouteModule::SW_X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 63.00)), module, RouteModule::SW_Y_INPUT));

		// Group 3: Output 1 (X1, Y1)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 78.00)), module, RouteModule::X1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.50)), module, RouteModule::Y1_OUTPUT));

		// Group 4: Output 2 (X2, Y2)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.50)), module, RouteModule::X2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, RouteModule::Y2_OUTPUT));
	}
};

Model* modelRoute = createModel<RouteModule, RouteWidget>("Route");
