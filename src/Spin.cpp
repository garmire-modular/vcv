#include "plugin.hpp"
#include <cmath>

struct Spin : Module {
	enum ParamId {
		X_SPIN_PARAM,
		X_CV_ATTEN_PARAM,
		Y_SPIN_PARAM,
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

	float autoPhaseX[16] = {};
	float autoPhaseY[16] = {};

	Spin() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters: Main knobs control manual spin angle sweep (-1 to +1 turns)
		configParam(X_SPIN_PARAM, -1.f, 1.f, 0.f, "X manual spin sweep", " turns");
		configParam(X_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "X auto-spin CV depth", "%", 0.f, 100.f);
		configParam(Y_SPIN_PARAM, -1.f, 1.f, 0.f, "Y manual spin sweep", " turns");
		configParam(Y_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Y auto-spin CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_CV_INPUT, "X auto-spin CV");
		configInput(Y_CV_INPUT, "Y auto-spin CV");

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

			// Auto-spin frequency controlled by CV inputs (scaled up to 10 Hz at 100% attenuverter with 5V CV)
			float xAutoRate = (xCv / 5.f) * 10.f * params[X_CV_ATTEN_PARAM].getValue();
			float yAutoRate = (yCv / 5.f) * 10.f * params[Y_CV_ATTEN_PARAM].getValue();

			autoPhaseX[c] += xAutoRate * args.sampleTime;
			autoPhaseY[c] += yAutoRate * args.sampleTime;

			if (autoPhaseX[c] > 1.f) autoPhaseX[c] -= 1.f; else if (autoPhaseX[c] < -1.f) autoPhaseX[c] += 1.f;
			if (autoPhaseY[c] > 1.f) autoPhaseY[c] -= 1.f; else if (autoPhaseY[c] < -1.f) autoPhaseY[c] += 1.f;

			// Total spin phase combines manual knob angle sweep + CV-managed auto-spin phase
			float totalTurnsX = params[X_SPIN_PARAM].getValue() + autoPhaseX[c];
			float totalTurnsY = params[Y_SPIN_PARAM].getValue() + autoPhaseY[c];

			float angleX = totalTurnsX * 2.f * (float)M_PI;
			float angleY = totalTurnsY * 2.f * (float)M_PI;

			// Spin transformation: X' = cos(Ax)*X - sin(Ay)*Y, Y' = sin(Ax)*X + cos(Ay)*Y
			float xOut = std::cos(angleX) * xIn - std::sin(angleY) * yIn;
			float yOut = std::sin(angleX) * xIn + std::cos(angleY) * yIn;

			// Laser trajectory signal output (-12V to +12V galvo safety clamp)
			xOut = clamp(xOut, -12.f, 12.f);
			yOut = clamp(yOut, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(xOut, c);
			outputs[Y_OUTPUT].setVoltage(yOut, c);
		}

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
	}
};

struct SpinWidget : ModuleWidget {
	SpinWidget(Spin* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Spin.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: X & Y Manual Spin Sweep Knobs (Center Y = 0.85 in = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Spin::X_SPIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Spin::Y_SPIN_PARAM));

		// Row 2: Auto-Spin CV Depth Trimpots (Center Y = 3.15 in = 80.01 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 80.01)), module, Spin::X_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 80.01)), module, Spin::Y_CV_ATTEN_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 3.70 in = 93.98 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 93.98)), module, Spin::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 93.98)), module, Spin::Y_INPUT));

		// Row 2: Auto-Spin CV Inputs (Center Y = 4.15 in = 105.41 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.41)), module, Spin::X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 105.41)), module, Spin::Y_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 4.60 in = 116.84 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 116.84)), module, Spin::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 116.84)), module, Spin::Y_OUTPUT));
	}
};

Model* modelSpin = createModel<Spin, SpinWidget>("Spin");
