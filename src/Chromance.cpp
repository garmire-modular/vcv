#include "plugin.hpp"
#include "dsp/Oscillator.hpp"

// ─────────────────────────────────────────────────────────────────────
//  Chromance — Decoupled Single-Strip Laser Oscillator
// ─────────────────────────────────────────────────────────────────────

struct WaveformParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return garmire::waveformName(getValue());
	}
};

struct MultDivParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float raw = getValue();
		float ratio = std::pow(2.f, raw);
		char buf[32];
		if (ratio >= 1.f) {
			snprintf(buf, sizeof(buf), "\xc3\x97%.2f", ratio);
		} else {
			snprintf(buf, sizeof(buf), "\xc3\xb7%.2f", 1.f / ratio);
		}
		return std::string(buf);
	}
	std::string getUnit() override { return ""; }
};

struct FineTuneParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float raw = getValue();
		float ratio = std::pow(1.5f, raw);
		char buf[32];
		snprintf(buf, sizeof(buf), "\xc3\x97%.3f", ratio);
		return std::string(buf);
	}
	std::string getUnit() override { return ""; }
};

// ── 1/2 Size 3-Position Switch (TOP = Off, MID = S&H, BOT = T&H) ───
struct SmallCKSSThree : app::SvgSwitch {
	SmallCKSSThree() {
		// Frame 0 (value 0.0) = TOP position (Off, default)
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThree_2.svg")));
		// Frame 1 (value 1.0) = MIDDLE position (S&H)
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThree_1.svg")));
		// Frame 2 (value 2.0) = BOTTOM position (T&H)
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThree_0.svg")));
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		// Translate by 25% of box size and scale down to 50% size centered
		nvgTranslate(args.vg, box.size.x * 0.25f, box.size.y * 0.25f);
		nvgScale(args.vg, 0.5f, 0.5f);
		app::SvgSwitch::draw(args);
		nvgRestore(args.vg);
	}
};

struct Chromance : Module {
	enum VoltageRange {
		RANGE_BI_5V = 0,    // -5V to +5V
		RANGE_UNI_5V,       // 0V to 5V (Default)
		RANGE_UNI_10V       // 0V to 10V
	};

	VoltageRange voltageRange = RANGE_UNI_5V;

	enum ParamId {
		FREQ_PARAM,
		FINE_PARAM,
		MULTDIV_PARAM,
		PHASE_PARAM,
		WAVE_PARAM,
		AMP_PARAM,

		FREQ_TRIM_PARAM,
		FINE_TRIM_PARAM,
		MULTDIV_TRIM_PARAM,
		PHASE_TRIM_PARAM,
		WAVE_TRIM_PARAM,
		AMP_TRIM_PARAM,

		SH_MODE_PARAM,   // 0 = Off (Top, default), 1 = S&H (Middle), 2 = T&H (Bottom)
		SH_RATE_PARAM,   // Internal clock rate

		PARAMS_LEN
	};

	enum InputId {
		FREQ_CV_INPUT,
		FINE_CV_INPUT,
		MULTDIV_CV_INPUT,
		PHASE_CV_INPUT,
		WAVE_CV_INPUT,
		AMP_CV_INPUT,
		SYNC_INPUT,       // Sync In on the left
		CLK_INPUT,        // S/T&H In on the right

		INPUTS_LEN
	};

	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		SH_CLK_LIGHT,     // Very small red LED indicating S/T&H rate / clock CV
		LIGHTS_LEN
	};

	// ── Pre-allocated DSP State (MetaModule safe) ───────────────────
	struct ChannelState {
		float phase = 0.f;
		float syncPeriod = 1.f;
		float timeSinceSync = 0.f;
		rack::dsp::SchmittTrigger syncTrigger;

		// S&H / T&H state
		float shHeldValue = 0.f;
		float shClockPhase = 0.f;
		rack::dsp::SchmittTrigger shTrigger;
	};

	ChannelState state[16];

	Chromance() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Main Knobs
		configParam(FREQ_PARAM, -4.f, 4.f, 0.f, "Frequency", " Hz", 2.f, 1.f);
		configParam<FineTuneParamQuantity>(FINE_PARAM, -1.f, 1.f, 0.f, "Fine Tune");
		configParam<MultDivParamQuantity>(MULTDIV_PARAM, -5.f, 5.f, 0.f, "Mult/Div");
		configParam(PHASE_PARAM, 0.f, 1.f, 0.f, "Phase Offset", "\xc2\xb0", 0.f, 360.f);
		configParam<WaveformParamQuantity>(WAVE_PARAM, 0.f, 1.f, 0.f, "Waveform Morph");
		
		// Oscillator level initialized to 100% (1.0) by default
		configParam(AMP_PARAM, 0.f, 1.f, 1.f, "Oscillator Level", "%", 0.f, 100.f);

		// CV Attenuverters default to 0% (0.0)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(FINE_TRIM_PARAM, -1.f, 1.f, 0.f, "Fine tune CV depth", "%", 0.f, 100.f);
		configParam(MULTDIV_TRIM_PARAM, -1.f, 1.f, 0.f, "Mult/div CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);
		configParam(WAVE_TRIM_PARAM, -1.f, 1.f, 0.f, "Waveform CV depth", "%", 0.f, 100.f);
		configParam(AMP_TRIM_PARAM, -1.f, 1.f, 0.f, "Level CV depth", "%", 0.f, 100.f);

		// S&H / T&H Switch: 0 = Off (Top, default), 1 = S&H (Middle), 2 = T&H (Bottom)
		configSwitch(SH_MODE_PARAM, 0.f, 2.f, 0.f, "S&H / T&H Mode", {"Off (Top)", "Sample & Hold (Middle)", "Track & Hold (Bottom)"});
		configParam(SH_RATE_PARAM, 0.f, 1.f, 0.5f, "S&H Rate");

		// Inputs & Outputs
		configInput(FREQ_CV_INPUT, "Frequency CV Input");
		configInput(FINE_CV_INPUT, "Fine Tune CV Input");
		configInput(MULTDIV_CV_INPUT, "Mult/Div CV Input");
		configInput(PHASE_CV_INPUT, "Phase CV Input");
		configInput(WAVE_CV_INPUT, "Waveform CV Input");
		configInput(AMP_CV_INPUT, "Level CV Input");
		configInput(SYNC_INPUT, "Sync In Input");
		configInput(CLK_INPUT, "S/T&H In Input");

		configOutput(OUT_OUTPUT, "Oscillator Output");
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* vRangeJ = json_object_get(rootJ, "voltageRange");
		if (vRangeJ) voltageRange = (VoltageRange)json_integer_value(vRangeJ);
	}

	float getModParam(int paramId, int attvId, int inputId, int channel) {
		float val = params[paramId].getValue();
		if (inputs[inputId].isConnected()) {
			val += (inputs[inputId].getPolyVoltage(channel) / 5.f) * params[attvId].getValue();
		}
		return val;
	}

	float scaleOutput(float normBipolar) {
		// normBipolar is in range [-1.0, 1.0] from waveform morph
		switch (voltageRange) {
			case RANGE_BI_5V:   return normBipolar * 5.f;                    // -5V to +5V
			case RANGE_UNI_5V:  return (normBipolar * 0.5f + 0.5f) * 5.f;   // 0V to 5V (Default)
			case RANGE_UNI_10V: return (normBipolar * 0.5f + 0.5f) * 10.f;  // 0V to 10V
			default:            return (normBipolar * 0.5f + 0.5f) * 5.f;
		}
	}

	float processSH(int c, float input, bool hasExtClock, bool internalTrigger, bool internalGateHigh) {
		int mode = (int)params[SH_MODE_PARAM].getValue();
		if (mode == 0) {
			// Off: Bit-transparent bypass
			state[c].shHeldValue = input;
			return input;
		}

		if (mode == 1) {
			// S&H: sample on rising edge
			bool triggered = false;
			if (hasExtClock) {
				triggered = state[c].shTrigger.process(
					inputs[CLK_INPUT].getPolyVoltage(c), 0.1f, 2.f);
			} else {
				triggered = internalTrigger;
			}
			if (triggered) {
				state[c].shHeldValue = input;
			}
		} else {
			// T&H: track while high, hold while low
			bool gateHigh;
			if (hasExtClock) {
				gateHigh = (inputs[CLK_INPUT].getPolyVoltage(c) >= 2.f);
			} else {
				gateHigh = internalGateHigh;
			}
			if (gateHigh) {
				state[c].shHeldValue = input;
			}
		}
		return state[c].shHeldValue;
	}

	void process(const ProcessArgs& args) override {
		int freqCh = inputs[FREQ_CV_INPUT].getChannels();
		int waveCh = inputs[WAVE_CV_INPUT].getChannels();
		int syncCh = inputs[SYNC_INPUT].getChannels();
		int clkCh  = inputs[CLK_INPUT].getChannels();

		int channels = std::max({freqCh, waveCh, syncCh, clkCh, 1});
		outputs[OUT_OUTPUT].setChannels(channels);

		// Advance internal S/T&H clock oscillator ONLY when external clock is unpatched
		bool hasExtClock = inputs[CLK_INPUT].isConnected();
		bool internalTrigger = false;
		bool internalGateHigh = false;

		if (!hasExtClock) {
			float rate = clamp(params[SH_RATE_PARAM].getValue(), 0.f, 1.f);
			float freq = 0.5f * std::pow(2000.f, rate);
			state[0].shClockPhase += freq * args.sampleTime;
			if (state[0].shClockPhase >= 1.f) {
				state[0].shClockPhase -= std::floor(state[0].shClockPhase);
				internalTrigger = true;
			}
			internalGateHigh = (state[0].shClockPhase < 0.5f);
		}

		for (int c = 0; c < channels; c++) {
			ChannelState& cs = state[c];

			// Hard sync
			bool hasSync = inputs[SYNC_INPUT].isConnected();
			if (hasSync) {
				cs.timeSinceSync += args.sampleTime;
				if (cs.syncTrigger.process(inputs[SYNC_INPUT].getPolyVoltage(c), 0.1f, 2.f)) {
					if (cs.timeSinceSync > 0.001f) {
						cs.syncPeriod = cs.timeSinceSync;
					}
					cs.timeSinceSync = 0.f;
					cs.phase = 0.f;
				}
			}

			// Freq & Mult/Div & Fine
			float baseFreqParam = getModParam(FREQ_PARAM, FREQ_TRIM_PARAM, FREQ_CV_INPUT, c);
			float baseFreqPitch = std::pow(2.f, clamp(baseFreqParam, -4.f, 4.f));

			float multDivParam = getModParam(MULTDIV_PARAM, MULTDIV_TRIM_PARAM, MULTDIV_CV_INPUT, c);
			float multDiv = std::pow(2.f, clamp(multDivParam, -5.f, 5.f));

			float fineParam = getModParam(FINE_PARAM, FINE_TRIM_PARAM, FINE_CV_INPUT, c);
			float fineTune = std::pow(1.5f, clamp(fineParam, -1.f, 1.f));

			float freq;
			if (hasSync && cs.syncPeriod > 0.f) {
				freq = (1.f / cs.syncPeriod) * multDiv * fineTune;
			} else {
				freq = 1.f * baseFreqPitch * multDiv * fineTune;
			}

			// Phase accumulation
			cs.phase += freq * args.sampleTime;
			cs.phase -= std::floor(cs.phase);

			float phaseOffset = getModParam(PHASE_PARAM, PHASE_TRIM_PARAM, PHASE_CV_INPUT, c);
			float effPhase = cs.phase + phaseOffset;
			effPhase -= std::floor(effPhase);

			// Waveform morph [-1.0, 1.0]
			float waveMorph = clamp(getModParam(WAVE_PARAM, WAVE_TRIM_PARAM, WAVE_CV_INPUT, c), 0.f, 1.f);
			float oscRaw = garmire::waveformMorph(effPhase, waveMorph);

			// Amplitude (0.0 to 1.0)
			float amp = clamp(getModParam(AMP_PARAM, AMP_TRIM_PARAM, AMP_CV_INPUT, c), 0.f, 1.f);
			float oscScaled = scaleOutput(oscRaw) * amp;

			// S&H / T&H processing
			float finalOut = processSH(c, oscScaled, hasExtClock, internalTrigger, internalGateHigh);

			outputs[OUT_OUTPUT].setVoltage(finalOut, c);
		}

		// Update S/T&H Rate / Clock LED Indicator (Channel 0)
		float lightVal = 0.f;
		if (hasExtClock) {
			// External clock connected: reflect gate/clock voltage level strictly
			float clkVolts = inputs[CLK_INPUT].getVoltage();
			lightVal = (clkVolts >= 2.0f) ? 1.0f : 0.0f;
		} else {
			// Internal clock: reflect internal clock square wave gate high state
			lightVal = internalGateHigh ? 1.0f : 0.0f;
		}
		lights[SH_CLK_LIGHT].setBrightness(lightVal);
	}
};

struct ChromanceWidget : ModuleWidget {
	ChromanceWidget(Chromance* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Chromance.svg")));

		// 10HP Screws (width 50.80mm)
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 3 Centered Main Knob Columns: X1 = 10.16mm, X2 = 25.40mm (Midline), X3 = 40.64mm
		// Row 1 Main Knobs (Frequency Group): FREQ (10.16), FINE (25.40), MULT/DIV (40.64) at Center Y = 21.59mm
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 21.59)), module, Chromance::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.40, 21.59)), module, Chromance::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(40.64, 21.59)), module, Chromance::MULTDIV_PARAM));

		// Row 2 Main Knobs (Waveform & Output Group): WAVE (10.16), PHASE (25.40), AMP (40.64) at Center Y = 43.00mm
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 43.00)), module, Chromance::WAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.40, 43.00)), module, Chromance::PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(40.64, 43.00)), module, Chromance::AMP_PARAM));

		// Attenuverters & S/T&H Horizontal Plane (4 Columns: 6.35mm, 19.05mm, 31.75mm, 44.45mm)
		// Row 1 (Frequency Group): FREQ ATTV (6.35), FINE ATTV (19.05), MULT ATTV (31.75), S/T&H SWITCH + LED (44.45) (Center Y = 61.50mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.35, 61.50)), module, Chromance::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(19.05, 61.50)), module, Chromance::FINE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.75, 61.50)), module, Chromance::MULTDIV_TRIM_PARAM));
		addParam(createParamCentered<SmallCKSSThree>(mm2px(Vec(42.70, 61.50)), module, Chromance::SH_MODE_PARAM));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(46.20, 61.50)), module, Chromance::SH_CLK_LIGHT));

		// Row 2 (Waveform & Output Group): WAVE ATTV (6.35), PHASE ATTV (19.05), AMP ATTV (31.75), S/T&H RATE TRIMPOT (44.45) (Center Y = 72.50mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.35, 72.50)), module, Chromance::WAVE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(19.05, 72.50)), module, Chromance::PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.75, 72.50)), module, Chromance::AMP_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(44.45, 72.50)), module, Chromance::SH_RATE_PARAM));

		// Jack Grid (3 Rows x 3 Columns: 10.16mm, 25.40mm, 40.64mm) with generous 13mm pitch
		// Jack Row 1 (Frequency CV Inputs, Center Y = 92.00mm): FREQ (10.16), FINE (25.40), MULT (40.64)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 92.00)), module, Chromance::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.40, 92.00)), module, Chromance::FINE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40.64, 92.00)), module, Chromance::MULTDIV_CV_INPUT));

		// Jack Row 2 (Waveform & Output CV Inputs, Center Y = 105.00mm): WAVE (10.16), PHASE (25.40), AMP (40.64)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 105.00)), module, Chromance::WAVE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.40, 105.00)), module, Chromance::PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40.64, 105.00)), module, Chromance::AMP_CV_INPUT));

		// Jack Row 3 (Bottom Row Outputs & Auxiliary Inputs, Center Y = 118.00mm): SYNC IN (10.16), OUT (25.40), S/T&H IN (40.64)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 118.00)), module, Chromance::SYNC_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.40, 118.00)), module, Chromance::OUT_OUTPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40.64, 118.00)), module, Chromance::CLK_INPUT));
	}

	void appendContextMenu(ui::Menu* menu) override {
		Chromance* module = dynamic_cast<Chromance*>(this->module);
		if (!module) return;

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Output Voltage Range"));

		struct VoltageRangeItem : ui::MenuItem {
			Chromance* module;
			Chromance::VoltageRange range;
			void onAction(const event::Action& e) override {
				module->voltageRange = range;
			}
		};

		const char* const names[3] = {
			"-5V to +5V (Bipolar)",
			"0V to 5V (Unipolar, Default)",
			"0V to 10V (Unipolar)"
		};
		Chromance::VoltageRange ranges[3] = {
			Chromance::RANGE_BI_5V,
			Chromance::RANGE_UNI_5V,
			Chromance::RANGE_UNI_10V
		};

		for (int i = 0; i < 3; i++) {
			VoltageRangeItem* item = new VoltageRangeItem;
			item->text = names[i];
			item->rightText = CHECKMARK(module->voltageRange == ranges[i]);
			item->module = module;
			item->range = ranges[i];
			menu->addChild(item);
		}
	}
};

Model* modelChromance = createModel<Chromance, ChromanceWidget>("Chromance");
