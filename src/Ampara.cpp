#include "plugin.hpp"
#include "core/AmparaCore.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Ampara — Audio-Reactive Vector Polygon Synthesizer (16 HP)
//  Generate class module producing animated geometric regular polygons
//  (Circle to Octagon) wrapped with real-time stereo audio waveforms and
//  shaped by dual-channel envelope followers, Milkdrop polar warp,
//  and stereo morphing.
// ─────────────────────────────────────────────────────────────────────

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Ampara : Module {
	using RangeMode = garmire::ampara::RangeMode;

	enum ParamId {
		// Row 1 Knobs: FREQ, FINE, SHAPE, WARP, WINDOW
		FREQ_PARAM,
		FINE_PARAM,
		SHAPE_PARAM,
		WARP_PARAM,
		WINDOW_PARAM,

		// Row 2 Knobs: DISP, RISE, FALL, MORPH, PHASE
		DISP_PARAM,
		RISE_PARAM,
		FALL_PARAM,
		MORPH_PARAM,
		PHASE_PARAM,

		// Range button
		RANGE_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 1
		FREQ_TRIM_PARAM,
		SHAPE_TRIM_PARAM,
		WARP_TRIM_PARAM,
		WINDOW_TRIM_PARAM,
		DISP_TRIM_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 2
		RISE_TRIM_PARAM,
		FALL_TRIM_PARAM,
		MORPH_TRIM_PARAM,
		PHASE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Jack Row 1 (Inputs)
		FREQ_CV_INPUT,
		SHAPE_CV_INPUT,
		WARP_CV_INPUT,
		WINDOW_CV_INPUT,

		// Jack Row 2 (Inputs)
		DISP_CV_INPUT,
		RISE_CV_INPUT,
		FALL_CV_INPUT,
		MORPH_CV_INPUT,

		// Jack Row 3 (Inputs & Audio)
		PHASE_CV_INPUT,
		SYNC_INPUT,
		L_INPUT,
		R_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Jack Row 4 (Outputs & Sync Out)
		SYNC_OUTPUT,
		X_OUTPUT,
		Y_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_TEAL,
		RANGE_LIGHT_MAGENTA,

		LIGHTS_LEN
	};

	RangeMode rangeMode = garmire::ampara::RANGE_LFO;
	dsp::BooleanTrigger rangeTrigger;

	// Real-time safe 16-voice polyphonic core DSP engines
	garmire::ampara::VoiceCore voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Ampara* getAmpara() {
			return dynamic_cast<Ampara*>(module);
		}

		float getDisplayValue() override {
			Ampara* ampara = getAmpara();
			if (!ampara) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (ampara->paramQuantities.size() > Ampara::FINE_PARAM) ? ampara->params[Ampara::FINE_PARAM].getValue() : 0.f;

			if (ampara->rangeMode == garmire::ampara::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (ampara->rangeMode == garmire::ampara::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 50.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Ampara* ampara = getAmpara();
			if (!ampara) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (ampara->rangeMode == garmire::ampara::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		std::string getUnit() override {
			Ampara* ampara = getAmpara();
			if (!ampara) return "";
			return (ampara->rangeMode == garmire::ampara::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	struct ShapeParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float val = getValue();
			char buf[64];
			if (val < 2.5f) {
				std::snprintf(buf, sizeof(buf), "Circle");
			} else if (val < 3.5f) {
				std::snprintf(buf, sizeof(buf), "Triangle (%.1f)", val);
			} else if (val < 4.5f) {
				std::snprintf(buf, sizeof(buf), "Square (%.1f)", val);
			} else if (val < 5.5f) {
				std::snprintf(buf, sizeof(buf), "Pentagon (%.1f)", val);
			} else if (val < 6.5f) {
				std::snprintf(buf, sizeof(buf), "Hexagon (%.1f)", val);
			} else if (val < 7.5f) {
				std::snprintf(buf, sizeof(buf), "Heptagon (%.1f)", val);
			} else {
				std::snprintf(buf, sizeof(buf), "Octagon (%.1f)", val);
			}
			return std::string(buf);
		}
	};

	struct WindowParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			return 0.2f * std::pow(250.0f, getValue());
		}
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", getDisplayValue());
			return std::string(buf);
		}
		std::string getUnit() override {
			return " ms";
		}
	};

	struct RiseParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			return 0.1f * std::pow(2000.0f, getValue());
		}
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", getDisplayValue());
			return std::string(buf);
		}
		std::string getUnit() override {
			return " ms";
		}
	};

	struct FallParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			return 2.0f * std::pow(1000.0f, getValue());
		}
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", getDisplayValue());
			return std::string(buf);
		}
		std::string getUnit() override {
			return " ms";
		}
	};

	struct PhaseParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			return getValue() * 360.0f;
		}
		std::string getDisplayValueString() override {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", getDisplayValue());
			return std::string(buf);
		}
		std::string getUnit() override {
			return "°";
		}
	};

	Ampara() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Knobs: Row 1
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency", " Hz");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine Tune", "%", 0.f, 100.f);
		configParam<ShapeParamQuantity>(SHAPE_PARAM, 2.f, 8.f, 2.f, "Shape", "");
		configParam(WARP_PARAM, 0.f, 1.f, 0.f, "Warp", "%", 0.f, 100.f);
		configParam<WindowParamQuantity>(WINDOW_PARAM, 0.f, 1.f, 0.5f, "Buffer Window", " ms");

		// Knobs: Row 2
		configParam(DISP_PARAM, 0.f, 2.f, 1.f, "Displacement", "%", 0.f, 100.f);
		configParam<RiseParamQuantity>(RISE_PARAM, 0.f, 1.f, 0.2f, "Rise Time", " ms");
		configParam<FallParamQuantity>(FALL_PARAM, 0.f, 1.f, 0.4f, "Fall Time", " ms");
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph Routing", "%", 0.f, 100.f);
		configParam<PhaseParamQuantity>(PHASE_PARAM, 0.f, 1.f, 0.f, "Phase", "°");

		// Range Button
		configButton(RANGE_PARAM, "Frequency Range");

		// Attenuverter Trimpots (Sentence case with CV uppercase and depth lowercase per AGENTS.md 6.5.4)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(SHAPE_TRIM_PARAM, -1.f, 1.f, 0.f, "Shape CV depth", "%", 0.f, 100.f);
		configParam(WARP_TRIM_PARAM, -1.f, 1.f, 0.f, "Warp CV depth", "%", 0.f, 100.f);
		configParam(WINDOW_TRIM_PARAM, -1.f, 1.f, 0.f, "Window CV depth", "%", 0.f, 100.f);
		configParam(DISP_TRIM_PARAM, -1.f, 1.f, 0.f, "Displacement CV depth", "%", 0.f, 100.f);

		configParam(RISE_TRIM_PARAM, -1.f, 1.f, 0.f, "Rise CV depth", "%", 0.f, 100.f);
		configParam(FALL_TRIM_PARAM, -1.f, 1.f, 0.f, "Fall CV depth", "%", 0.f, 100.f);
		configParam(MORPH_TRIM_PARAM, -1.f, 1.f, 0.f, "Morph CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(FREQ_CV_INPUT, "Frequency CV (1V/Oct)");
		configInput(SHAPE_CV_INPUT, "Shape CV");
		configInput(WARP_CV_INPUT, "Warp CV");
		configInput(WINDOW_CV_INPUT, "Window CV");

		configInput(DISP_CV_INPUT, "Displacement CV");
		configInput(RISE_CV_INPUT, "Rise CV");
		configInput(FALL_CV_INPUT, "Fall CV");
		configInput(MORPH_CV_INPUT, "Morph CV");

		configInput(PHASE_CV_INPUT, "Phase CV");
		configInput(SYNC_INPUT, "Sync In");
		configInput(L_INPUT, "Audio In L");
		configInput(R_INPUT, "Audio In R (Normalled to L)");

		// Outputs
		configOutput(SYNC_OUTPUT, "Sync Out");
		configOutput(X_OUTPUT, "X (±5V Deflection)");
		configOutput(Y_OUTPUT, "Y (±5V Deflection)");

		for (int c = 0; c < 16; ++c) {
			voices[c].reset();
		}
	}

	void onReset() override {
		rangeMode = garmire::ampara::RANGE_LFO;
		for (int c = 0; c < 16; ++c) {
			voices[c].reset();
		}
	}

	void process(const ProcessArgs& args) override {
		// Handle Range toggle button
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)(((int)rangeMode + 1) % 3);
		}

		// Update 3-Color Range LED
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == garmire::ampara::RANGE_VERY_SLOW ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_TEAL].setBrightness(rangeMode == garmire::ampara::RANGE_LFO ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_MAGENTA].setBrightness(rangeMode == garmire::ampara::RANGE_VCO ? 1.0f : 0.0f);

		// Determine polyphony
		int channels = 1;
		channels = std::max(channels, inputs[L_INPUT].getChannels());
		channels = std::max(channels, inputs[R_INPUT].getChannels());
		channels = std::max(channels, inputs[FREQ_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[SHAPE_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[WARP_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[WINDOW_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[DISP_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[RISE_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[FALL_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[MORPH_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[PHASE_CV_INPUT].getChannels());
		channels = std::max(channels, inputs[SYNC_INPUT].getChannels());
		channels = std::min(channels, 16);

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
		outputs[SYNC_OUTPUT].setChannels(channels);

		float coarseFreq = params[FREQ_PARAM].getValue();
		float fineFreq = params[FINE_PARAM].getValue();
		float shapeVal = params[SHAPE_PARAM].getValue();
		float warpVal = params[WARP_PARAM].getValue();
		float windowVal = params[WINDOW_PARAM].getValue();

		float dispVal = params[DISP_PARAM].getValue();
		float riseVal = params[RISE_PARAM].getValue();
		float fallVal = params[FALL_PARAM].getValue();
		float morphVal = params[MORPH_PARAM].getValue();
		float phaseVal = params[PHASE_PARAM].getValue();

		float freqTrim = params[FREQ_TRIM_PARAM].getValue();
		float shapeTrim = params[SHAPE_TRIM_PARAM].getValue();
		float warpTrim = params[WARP_TRIM_PARAM].getValue();
		float windowTrim = params[WINDOW_TRIM_PARAM].getValue();
		float dispTrim = params[DISP_TRIM_PARAM].getValue();

		float riseTrim = params[RISE_TRIM_PARAM].getValue();
		float fallTrim = params[FALL_TRIM_PARAM].getValue();
		float morphTrim = params[MORPH_TRIM_PARAM].getValue();
		float phaseTrim = params[PHASE_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float audioL = inputs[L_INPUT].getPolyVoltage(c);
			float audioR = inputs[R_INPUT].isConnected() ? inputs[R_INPUT].getPolyVoltage(c) : audioL;

			float freqCV = inputs[FREQ_CV_INPUT].getPolyVoltage(c) * freqTrim;
			float shapeCV = inputs[SHAPE_CV_INPUT].getPolyVoltage(c) * shapeTrim;
			float warpCV = inputs[WARP_CV_INPUT].getPolyVoltage(c) * warpTrim;
			float windowCV = inputs[WINDOW_CV_INPUT].getPolyVoltage(c) * windowTrim;
			float dispCV = inputs[DISP_CV_INPUT].getPolyVoltage(c) * dispTrim;

			float riseCV = inputs[RISE_CV_INPUT].getPolyVoltage(c) * riseTrim;
			float fallCV = inputs[FALL_CV_INPUT].getPolyVoltage(c) * fallTrim;
			float morphCV = inputs[MORPH_CV_INPUT].getPolyVoltage(c) * morphTrim;
			float phaseCV = inputs[PHASE_CV_INPUT].getPolyVoltage(c) * phaseTrim;

			float syncIn = inputs[SYNC_INPUT].getPolyVoltage(c);

			float outX = 0.0f;
			float outY = 0.0f;
			float syncOut = 0.0f;

			voices[c].process(
				args.sampleRate,
				coarseFreq, fineFreq, rangeMode, freqCV,
				shapeVal, shapeCV,
				warpVal, warpCV,
				windowVal, windowCV,
				dispVal, dispCV,
				riseVal, riseCV,
				fallVal, fallCV,
				morphVal, morphCV,
				phaseVal, phaseCV,
				audioL, audioR,
				syncIn,
				outX, outY, syncOut
			);

			outputs[X_OUTPUT].setVoltage(outX, c);
			outputs[Y_OUTPUT].setVoltage(outY, c);
			outputs[SYNC_OUTPUT].setVoltage(syncOut, c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) {
			rangeMode = (RangeMode)json_integer_value(rmJ);
		}
	}
};

// ── Custom 3-Color Range Light Widget (Palette: #e1be6a, #40b0a6, #d35fb7) ──
template <typename TBase = GrayModuleLightWidget>
struct TAmparaRangeLight : TBase {
	TAmparaRangeLight() {
		// Range 0: Very Slow = #e1be6a (Warm Gold)
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff));
		// Range 1: LFO = #40b0a6 (Teal)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff));
		// Range 2: VCO = #d35fb7 (Magenta)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff));
	}
};
struct AmparaRangeLightWidget : SmallLight<TAmparaRangeLight<>> {};

struct AmparaWidget : ModuleWidget {
	AmparaWidget(Ampara* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ampara.svg")));

		// 16 HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Knob Columns (5 columns across 81.28 mm)
		// col_x = [12.14, 26.39, 40.64, 54.89, 69.14]
		const float knob_cols[5] = {12.14f, 26.39f, 40.64f, 54.89f, 69.14f};

		// Row 1 Knobs (Center Y = 21.59 mm): FREQ, FINE, SHAPE, WARP, WINDOW
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[0], 21.59f)), module, Ampara::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[1], 21.59f)), module, Ampara::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[2], 21.59f)), module, Ampara::SHAPE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[3], 21.59f)), module, Ampara::WARP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[4], 21.59f)), module, Ampara::WINDOW_PARAM));

		// Range Button and LED centered between Col 1 and Col 2 (19.265 mm)
		const float btn_x = 19.265f;
		addChild(createLightCentered<AmparaRangeLightWidget>(mm2px(Vec(btn_x, 15.50f)), module, Ampara::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<TL1105>(mm2px(Vec(btn_x, 21.59f)), module, Ampara::RANGE_PARAM));

		// Row 2 Knobs (Center Y = 43.00 mm): DISP, RISE, FALL, MORPH, PHASE
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[0], 43.00f)), module, Ampara::DISP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[1], 43.00f)), module, Ampara::RISE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[2], 43.00f)), module, Ampara::FALL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[3], 43.00f)), module, Ampara::MORPH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(knob_cols[4], 43.00f)), module, Ampara::PHASE_PARAM));

		// Zone 3: Attenuverter Trimpots
		// Row 1 Trimpots (5 cols at Y = 63.50 mm): FREQ, SHAPE, WARP, WINDOW, DISP
		const float trim_row1_cols[5] = {12.14f, 26.39f, 40.64f, 54.89f, 69.14f};
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row1_cols[0], 63.50f)), module, Ampara::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row1_cols[1], 63.50f)), module, Ampara::SHAPE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row1_cols[2], 63.50f)), module, Ampara::WARP_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row1_cols[3], 63.50f)), module, Ampara::WINDOW_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row1_cols[4], 63.50f)), module, Ampara::DISP_TRIM_PARAM));

		// Row 2 Trimpots (4 cols at Y = 75.50 mm): RISE, FALL, MORPH, PHASE
		const float trim_row2_cols[4] = {19.265f, 33.515f, 47.765f, 62.015f};
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row2_cols[0], 75.50f)), module, Ampara::RISE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row2_cols[1], 75.50f)), module, Ampara::FALL_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row2_cols[2], 75.50f)), module, Ampara::MORPH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(trim_row2_cols[3], 75.50f)), module, Ampara::PHASE_TRIM_PARAM));

		// Zone 4: I/O Jacks (4 Columns: 13.09, 31.46, 49.82, 68.19 mm)
		const float jack_cols[4] = {13.09f, 31.46f, 49.82f, 68.19f};

		// Jack Row 1 (Inputs at Y = 89.50 mm): FREQ, SHAPE, WARP, WINDOW
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[0], 89.50f)), module, Ampara::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[1], 89.50f)), module, Ampara::SHAPE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[2], 89.50f)), module, Ampara::WARP_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[3], 89.50f)), module, Ampara::WINDOW_CV_INPUT));

		// Jack Row 2 (Inputs at Y = 99.00 mm): DISP, RISE, FALL, MORPH
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[0], 99.00f)), module, Ampara::DISP_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[1], 99.00f)), module, Ampara::RISE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[2], 99.00f)), module, Ampara::FALL_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[3], 99.00f)), module, Ampara::MORPH_CV_INPUT));

		// Jack Row 3 (Inputs & Audio at Y = 108.50 mm): PHASE, SYNC IN, IN L, IN R
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[0], 108.50f)), module, Ampara::PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[1], 108.50f)), module, Ampara::SYNC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[2], 108.50f)), module, Ampara::L_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack_cols[3], 108.50f)), module, Ampara::R_INPUT));

		// Jack Row 4 (Outputs at Fixed Y = 118.00 mm): SYNC OUT, (spare), OUT X, OUT Y
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack_cols[0], 118.00f)), module, Ampara::SYNC_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack_cols[2], 118.00f)), module, Ampara::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack_cols[3], 118.00f)), module, Ampara::Y_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Ampara* ampara = dynamic_cast<Ampara*>(module);
		if (!ampara) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};
		const char* rightTexts[] = {"Yellow", "Teal", "Magenta"};

		for (int i = 0; i < 3; i++) {
			Ampara::RangeMode mode = (Ampara::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], rightTexts[i],
				[=]() { return ampara->rangeMode == mode; },
				[=]() { ampara->rangeMode = mode; }
			));
		}
	}
};

Model* modelAmpara = createModel<Ampara, AmparaWidget>("Ampara");
