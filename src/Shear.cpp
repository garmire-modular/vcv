#include "plugin.hpp"
#include <cmath>

struct Shear : Module {
	enum ParamId {
		X_SHEAR_PARAM,
		X_CV_ATTEN_PARAM,
		Y_SHEAR_PARAM,
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

	Shear() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters (2D Affine shear factor -2.0 to +2.0)
		configParam(X_SHEAR_PARAM, -2.f, 2.f, 0.f, "X shear factor", "x");
		configParam(X_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "X shear CV depth", "%", 0.f, 100.f);
		configParam(Y_SHEAR_PARAM, -2.f, 2.f, 0.f, "Y shear factor", "x");
		configParam(Y_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Y shear CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_CV_INPUT, "X shear CV");
		configInput(Y_CV_INPUT, "Y shear CV");

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

			// Calculate effective shear factors (knob + CV * attenuverter)
			float sX = params[X_SHEAR_PARAM].getValue() + (xCv / 5.f) * params[X_CV_ATTEN_PARAM].getValue();
			float sY = params[Y_SHEAR_PARAM].getValue() + (yCv / 5.f) * params[Y_CV_ATTEN_PARAM].getValue();

			// 2D Affine shearing transformation:
			// X_sheared = X_in + sX * Y_in
			// Y_sheared = Y_in + sY * X_in
			float xSheared = xIn + sX * yIn;
			float ySheared = yIn + sY * xIn;

			// Laser trajectory signal output (-12V to +12V galvo safety clamp)
			float xOut = clamp(xSheared, -12.f, 12.f);
			float yOut = clamp(ySheared, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(xOut, c);
			outputs[Y_OUTPUT].setVoltage(yOut, c);
		}

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
	}
};

struct ShearWidget : ModuleWidget {
	ShearWidget(Shear* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Shear.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: X & Y Shear Depth Knobs (Center Y = 0.85 in = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Shear::X_SHEAR_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Shear::Y_SHEAR_PARAM));

		// Row 2: CV Depth Trimpots (Center Y = 3.15 in = 80.01 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 80.01)), module, Shear::X_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 80.01)), module, Shear::Y_CV_ATTEN_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 3.70 in = 93.98 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 93.98)), module, Shear::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 93.98)), module, Shear::Y_INPUT));

		// Row 2: CV Inputs (Center Y = 4.15 in = 105.41 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.41)), module, Shear::X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 105.41)), module, Shear::Y_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 4.60 in = 116.84 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 116.84)), module, Shear::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 116.84)), module, Shear::Y_OUTPUT));
	}
};

Model* modelShear = createModel<Shear, ShearWidget>("Shear");
