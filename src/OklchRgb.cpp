#include "plugin.hpp"
#include "dsp/ColorEngine.hpp"

// ─────────────────────────────────────────────────────────────────────
//  OKLCH to RGB — Decoupled Perceptual Color Engine
// ─────────────────────────────────────────────────────────────────────

struct OklchRgb : Module {
	enum ParamId {
		LIGHT_PARAM,
		CHROMA_PARAM,
		HUE_PARAM,
		LIGHT_TRIM_PARAM,
		CHROMA_TRIM_PARAM,
		HUE_TRIM_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		LIGHT_INPUT,
		CHROMA_INPUT,
		HUE_INPUT,
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

	OklchRgb() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Main Knobs
		configParam(LIGHT_PARAM, 0.f, 1.f, 0.8f, "Lightness", "%", 0.f, 100.f);
		configParam(CHROMA_PARAM, 0.f, 0.4f, 0.2f, "Chroma");
		configParam(HUE_PARAM, 0.f, 1.f, 0.f, "Hue", "\xc2\xb0", 0.f, 360.f);

		// Attenuverters (-1 to +1, default 0.0)
		configParam(LIGHT_TRIM_PARAM, -1.f, 1.f, 0.f, "Lightness CV Depth", "%", 0.f, 100.f);
		configParam(CHROMA_TRIM_PARAM, -1.f, 1.f, 0.f, "Chroma CV Depth", "%", 0.f, 100.f);
		configParam(HUE_TRIM_PARAM, -1.f, 1.f, 0.f, "Hue CV Depth", "%", 0.f, 100.f);

		// Inputs
		configInput(LIGHT_INPUT, "Lightness CV");
		configInput(CHROMA_INPUT, "Chroma CV");
		configInput(HUE_INPUT, "Hue CV");

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
		int lightCh  = inputs[LIGHT_INPUT].getChannels();
		int chromaCh = inputs[CHROMA_INPUT].getChannels();
		int hueCh    = inputs[HUE_INPUT].getChannels();

		int channels = std::max({lightCh, chromaCh, hueCh, 1});

		outputs[R_OUTPUT].setChannels(channels);
		outputs[G_OUTPUT].setChannels(channels);
		outputs[B_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			float L = clamp(getModParam(LIGHT_PARAM, LIGHT_TRIM_PARAM, LIGHT_INPUT, c), 0.f, 1.f);
			float C = clamp(getModParam(CHROMA_PARAM, CHROMA_TRIM_PARAM, CHROMA_INPUT, c), 0.f, 0.4f);
			float hNorm = getModParam(HUE_PARAM, HUE_TRIM_PARAM, HUE_INPUT, c);

			float hDeg = hNorm * 360.f;

			float r = 0.f, g = 0.f, b = 0.f;
			garmire::color::oklchToRgb(L, C, hDeg, r, g, b);

			outputs[R_OUTPUT].setVoltage(clamp(r, 0.f, 1.f) * 5.0f, c);
			outputs[G_OUTPUT].setVoltage(clamp(g, 0.f, 1.f) * 5.0f, c);
			outputs[B_OUTPUT].setVoltage(clamp(b, 0.f, 1.f) * 5.0f, c);
		}
	}
};

struct OklchColorPreviewWidget : Widget {
	OklchRgb* module;

	OklchColorPreviewWidget() : module(nullptr) {}

	void draw(const DrawArgs& args) override {
		float r = 1.0f, g = 0.48f, b = 0.52f;
		if (module) {
			r = clamp(module->outputs[OklchRgb::R_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			g = clamp(module->outputs[OklchRgb::G_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			b = clamp(module->outputs[OklchRgb::B_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
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

struct OklchRgbWidget : ModuleWidget {
	OklchRgbWidget(OklchRgb* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OklchRgb.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Knobs (X = 15.24mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 21.59)), module, OklchRgb::LIGHT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 40.00)), module, OklchRgb::CHROMA_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 58.41)), module, OklchRgb::HUE_PARAM));

		// Live Color Preview Rounded Rectangle (25.40mm x 3.81mm [0.15in] at center Y = 71.58mm)
		OklchColorPreviewWidget* preview = createWidget<OklchColorPreviewWidget>(mm2px(Vec(2.54, 69.675)));
		preview->box.size = mm2px(Vec(25.40, 3.81));
		preview->module = module;
		addChild(preview);

		// Trimpots (Row Y = 83.00mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.00, 83.00)), module, OklchRgb::LIGHT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(15.24, 83.00)), module, OklchRgb::CHROMA_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(24.48, 83.00)), module, OklchRgb::HUE_TRIM_PARAM));

		// CV Inputs (Middle Jack Row Y = 105.41mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.00, 105.41)), module, OklchRgb::LIGHT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 105.41)), module, OklchRgb::CHROMA_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24.48, 105.41)), module, OklchRgb::HUE_INPUT));

		// Outputs (Bottom Jack Row Y = 116.84mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.00, 116.84)), module, OklchRgb::R_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 116.84)), module, OklchRgb::G_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(24.48, 116.84)), module, OklchRgb::B_OUTPUT));
	}
};

Model* modelOklchRgb = createModel<OklchRgb, OklchRgbWidget>("OklchRgb");
