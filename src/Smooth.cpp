#include "plugin.hpp"
#include <cmath>

struct Smooth : Module {
	enum ParamId {
		X_FREQ_PARAM,
		Y_FREQ_PARAM,
		X_SLOPE_PARAM,
		Y_SLOPE_PARAM,
		X_FREQ_CV_ATTEN_PARAM,
		Y_FREQ_CV_ATTEN_PARAM,
		X_SLOPE_CV_ATTEN_PARAM,
		Y_SLOPE_CV_ATTEN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_FREQ_CV_INPUT,
		Y_FREQ_CV_INPUT,
		X_SLOPE_CV_INPUT,
		Y_SLOPE_CV_INPUT,
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

	// 8-stage lowpass filter states for polyphonic channels (up to 16 channels)
	float stateX[16][8] = {};
	float stateY[16][8] = {};

	Smooth() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Frequency Cutoff parameters (0% to 100%)
		configParam(X_FREQ_PARAM, 0.f, 1.f, 0.f, "X smooth cutoff frequency", "%", 0.f, 100.f);
		configParam(Y_FREQ_PARAM, 0.f, 1.f, 0.f, "Y smooth cutoff frequency", "%", 0.f, 100.f);

		// Slope parameters (1-pole -6dB/oct to 8-pole -48dB/oct)
		configParam(X_SLOPE_PARAM, 1.f, 8.f, 1.f, "X filter slope (poles)", " poles");
		configParam(Y_SLOPE_PARAM, 1.f, 8.f, 1.f, "Y filter slope (poles)", " poles");

		// Attenuverter parameters
		configParam(X_FREQ_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "X cutoff CV depth", "%", 0.f, 100.f);
		configParam(Y_FREQ_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Y cutoff CV depth", "%", 0.f, 100.f);
		configParam(X_SLOPE_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "X slope CV depth", "%", 0.f, 100.f);
		configParam(Y_SLOPE_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Y slope CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_FREQ_CV_INPUT, "X cutoff CV");
		configInput(Y_FREQ_CV_INPUT, "Y cutoff CV");
		configInput(X_SLOPE_CV_INPUT, "X slope CV");
		configInput(Y_SLOPE_CV_INPUT, "Y slope CV");

		// Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	void onReset() override {
		for (int c = 0; c < 16; c++) {
			for (int st = 0; st < 8; st++) {
				stateX[c][st] = 0.f;
				stateY[c][st] = 0.f;
			}
		}
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max({
			inputs[X_INPUT].getChannels(),
			inputs[Y_INPUT].getChannels(),
			inputs[X_FREQ_CV_INPUT].getChannels(),
			inputs[Y_FREQ_CV_INPUT].getChannels(),
			inputs[X_SLOPE_CV_INPUT].getChannels(),
			inputs[Y_SLOPE_CV_INPUT].getChannels()
		});
		if (channels == 0) {
			channels = 1;
		}

		for (int c = 0; c < channels; c++) {
			// Normalisation: Y input normalises to X input if unpatched
			float xIn = inputs[X_INPUT].getPolyVoltage(c);
			float yIn = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : xIn;

			// Normalisation: Switching jack logic for FREQ and SLOPE CV
			float xFreqCv = inputs[X_FREQ_CV_INPUT].getPolyVoltage(c);
			float yFreqCv = inputs[Y_FREQ_CV_INPUT].isConnected() ? inputs[Y_FREQ_CV_INPUT].getPolyVoltage(c) : xFreqCv;

			float xSlopeCv = inputs[X_SLOPE_CV_INPUT].getPolyVoltage(c);
			float ySlopeCv = inputs[Y_SLOPE_CV_INPUT].isConnected() ? inputs[Y_SLOPE_CV_INPUT].getPolyVoltage(c) : xSlopeCv;

			// Calculate cutoff amounts (0.0 to 1.0)
			float sX = clamp(params[X_FREQ_PARAM].getValue() + (xFreqCv / 5.f) * params[X_FREQ_CV_ATTEN_PARAM].getValue(), 0.f, 1.f);
			float sY = clamp(params[Y_FREQ_PARAM].getValue() + (yFreqCv / 5.f) * params[Y_FREQ_CV_ATTEN_PARAM].getValue(), 0.f, 1.f);

			// Calculate continuous slope values (1.0 to 8.0 poles)
			float nPolesX = clamp(params[X_SLOPE_PARAM].getValue() + (xSlopeCv / 5.f) * params[X_SLOPE_CV_ATTEN_PARAM].getValue() * 7.f, 1.f, 8.f);
			float nPolesY = clamp(params[Y_SLOPE_PARAM].getValue() + (ySlopeCv / 5.f) * params[Y_SLOPE_CV_ATTEN_PARAM].getValue() * 7.f, 1.f, 8.f);

			// Calculate 1-pole lowpass filter coefficients (20kHz down to 2Hz)
			float alphaX = 1.f;
			if (sX > 0.f) {
				float fcX = 20000.f * std::pow(10.f, -4.f * sX);
				alphaX = clamp(1.f - std::exp(-2.f * (float)M_PI * fcX * args.sampleTime), 0.f, 1.f);
			}

			float alphaY = 1.f;
			if (sY > 0.f) {
				float fcY = 20000.f * std::pow(10.f, -4.f * sY);
				alphaY = clamp(1.f - std::exp(-2.f * (float)M_PI * fcY * args.sampleTime), 0.f, 1.f);
			}

			// Cascade 8 1-pole filter stages for X
			float currX = xIn;
			float stageXOut[8];
			for (int st = 0; st < 8; st++) {
				stateX[c][st] += alphaX * (currX - stateX[c][st]);
				stageXOut[st] = stateX[c][st];
				currX = stateX[c][st];
			}

			// Morph continuously between stage k and stage k+1 for X
			int kX = clamp((int)std::floor(nPolesX), 1, 8);
			float fracX = nPolesX - (float)kX;
			float rawXOut = stageXOut[kX - 1];
			if (kX < 8 && fracX > 0.f) {
				rawXOut = (1.f - fracX) * stageXOut[kX - 1] + fracX * stageXOut[kX];
			}

			// Cascade 8 1-pole filter stages for Y
			float currY = yIn;
			float stageYOut[8];
			for (int st = 0; st < 8; st++) {
				stateY[c][st] += alphaY * (currY - stateY[c][st]);
				stageYOut[st] = stateY[c][st];
				currY = stateY[c][st];
			}

			// Morph continuously between stage k and stage k+1 for Y
			int kY = clamp((int)std::floor(nPolesY), 1, 8);
			float fracY = nPolesY - (float)kY;
			float rawYOut = stageYOut[kY - 1];
			if (kY < 8 && fracY > 0.f) {
				rawYOut = (1.f - fracY) * stageYOut[kY - 1] + fracY * stageYOut[kY];
			}

			// Laser trajectory signal output (-12V to +12V galvo safety clamp)
			float xOut = clamp(rawXOut, -12.f, 12.f);
			float yOut = clamp(rawYOut, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(xOut, c);
			outputs[Y_OUTPUT].setVoltage(yOut, c);
		}

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
	}
};

struct SmoothWidget : ModuleWidget {
	SmoothWidget(Smooth* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Smooth.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: X & Y Cutoff Frequency Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Smooth::X_FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Smooth::Y_FREQ_PARAM));

		// Row 2: X & Y Filter Slope Knobs (1-8 poles) (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Smooth::X_SLOPE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Smooth::Y_SLOPE_PARAM));

		// Row 3: Cutoff Frequency CV Depth Trimpots (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Smooth::X_FREQ_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Smooth::Y_FREQ_CV_ATTEN_PARAM));

		// Row 4: Slope CV Depth Trimpots (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Smooth::X_SLOPE_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Smooth::Y_SLOPE_CV_ATTEN_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Smooth::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Smooth::Y_INPUT));

		// Row 2: Cutoff CV Inputs (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Smooth::X_FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Smooth::Y_FREQ_CV_INPUT));

		// Row 3: Slope CV Inputs (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Smooth::X_SLOPE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Smooth::Y_SLOPE_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Smooth::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Smooth::Y_OUTPUT));
	}
};

Model* modelSmooth = createModel<Smooth, SmoothWidget>("Smooth");
