#include "plugin.hpp"
#include <cmath>

struct Steps : Module {
	enum ParamId {
		STEPS_X_PARAM,
		STEPS_Y_PARAM,
		STEPS_X_TRIM_PARAM,
		STEPS_Y_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		STEPS_X_CV_INPUT,
		STEPS_Y_CV_INPUT,
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

	Steps() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Steps Crush params (0 to 5, default 0.0)
		configParam(STEPS_X_PARAM, 0.f, 5.f, 0.f, "X Steps Crush", "%", 0.f, 100.f / 5.f);
		configParam(STEPS_Y_PARAM, 0.f, 5.f, 0.f, "Y Steps Crush", "%", 0.f, 100.f / 5.f);

		// Steps CV Attenuverters (-1 to +1, default 0.0)
		configParam(STEPS_X_TRIM_PARAM, -1.f, 1.f, 0.f, "X steps CV depth", "%", 0.f, 100.f);
		configParam(STEPS_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Y steps CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X Signal");
		configInput(Y_INPUT, "Y Signal (Normalizes from X)");
		configInput(STEPS_X_CV_INPUT, "X Steps CV");
		configInput(STEPS_Y_CV_INPUT, "Y Steps CV (Normalizes from X)");

		// Outputs
		configOutput(X_OUTPUT, "X Signal");
		configOutput(Y_OUTPUT, "Y Signal");
	}

	static float unquantizedBitCrush(float vIn, float bits) {
		float vPeak = std::max(std::abs(vIn), 5.f);
		float nSteps = std::pow(2.f, bits);
		float halfSteps = nSteps * 0.5f;

		float x = vIn / vPeak;
		float xQuant = std::round(x * halfSteps) / halfSteps;

		return clamp(xQuant * vPeak, -12.f, 12.f);
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int cvXChannels = inputs[STEPS_X_CV_INPUT].getChannels();
		int cvYChannels = inputs[STEPS_Y_CV_INPUT].getChannels();

		int channels = std::max({xChannels, yChannels, cvXChannels, cvYChannels, 1});

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float stepsXParam = params[STEPS_X_PARAM].getValue();
		float stepsYParam = params[STEPS_Y_PARAM].getValue();

		float stepsXTrim = params[STEPS_X_TRIM_PARAM].getValue();
		float stepsYTrim = params[STEPS_Y_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool cvYCvConnected = inputs[STEPS_Y_CV_INPUT].isConnected();

		for (int c = 0; c < channels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cvX = inputs[STEPS_X_CV_INPUT].getPolyVoltage(c);
			float cvY = cvYCvConnected ? inputs[STEPS_Y_CV_INPUT].getPolyVoltage(c) : cvX;

			// Normalized Crush Control (0.0 to 1.0)
			float sX = clamp((stepsXParam / 5.f) + (cvX / 5.f) * stepsXTrim, 0.f, 1.f);
			float sY = clamp((stepsYParam / 5.f) + (cvY / 5.f) * stepsYTrim, 0.f, 1.f);

			auto processCrush = [](float inVal, float sVal) -> float {
				// 0% to 5% Crossfade: 0% is 100% dry pass-through, fading over first 5% into 16-bit to 1-bit unquantized bit crusher
				if (sVal <= 0.05f) {
					float blend = sVal / 0.05f;
					float crushed16 = unquantizedBitCrush(inVal, 16.0f);
					return (1.f - blend) * inVal + blend * crushed16;
				}

				// 5% to 100%: Non-linear perceptual bit depth curve (giving expanded knob travel for 6.0 to 1.0 bit heavy crush region)
				float t = (sVal - 0.05f) / 0.95f;
				float bits = 1.0f + 15.0f * std::pow(1.0f - t, 2.2f);
				return unquantizedBitCrush(inVal, bits);
			};

			float outX = processCrush(inX, sX);
			float outY = processCrush(inY, sY);

			// Safety clamping for laser galvo drives (-12V to +12V)
			outX = clamp(outX, -12.f, 12.f);
			outY = clamp(outY, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(outX, c);
			outputs[Y_OUTPUT].setVoltage(outY, c);
		}
	}
};

struct StepsWidget : ModuleWidget {
	StepsWidget(Steps* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Steps.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: X & Y Steps Crush Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Steps::STEPS_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Steps::STEPS_Y_PARAM));

		// Attenuverter Row (Center Y = 73.00 mm, bottom-aligned with 1-knob layout)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Steps::STEPS_X_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Steps::STEPS_Y_TRIM_PARAM));

		// Bottom I/O Jacks (Bottom-aligned stacking)
		// Row 1: Signal Inputs (Center Y = 89.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Steps::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Steps::Y_INPUT));

		// Row 2: Amount CV Inputs (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Steps::STEPS_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Steps::STEPS_Y_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Steps::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Steps::Y_OUTPUT));
	}
};

Model* modelSteps = createModel<Steps, StepsWidget>("Steps");
