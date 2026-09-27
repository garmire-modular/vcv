#include "plugin.hpp"

struct Position : Module {
	enum ParamId {
		X_POS_PARAM,
		X_CV_ATTEN_PARAM,
		Y_POS_PARAM,
		Y_CV_ATTEN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_CV_INPUT,
		Y_CV_INPUT,
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

	Position() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters (-10V to +10V position offset)
		configParam(X_POS_PARAM, -10.f, 10.f, 0.f, "X position offset", " V");
		configParam(X_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "X position CV depth", "%", 0.f, 100.f);
		configParam(Y_POS_PARAM, -10.f, 10.f, 0.f, "Y position offset", " V");
		configParam(Y_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Y position CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_CV_INPUT, "X position CV");
		configInput(Y_CV_INPUT, "Y position CV");

		// Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max({inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels(), inputs[X_CV_INPUT].getChannels(), inputs[Y_CV_INPUT].getChannels()});
		if (channels == 0) {
			channels = 1;
		}

		for (int c = 0; c < channels; c++) {
			// Normalisation: Y input normalises to X input if unpatched
			float xIn = inputs[X_INPUT].getPolyVoltage(c);
			float yIn = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : xIn;

			// Normalisation: Y CV normalises to X CV if unpatched (switching jack logic)
			float xCv = inputs[X_CV_INPUT].getPolyVoltage(c);
			float yCv = inputs[Y_CV_INPUT].isConnected() ? inputs[Y_CV_INPUT].getPolyVoltage(c) : xCv;

			// 5V CV input at 100% attenuverter provides full 10V offset modulation
			float xOffset = params[X_POS_PARAM].getValue() + (xCv / 5.f) * 10.f * params[X_CV_ATTEN_PARAM].getValue();
			float yOffset = params[Y_POS_PARAM].getValue() + (yCv / 5.f) * 10.f * params[Y_CV_ATTEN_PARAM].getValue();

			// Laser trajectory signal output (-12V to +12V galvo safety clamp)
			float xOut = clamp(xIn + xOffset, -12.f, 12.f);
			float yOut = clamp(yIn + yOffset, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(xOut, c);
			outputs[Y_OUTPUT].setVoltage(yOut, c);
		}

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
	}
};

struct PositionWidget : ModuleWidget {
	PositionWidget(Position* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Position.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: Main Position Knobs (Center Y = 0.85 in = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Position::X_POS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Position::Y_POS_PARAM));

		// Row 2: CV Depth Trimpots (Center Y = 3.15 in = 80.01 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 80.01)), module, Position::X_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 80.01)), module, Position::Y_CV_ATTEN_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 3.70 in = 93.98 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 93.98)), module, Position::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 93.98)), module, Position::Y_INPUT));

		// Row 2: CV Inputs (Center Y = 4.15 in = 105.41 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.41)), module, Position::X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 105.41)), module, Position::Y_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 4.60 in = 116.84 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 116.84)), module, Position::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 116.84)), module, Position::Y_OUTPUT));
	}
};

Model* modelPosition = createModel<Position, PositionWidget>("Position");
