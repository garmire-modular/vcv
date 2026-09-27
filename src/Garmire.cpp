#include "plugin.hpp"
#include "dsp/ColorEngine.hpp"
#include "dsp/Oscillator.hpp"
#include "dsp/Instability.hpp"

// ─────────────────────────────────────────────────────────────────────
//  Garmire — Laser RGB & Blanking Controller
// ─────────────────────────────────────────────────────────────────────

enum Channel { CH_R = 0, CH_G, CH_B, CH_BLANK, NUM_CHANNELS };

// Per-channel parameter offsets (14 params per channel)
enum ChannelParamOffset {
	CHP_FREQ = 0,
	CHP_FREQ_ATTV,
	CHP_FINE,
	CHP_FINE_ATTV,
	CHP_MULTDIV,
	CHP_MULTDIV_ATTV,
	CHP_PHASE,
	CHP_PHASE_ATTV,
	CHP_WAVE,         // Waveform morph (R/G/B) or Envelope shape (Blank)
	CHP_WAVE_ATTV,
	CHP_AMP,          // Oscillator Level (R/G/B) or Modulation Depth (Blank)
	CHP_AMP_ATTV,
	CHP_SH_MODE,      // 0 = S&H, 1 = T&H
	CHP_SH_RATE,      // Internal clock rate
	CHP_COUNT         // = 14
};

// Per-channel input offsets (8 inputs per channel)
enum ChannelInputOffset {
	CHI_FREQ = 0,
	CHI_SH_CLK,
	CHI_FINE,
	CHI_MULTDIV,
	CHI_PHASE,
	CHI_WAVE,
	CHI_SYNC,
	CHI_AMP,
	CHI_COUNT         // = 8
};

// Forward declaration
struct Garmire;

// ── Custom ParamQuantity: waveform morph tooltip ────────────────────
struct WaveformParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return garmire::waveformName(getValue());
	}
};

// ── Custom ParamQuantity: blanking shape tooltip ────────────────────
struct BlankingShapeParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return garmire::blankingShapeName(getValue());
	}
};

// ── Custom ParamQuantity: mult/div with ratio display ───────────────
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

// ── Custom ParamQuantity: fine tune with interval display ───────────
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


// ─────────────────────────────────────────────────────────────────────
//  Module
// ─────────────────────────────────────────────────────────────────────
struct Garmire : Module {
	// ── Param IDs ───────────────────────────────────────────────────
	enum ParamId {
		// Color Space Engine (7)
		COLOR_MODE_PARAM,
		COLOR_P1_PARAM,      COLOR_P1_ATTV_PARAM,
		COLOR_P2_PARAM,      COLOR_P2_ATTV_PARAM,
		COLOR_P3_PARAM,      COLOR_P3_ATTV_PARAM,

		// Global Instability / Drift (6)
		INST_FREQ_PARAM,     INST_FREQ_ATTV_PARAM,
		INST_PHASE_PARAM,    INST_PHASE_ATTV_PARAM,
		INST_AMP_PARAM,      INST_AMP_ATTV_PARAM,

		// Per-channel params (4 × 14 = 56)
		CHANNEL_PARAMS_START,

		NUM_PARAMS = CHANNEL_PARAMS_START + NUM_CHANNELS * CHP_COUNT
	};

	// ── Input IDs ───────────────────────────────────────────────────
	enum InputId {
		// Color Engine CV (3)
		COLOR_P1_INPUT,
		COLOR_P2_INPUT,
		COLOR_P3_INPUT,

		// Global Instability CV (3)
		INST_FREQ_INPUT,
		INST_PHASE_INPUT,
		INST_AMP_INPUT,

		// Per-channel inputs (4 × 8 = 32)
		CHANNEL_INPUTS_START,

		NUM_INPUTS = CHANNEL_INPUTS_START + NUM_CHANNELS * CHI_COUNT
	};

	// ── Output IDs ──────────────────────────────────────────────────
	enum OutputId {
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		NUM_OUTPUTS
	};

	// ── Light IDs ───────────────────────────────────────────────────
	enum LightId {
		NUM_LIGHTS
	};

	// ── Helpers ─────────────────────────────────────────────────────
	static int channelParam(int ch, int offset) {
		return CHANNEL_PARAMS_START + ch * CHP_COUNT + offset;
	}
	static int channelInput(int ch, int offset) {
		return CHANNEL_INPUTS_START + ch * CHI_COUNT + offset;
	}

	// ── Enums ───────────────────────────────────────────────────────
	enum ColorMode { MODE_RGB = 0, MODE_HSV, MODE_OKLCH };
	enum VoltageRange { RANGE_BI_5V = 0, RANGE_UNI_5V, RANGE_UNI_10V };

	enum DriftScope {
		SCOPE_GLOBAL = 0,
		SCOPE_R,
		SCOPE_G,
		SCOPE_B,
		SCOPE_RG,
		SCOPE_RB,
		SCOPE_GB,
		NUM_SCOPES
	};
	static const char* getScopeName(int s) {
		static const char* const names[NUM_SCOPES] = {
			"Global (All)", "Red", "Green", "Blue", "Red + Green", "Red + Blue", "Green + Blue"
		};
		if (s >= 0 && s < NUM_SCOPES) return names[s];
		return "";
	}

	bool isChannelInScope(int ch, DriftScope scope) const {
		switch (scope) {
			case SCOPE_GLOBAL: return true;
			case SCOPE_R:      return ch == CH_R;
			case SCOPE_G:      return ch == CH_G;
			case SCOPE_B:      return ch == CH_B;
			case SCOPE_RG:     return ch == CH_R || ch == CH_G;
			case SCOPE_RB:     return ch == CH_R || ch == CH_B;
			case SCOPE_GB:     return ch == CH_G || ch == CH_B;
			default:           return true;
		}
	}

	// ── Configuration state ─────────────────────────────────────────
	DriftScope freqDriftScope = SCOPE_GLOBAL;
	DriftScope phaseDriftScope = SCOPE_GLOBAL;
	DriftScope ampDriftScope = SCOPE_GLOBAL;
	VoltageRange voltageRange = RANGE_UNI_5V;

	// ── Per-channel DSP state (pre-allocated for MetaModule) ────────
	struct ChannelState {
		float phase = 0.f;               // Phase accumulator [0, 1)
		float syncPeriod = 1.f;          // Measured sync period (seconds)
		float timeSinceSync = 0.f;       // Timer for measuring period
		float phaseDrift = 0.f;          // Instability: current phase drift
		float ampDrift = 0.f;            // Instability: current amplitude drift
		rack::dsp::SchmittTrigger syncTrigger;

		// S&H / T&H state
		float shHeldValue = 0.f;         // Current held value
		float shClockPhase = 0.f;        // Internal clock phase [0, 1)
		rack::dsp::SchmittTrigger shTrigger;
	};

	// ── Module state ────────────────────────────────────────────────
	float baseRgb[3] = {0.f, 0.f, 0.f};
	ChannelState channels[NUM_CHANNELS];
	float channelOut[NUM_CHANNELS] = {};

	// ── Instability drift generators (3 per channel, independently seeded) ──
	garmire::DriftGenerator driftFreq[NUM_CHANNELS];
	garmire::DriftGenerator driftPhase[NUM_CHANNELS];
	garmire::DriftGenerator driftAmp[NUM_CHANNELS];

	// ── Constructor ─────────────────────────────────────────────────
	Garmire() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		// Seed drift generators
		for (int ch = 0; ch < NUM_CHANNELS; ch++) {
			driftFreq[ch].init(random::u32());
			driftPhase[ch].init(random::u32());
			driftAmp[ch].init(random::u32());
		}

		// ─ Color Engine ─────────────────────────────────────────────
		configSwitch(COLOR_MODE_PARAM, 0.f, 2.f, 0.f, "Color Mode",
			{"RGB", "HSV", "OKLCH"});

		configParam(COLOR_P1_PARAM, 0.f, 1.f, 0.f, "Color Param 1 (R / H / L)");
		configParam(COLOR_P1_ATTV_PARAM, -1.f, 1.f, 0.f, "Color P1 CV Atten");
		configInput(COLOR_P1_INPUT, "Color Param 1 CV");

		configParam(COLOR_P2_PARAM, 0.f, 1.f, 0.f, "Color Param 2 (G / S / C)");
		configParam(COLOR_P2_ATTV_PARAM, -1.f, 1.f, 0.f, "Color P2 CV Atten");
		configInput(COLOR_P2_INPUT, "Color Param 2 CV");

		configParam(COLOR_P3_PARAM, 0.f, 1.f, 0.f, "Color Param 3 (B / V / H)");
		configParam(COLOR_P3_ATTV_PARAM, -1.f, 1.f, 0.f, "Color P3 CV Atten");
		configInput(COLOR_P3_INPUT, "Color Param 3 CV");

		// ─ Global Instability ───────────────────────────────────────
		configParam(INST_FREQ_PARAM, 0.f, 1.f, 0.f, "Global Freq Drift Rate");
		configParam(INST_FREQ_ATTV_PARAM, -1.f, 1.f, 0.f, "Freq Drift CV Atten");
		configInput(INST_FREQ_INPUT, "Freq Drift CV");

		configParam(INST_PHASE_PARAM, 0.f, 1.f, 0.f, "Global Phase Drift Rate");
		configParam(INST_PHASE_ATTV_PARAM, -1.f, 1.f, 0.f, "Phase Drift CV Atten");
		configInput(INST_PHASE_INPUT, "Phase Drift CV");

		configParam(INST_AMP_PARAM, 0.f, 1.f, 0.f, "Global Amp Drift Rate");
		configParam(INST_AMP_ATTV_PARAM, -1.f, 1.f, 0.f, "Amp Drift CV Atten");
		configInput(INST_AMP_INPUT, "Amp Drift CV");

		// ─ Per-channel configuration ────────────────────────────────
		const char* chNames[NUM_CHANNELS] = {"Red", "Green", "Blue", "Blanking"};
		for (int ch = 0; ch < NUM_CHANNELS; ch++) {
			std::string pre = std::string(chNames[ch]) + " ";

			// Frequency
			configParam(channelParam(ch, CHP_FREQ), -4.f, 4.f, 0.f,
				pre + "Frequency", " Hz", 2.f, 1.f);
			configParam(channelParam(ch, CHP_FREQ_ATTV), -1.f, 1.f, 0.f,
				pre + "Freq CV Atten");
			configInput(channelInput(ch, CHI_FREQ), pre + "Frequency CV");

			// Fine tune
			configParam<FineTuneParamQuantity>(channelParam(ch, CHP_FINE),
				-1.f, 1.f, 0.f, pre + "Fine Tune");
			configParam(channelParam(ch, CHP_FINE_ATTV), -1.f, 1.f, 0.f,
				pre + "Fine CV Atten");
			configInput(channelInput(ch, CHI_FINE), pre + "Fine Tune CV");

			// Mult/Div
			configParam<MultDivParamQuantity>(channelParam(ch, CHP_MULTDIV),
				-5.f, 5.f, 0.f, pre + "Mult/Div");
			configParam(channelParam(ch, CHP_MULTDIV_ATTV), -1.f, 1.f, 0.f,
				pre + "Mult/Div CV Atten");
			configInput(channelInput(ch, CHI_MULTDIV), pre + "Mult/Div CV");

			// Phase
			configParam(channelParam(ch, CHP_PHASE), 0.f, 1.f, 0.f,
				pre + "Phase", "\xc2\xb0", 0.f, 360.f);
			configParam(channelParam(ch, CHP_PHASE_ATTV), -1.f, 1.f, 0.f,
				pre + "Phase CV Atten");
			configInput(channelInput(ch, CHI_PHASE), pre + "Phase CV");

			// Waveform / Shape
			if (ch < CH_BLANK) {
				configParam<WaveformParamQuantity>(channelParam(ch, CHP_WAVE),
					0.f, 1.f, 0.f, pre + "Waveform");
			} else {
				configParam<BlankingShapeParamQuantity>(channelParam(ch, CHP_WAVE),
					0.f, 1.f, 0.5f, pre + "Envelope Shape");
			}
			configParam(channelParam(ch, CHP_WAVE_ATTV), -1.f, 1.f, 0.f,
				pre + "Wave CV Atten");
			configInput(channelInput(ch, CHI_WAVE), pre + "Waveform CV");

			// Amp / Level
			configParam(channelParam(ch, CHP_AMP), 0.f, 1.f, (ch < CH_BLANK ? 0.f : 0.f),
				pre + (ch < CH_BLANK ? "Oscillator Level" : "Blanking Depth"), "%", 0.f, 100.f);
			configParam(channelParam(ch, CHP_AMP_ATTV), -1.f, 1.f, 0.f,
				pre + "Amp CV Atten");
			configInput(channelInput(ch, CHI_AMP), pre + "Amp CV");

			// S&H / T&H
			configSwitch(channelParam(ch, CHP_SH_MODE), 0.f, 1.f, 0.f,
				pre + "S&H / T&H Mode", {"S&H", "T&H"});
			configParam(channelParam(ch, CHP_SH_RATE), 0.f, 1.f, 1.f,
				pre + "S&H Rate");
			configInput(channelInput(ch, CHI_SH_CLK), pre + "S&H Clock / Gate");

			// Sync
			configInput(channelInput(ch, CHI_SYNC), pre + "Sync");
		}

		// ─ Outputs ──────────────────────────────────────────────────
		configOutput(R_OUTPUT, "Red Output");
		configOutput(G_OUTPUT, "Green Output");
		configOutput(B_OUTPUT, "Blue Output");
	}

	// ── Serialization ───────────────────────────────────────────────
	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "freqDriftScope", json_integer((int)freqDriftScope));
		json_object_set_new(rootJ, "phaseDriftScope", json_integer((int)phaseDriftScope));
		json_object_set_new(rootJ, "ampDriftScope", json_integer((int)ampDriftScope));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* fScopeJ = json_object_get(rootJ, "freqDriftScope");
		if (fScopeJ) freqDriftScope = (DriftScope)json_integer_value(fScopeJ);

		json_t* pScopeJ = json_object_get(rootJ, "phaseDriftScope");
		if (pScopeJ) phaseDriftScope = (DriftScope)json_integer_value(pScopeJ);

		json_t* aScopeJ = json_object_get(rootJ, "ampDriftScope");
		if (aScopeJ) ampDriftScope = (DriftScope)json_integer_value(aScopeJ);

		json_t* vRangeJ = json_object_get(rootJ, "voltageRange");
		if (vRangeJ) voltageRange = (VoltageRange)json_integer_value(vRangeJ);
	}

	// ── Read knob + attenuverter + CV ───────────────────────────────
	float getModParam(int paramId, int attvId, int inputId) {
		float val = params[paramId].getValue();
		if (inputs[inputId].isConnected()) {
			val += inputs[inputId].getVoltage() / 5.f * params[attvId].getValue();
		}
		return val;
	}

	// ── Scale [0,1] to output voltage ───────────────────────────────
	float scaleOutput(float normalized, VoltageRange range) {
		switch (range) {
			case RANGE_BI_5V:   return normalized * 10.f - 5.f;
			case RANGE_UNI_5V:  return normalized * 5.f;
			case RANGE_UNI_10V: return normalized * 10.f;
			default:            return normalized * 5.f;
		}
	}

	// ── S&H / T&H processing ────────────────────────────────────────
	float processSH(int ch, float input, float deltaTime) {
		ChannelState& cs = channels[ch];

		int mode = (int)params[channelParam(ch, CHP_SH_MODE)].getValue();
		bool hasExtClock = inputs[channelInput(ch, CHI_SH_CLK)].isConnected();

		float rate = clamp(params[channelParam(ch, CHP_SH_RATE)].getValue(), 0.f, 1.f);

		// Bypass if rate fully CW and no external clock
		if (!hasExtClock && rate >= 0.99f) {
			cs.shHeldValue = input;
			return input;
		}

		if (mode == 0) {
			// S&H: sample on rising edge
			bool triggered = false;
			if (hasExtClock) {
				triggered = cs.shTrigger.process(
					inputs[channelInput(ch, CHI_SH_CLK)].getVoltage(), 0.1f, 2.f);
			} else {
				float freq = 0.5f * std::pow(2000.f, rate);
				cs.shClockPhase += freq * deltaTime;
				if (cs.shClockPhase >= 1.f) {
					cs.shClockPhase -= std::floor(cs.shClockPhase);
					triggered = true;
				}
			}
			if (triggered) {
				cs.shHeldValue = input;
			}
		} else {
			// T&H: track while high, hold while low
			bool gateHigh;
			if (hasExtClock) {
				gateHigh = inputs[channelInput(ch, CHI_SH_CLK)].getVoltage() >= 2.f;
			} else {
				float freq = 0.5f * std::pow(2000.f, rate);
				cs.shClockPhase += freq * deltaTime;
				cs.shClockPhase -= std::floor(cs.shClockPhase);
				gateHigh = cs.shClockPhase < 0.5f;
			}
			if (gateHigh) {
				cs.shHeldValue = input;
			}
		}
		return cs.shHeldValue;
	}

	// ═════════════════════════════════════════════════════════════════
	//  PROCESS
	// ═════════════════════════════════════════════════════════════════
	void process(const ProcessArgs& args) override {
		// ─ 1. Color Engine ──────────────────────────────────────────
		int mode = (int)params[COLOR_MODE_PARAM].getValue();
		float p1 = clamp(getModParam(COLOR_P1_PARAM, COLOR_P1_ATTV_PARAM, COLOR_P1_INPUT), 0.f, 1.f);
		float p2 = clamp(getModParam(COLOR_P2_PARAM, COLOR_P2_ATTV_PARAM, COLOR_P2_INPUT), 0.f, 1.f);
		float p3 = clamp(getModParam(COLOR_P3_PARAM, COLOR_P3_ATTV_PARAM, COLOR_P3_INPUT), 0.f, 1.f);

		switch ((ColorMode)mode) {
			case MODE_RGB:
				baseRgb[0] = p1;
				baseRgb[1] = p2;
				baseRgb[2] = p3;
				break;
			case MODE_HSV:
				garmire::color::hsvToRgb(p1 * 360.f, p2, p3,
					baseRgb[0], baseRgb[1], baseRgb[2]);
				break;
			case MODE_OKLCH:
				garmire::color::oklchToRgb(p1, p2 * 0.4f, p3 * 360.f,
					baseRgb[0], baseRgb[1], baseRgb[2]);
				break;
		}

		// ─ 2. Global Instability Character ──────────────────────────
		float instFreqRate = clamp(getModParam(
			INST_FREQ_PARAM, INST_FREQ_ATTV_PARAM, INST_FREQ_INPUT), 0.f, 1.f);
		float instPhaseRate = clamp(getModParam(
			INST_PHASE_PARAM, INST_PHASE_ATTV_PARAM, INST_PHASE_INPUT), 0.f, 1.f);
		float instAmpRate = clamp(getModParam(
			INST_AMP_PARAM, INST_AMP_ATTV_PARAM, INST_AMP_INPUT), 0.f, 1.f);

		// ─ 3. Per-Channel Oscillators ───────────────────────────────
		for (int ch = 0; ch < NUM_CHANNELS; ch++) {
			ChannelState& cs = channels[ch];

			// ── Sync input ──────────────────────────────────────────
			bool hasSyncCable = inputs[channelInput(ch, CHI_SYNC)].isConnected();
			if (hasSyncCable) {
				cs.timeSinceSync += args.sampleTime;
				if (cs.syncTrigger.process(
						inputs[channelInput(ch, CHI_SYNC)].getVoltage(), 0.1f, 2.f)) {
					if (cs.timeSinceSync > 0.001f) {
						cs.syncPeriod = cs.timeSinceSync;
					}
					cs.timeSinceSync = 0.f;
					cs.phase = 0.f; // Hard sync
				}
			}

			// ── Frequency calculation ───────────────────────────────
			float baseFreqParam = getModParam(
				channelParam(ch, CHP_FREQ),
				channelParam(ch, CHP_FREQ_ATTV),
				channelInput(ch, CHI_FREQ));
			float baseFreqPitch = std::pow(2.f, clamp(baseFreqParam, -4.f, 4.f));

			float multDivParam = getModParam(
				channelParam(ch, CHP_MULTDIV),
				channelParam(ch, CHP_MULTDIV_ATTV),
				channelInput(ch, CHI_MULTDIV));
			float multDiv = std::pow(2.f, clamp(multDivParam, -5.f, 5.f));

			float fineParam = getModParam(
				channelParam(ch, CHP_FINE),
				channelParam(ch, CHP_FINE_ATTV),
				channelInput(ch, CHI_FINE));
			float fineTune = std::pow(1.5f, clamp(fineParam, -1.f, 1.f));

			float freq;
			if (hasSyncCable && cs.syncPeriod > 0.f) {
				freq = (1.f / cs.syncPeriod) * multDiv * fineTune;
			} else {
				freq = 1.f * baseFreqPitch * multDiv * fineTune;
			}

			// ── Instability Drift ───────────────────────────────────
			// Always tick generators to preserve continuity
			float dFreq  = driftFreq[ch].process(args.sampleTime, instFreqRate);
			float dPhase = driftPhase[ch].process(args.sampleTime, instPhaseRate);
			float dAmp   = driftAmp[ch].process(args.sampleTime, instAmpRate);

			if (isChannelInScope(ch, freqDriftScope)) {
				freq *= (1.f + dFreq * instFreqRate * 0.15f);
			}
			if (isChannelInScope(ch, phaseDriftScope)) {
				cs.phaseDrift = dPhase * instPhaseRate * 0.1f;
			} else {
				cs.phaseDrift = 0.f;
			}
			if (isChannelInScope(ch, ampDriftScope)) {
				cs.ampDrift = dAmp * instAmpRate * 0.25f;
			} else {
				cs.ampDrift = 0.f;
			}

			// ── Phase accumulation ──────────────────────────────────
			cs.phase += freq * args.sampleTime;
			cs.phase -= std::floor(cs.phase);

			// ── Phase offset ────────────────────────────────────────
			float phaseOffset = getModParam(
				channelParam(ch, CHP_PHASE),
				channelParam(ch, CHP_PHASE_ATTV),
				channelInput(ch, CHI_PHASE));
			float effectivePhase = cs.phase + phaseOffset + cs.phaseDrift;
			effectivePhase -= std::floor(effectivePhase);

			// ── Waveform / Envelope shape ───────────────────────────
			float waveMorph = clamp(getModParam(
				channelParam(ch, CHP_WAVE),
				channelParam(ch, CHP_WAVE_ATTV),
				channelInput(ch, CHI_WAVE)), 0.f, 1.f);

			float oscRaw;
			if (ch < CH_BLANK) {
				oscRaw = garmire::waveformMorph(effectivePhase, waveMorph);
			} else {
				oscRaw = garmire::blankingEnvelope(effectivePhase, waveMorph);
			}

			// ── Amplitude / Level ───────────────────────────────────
			float amp = clamp(getModParam(
				channelParam(ch, CHP_AMP),
				channelParam(ch, CHP_AMP_ATTV),
				channelInput(ch, CHI_AMP)), 0.f, 1.f);

			amp *= (1.f + cs.ampDrift);

			// ── S&H / T&H ───────────────────────────────────────────
			if (ch < CH_BLANK) {
				channelOut[ch] = processSH(ch, oscRaw * amp, args.sampleTime);
			} else {
				// Blanking channel: S&H applied to envelope
				channelOut[CH_BLANK] = processSH(CH_BLANK, oscRaw, args.sampleTime);
			}
		}

		// ─ 4. Blanking Master 3xVCA ─────────────────────────────────
		float blankDepth = clamp(getModParam(
			channelParam(CH_BLANK, CHP_AMP),
			channelParam(CH_BLANK, CHP_AMP_ATTV),
			channelInput(CH_BLANK, CHI_AMP)), 0.f, 1.f);
		blankDepth *= (1.f + channels[CH_BLANK].ampDrift);
		blankDepth = clamp(blankDepth, 0.f, 1.f);

		// When blankDepth is 0: blanking is 1.0 (fully open).
		// When blankDepth > 0: envelope modulates between 1.0 and 0.0.
		float blankingMod = 1.f - blankDepth * (1.f - channelOut[CH_BLANK]);
		blankingMod = clamp(blankingMod, 0.f, 1.f);

		// ─ 5. Combine: Bias + Oscillator × Blanking ─────────────────
		float rRaw = (baseRgb[0] + channelOut[CH_R]) * blankingMod;
		float gRaw = (baseRgb[1] + channelOut[CH_G]) * blankingMod;
		float bRaw = (baseRgb[2] + channelOut[CH_B]) * blankingMod;

		float rOut = clamp(rRaw, 0.f, 1.f);
		float gOut = clamp(gRaw, 0.f, 1.f);
		float bOut = clamp(bRaw, 0.f, 1.f);

		// ─ 6. Output Voltage Scaling ────────────────────────────────
		outputs[R_OUTPUT].setVoltage(scaleOutput(rOut, voltageRange));
		outputs[G_OUTPUT].setVoltage(scaleOutput(gOut, voltageRange));
		outputs[B_OUTPUT].setVoltage(scaleOutput(bOut, voltageRange));
	}
};


// ─────────────────────────────────────────────────────────────────────
//  Custom Knob Widgets (Green and Blue Davies)
// ─────────────────────────────────────────────────────────────────────

struct Davies1900hGreenKnob : Davies1900hKnob {
	Davies1900hGreenKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/Davies1900hGreen.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/Davies1900hGreen_bg.svg")));
	}
};

struct Davies1900hBlueKnob : Davies1900hKnob {
	Davies1900hBlueKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/Davies1900hBlue.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/Davies1900hBlue_bg.svg")));
	}
};


// ─────────────────────────────────────────────────────────────────────
//  Widget
// ─────────────────────────────────────────────────────────────────────

struct GarmireWidget : ModuleWidget {

	// ── Coordinates mapping from 1024×746 mockup to 177.8×128.5 mm (35 HP)
	static constexpr float M_W = 1024.f;
	static constexpr float M_H = 746.f;
	static constexpr float P_W = 177.8f;
	static constexpr float P_H = 128.5f;

	static float sx(float x) { return x * (P_W / M_W); }
	static float sy(float y) { return y * (P_H / M_H); }

	GarmireWidget(Garmire* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Garmire.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addColorEngine();
		addDriftEngine();
		for (int ch = 0; ch < NUM_CHANNELS; ch++) {
			addChannelStrip(ch);
		}
	}

	// ── Color Space Engine ──────────────────────────────────────────
	void addColorEngine() {
		float x = sx(94.f);

		// Knobs (row 1, 2, 3)
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(x, sy(138.f))), module, Garmire::COLOR_P1_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(x, sy(258.f))), module, Garmire::COLOR_P2_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(x, sy(378.f))), module, Garmire::COLOR_P3_PARAM));

		// Attenuverters (trimpots)
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(x, sy(421.2f))), module, Garmire::COLOR_P1_ATTV_PARAM));
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(x, sy(458.9f))), module, Garmire::COLOR_P2_ATTV_PARAM));
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(x, sy(505.5f))), module, Garmire::COLOR_P3_ATTV_PARAM));

		// Mode switch (3-position slide switch)
		addParam(createParamCentered<CKSSThree>(
			mm2px(Vec(sx(38.f), sy(475.f))), module, Garmire::COLOR_MODE_PARAM));

		// CV Inputs (jacks)
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(x, sy(562.9f))), module, Garmire::COLOR_P1_INPUT));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(x, sy(618.3f))), module, Garmire::COLOR_P2_INPUT));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(x, sy(673.8f))), module, Garmire::COLOR_P3_INPUT));
	}

	// ── Global Drift Engine ─────────────────────────────────────────
	void addDriftEngine() {
		float x = sx(189.f);

		// Knobs
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(x, sy(138.f))), module, Garmire::INST_FREQ_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(x, sy(258.f))), module, Garmire::INST_PHASE_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(x, sy(378.f))), module, Garmire::INST_AMP_PARAM));

		// Attenuverters (trimpots)
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(x, sy(421.2f))), module, Garmire::INST_FREQ_ATTV_PARAM));
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(x, sy(458.9f))), module, Garmire::INST_PHASE_ATTV_PARAM));
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(x, sy(505.5f))), module, Garmire::INST_AMP_ATTV_PARAM));

		// CV Inputs (jacks)
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(x, sy(562.7f))), module, Garmire::INST_FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(x, sy(618.7f))), module, Garmire::INST_PHASE_INPUT));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(x, sy(674.0f))), module, Garmire::INST_AMP_INPUT));
	}

	// ── Channel Strip ───────────────────────────────────────────────
	void addChannelStrip(int ch) {
		const float chX[4][3] = {
			{283.5f, 331.0f, 378.5f}, // Red
			{473.0f, 521.0f, 568.0f}, // Green
			{663.0f, 710.4f, 757.5f}, // Blue
			{852.5f, 900.0f, 947.0f}  // Blanking
		};

		float xL = sx(chX[ch][0]);
		float xM = sx(chX[ch][1]);
		float xR = sx(chX[ch][2]);

		// Row 1 Knobs: Freq (left) & Fine (right)
		if (ch == 0) {
			addParam(createParamCentered<Davies1900hRedKnob>(
				mm2px(Vec(xL, sy(138.f))), module, Garmire::channelParam(ch, CHP_FREQ)));
		} else if (ch == 1) {
			addParam(createParamCentered<Davies1900hGreenKnob>(
				mm2px(Vec(xL, sy(138.f))), module, Garmire::channelParam(ch, CHP_FREQ)));
		} else if (ch == 2) {
			addParam(createParamCentered<Davies1900hBlueKnob>(
				mm2px(Vec(xL, sy(138.f))), module, Garmire::channelParam(ch, CHP_FREQ)));
		} else {
			addParam(createParamCentered<Davies1900hWhiteKnob>(
				mm2px(Vec(xL, sy(138.f))), module, Garmire::channelParam(ch, CHP_FREQ)));
		}

		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(xR, sy(138.f))), module, Garmire::channelParam(ch, CHP_FINE)));

		// Row 2 Knobs: Mult/Div (left) & Phase (right)
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(xL, sy(258.f))), module, Garmire::channelParam(ch, CHP_MULTDIV)));
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(xR, sy(258.f))), module, Garmire::channelParam(ch, CHP_PHASE)));

		// Row 3 Knobs: Wave (left) & Amp (right)
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(xL, sy(378.f))), module, Garmire::channelParam(ch, CHP_WAVE)));
		addParam(createParamCentered<Davies1900hBlackKnob>(
			mm2px(Vec(xR, sy(378.f))), module, Garmire::channelParam(ch, CHP_AMP)));

		// Trimpot Row 1: Freq Attv (left) & Fine Attv (right)
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(xL, sy(421.2f))), module, Garmire::channelParam(ch, CHP_FREQ_ATTV)));
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(xR, sy(421.2f))), module, Garmire::channelParam(ch, CHP_FINE_ATTV)));

		// Trimpot Row 2: MultDiv Attv (left), S/T&H Switch (mid), Phase Attv (right)
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(xL, sy(458.9f))), module, Garmire::channelParam(ch, CHP_MULTDIV_ATTV)));
		addParam(createParamCentered<CKSS>(
			mm2px(Vec(xM, sy(458.9f))), module, Garmire::channelParam(ch, CHP_SH_MODE)));
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(xR, sy(458.9f))), module, Garmire::channelParam(ch, CHP_PHASE_ATTV)));

		// Trimpot Row 3: Wave Attv (left), S/T&H Rate (mid), Amp Attv (right)
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(xL, sy(505.5f))), module, Garmire::channelParam(ch, CHP_WAVE_ATTV)));
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(xM, sy(505.5f))), module, Garmire::channelParam(ch, CHP_SH_RATE)));
		addParam(createParamCentered<Trimpot>(
			mm2px(Vec(xR, sy(505.5f))), module, Garmire::channelParam(ch, CHP_AMP_ATTV)));

		// Jack Row 1: Freq CV (left), S/T&H Clock (mid), Fine CV (right)
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(xL, sy(562.8f))), module, Garmire::channelInput(ch, CHI_FREQ)));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(xM, sy(562.8f))), module, Garmire::channelInput(ch, CHI_SH_CLK)));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(xR, sy(562.8f))), module, Garmire::channelInput(ch, CHI_FINE)));

		// Jack Row 2: MultDiv CV (left), Output (mid), Phase CV (right)
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(xL, sy(618.5f))), module, Garmire::channelInput(ch, CHI_MULTDIV)));

		if (ch == 0) {
			addOutput(createOutputCentered<PJ301MPort>(
				mm2px(Vec(xM, sy(618.7f))), module, Garmire::R_OUTPUT));
		} else if (ch == 1) {
			addOutput(createOutputCentered<PJ301MPort>(
				mm2px(Vec(xM, sy(618.7f))), module, Garmire::G_OUTPUT));
		} else if (ch == 2) {
			addOutput(createOutputCentered<PJ301MPort>(
				mm2px(Vec(xM, sy(618.3f))), module, Garmire::B_OUTPUT));
		}
		// Note: Channel 3 (Blanking) center jack is intentionally empty!

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(xR, sy(618.5f))), module, Garmire::channelInput(ch, CHI_PHASE)));

		// Jack Row 3: Wave CV (left), Sync (mid), Amp CV (right)
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(xL, sy(673.9f))), module, Garmire::channelInput(ch, CHI_WAVE)));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(xM, sy(673.9f))), module, Garmire::channelInput(ch, CHI_SYNC)));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(xR, sy(673.9f))), module, Garmire::channelInput(ch, CHI_AMP)));
	}

	// ── Right-Click Context Menu ────────────────────────────────────
	void appendContextMenu(Menu* menu) override {
		Garmire* module = dynamic_cast<Garmire*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Output Voltage Range"));

		const char* rangeNames[] = {"\xc2\xb1""5V", "0\xe2\x80\x93""5V (Default)", "0\xe2\x80\x93""10V"};
		for (int i = 0; i < 3; i++) {
			struct RangeItem : MenuItem {
				Garmire* module;
				Garmire::VoltageRange range;
				void onAction(const event::Action& e) override {
					module->voltageRange = range;
				}
			};
			RangeItem* item = createMenuItem<RangeItem>(rangeNames[i]);
			item->module = module;
			item->range = (Garmire::VoltageRange)i;
			item->rightText = (module->voltageRange == i) ? "\xe2\x9c\x94" : "";
			menu->addChild(item);
		}

		// Drift Target Scopes
		struct ScopeSubmenu : MenuItem {
			Garmire* module;
			int driftType; // 0 = freq, 1 = phase, 2 = amp

			Menu* createChildMenu() override {
				Menu* childMenu = new Menu;
				for (int s = 0; s < Garmire::NUM_SCOPES; s++) {
					struct ScopeItem : MenuItem {
						Garmire* module;
						int driftType;
						Garmire::DriftScope scope;
						void onAction(const event::Action& e) override {
							if (driftType == 0) module->freqDriftScope = scope;
							else if (driftType == 1) module->phaseDriftScope = scope;
							else if (driftType == 2) module->ampDriftScope = scope;
						}
					};
					ScopeItem* item = createMenuItem<ScopeItem>(Garmire::getScopeName(s));
					item->module = module;
					item->driftType = driftType;
					item->scope = (Garmire::DriftScope)s;
					Garmire::DriftScope curScope = (driftType == 0) ? module->freqDriftScope :
					                               (driftType == 1) ? module->phaseDriftScope :
					                               module->ampDriftScope;
					item->rightText = (curScope == s) ? "\xe2\x9c\x94" : "";
					childMenu->addChild(item);
				}
				return childMenu;
			}
		};

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Drift Target Scopes"));

		ScopeSubmenu* freqMenu = createMenuItem<ScopeSubmenu>("Frequency Drift Target");
		freqMenu->module = module;
		freqMenu->driftType = 0;
		freqMenu->rightText = Garmire::getScopeName(module->freqDriftScope);
		menu->addChild(freqMenu);

		ScopeSubmenu* phaseMenu = createMenuItem<ScopeSubmenu>("Phase Drift Target");
		phaseMenu->module = module;
		phaseMenu->driftType = 1;
		phaseMenu->rightText = Garmire::getScopeName(module->phaseDriftScope);
		menu->addChild(phaseMenu);

		ScopeSubmenu* ampMenu = createMenuItem<ScopeSubmenu>("Amplitude Drift Target");
		ampMenu->module = module;
		ampMenu->driftType = 2;
		ampMenu->rightText = Garmire::getScopeName(module->ampDriftScope);
		menu->addChild(ampMenu);
	}
};

// ── Model ───────────────────────────────────────────────────────────
Model* modelGarmire = createModel<Garmire, GarmireWidget>("Garmire");
