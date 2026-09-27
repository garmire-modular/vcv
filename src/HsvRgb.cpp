#include "plugin.hpp"
#include "dsp/ColorEngine.hpp"

// ─────────────────────────────────────────────────────────────────────
//  HSV / HSL to RGB — Decoupled Color Space Converter
// ─────────────────────────────────────────────────────────────────────

struct HsvRgb : Module {
	enum ParamId {
		HUE_PARAM,
		SAT_PARAM,
		VAL_PARAM,
		MODE_PARAM,        // 0 = HSV, 1 = HSL
		HUE_TRIM_PARAM,
		SAT_TRIM_PARAM,
		VAL_TRIM_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		HUE_INPUT,
		SAT_INPUT,
		VAL_INPUT,
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

	HsvRgb() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Main Knobs
		configParam(HUE_PARAM, 0.f, 1.f, 0.f, "Hue", "\xc2\xb0", 0.f, 360.f);
		configParam(SAT_PARAM, 0.f, 1.f, 1.f, "Saturation", "%", 0.f, 100.f);
		configParam(VAL_PARAM, 0.f, 1.f, 1.f, "Value / Lightness", "%", 0.f, 100.f);

		// Mode Switch (0 = HSV, 1 = HSL)
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Color Model", {"HSV", "HSL"});

		// Attenuverters (-1 to +1, default 0.0)
		configParam(HUE_TRIM_PARAM, -1.f, 1.f, 0.f, "Hue CV Depth", "%", 0.f, 100.f);
		configParam(SAT_TRIM_PARAM, -1.f, 1.f, 0.f, "Saturation CV Depth", "%", 0.f, 100.f);
		configParam(VAL_TRIM_PARAM, -1.f, 1.f, 0.f, "Value CV Depth", "%", 0.f, 100.f);

		// Inputs
		configInput(HUE_INPUT, "Hue CV");
		configInput(SAT_INPUT, "Saturation CV");
		configInput(VAL_INPUT, "Value / Lightness CV");

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
		int hueCh = inputs[HUE_INPUT].getChannels();
		int satCh = inputs[SAT_INPUT].getChannels();
		int valCh = inputs[VAL_INPUT].getChannels();

		int channels = std::max({hueCh, satCh, valCh, 1});

		outputs[R_OUTPUT].setChannels(channels);
		outputs[G_OUTPUT].setChannels(channels);
		outputs[B_OUTPUT].setChannels(channels);

		bool isHsl = params[MODE_PARAM].getValue() >= 0.5f;

		for (int c = 0; c < channels; c++) {
			float hNorm = getModParam(HUE_PARAM, HUE_TRIM_PARAM, HUE_INPUT, c);
			float sNorm = clamp(getModParam(SAT_PARAM, SAT_TRIM_PARAM, SAT_INPUT, c), 0.f, 1.f);
			float vNorm = clamp(getModParam(VAL_PARAM, VAL_TRIM_PARAM, VAL_INPUT, c), 0.f, 1.f);

			float hDeg = hNorm * 360.f;

			float r = 0.f, g = 0.f, b = 0.f;

			if (!isHsl) {
				garmire::color::hsvToRgb(hDeg, sNorm, vNorm, r, g, b);
			} else {
				garmire::color::hslToRgb(hDeg, sNorm, vNorm, r, g, b);
			}

			// Outputs mapped to 0-5V linear RGB voltages
			outputs[R_OUTPUT].setVoltage(clamp(r, 0.f, 1.f) * 5.0f, c);
			outputs[G_OUTPUT].setVoltage(clamp(g, 0.f, 1.f) * 5.0f, c);
			outputs[B_OUTPUT].setVoltage(clamp(b, 0.f, 1.f) * 5.0f, c);
		}
	}
};

struct HsvRgbColorPreviewWidget : Widget {
	HsvRgb* module;

	HsvRgbColorPreviewWidget() : module(nullptr) {}

	void draw(const DrawArgs& args) override {
		float r = 1.0f, g = 0.0f, b = 0.0f;
		if (module) {
			r = clamp(module->outputs[HsvRgb::R_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			g = clamp(module->outputs[HsvRgb::G_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
			b = clamp(module->outputs[HsvRgb::B_OUTPUT].getVoltage() / 5.0f, 0.0f, 1.0f);
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

struct HsvRgbWidget : ModuleWidget {
	HsvRgbWidget(HsvRgb* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/HsvRgb.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Knobs (Center X = 15.24mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 21.59)), module, HsvRgb::HUE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 40.00)), module, HsvRgb::SAT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 58.41)), module, HsvRgb::VAL_PARAM));

		// Small & Offset Mode Switch (X = 24.48mm, Y = 58.41mm, next to VAL knob)
		addParam(createParamCentered<CKSS>(mm2px(Vec(24.48, 58.41)), module, HsvRgb::MODE_PARAM));

		// Live Color Preview Rounded Rectangle (25.40mm x 3.81mm [0.15in] at center Y = 71.58mm)
		HsvRgbColorPreviewWidget* preview = createWidget<HsvRgbColorPreviewWidget>(mm2px(Vec(2.54, 69.675)));
		preview->box.size = mm2px(Vec(25.40, 3.81));
		preview->module = module;
		addChild(preview);

		// Trimpots (Row Y = 83.00mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.00, 83.00)), module, HsvRgb::HUE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(15.24, 83.00)), module, HsvRgb::SAT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(24.48, 83.00)), module, HsvRgb::VAL_TRIM_PARAM));

		// CV Inputs (Middle Jack Row Y = 105.41mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.00, 105.41)), module, HsvRgb::HUE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 105.41)), module, HsvRgb::SAT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24.48, 105.41)), module, HsvRgb::VAL_INPUT));

		// Outputs (Bottom Jack Row Y = 116.84mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.00, 116.84)), module, HsvRgb::R_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 116.84)), module, HsvRgb::G_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(24.48, 116.84)), module, HsvRgb::B_OUTPUT));
	}
};

Model* modelHsvRgb = createModel<HsvRgb, HsvRgbWidget>("HsvRgb");
