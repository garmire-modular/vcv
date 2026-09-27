#include "plugin.hpp"
#include <cmath>

struct Quake : Module {
	enum ParamId {
		AMOUNT_X_PARAM,
		AMOUNT_Y_PARAM,
		SPEED_X_PARAM,
		SPEED_Y_PARAM,
		AMOUNT_X_TRIM_PARAM,
		AMOUNT_Y_TRIM_PARAM,
		SPEED_X_TRIM_PARAM,
		SPEED_Y_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		AMOUNT_X_CV_INPUT,
		AMOUNT_Y_CV_INPUT,
		SPEED_X_CV_INPUT,
		SPEED_Y_CV_INPUT,
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

	// Per-channel internal state for humanization noise generators (up to 16 poly channels)
	struct ChannelState {
		float phaseX = 0.f;
		float phaseY = 0.f;
		float targetJitterX = 0.f;
		float currentJitterX = 0.f;
		float targetJitterY = 0.f;
		float currentJitterY = 0.f;
		uint32_t seedX = 12345;
		uint32_t seedY = 67890;
	};

	ChannelState state[16];

	Quake() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Amount params (0 to 5, default 0.0)
		configParam(AMOUNT_X_PARAM, 0.f, 5.f, 0.f, "X Humanize Amount", "%", 0.f, 100.f / 5.f);
		configParam(AMOUNT_Y_PARAM, 0.f, 5.f, 0.f, "Y Humanize Amount", "%", 0.f, 100.f / 5.f);

		// Speed params (0.1Hz to 500Hz, log scale, default 10Hz)
		configParam(SPEED_X_PARAM, 0.1f, 500.f, 10.f, "X Speed", " Hz");
		configParam(SPEED_Y_PARAM, 0.1f, 500.f, 10.f, "Y Speed", " Hz");

		// Amount CV Attenuverters (-1 to +1, default 0.0)
		configParam(AMOUNT_X_TRIM_PARAM, -1.f, 1.f, 0.f, "X amount CV depth", "%", 0.f, 100.f);
		configParam(AMOUNT_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Y amount CV depth", "%", 0.f, 100.f);

		// Speed CV Attenuverters (-1 to +1, default 0.0)
		configParam(SPEED_X_TRIM_PARAM, -1.f, 1.f, 0.f, "X speed CV depth", "%", 0.f, 100.f);
		configParam(SPEED_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Y speed CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X Signal");
		configInput(Y_INPUT, "Y Signal (Normalizes from X)");
		configInput(AMOUNT_X_CV_INPUT, "X Amount CV");
		configInput(AMOUNT_Y_CV_INPUT, "Y Amount CV (Normalizes from X)");
		configInput(SPEED_X_CV_INPUT, "X Speed CV");
		configInput(SPEED_Y_CV_INPUT, "Y Speed CV (Normalizes from X)");

		// Outputs
		configOutput(X_OUTPUT, "X Signal");
		configOutput(Y_OUTPUT, "Y Signal");

		for (int i = 0; i < 16; i++) {
			state[i].seedX = 12345 + i * 999;
			state[i].seedY = 67890 + i * 777;
		}
	}

	float nextRandom(uint32_t& seed) {
		seed = seed * 1664525u + 1013904223u;
		return ((float)(seed & 0x00ffffff) / (float)0x00ffffff) * 2.f - 1.f;
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int amtXChannels = inputs[AMOUNT_X_CV_INPUT].getChannels();
		int amtYChannels = inputs[AMOUNT_Y_CV_INPUT].getChannels();
		int speedXChannels = inputs[SPEED_X_CV_INPUT].getChannels();
		int speedYChannels = inputs[SPEED_Y_CV_INPUT].getChannels();

		int channels = std::max({xChannels, yChannels, amtXChannels, amtYChannels, speedXChannels, speedYChannels, 1});

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float amtXParam = params[AMOUNT_X_PARAM].getValue();
		float amtYParam = params[AMOUNT_Y_PARAM].getValue();
		float speedXParam = params[SPEED_X_PARAM].getValue();
		float speedYParam = params[SPEED_Y_PARAM].getValue();

		float amtXTrim = params[AMOUNT_X_TRIM_PARAM].getValue();
		float amtYTrim = params[AMOUNT_Y_TRIM_PARAM].getValue();
		float speedXTrim = params[SPEED_X_TRIM_PARAM].getValue();
		float speedYTrim = params[SPEED_Y_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool amtYCvConnected = inputs[AMOUNT_Y_CV_INPUT].isConnected();
		bool speedYCvConnected = inputs[SPEED_Y_CV_INPUT].isConnected();

		for (int c = 0; c < channels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float amtXCv = inputs[AMOUNT_X_CV_INPUT].getPolyVoltage(c);
			float amtYCv = amtYCvConnected ? inputs[AMOUNT_Y_CV_INPUT].getPolyVoltage(c) : amtXCv;

			float speedXCv = inputs[SPEED_X_CV_INPUT].getPolyVoltage(c);
			float speedYCv = speedYCvConnected ? inputs[SPEED_Y_CV_INPUT].getPolyVoltage(c) : speedXCv;

			// Amount 0.0 to 1.0
			float amtX = clamp((amtXParam / 5.f) + (amtXCv / 5.f) * amtXTrim, 0.f, 1.f);
			float amtY = clamp((amtYParam / 5.f) + (amtYCv / 5.f) * amtYTrim, 0.f, 1.f);

			// Speed 0.1Hz to 500Hz
			float speedX = clamp(speedXParam * std::pow(2.f, (speedXCv / 5.f) * speedXTrim * 3.f), 0.1f, 500.f);
			float speedY = clamp(speedYParam * std::pow(2.f, (speedYCv / 5.f) * speedYTrim * 3.f), 0.1f, 500.f);

			// Process X Humanization
			state[c].phaseX += speedX * args.sampleTime;
			if (state[c].phaseX >= 1.f) {
				state[c].phaseX -= 1.f;
				state[c].targetJitterX = nextRandom(state[c].seedX);
			}
			// Lowpass filter for smooth humanization transition
			state[c].currentJitterX += (state[c].targetJitterX - state[c].currentJitterX) * std::min(1.f, 20.f * speedX * args.sampleTime);

			// Process Y Humanization
			state[c].phaseY += speedY * args.sampleTime;
			if (state[c].phaseY >= 1.f) {
				state[c].phaseY -= 1.f;
				state[c].targetJitterY = nextRandom(state[c].seedY);
			}
			state[c].currentJitterY += (state[c].targetJitterY - state[c].currentJitterY) * std::min(1.f, 20.f * speedY * args.sampleTime);

			// Apply organic humanizing micro-displacement (up to 1.5V max at 100% amount)
			float outX = inX + (state[c].currentJitterX * amtX * 1.5f);
			float outY = inY + (state[c].currentJitterY * amtY * 1.5f);

			// Galvo safety clamping (-12V to +12V)
			outX = clamp(outX, -12.f, 12.f);
			outY = clamp(outY, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(outX, c);
			outputs[Y_OUTPUT].setVoltage(outY, c);
		}
	}
};

struct QuakeWidget : ModuleWidget {
	QuakeWidget(Quake* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Quake.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Top 2/3: Main Controls & Attenuverters
		// Row 1: X & Y Amount Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Quake::AMOUNT_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Quake::AMOUNT_Y_PARAM));

		// Row 2: X & Y Speed Knobs (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Quake::SPEED_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Quake::SPEED_Y_PARAM));

		// Row 3: Amount CV Depth Trimpots (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Quake::AMOUNT_X_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Quake::AMOUNT_Y_TRIM_PARAM));

		// Row 4: Speed CV Depth Trimpots (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Quake::SPEED_X_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Quake::SPEED_Y_TRIM_PARAM));

		// Bottom 1/3: I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Quake::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Quake::Y_INPUT));

		// Row 2: Amount CV Inputs (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Quake::AMOUNT_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Quake::AMOUNT_Y_CV_INPUT));

		// Row 3: Speed CV Inputs (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Quake::SPEED_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Quake::SPEED_Y_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Quake::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Quake::Y_OUTPUT));
	}
};

Model* modelQuake = createModel<Quake, QuakeWidget>("Quake");
