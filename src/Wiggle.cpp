#include "plugin.hpp"
#include <cmath>

struct Wiggle : Module {
	enum ParamId {
		X_DEPTH_PARAM,
		Y_DEPTH_PARAM,
		X_RATE_PARAM,
		Y_RATE_PARAM,
		X_DEPTH_CV_ATTEN_PARAM,
		Y_DEPTH_CV_ATTEN_PARAM,
		X_RATE_CV_ATTEN_PARAM,
		Y_RATE_CV_ATTEN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_DEPTH_CV_INPUT,
		Y_DEPTH_CV_INPUT,
		X_RATE_CV_INPUT,
		Y_RATE_CV_INPUT,
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

	// Wobble oscillator phase states for polyphonic channels (up to 16 channels)
	float phaseX[16] = {};
	float phaseY[16] = {};

	Wiggle() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Depth parameters (0% to 100%)
		configParam(X_DEPTH_PARAM, 0.f, 1.f, 0.f, "X wiggle depth", "%", 0.f, 100.f);
		configParam(Y_DEPTH_PARAM, 0.f, 1.f, 0.f, "Y wiggle depth", "%", 0.f, 100.f);

		// Row 2: Rate parameters (0% to 100%)
		configParam(X_RATE_PARAM, 0.f, 1.f, 0.5f, "X wiggle rate", "%", 0.f, 100.f);
		configParam(Y_RATE_PARAM, 0.f, 1.f, 0.5f, "Y wiggle rate", "%", 0.f, 100.f);

		// Attenuverters
		configParam(X_DEPTH_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "X depth CV depth", "%", 0.f, 100.f);
		configParam(Y_DEPTH_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Y depth CV depth", "%", 0.f, 100.f);
		configParam(X_RATE_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "X rate CV depth", "%", 0.f, 100.f);
		configParam(Y_RATE_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Y rate CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_DEPTH_CV_INPUT, "X depth CV");
		configInput(Y_DEPTH_CV_INPUT, "Y depth CV");
		configInput(X_RATE_CV_INPUT, "X rate CV");
		configInput(Y_RATE_CV_INPUT, "Y rate CV");

		// Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	void onReset() override {
		for (int c = 0; c < 16; c++) {
			phaseX[c] = 0.f;
			phaseY[c] = 0.f;
		}
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max({
			inputs[X_INPUT].getChannels(),
			inputs[Y_INPUT].getChannels(),
			inputs[X_DEPTH_CV_INPUT].getChannels(),
			inputs[Y_DEPTH_CV_INPUT].getChannels(),
			inputs[X_RATE_CV_INPUT].getChannels(),
			inputs[Y_RATE_CV_INPUT].getChannels()
		});
		if (channels == 0) {
			channels = 1;
		}

		for (int c = 0; c < channels; c++) {
			// Normalisation: Y input normalises to X input if unpatched
			float xIn = inputs[X_INPUT].getPolyVoltage(c);
			float yIn = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : xIn;

			// Normalisation: Switching jack logic for DEPTH and RATE CV
			float xDepthCv = inputs[X_DEPTH_CV_INPUT].getPolyVoltage(c);
			float yDepthCv = inputs[Y_DEPTH_CV_INPUT].isConnected() ? inputs[Y_DEPTH_CV_INPUT].getPolyVoltage(c) : xDepthCv;

			float xRateCv = inputs[X_RATE_CV_INPUT].getPolyVoltage(c);
			float yRateCv = inputs[Y_RATE_CV_INPUT].isConnected() ? inputs[Y_RATE_CV_INPUT].getPolyVoltage(c) : xRateCv;

			// Calculate effective depth amounts (0.0 to 1.0)
			float depthX = clamp(params[X_DEPTH_PARAM].getValue() + (xDepthCv / 5.f) * params[X_DEPTH_CV_ATTEN_PARAM].getValue(), 0.f, 1.f);
			float depthY = clamp(params[Y_DEPTH_PARAM].getValue() + (yDepthCv / 5.f) * params[Y_DEPTH_CV_ATTEN_PARAM].getValue(), 0.f, 1.f);

			// Calculate effective rate amounts (0.0 to 1.0)
			float rateValX = clamp(params[X_RATE_PARAM].getValue() + (xRateCv / 5.f) * params[X_RATE_CV_ATTEN_PARAM].getValue(), 0.f, 1.f);
			float rateValY = clamp(params[Y_RATE_PARAM].getValue() + (yRateCv / 5.f) * params[Y_RATE_CV_ATTEN_PARAM].getValue(), 0.f, 1.f);

			// Convert rate (0..1) to frequency (0.1 Hz to 500 Hz exponential)
			float fX = 0.1f * std::pow(5000.f, rateValX);
			float fY = 0.1f * std::pow(5000.f, rateValY);

			// Advance oscillator phases
			phaseX[c] += fX * args.sampleTime;
			if (phaseX[c] >= 1.f) phaseX[c] -= std::floor(phaseX[c]);

			phaseY[c] += fY * args.sampleTime;
			if (phaseY[c] >= 1.f) phaseY[c] -= std::floor(phaseY[c]);

			// Multi-harmonic organic laser wobble waveforms
			float pX = phaseX[c] * 2.f * (float)M_PI;
			float wobbleX = std::sin(pX) + 0.5f * std::sin(2.37f * pX + 1.2f) + 0.25f * std::sin(5.13f * pX + 2.7f);
			wobbleX *= (1.f / 1.75f);

			float pY = phaseY[c] * 2.f * (float)M_PI;
			float wobbleY = std::sin(pY + 1.57f) + 0.5f * std::sin(2.41f * pY + 0.8f) + 0.25f * std::sin(4.89f * pY + 3.1f);
			wobbleY *= (1.f / 1.75f);

			// Calculate voltage displacement (0V to 5V max)
			float dispX = depthX * 5.0f * wobbleX;
			float dispY = depthY * 5.0f * wobbleY;

			// Laser trajectory signal output (-12V to +12V galvo safety clamp)
			float xOut = clamp(xIn + dispX, -12.f, 12.f);
			float yOut = clamp(yIn + dispY, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(xOut, c);
			outputs[Y_OUTPUT].setVoltage(yOut, c);
		}

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
	}
};

struct WiggleWidget : ModuleWidget {
	WiggleWidget(Wiggle* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Wiggle.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: X & Y Wiggle Depth Knobs (Labelled X and Y on panel, Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Wiggle::X_DEPTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Wiggle::Y_DEPTH_PARAM));

		// Row 2: X & Y Wiggle Rate Knobs (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Wiggle::X_RATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Wiggle::Y_RATE_PARAM));

		// Row 3: Depth CV Depth Trimpots (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Wiggle::X_DEPTH_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Wiggle::Y_DEPTH_CV_ATTEN_PARAM));

		// Row 4: Rate CV Depth Trimpots (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Wiggle::X_RATE_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Wiggle::Y_RATE_CV_ATTEN_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Wiggle::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Wiggle::Y_INPUT));

		// Row 2: Depth CV Inputs (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Wiggle::X_DEPTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Wiggle::Y_DEPTH_CV_INPUT));

		// Row 3: Rate CV Inputs (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Wiggle::X_RATE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Wiggle::Y_RATE_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Wiggle::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Wiggle::Y_OUTPUT));
	}
};

Model* modelWiggle = createModel<Wiggle, WiggleWidget>("Wiggle");
