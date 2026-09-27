#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Knots — Dual X/Y Polar Angle Harmonic Multiplier & Lissajous Engine
//  6 HP module multiplying polar angle velocities independently on X
//  and Y axes to generate intertwined Lissajous knots, kaleidoscopic
//  radial sectors, and harmonic loop trajectories.
// ─────────────────────────────────────────────────────────────────────

struct Knots : Module {
	enum ParamId {
		X_KNOTS_PARAM,
		Y_KNOTS_PARAM,
		X_KNOTS_TRIM_PARAM,
		Y_KNOTS_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_KNOTS_CV_INPUT,
		Y_KNOTS_CV_INPUT,
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

	Knots() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Knots Multipliers (1.0 to 16.0 harmonics, default 1.0 for 1:1 bypass)
		configParam(X_KNOTS_PARAM, 1.f, 16.f, 1.f, "X knots", "", 0.f, 1.f, 0.f);
		configParam(Y_KNOTS_PARAM, 1.f, 16.f, 1.f, "Y knots", "", 0.f, 1.f, 0.f);

		// Attenuverters (default 0%)
		configParam(X_KNOTS_TRIM_PARAM, -1.f, 1.f, 0.f, "X knots CV depth", "%", 0.f, 100.f);
		configParam(Y_KNOTS_TRIM_PARAM, -1.f, 1.f, 0.f, "Y knots CV depth", "%", 0.f, 100.f);

		// Inputs (Rack automatically appends "input" to tooltips)
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_KNOTS_CV_INPUT, "X knots CV");
		configInput(Y_KNOTS_CV_INPUT, "Y knots CV");

		// Outputs (Rack automatically appends "output" to tooltips)
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int xCvChannels = inputs[X_KNOTS_CV_INPUT].getChannels();
		int yCvChannels = inputs[Y_KNOTS_CV_INPUT].getChannels();

		int numChannels = std::max({xChannels, yChannels, xCvChannels, yCvChannels, 1});

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float xKnotsParam = params[X_KNOTS_PARAM].getValue();
		float yKnotsParam = params[Y_KNOTS_PARAM].getValue();
		float xKnotsTrim = params[X_KNOTS_TRIM_PARAM].getValue();
		float yKnotsTrim = params[Y_KNOTS_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool cvYConnected = inputs[Y_KNOTS_CV_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float xKnotsCV = inputs[X_KNOTS_CV_INPUT].getPolyVoltage(c) / 5.f;
			float yKnotsCV = cvYConnected ? inputs[Y_KNOTS_CV_INPUT].getPolyVoltage(c) : xKnotsCV;

			float kX = clamp(xKnotsParam + xKnotsCV * xKnotsTrim * 8.f, 1.f, 16.f);
			float kY = clamp(yKnotsParam + yKnotsCV * yKnotsTrim * 8.f, 1.f, 16.f);

			// Polar transformation: distance r is preserved, angle theta is multiplied
			float r = std::hypot(inX, inY);
			float theta = std::atan2(inY, inX);

			// Polar angle harmonic multiplication & sector wrapping
			float thetaX = std::fmod((theta + (float)M_PI) * kX, 2.f * (float)M_PI);
			if (thetaX < 0.f) thetaX += 2.f * (float)M_PI;
			thetaX -= (float)M_PI;

			float thetaY = std::fmod((theta + (float)M_PI) * kY, 2.f * (float)M_PI);
			if (thetaY < 0.f) thetaY += 2.f * (float)M_PI;
			thetaY -= (float)M_PI;

			float outX = r * std::cos(thetaX);
			float outY = r * std::sin(thetaY);

			// Laser safety voltage bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.f, 12.f), c);
		}
	}
};

struct KnotsWidget : ModuleWidget {
	KnotsWidget(Knots* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Knots.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: X Knots (7.62 mm) & Y Knots (22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Knots::X_KNOTS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Knots::Y_KNOTS_PARAM));

		// Attenuverter Trimpots
		// Trimpot Row 1: X Knots CV (7.62 mm), Y Knots CV (22.86 mm) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Knots::X_KNOTS_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Knots::Y_KNOTS_TRIM_PARAM));

		// Bottom I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Knots::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Knots::Y_INPUT));

		// Row 2: X Knots CV (X = 7.62 mm), Y Knots CV (X = 22.86 mm) (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Knots::X_KNOTS_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Knots::Y_KNOTS_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Knots::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Knots::Y_OUTPUT));
	}
};

Model* modelKnots = createModel<Knots, KnotsWidget>("Knots");
