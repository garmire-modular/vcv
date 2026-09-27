#include "plugin.hpp"

// ─────────────────────────────────────────────────────────────────────
//  RGB — Direct Linear Color Generator with Real-Time Preview
// ─────────────────────────────────────────────────────────────────────

struct Rgb : Module {
	enum ParamId {
		RED_PARAM,
		GREEN_PARAM,
		BLUE_PARAM,
		RED_TRIM_PARAM,
		GREEN_TRIM_PARAM,
		BLUE_TRIM_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		RED_INPUT,
		GREEN_INPUT,
		BLUE_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	Rgb() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Main Knobs
		configParam(RED_PARAM, 0.f, 1.f, 1.f, "Red Voltage", "%", 0.f, 100.f);
		configParam(GREEN_PARAM, 0.f, 1.f, 0.f, "Green Voltage", "%", 0.f, 100.f);
		configParam(BLUE_PARAM, 0.f, 1.f, 0.f, "Blue Voltage", "%", 0.f, 100.f);

		// Attenuverters (-1 to +1, default 0.0)
		configParam(RED_TRIM_PARAM, -1.f, 1.f, 0.f, "Red CV Depth", "%", 0.f, 100.f);
		configParam(GREEN_TRIM_PARAM, -1.f, 1.f, 0.f, "Green CV Depth", "%", 0.f, 100.f);
		configParam(BLUE_TRIM_PARAM, -1.f, 1.f, 0.f, "Blue CV Depth", "%", 0.f, 100.f);

		// Inputs
		configInput(RED_INPUT, "Red CV");
		configInput(GREEN_INPUT, "Green CV");
		configInput(BLUE_INPUT, "Blue CV");

		// Outputs
		configOutput(R_OUTPUT, "Red Signal (0-5V)");
		configOutput(G_OUTPUT, "Green Signal (0-5V)");
		configOutput(B_OUTPUT, "Blue Signal (0-5V)");
	}

	float getModParam(int paramId, int attvId, int inputId, int channel) {
		float val = params[paramId].getValue();
		if (inputs[inputId].isConnected()) {
			val += (inputs[inputId].getPolyVoltage(channel) / 5.f) * params[attvId].getValue();
		}
		return val;
	}

	void process(const ProcessArgs& args) override {
		int rCh = inputs[RED_INPUT].getChannels();
		int gCh = inputs[GREEN_INPUT].getChannels();
		int bCh = inputs[BLUE_INPUT].getChannels();

		int channels = std::max({rCh, gCh, bCh, 1});

		outputs[R_OUTPUT].setChannels(channels);
		outputs[G_OUTPUT].setChannels(channels);
		outputs[B_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			float rNorm = clamp(getModParam(RED_PARAM, RED_TRIM_PARAM, RED_INPUT, c), 0.f, 1.f);
			float gNorm = clamp(getModParam(GREEN_PARAM, GREEN_TRIM_PARAM, GREEN_INPUT, c), 0.f, 1.f);
			float bNorm = clamp(getModParam(BLUE_PARAM, BLUE_TRIM_PARAM, BLUE_INPUT, c), 0.f, 1.f);

			outputs[R_OUTPUT].setVoltage(rNorm * 5.0f, c);
			outputs[G_OUTPUT].setVoltage(gNorm * 5.0f, c);
			outputs[B_OUTPUT].setVoltage(bNorm * 5.0f, c);
		}
	}
};

struct RgbColorPreviewWidget : Widget {
	Rgb* module;

	RgbColorPreviewWidget() : module(nullptr) {}

	void draw(const DrawArgs& args) override {
		float r = 1.0f, g = 0.0f, b = 0.0f;
		if (module) {
			r = clamp(module->outputs[Rgb::R_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			g = clamp(module->outputs[Rgb::G_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			b = clamp(module->outputs[Rgb::B_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
		}

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, mm2px(1.0f));
		nvgFillColor(args.vg, nvgRGBf(r, g, b));
		nvgFill(args.vg);

		nvgStrokeColor(args.vg, nvgRGB(0x2c, 0x2c, 0x2c));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);
	}
};

struct RgbWidget : ModuleWidget {
	RgbWidget(Rgb* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Rgb.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Knobs (Center X = 15.24mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 21.59)), module, Rgb::RED_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 40.00)), module, Rgb::GREEN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 58.41)), module, Rgb::BLUE_PARAM));

		// Live Color Preview Rounded Rectangle (25.40mm x 3.81mm [0.15in] at center Y = 71.58mm)
		RgbColorPreviewWidget* preview = createWidget<RgbColorPreviewWidget>(mm2px(Vec(2.54, 69.675)));
		preview->box.size = mm2px(Vec(25.40, 3.81));
		preview->module = module;
		addChild(preview);

		// Trimpots (Row Y = 83.00mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.00, 83.00)), module, Rgb::RED_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(15.24, 83.00)), module, Rgb::GREEN_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(24.48, 83.00)), module, Rgb::BLUE_TRIM_PARAM));

		// CV Inputs (Middle Jack Row Y = 105.41mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.00, 105.41)), module, Rgb::RED_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 105.41)), module, Rgb::GREEN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24.48, 105.41)), module, Rgb::BLUE_INPUT));

		// Outputs (Bottom Jack Row Y = 116.84mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.00, 116.84)), module, Rgb::R_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 116.84)), module, Rgb::G_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(24.48, 116.84)), module, Rgb::B_OUTPUT));
	}
};

Model* modelRgb = createModel<Rgb, RgbWidget>("Rgb");
