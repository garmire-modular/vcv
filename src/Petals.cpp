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

			// Square boundary compensation to base inradius
			float c4 = std::max(std::abs(std::cos(theta)), std::abs(std::sin(theta)));
			float rBase = r * c4;

			auto transformChannel = [&](float segm, float& outR, float& outTheta) {
				float n = std::abs(segm);
				if (n <= 1.f) {
					outR = r;
					outTheta = theta;
					return;
				}
				float dirVal = (segm >= 0.f) ? 1.f : -1.f;

				auto getIntegerPetal = [&](int count, float& pR, float& pTheta) {
					if (count <= 1) {
						pR = r;
						pTheta = theta;
						return;
					}
					float theta0 = (float)M_PI / (2.f * (float)count);
					float targetBulge = std::sqrt(2.f) - 1.f;
					if (count == 2) {
						float normCn = targetBulge * std::max(0.f, std::cos(2.f * (theta - theta0)));
						float centerScale = 1.f / (1.f + 0.04f * (2.f - 1.f));
						pR = rBase * (centerScale + normCn);
					} else {
						float dTh = 2.f * (float)M_PI / (float)count;
						float phi = theta - theta0;
						float localTh = std::fmod(phi + (float)M_PI / (float)count, dTh);
						if (localTh < 0.f) localTh += dTh;
						localTh -= (float)M_PI / (float)count;

						float cn = std::cos(localTh);
						float actualBulge = (1.f / std::cos((float)M_PI / (float)count)) - 1.f;
						float bulgeScale = targetBulge / std::max(0.01f, actualBulge);
						float normCn = (1.f / std::max(0.4f, cn) - 1.f) * bulgeScale;
						float centerScale = 1.f / (1.f + 0.04f * ((float)count - 1.f));
						pR = rBase * (centerScale + normCn);
					}
					float phi = theta - theta0;
					pTheta = theta + dirVal * std::sin((float)count * phi);
				};

				int k = std::min(15, (int)std::floor(n));
				int kNext = std::min(16, k + 1);
				float f = clamp(n - (float)k, 0.f, 1.f);
				float ease = f * f * (3.f - 2.f * f);

				float r1 = r, th1 = theta;
				float r2 = r, th2 = theta;
				getIntegerPetal(k, r1, th1);
				getIntegerPetal(kNext, r2, th2);

				outR = (1.f - ease) * r1 + ease * r2;
				outTheta = (1.f - ease) * th1 + ease * th2;
			};

			float rX = r, thetaX = theta;
			float rY = r, thetaY = theta;
			transformChannel(xSegm, rX, thetaX);
			transformChannel(ySegm, rY, thetaY);

			float outX = rX * std::cos(thetaX);
			float outY = rY * std::sin(thetaY);

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
