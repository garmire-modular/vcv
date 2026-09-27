#include "plugin.hpp"

// ─────────────────────────────────────────────────────────────────────
//  Trough — Dual X/Y Center-Clipping Trough Waveshaper & Interpolator
//  Inspired by Pittsburgh Modular Flamingo (Trough / Lower-Half Clipping)
// ─────────────────────────────────────────────────────────────────────

struct Trough : Module {
	enum ParamId {
		TROUGH_X_PARAM,
		TROUGH_Y_PARAM,
		TILT_X_PARAM,
		TILT_Y_PARAM,
		TROUGH_X_TRIM_PARAM,
		TROUGH_Y_TRIM_PARAM,
		TILT_X_TRIM_PARAM,
		TILT_Y_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		TROUGH_X_CV_INPUT,
		TROUGH_Y_CV_INPUT,
		TILT_X_CV_INPUT,
		TILT_Y_CV_INPUT,
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

	Trough() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Trough params (0 to 5, default 0.0)
		configParam(TROUGH_X_PARAM, 0.f, 5.f, 0.f, "X Trough Depth", "%", 0.f, 100.f / 5.f);
		configParam(TROUGH_Y_PARAM, 0.f, 5.f, 0.f, "Y Trough Depth", "%", 0.f, 100.f / 5.f);

		// Tilt params (-5V to +5V, default 0V)
		configParam(TILT_X_PARAM, -5.f, 5.f, 0.f, "X Tilt Offset", " V");
		configParam(TILT_Y_PARAM, -5.f, 5.f, 0.f, "Y Tilt Offset", " V");

		// Trough CV Attenuverters (-1 to +1, default 0.0)
		configParam(TROUGH_X_TRIM_PARAM, -1.f, 1.f, 0.f, "X trough CV depth", "%", 0.f, 100.f);
		configParam(TROUGH_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Y trough CV depth", "%", 0.f, 100.f);

		// Tilt CV Attenuverters (-1 to +1, default 0.0)
		configParam(TILT_X_TRIM_PARAM, -1.f, 1.f, 0.f, "X tilt CV depth", "%", 0.f, 100.f);
		configParam(TILT_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Y tilt CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X Signal");
		configInput(Y_INPUT, "Y Signal (Normalizes from X)");
		configInput(TROUGH_X_CV_INPUT, "X Trough CV");
		configInput(TROUGH_Y_CV_INPUT, "Y Trough CV (Normalizes from X)");
		configInput(TILT_X_CV_INPUT, "X Tilt CV");
		configInput(TILT_Y_CV_INPUT, "Y Tilt CV (Normalizes from X)");

		// Outputs
		configOutput(X_OUTPUT, "X Signal");
		configOutput(Y_OUTPUT, "Y Signal");
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int troughXChannels = inputs[TROUGH_X_CV_INPUT].getChannels();
		int troughYChannels = inputs[TROUGH_Y_CV_INPUT].getChannels();
		int tiltXChannels = inputs[TILT_X_CV_INPUT].getChannels();
		int tiltYChannels = inputs[TILT_Y_CV_INPUT].getChannels();

		int channels = std::max({xChannels, yChannels, troughXChannels, troughYChannels, tiltXChannels, tiltYChannels, 1});

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float troughXParam = params[TROUGH_X_PARAM].getValue();
		float troughYParam = params[TROUGH_Y_PARAM].getValue();
		float tiltXParam = params[TILT_X_PARAM].getValue();
		float tiltYParam = params[TILT_Y_PARAM].getValue();

		float troughXTrim = params[TROUGH_X_TRIM_PARAM].getValue();
		float troughYTrim = params[TROUGH_Y_TRIM_PARAM].getValue();
		float tiltXTrim = params[TILT_X_TRIM_PARAM].getValue();
		float tiltYTrim = params[TILT_Y_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool troughYCvConnected = inputs[TROUGH_Y_CV_INPUT].isConnected();
		bool tiltYCvConnected = inputs[TILT_Y_CV_INPUT].isConnected();

		for (int c = 0; c < channels; c++) {
			// X & Y Inputs
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			// Trough CVs
			float troughXCv = inputs[TROUGH_X_CV_INPUT].getPolyVoltage(c);
			float troughYCv = troughYCvConnected ? inputs[TROUGH_Y_CV_INPUT].getPolyVoltage(c) : troughXCv;

			// Tilt CVs
			float tiltXCv = inputs[TILT_X_CV_INPUT].getPolyVoltage(c);
			float tiltYCv = tiltYCvConnected ? inputs[TILT_Y_CV_INPUT].getPolyVoltage(c) : tiltXCv;

			// Effective Trough Depth (normalized 0.0 to 1.0+)
			float normTroughX = std::max(0.f, (troughXParam / 5.f) + (troughXCv / 5.f) * troughXTrim);
			float normTroughY = std::max(0.f, (troughYParam / 5.f) + (troughYCv / 5.f) * troughYTrim);

			// Effective Tilt Offset
			float tiltX = tiltXParam + tiltXCv * tiltXTrim;
			float tiltY = tiltYParam + tiltYCv * tiltYTrim;

			// Signal-Aware Trough Center Clipping & Harmonic Interpolation process
			auto processChannel = [](float inVal, float normTrough, float tiltVal) -> float {
				// DC Tilt Offset
				float shifted = inVal + tiltVal;

				// Base reference amplitude
				float vPeak = std::max(std::abs(inVal), 5.f);

				// Multiplier factor for center clipping
				float k = 1.f + 9.f * normTrough;

				// Normalized phase
				float xNorm = shifted / vPeak;

				// Trough (negative half-space) center-clipping transfer function:
				// When xNorm < 0, negative values pull toward center with harmonic interpolation curve
				float folded;
				if (xNorm < 0.f) {
					// Sine-folded trough compression with gain interpolation restoration
					folded = -vPeak * std::sin((float)M_PI_2 * std::fabs(std::sin((float)M_PI_2 * k * xNorm)));
				} else {
					// Positive crest passed through with gain compensation
					folded = shifted;
				}

				// Blend dry/wet at 0% trough depth
				float outVal;
				if (normTrough <= 0.05f) {
					float blend = normTrough / 0.05f;
					outVal = (1.f - blend) * inVal + blend * folded;
				} else {
					outVal = folded;
				}

				return outVal;
			};

			float outX = processChannel(inX, normTroughX, tiltX);
			float outY = processChannel(inY, normTroughY, tiltY);

			// Clamping safety for galvo drives (-12V to +12V)
			outX = clamp(outX, -12.f, 12.f);
			outY = clamp(outY, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(outX, c);
			outputs[Y_OUTPUT].setVoltage(outY, c);
		}
	}
};

struct TroughWidget : ModuleWidget {
	TroughWidget(Trough* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Trough.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Top 2/3: Controls & Attenuverters
		// Row 1: X & Y Trough Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Trough::TROUGH_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Trough::TROUGH_Y_PARAM));

		// Row 2: X & Y Tilt Knobs (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Trough::TILT_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Trough::TILT_Y_PARAM));

		// Row 3: Trough CV Depth Trimpots (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Trough::TROUGH_X_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Trough::TROUGH_Y_TRIM_PARAM));

		// Row 4: Tilt CV Depth Trimpots (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Trough::TILT_X_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Trough::TILT_Y_TRIM_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Trough::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Trough::Y_INPUT));

		// Row 2: Trough CV Inputs (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Trough::TROUGH_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Trough::TROUGH_Y_CV_INPUT));

		// Row 3: Tilt CV Inputs (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Trough::TILT_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Trough::TILT_Y_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Trough::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Trough::Y_OUTPUT));
	}
};

Model* modelTrough = createModel<Trough, TroughWidget>("Trough");
