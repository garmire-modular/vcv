#include "plugin.hpp"
#include <cmath>
#include <algorithm>

struct Petals : Module {
	enum ParamId {
		X_SEGM_PARAM,
		Y_SEGM_PARAM,
		X_SEGM_TRIM_PARAM,
		Y_SEGM_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		X_SEGM_CV_INPUT,
		Y_SEGM_CV_INPUT,
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

	Petals() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// X & Y Segment Multipliers (-16.0 to +16.0, default +1.0 for 1:1 bypass)
		configParam(X_SEGM_PARAM, -16.f, 16.f, 1.f, "X petals", "", 0.f, 1.f, 0.f);
		configParam(Y_SEGM_PARAM, -16.f, 16.f, 1.f, "Y petals", "", 0.f, 1.f, 0.f);

		// Attenuverters (default 0%)
		configParam(X_SEGM_TRIM_PARAM, -1.f, 1.f, 0.f, "X petals CV depth", "%", 0.f, 100.f);
		configParam(Y_SEGM_TRIM_PARAM, -1.f, 1.f, 0.f, "Y petals CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(X_SEGM_CV_INPUT, "X petals CV");
		configInput(Y_SEGM_CV_INPUT, "Y petals CV");

		// Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int xCvChannels = inputs[X_SEGM_CV_INPUT].getChannels();
		int yCvChannels = inputs[Y_SEGM_CV_INPUT].getChannels();

		int numChannels = std::max({xChannels, yChannels, xCvChannels, yCvChannels, 1});

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float xSegmParam = params[X_SEGM_PARAM].getValue();
		float ySegmParam = params[Y_SEGM_PARAM].getValue();
		float xSegmTrim = params[X_SEGM_TRIM_PARAM].getValue();
		float ySegmTrim = params[Y_SEGM_TRIM_PARAM].getValue();

		bool yInputConnected = inputs[Y_INPUT].isConnected();
		bool cvYConnected = inputs[Y_SEGM_CV_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float xSegmCV = inputs[X_SEGM_CV_INPUT].getPolyVoltage(c) / 5.f;
			float ySegmCV = cvYConnected ? inputs[Y_SEGM_CV_INPUT].getPolyVoltage(c) : xSegmCV;

			float xSegm = clamp(xSegmParam + xSegmCV * xSegmTrim * 8.f, -16.f, 16.f);
			float ySegm = clamp(ySegmParam + ySegmCV * ySegmTrim * 8.f, -16.f, 16.f);

			// Polar transformation
			float r = std::sqrt(inX * inX + inY * inY);
			float theta = std::atan2(inY, inX);

			// Rose / Ranunculus spiraling radius transformation for X
			float nX = std::abs(xSegm);
			float rScaledX = r;
			float thetaX = theta;
			if (nX > 1.f) {
				float normTheta = (theta + (float)M_PI) / (2.f * (float)M_PI);
				float phaseX = std::fmod(normTheta * nX, nX);
				if (phaseX < 0.f) phaseX += nX;
				int kX = (int)std::floor(phaseX);

				int lX = kX / 4;
				int topLayerX = (int)std::floor((nX - 1.f) / 4.f);
				int deltaLX = std::max(0, topLayerX - lX);

				float spiralScaleX = 1.f / (1.f + 0.35f * deltaLX);
				float baseScaleX = 1.f / (1.f + 0.05f * (nX - 1.f));
				float scaleX = (xSegm - (xSegm > 0.f ? 1.f : -1.f)) * (0.4f / (1.f + 0.05f * nX));

				thetaX = theta + scaleX * std::sin(theta * xSegm);
				rScaledX = r * baseScaleX * spiralScaleX;
			}

			// Rose / Ranunculus spiraling radius transformation for Y
			float nY = std::abs(ySegm);
			float rScaledY = r;
			float thetaY = theta;
			if (nY > 1.f) {
				float normTheta = (theta + (float)M_PI) / (2.f * (float)M_PI);
				float phaseY = std::fmod(normTheta * nY, nY);
				if (phaseY < 0.f) phaseY += nY;
				int kY = (int)std::floor(phaseY);

				int lY = kY / 4;
				int topLayerY = (int)std::floor((nY - 1.f) / 4.f);
				int deltaLY = std::max(0, topLayerY - lY);

				float spiralScaleY = 1.f / (1.f + 0.35f * deltaLY);
				float baseScaleY = 1.f / (1.f + 0.05f * (nY - 1.f));
				float scaleY = (ySegm - (ySegm > 0.f ? 1.f : -1.f)) * (0.4f / (1.f + 0.05f * nY));

				thetaY = theta + scaleY * std::sin(theta * ySegm);
				rScaledY = r * baseScaleY * spiralScaleY;
			}

			float outX = rScaledX * std::cos(thetaX);
			float outY = rScaledY * std::sin(thetaY);

			// Laser safety voltage bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.f, 12.f), c);
		}
	}
};

struct PetalsWidget : ModuleWidget {
	PetalsWidget(Petals* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Petals.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: X SEGM (7.62 mm) & Y SEGM (22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Petals::X_SEGM_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Petals::Y_SEGM_PARAM));

		// Attenuverter Trimpots
		// Trimpot Row 1: X SEGM CV (7.62 mm), Y SEGM CV (22.86 mm) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Petals::X_SEGM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Petals::Y_SEGM_TRIM_PARAM));

		// Bottom I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Petals::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Petals::Y_INPUT));

		// Row 2: X SEGM CV (X = 7.62 mm), Y SEGM CV (X = 22.86 mm) (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Petals::X_SEGM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Petals::Y_SEGM_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Petals::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Petals::Y_OUTPUT));
	}
};

Model* modelPetals = createModel<Petals, PetalsWidget>("Petals");
