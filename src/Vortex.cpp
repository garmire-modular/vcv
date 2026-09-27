#include "plugin.hpp"
#include <cmath>
#include <algorithm>

struct Vortex : Module {
	enum ParamId {
		X_DEPTH_PARAM,
		Y_DEPTH_PARAM,
		X_COUNT_PARAM,
		Y_COUNT_PARAM,
		X_DEPTH_TRIM_PARAM,
		Y_DEPTH_TRIM_PARAM,
		X_COUNT_TRIM_PARAM,
		Y_COUNT_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_DEPTH_CV_INPUT,
		Y_DEPTH_CV_INPUT,
		X_COUNT_CV_INPUT,
		Y_COUNT_CV_INPUT,
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

	Vortex() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Vortex Depth (0.0 to 1.0 strength, default 0.0)
		configParam(X_DEPTH_PARAM, 0.f, 1.f, 0.f, "X Vortex Depth", "%", 0.f, 100.f);
		configParam(Y_DEPTH_PARAM, 0.f, 1.f, 0.f, "Y Vortex Depth", "%", 0.f, 100.f);

		// Vortex Count (1.0 to 8.0 harmonic multiplier, default 1.0)
		configParam(X_COUNT_PARAM, 1.f, 8.f, 1.f, "X Vortex Count", "", 0.f, 1.f);
		configParam(Y_COUNT_PARAM, 1.f, 8.f, 1.f, "Y Vortex Count", "", 0.f, 1.f);

		// CV Attenuverters (-1.0 to +1.0)
		configParam(X_DEPTH_TRIM_PARAM, -1.f, 1.f, 0.f, "X Depth CV Attenuverter", "%", 0.f, 100.f);
		configParam(Y_DEPTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Y Depth CV Attenuverter", "%", 0.f, 100.f);
		configParam(X_COUNT_TRIM_PARAM, -1.f, 1.f, 0.f, "X Count CV Attenuverter", "%", 0.f, 100.f);
		configParam(Y_COUNT_TRIM_PARAM, -1.f, 1.f, 0.f, "Y Count CV Attenuverter", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X Signal");
		configInput(Y_INPUT, "Y Signal (Normalizes from X)");
		configInput(X_DEPTH_CV_INPUT, "X Depth CV");
		configInput(Y_DEPTH_CV_INPUT, "Y Depth CV (Normalizes from X Depth CV)");
		configInput(X_COUNT_CV_INPUT, "X Count CV");
		configInput(Y_COUNT_CV_INPUT, "Y Count CV (Normalizes from X Count CV)");

		// Outputs
		configOutput(X_OUTPUT, "X Signal");
		configOutput(Y_OUTPUT, "Y Signal");
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int xDepthCvChannels = inputs[X_DEPTH_CV_INPUT].getChannels();
		int yDepthCvChannels = inputs[Y_DEPTH_CV_INPUT].getChannels();
		int xCountCvChannels = inputs[X_COUNT_CV_INPUT].getChannels();
		int yCountCvChannels = inputs[Y_COUNT_CV_INPUT].getChannels();

		int numChannels = std::max({xChannels, yChannels, xDepthCvChannels, yDepthCvChannels, xCountCvChannels, yCountCvChannels, 1});

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float xDepthParam = params[X_DEPTH_PARAM].getValue();
		float yDepthParam = params[Y_DEPTH_PARAM].getValue();
		float xCountParam = params[X_COUNT_PARAM].getValue();
		float yCountParam = params[Y_COUNT_PARAM].getValue();

		float xDepthTrim = params[X_DEPTH_TRIM_PARAM].getValue();
		float yDepthTrim = params[Y_DEPTH_TRIM_PARAM].getValue();
		float xCountTrim = params[X_COUNT_TRIM_PARAM].getValue();
		float yCountTrim = params[Y_COUNT_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool cvYDepthConnected = inputs[Y_DEPTH_CV_INPUT].isConnected();
		bool cvYCountConnected = inputs[Y_COUNT_CV_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float xDepthCV = inputs[X_DEPTH_CV_INPUT].getPolyVoltage(c) / 5.f;
			float yDepthCV = cvYDepthConnected ? inputs[Y_DEPTH_CV_INPUT].getPolyVoltage(c) : xDepthCV;

			float xCountCV = inputs[X_COUNT_CV_INPUT].getPolyVoltage(c) / 5.f;
			float yCountCV = cvYCountConnected ? inputs[Y_COUNT_CV_INPUT].getPolyVoltage(c) : xCountCV;

			float depthX = clamp(xDepthParam + xDepthCV * xDepthTrim, 0.f, 1.f);
			float depthY = clamp(yDepthParam + yDepthCV * yDepthTrim, 0.f, 1.f);

			float countX = clamp(xCountParam + xCountCV * xCountTrim * 4.f, 1.f, 8.f);
			float countY = clamp(yCountParam + yCountCV * yCountTrim * 4.f, 1.f, 8.f);

			// Polar transformation
			float r = std::sqrt(inX * inX + inY * inY);
			float theta = std::atan2(inY, inX);

			// Vortex Phase Shift Modulation
			float phaseX = theta * countX;
			float phaseY = theta * countY;

			float deltaThetaX = depthX * std::sin(phaseX);
			float deltaThetaY = depthY * std::sin(phaseY);

			// Radial/Angular trajectory deformation
			float rX = r * (1.f + 0.2f * depthX * std::cos(phaseX));
			float rY = r * (1.f + 0.2f * depthY * std::cos(phaseY));

			float outX = rX * std::cos(theta + deltaThetaX);
			float outY = rY * std::sin(theta + deltaThetaY);

			// Laser safety voltage bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.f, 12.f), c);
		}
	}
};

struct VortexWidget : ModuleWidget {
	VortexWidget(Vortex* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Vortex.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: X DEPTH (7.62 mm) & Y DEPTH (22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Vortex::X_DEPTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Vortex::Y_DEPTH_PARAM));

		// Row 2: X COUNT (7.62 mm) & Y COUNT (22.86 mm) Knobs (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Vortex::X_COUNT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Vortex::Y_COUNT_PARAM));

		// Attenuverter Trimpots
		// Trimpot Row 1: X DEPTH CV (7.62 mm), Y DEPTH CV (22.86 mm) (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Vortex::X_DEPTH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, Vortex::Y_DEPTH_TRIM_PARAM));

		// Trimpot Row 2: X COUNT CV (7.62 mm), Y COUNT CV (22.86 mm) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Vortex::X_COUNT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Vortex::Y_COUNT_TRIM_PARAM));

		// Bottom I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Vortex::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Vortex::Y_INPUT));

		// Row 2: X DEPTH CV (X = 7.62 mm), Y DEPTH CV (X = 22.86 mm) (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Vortex::X_DEPTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Vortex::Y_DEPTH_CV_INPUT));

		// Row 3: X COUNT CV (X = 7.62 mm), Y COUNT CV (X = 22.86 mm) (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Vortex::X_COUNT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Vortex::Y_COUNT_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Vortex::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Vortex::Y_OUTPUT));
	}
};

Model* modelVortex = createModel<Vortex, VortexWidget>("Vortex");
