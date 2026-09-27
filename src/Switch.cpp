#include "plugin.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Switch — 2x2:1 CV Switch (Dual X/Y A/B Router)
//  CV-controlled dual X/Y router passing main X/Y inputs (Input 1) when
//  track pulse is LOW and alternate X/Y inputs (Input 2) when HIGH.
// ─────────────────────────────────────────────────────────────────────

struct SwitchModule : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		X1_INPUT,
		Y1_INPUT,
		X2_INPUT,
		Y2_INPUT,
		SW_X_INPUT,
		SW_Y_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		X_OUTPUT,
		Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bool highX[16] = {false};
	bool highY[16] = {false};

	SwitchModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Inputs
		configInput(X1_INPUT, "X 1");
		configInput(Y1_INPUT, "Y 1");
		configInput(X2_INPUT, "X 2");
		configInput(Y2_INPUT, "Y 2");
		configInput(SW_X_INPUT, "X switch");
		configInput(SW_Y_INPUT, "Y switch");

		// Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	void onReset() override {
		for (int i = 0; i < 16; i++) {
			highX[i] = false;
			highY[i] = false;
		}
	}

	void process(const ProcessArgs& args) override {
		int x1Channels = inputs[X1_INPUT].getChannels();
		int y1Channels = inputs[Y1_INPUT].getChannels();
		int x2Channels = inputs[X2_INPUT].getChannels();
		int y2Channels = inputs[Y2_INPUT].getChannels();
		int swXChannels = inputs[SW_X_INPUT].getChannels();
		int swYChannels = inputs[SW_Y_INPUT].getChannels();

		int channels = std::max({x1Channels, y1Channels, x2Channels, y2Channels, swXChannels, swYChannels, 1});

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		bool swXConnected = inputs[SW_X_INPUT].isConnected();
		bool swYConnected = inputs[SW_Y_INPUT].isConnected();

		for (int c = 0; c < channels; c++) {
			// Inputs are distinct - no normalization between X and Y
			float inX1 = inputs[X1_INPUT].getPolyVoltage(c);
			float inY1 = inputs[Y1_INPUT].getPolyVoltage(c);

			float inX2 = inputs[X2_INPUT].getPolyVoltage(c);
			float inY2 = inputs[Y2_INPUT].getPolyVoltage(c);

			// Switch Gate CV: Y switch normalizes from X switch
			float swX = swXConnected ? inputs[SW_X_INPUT].getPolyVoltage(c) : 0.f;
			float swY = swYConnected ? inputs[SW_Y_INPUT].getPolyVoltage(c) : swX;

			// Schmitt trigger comparator with hysteresis:
			// Trips HIGH at >= 1.5V, returns to LOW at <= 0.8V
			if (!highX[c] && swX >= 1.5f) {
				highX[c] = true;
			} else if (highX[c] && swX <= 0.8f) {
				highX[c] = false;
			}

			if (!highY[c] && swY >= 1.5f) {
				highY[c] = true;
			} else if (highY[c] && swY <= 0.8f) {
				highY[c] = false;
			}

			// Instantaneous switching: LOW = Input 1, HIGH = Input 2
			float outX = highX[c] ? inX2 : inX1;
			float outY = highY[c] ? inY2 : inY1;

			// Galvo safety clamping (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.f, 12.f), c);
		}
	}
};

struct SwitchWidget : ModuleWidget {
	SwitchWidget(SwitchModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Switch.svg")));

		// 3HP Screws (Centered at x = 7.62 mm)
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Center column of jacks (x = 7.62 mm)
		// Group 1: Input 1 (Main)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 23.00)), module, SwitchModule::X1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 35.50)), module, SwitchModule::Y1_INPUT));

		// Group 2: Input 2 (Alternate)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 50.50)), module, SwitchModule::X2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 63.00)), module, SwitchModule::Y2_INPUT));

		// Group 3: Switch (Track CV)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 78.00)), module, SwitchModule::SW_X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.50)), module, SwitchModule::SW_Y_INPUT));

		// Group 4: Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.50)), module, SwitchModule::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, SwitchModule::Y_OUTPUT));
	}
};

Model* modelSwitch = createModel<SwitchModule, SwitchWidget>("Switch");

