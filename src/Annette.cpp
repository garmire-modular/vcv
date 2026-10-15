#include "plugin.hpp"
#include "FiguresEngine.hpp"
#include <cmath>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Annette — 16-Figure Acousmatic Trajectory Generator (12 HP)
//  Generate class module based on Annette Vande Gorne's Figures d'espace.
//  Controls:
//    Row 1: Coarse Freq, 3-state Range (Very Slow / LFO / VCO), Fine Freq
//    Row 2: Figure (1 to 16), Param 1 (P1), Param 2 (P2)
//  Attenuverters & Jacks:
//    Trimpot Row 1: Freq CV, FM CV
//    Trimpot Row 2: Figure CV, Param 1 CV, Param 2 CV
//    Jack Row 1 (Inputs): Freq CV, FM CV, Sync In
//    Jack Row 2 (Inputs): Figure CV, Param 1 CV, Param 2 CV
//    Jack Row 3 (Outputs): X, Y, Sync Out
// ─────────────────────────────────────────────────────────────────────

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Annette : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum ParamId {
		FREQ_PARAM,
		FINE_PARAM,
		RANGE_PARAM,

		FIGURE_PARAM,
		P1_PARAM,
		P2_PARAM,

		// CV Attenuverters (Zone 3)
		FREQ_TRIM_PARAM,
		FM_TRIM_PARAM,

		FIGURE_TRIM_PARAM,
		P1_TRIM_PARAM,
		P2_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Jack Row 1 (Inputs)
		FREQ_CV_INPUT,
		FM_CV_INPUT,
		SYNC_INPUT,

		// Jack Row 2 (Inputs)
		FIGURE_CV_INPUT,
		P1_CV_INPUT,
		P2_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Jack Row 3 (Outputs)
		X_OUTPUT,
		Y_OUTPUT,
		SYNC_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_ORANGE,
		RANGE_LIGHT_PURPLE,

		LIGHTS_LEN
	};

	RangeMode rangeMode = RANGE_LFO;
	dsp::BooleanTrigger rangeTrigger;
	figures::FiguresEngine figuresEngine;

	struct VoiceState {
		float basePhase = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
		float p1Smooth = 0.f;
		float p2Smooth = 0.f;
		bool initialized = false;
	};

	VoiceState voices[16];

	int getActiveMode() {
		float v = params[FIGURE_PARAM].getValue();
		int m = (int)std::round(v);
		return figures::clampVal(m, 0, (int)figures::NUM_MODES - 1);
	}

	float calculateBaseFrequency(float coarse, float fine) {
		if (rangeMode == RANGE_VERY_SLOW) {
			float periodCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float period = periodCoarse * (1.0f - fine * 0.10f);
			if (period < 0.001f) period = 0.001f;
			return 1.0f / period;
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + (200.0f - 0.01f) * (coarse * coarse);
			return clampf(fCoarse + fine * 20.0f, 0.001f, 250.0f);
		} else {
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
		}
	}

	struct FreqParamQuantity : ParamQuantity {
		Annette* getAnnette() {
			return dynamic_cast<Annette*>(this->module);
		}

		float getDisplayValue() override {
			Annette* annette = getAnnette();
			if (!annette) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (annette->paramQuantities.size() > Annette::FINE_PARAM) ? annette->params[Annette::FINE_PARAM].getValue() : 0.f;
			float freq = annette->calculateBaseFrequency(coarse, fine);

			if (annette->rangeMode == Annette::RANGE_VERY_SLOW) {
				return (freq > 1e-6f) ? (1.0f / freq) : 600.0f;
			}
			return freq;
		}

		std::string getUnit() override {
			Annette* annette = getAnnette();
			if (!annette) return " Hz";
			return (annette->rangeMode == Annette::RANGE_VERY_SLOW) ? " s" : " Hz";
		}

		std::string getDisplayValueString() override {
			Annette* annette = getAnnette();
			if (!annette) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (annette->rangeMode == Annette::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				if (val < 10.0f) {
					std::snprintf(buf, sizeof(buf), "%.3f", val);
				} else if (val < 100.0f) {
					std::snprintf(buf, sizeof(buf), "%.2f", val);
				} else {
					std::snprintf(buf, sizeof(buf), "%.1f", val);
				}
			}
			return std::string(buf);
		}
	};

	struct FigureParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int idx = figures::clampVal((int)std::round(getValue()), 0, (int)figures::NUM_MODES - 1);
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%d: %s", idx + 1, figures::METADATA[idx].name);
			return buf;
		}
	};

	struct FigureP1ParamQuantity : ParamQuantity {
		Annette* getAnnette() {
			return dynamic_cast<Annette*>(this->module);
		}

		std::string getLabel() override {
			Annette* m = getAnnette();
			if (!m) return "Param 1";
			int mode = m->getActiveMode();
			return figures::METADATA[mode].p1Name;
		}

		std::string getUnit() override {
			Annette* m = getAnnette();
			if (!m) return "";
			int mode = m->getActiveMode();
			return figures::METADATA[mode].p1Unit;
		}

		float getDisplayValue() override {
			Annette* m = getAnnette();
			float norm = getValue();
			if (!m) return norm;
			int mode = m->getActiveMode();
			const auto& meta = figures::METADATA[mode];
			return meta.p1Min + norm * (meta.p1Max - meta.p1Min);
		}

		std::string getDisplayValueString() override {
			float v = getDisplayValue();
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", v);
			return buf;
		}
	};

	struct FigureP2ParamQuantity : ParamQuantity {
		Annette* getAnnette() {
			return dynamic_cast<Annette*>(this->module);
		}

		std::string getLabel() override {
			Annette* m = getAnnette();
			if (!m) return "Param 2";
			int mode = m->getActiveMode();
			if (mode == figures::MODE_COMBINEE) {
				switch (m->figuresEngine.combineeVariant) {
					case figures::COMBINEE_DEPTH: return "Epicycle depth";
					case figures::COMBINEE_DIRECTION: return "Retrograde cusp direction";
					case figures::COMBINEE_PHASE_SHEAR: return "Phase shear";
				}
			}
			return figures::METADATA[mode].p2Name;
		}

		std::string getUnit() override {
			Annette* m = getAnnette();
			if (!m) return "";
			int mode = m->getActiveMode();
			if (mode == figures::MODE_COMBINEE && m->figuresEngine.combineeVariant == figures::COMBINEE_PHASE_SHEAR) {
				return " rad";
			}
			return figures::METADATA[mode].p2Unit;
		}

		float getDisplayValue() override {
			Annette* m = getAnnette();
			float norm = getValue();
			if (!m) return norm;
			int mode = m->getActiveMode();
			const auto& meta = figures::METADATA[mode];
			return meta.p2Min + norm * (meta.p2Max - meta.p2Min);
		}

		std::string getDisplayValueString() override {
			float v = getDisplayValue();
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", v);
			return buf;
		}
	};

	Annette() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: FREQ, RANGE, FINE
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency", " Hz");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configButton(RANGE_PARAM, "Range time-scale");

		// Row 2: FIGURE, PARAM 1, PARAM 2
		configParam<FigureParamQuantity>(FIGURE_PARAM, 0.f, 15.f, 0.f, "Figure");
		configParam<FigureP1ParamQuantity>(P1_PARAM, 0.f, 1.f, 0.4118f, "Param 1");
		configParam<FigureP2ParamQuantity>(P2_PARAM, 0.f, 1.f, 0.2857f, "Param 2");

		// Zone 3: CV Attenuverters (Mandatory naming per AGENTS.md Section 7.5.4)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 1.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "External FM CV depth", "%", 0.f, 100.f);

		configParam(FIGURE_TRIM_PARAM, -1.f, 1.f, 0.f, "Figure CV depth", "%", 0.f, 100.f);
		configParam(P1_TRIM_PARAM, -1.f, 1.f, 0.f, "Param 1 CV depth", "%", 0.f, 100.f);
		configParam(P2_TRIM_PARAM, -1.f, 1.f, 0.f, "Param 2 CV depth", "%", 0.f, 100.f);

		// Zone 4: Inputs (Mandatory sentence case, no redundant "input" per AGENTS.md Section 7.5.1 & 7.5.5)
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(FM_CV_INPUT, "External FM");
		configInput(SYNC_INPUT, "Sync");

		configInput(FIGURE_CV_INPUT, "Figure CV");
		configInput(P1_CV_INPUT, "Param 1 CV");
		configInput(P2_CV_INPUT, "Param 2 CV");

		// Zone 4: Outputs (Mandatory sentence case, no redundant "output")
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(SYNC_OUTPUT, "Sync");
	}

	void process(const ProcessArgs& args) override {
		// Handle 3-state Range Button Trigger
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		// Update 3-State LED Ring Indicator
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		int channels = std::max(1, inputs[FREQ_CV_INPUT].getChannels());
		if (inputs[FM_CV_INPUT].isConnected()) channels = std::max(channels, inputs[FM_CV_INPUT].getChannels());
		if (inputs[SYNC_INPUT].isConnected()) channels = std::max(channels, inputs[SYNC_INPUT].getChannels());
		if (inputs[FIGURE_CV_INPUT].isConnected()) channels = std::max(channels, inputs[FIGURE_CV_INPUT].getChannels());
		if (inputs[P1_CV_INPUT].isConnected()) channels = std::max(channels, inputs[P1_CV_INPUT].getChannels());
		if (inputs[P2_CV_INPUT].isConnected()) channels = std::max(channels, inputs[P2_CV_INPUT].getChannels());
		channels = std::min(channels, 16);

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
		outputs[SYNC_OUTPUT].setChannels(channels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam = params[FINE_PARAM].getValue();
		float defaultF0 = calculateBaseFrequency(coarseParam, fineParam);

		float fTrim = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim = params[FM_TRIM_PARAM].getValue();
		bool fmConnected = inputs[FM_CV_INPUT].isConnected();
		bool syncConnected = inputs[SYNC_INPUT].isConnected();

		float figParam = params[FIGURE_PARAM].getValue();
		float figTrim = params[FIGURE_TRIM_PARAM].getValue();

		float p1Param = params[P1_PARAM].getValue();
		float p1Trim = params[P1_TRIM_PARAM].getValue();

		float p2Param = params[P2_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();

		// Anti-zipper smoothing filter coefficient (~5ms tau)
		float lambda = 1.0f - std::exp(-args.sampleTime / 0.005f);

		for (int c = 0; c < channels; ++c) {
			VoiceState& vs = voices[c];

			bool syncTriggered = false;
			if (syncConnected) {
				vs.timeSinceSync += args.sampleTime;
				if (vs.syncTrigger.process(inputs[SYNC_INPUT].getPolyVoltage(c), 0.1f, 2.0f)) {
					if (vs.timeSinceSync > 0.0005f) {
						vs.syncPeriod = vs.timeSinceSync;
					}
					vs.timeSinceSync = 0.f;
					vs.basePhase = 0.f;
					syncTriggered = true;
					vs.syncPulse.trigger(1e-4f);
				}
			} else {
				vs.syncPeriod = 0.f;
			}

			float f0 = (syncConnected && vs.syncPeriod > 0.f) ? (1.f / vs.syncPeriod) : defaultF0;

			// Frequency modulation (1V/Oct)
			float freqCv = inputs[FREQ_CV_INPUT].getPolyVoltage(c);
			float fCarrier = f0 * std::pow(2.f, freqCv * fTrim);

			// Linear FM modulation
			float fActual;
			if (!fmConnected) {
				float carrierSelfMod = std::sin(2.f * (float)M_PI * vs.basePhase);
				float deltaF = fCarrier * carrierSelfMod * fmTrim;
				fActual = std::max(0.0001f, fCarrier + deltaF);
			} else {
				float extFmCv = inputs[FM_CV_INPUT].getPolyVoltage(c);
				float scaleHz = (rangeMode == RANGE_VERY_SLOW) ? (fCarrier * 2.f) : (rangeMode == RANGE_LFO ? 100.f : 500.f);
				float deltaF = (extFmCv / 5.f) * fmTrim * scaleHz;
				fActual = std::max(0.0001f, fCarrier + deltaF);
			}

			// Advance base phase
			vs.basePhase += fActual * args.sampleTime;

			if (!syncTriggered && vs.basePhase >= 1.f) {
				vs.syncPulse.trigger(1e-4f);
			}

			vs.basePhase -= std::floor(vs.basePhase);

			// Figure mode calculation
			float figCv = inputs[FIGURE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float currentFigVal = clampf(figParam + figCv * figTrim * 15.f, 0.f, 15.f);
			int currentMode = (int)std::round(currentFigVal);
			currentMode = figures::clampVal(currentMode, 0, (int)figures::NUM_MODES - 1);

			// Param 1 and Param 2 CV modulation
			float p1Cv = inputs[P1_CV_INPUT].getPolyVoltage(c) / 5.f;
			float p2Cv = inputs[P2_CV_INPUT].getPolyVoltage(c) / 5.f;

			float p1Norm = clampf(p1Param + p1Cv * p1Trim, 0.f, 1.f);
			float p2Norm = clampf(p2Param + p2Cv * p2Trim, 0.f, 1.f);

			const auto& meta = figures::METADATA[currentMode];
			float p1Target = meta.p1Min + p1Norm * (meta.p1Max - meta.p1Min);
			float p2Target = meta.p2Min + p2Norm * (meta.p2Max - meta.p2Min);

			if (!vs.initialized) {
				vs.p1Smooth = p1Target;
				vs.p2Smooth = p2Target;
				vs.initialized = true;
			} else {
				vs.p1Smooth += lambda * (p1Target - vs.p1Smooth);
				vs.p2Smooth += lambda * (p2Target - vs.p2Smooth);
			}

			// Analytical coordinate generation
			auto coords = figuresEngine.compute(currentMode, vs.basePhase, vs.p1Smooth, vs.p2Smooth);

			// Output scaling: Eurorack 10Vpp (±5V)
			outputs[X_OUTPUT].setVoltage(clampf(coords.first * 5.f, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(coords.second * 5.f, -12.f, 12.f), c);
			outputs[SYNC_OUTPUT].setVoltage(vs.syncPulse.process(args.sampleTime) ? 10.f : 0.f, c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "combineeVariant", json_integer((int)figuresEngine.combineeVariant));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) {
			rangeMode = (RangeMode)json_integer_value(rmJ);
		}
		json_t* cvJ = json_object_get(rootJ, "combineeVariant");
		if (cvJ) {
			figuresEngine.combineeVariant = (figures::CombineeVariant)json_integer_value(cvJ);
		}
	}
};

// ── Custom Hardware Tactile Button Widget ──
struct AnnetteCustomButton : app::SvgSwitch {
	AnnetteCustomButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Custom Illuminated Ring Widget (Concentric Ring around Tactile Button) ──
struct AnnetteRangeLightRing : ModuleLightWidget {
	AnnetteRangeLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));

		// Base colors for 3-Color Range Engine
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff)); // State 0: Warm Gold (Very Slow)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // State 1: Teal (LFO)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff)); // State 2: Magenta (VCO)
	}

	void drawBackground(const DrawArgs& args) override {
		Vec c = box.size.div(2.f);
		float r = mm2px(4.12f);
		float w = mm2px(1.40f);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, nvgRGBA(0x22, 0x22, 0x22, 0x99));
		nvgStrokeWidth(args.vg, w);
		nvgStroke(args.vg);
	}

	void drawLight(const DrawArgs& args) override {
		if (color.a <= 0.0f) return;
		Vec c = box.size.div(2.f);
		float r = mm2px(4.12f);
		float w = mm2px(1.40f);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, color);
		nvgStrokeWidth(args.vg, w);
		nvgStroke(args.vg);
	}

	void drawHalo(const DrawArgs& args) override {
		if (color.a <= 0.0f) return;
		Vec c = box.size.div(2.f);
		float r = mm2px(4.12f);
		float haloW = mm2px(1.00f);
		NVGcolor haloColor = color;
		haloColor.a *= 0.175f;

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, haloColor);
		nvgStrokeWidth(args.vg, mm2px(1.40f) + haloW);
		nvgStroke(args.vg);
	}
};

struct AnnetteWidget : ModuleWidget {
	AnnetteWidget(Annette* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Annette.svg")));

		// Columns across 12 HP: [12.00, 30.48, 48.96] mm
		const float col_x[3] = { 12.00f, 30.48f, 48.96f };

		// Row 1: FREQ & FINE Knobs, RANGE Button in middle (Center Y = 20.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 20.00f)), module, Annette::FREQ_PARAM));
		addChild(createLightCentered<AnnetteRangeLightRing>(mm2px(Vec(col_x[1], 20.00f)), module, Annette::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<AnnetteCustomButton>(mm2px(Vec(col_x[1], 20.00f)), module, Annette::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 20.00f)), module, Annette::FINE_PARAM));

		// Row 2: FIGURE, PARAM 1, PARAM 2 Knobs (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 37.00f)), module, Annette::FIGURE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 37.00f)), module, Annette::P1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 37.00f)), module, Annette::P2_PARAM));

		// Zone 3: CV Attenuverter Trimpots
		// Row 1: FREQ, FM (Center Y = 66.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 66.00f)), module, Annette::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 66.00f)), module, Annette::FM_TRIM_PARAM));

		// Row 2: FIGURE, PARAM 1, PARAM 2 (Center Y = 77.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 77.00f)), module, Annette::FIGURE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 77.00f)), module, Annette::P1_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 77.00f)), module, Annette::P2_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs): FREQ, FM, SYNC (Center Y = 93.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 93.00f)), module, Annette::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 93.00f)), module, Annette::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 93.00f)), module, Annette::SYNC_INPUT));

		// Row 2 (Inputs): FIGURE, PARAM 1, PARAM 2 (Center Y = 106.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 106.50f)), module, Annette::FIGURE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 106.50f)), module, Annette::P1_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 106.50f)), module, Annette::P2_CV_INPUT));

		// Row 3 (Outputs): X, Y, SYNC (Center Y = 120.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 120.00f)), module, Annette::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 120.00f)), module, Annette::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 120.00f)), module, Annette::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Annette* module = dynamic_cast<Annette*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator);

		// Frequency Range sub-menu
		MenuLabel* rangeLabel = new MenuLabel();
		rangeLabel->text = "Frequency Range";
		menu->addChild(rangeLabel);

		static const char* rangeNames[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 Hz - 200 Hz)",
			"VCO (150 Hz - 2000 Hz)"
		};

		for (int r = 0; r < 3; ++r) {
			menu->addChild(createCheckMenuItem(rangeNames[r], "",
				[module, r]() { return module->rangeMode == r; },
				[module, r]() { module->rangeMode = (Annette::RangeMode)r; }
			));
		}

		menu->addChild(new MenuSeparator);

		// Trajectory Combinée Variant sub-menu
		MenuLabel* combLabel = new MenuLabel();
		combLabel->text = "Trajectoire Combinée (Figure 15) Variant";
		menu->addChild(combLabel);

		menu->addChild(createCheckMenuItem("Sub-mode A: Epicycle Depth", "",
			[module]() { return module->figuresEngine.combineeVariant == figures::COMBINEE_DEPTH; },
			[module]() { module->figuresEngine.combineeVariant = figures::COMBINEE_DEPTH; }
		));
		menu->addChild(createCheckMenuItem("Sub-mode B: Retrograde Cusp Direction", "",
			[module]() { return module->figuresEngine.combineeVariant == figures::COMBINEE_DIRECTION; },
			[module]() { module->figuresEngine.combineeVariant = figures::COMBINEE_DIRECTION; }
		));
		menu->addChild(createCheckMenuItem("Sub-mode C: Phase Shear", "",
			[module]() { return module->figuresEngine.combineeVariant == figures::COMBINEE_PHASE_SHEAR; },
			[module]() { module->figuresEngine.combineeVariant = figures::COMBINEE_PHASE_SHEAR; }
		));
	}
};

Model* modelAnnette = createModel<Annette, AnnetteWidget>("Annette");
