#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Randi — Multichannel Stochastic Voltage Generator (8 HP)
//  Generate class module featuring standardized Freq, Range, Fine controls
//  and 3 columns of stochastic / random voltages:
//  - Column 1: 8 unique stepped random voltages (RND 1..8)
//  - Column 2: 8 unique slewed voltages arranged in pairs:
//              1+2 = 20% slew, 3+4 = 40% slew, 5+6 = 60% slew, 7+8 = 80% slew
//  - Column 3: SYNC in, followed by 7 distributed random voltages based on
//              a four-band fixed filter concept:
//              LOW, LOW+LOW-MID, LOW-MID, LOW-MID+HIGH-MID, HIGH-MID,
//              HIGH-MID+HIGH, HIGH
//  Selectable output voltage ranges: 0..1V, ±1V, 0..5V, ±5V, 0..10V, ±10V.
// ─────────────────────────────────────────────────────────────────────

namespace {
inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}
} // namespace

struct Randi : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum VoltageRange {
		RANGE_0_1V = 0,
		RANGE_BIPOLAR_1V,
		RANGE_0_5V,
		RANGE_BIPOLAR_5V, // Default
		RANGE_0_10V,
		RANGE_BIPOLAR_10V,
		VOLTAGE_RANGE_LEN
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};

	enum ParamId {
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		SYNC_INPUT, // Row 0, Column 3
		INPUTS_LEN
	};

	enum OutputId {
		// Column 1: 8 Stepped Randoms
		RND_1_OUTPUT,
		RND_2_OUTPUT,
		RND_3_OUTPUT,
		RND_4_OUTPUT,
		RND_5_OUTPUT,
		RND_6_OUTPUT,
		RND_7_OUTPUT,
		RND_8_OUTPUT,

		// Column 2: 8 Slewed Randoms (Pairs: 1+2=20%, 3+4=40%, 5+6=60%, 7+8=80%)
		SLEW_20_A_OUTPUT,
		SLEW_20_B_OUTPUT,
		SLEW_40_A_OUTPUT,
		SLEW_40_B_OUTPUT,
		SLEW_60_A_OUTPUT,
		SLEW_60_B_OUTPUT,
		SLEW_80_A_OUTPUT,
		SLEW_80_B_OUTPUT,

		// Column 3: 7 Filtered Distributed Randoms (Row 1..7)
		BAND_LOW_OUTPUT,
		BAND_LO_LM_OUTPUT,
		BAND_LOMID_OUTPUT,
		BAND_LM_HM_OUTPUT,
		BAND_HIMID_OUTPUT,
		BAND_HM_HI_OUTPUT,
		BAND_HIGH_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_ORANGE,
		RANGE_LIGHT_PURPLE,
		LIGHTS_LEN
	};

	RangeMode rangeMode = RANGE_LFO;
	VoltageRange voltageRange = RANGE_BIPOLAR_5V;
	SkinTheme skinTheme = THEME_MEDIUM;

	dsp::BooleanTrigger rangeTrigger;
	dsp::SchmittTrigger syncTrigger;

	float phase = 0.0f; // Clock phase [0, 1)

	// PRNG State (xorshift32)
	uint32_t rngState = 123456789;
	float nextRandomFloat() {
		rngState ^= rngState << 13;
		rngState ^= rngState >> 17;
		rngState ^= rngState << 5;
		// Map to [-1.0, 1.0]
		return (float)((int32_t)rngState) / 2147483648.0f;
	}

	// 8 stepped random target values
	float steppedVal[8] = {};

	// 8 slewed random values and 8 individual stepped sources for column 2
	float slewedSourceVal[8] = {};
	float slewedCurrentVal[8] = {};

	// 4-band fixed filter bank state
	// Band 0: Low (e.g. lowpass ~40 Hz)
	// Band 1: Low-Mid (bandpass ~160 Hz)
	// Band 2: High-Mid (bandpass ~640 Hz)
	// Band 3: High (highpass ~2500 Hz)
	struct BiquadState {
		float z1 = 0.f;
		float z2 = 0.f;
		float b0 = 1.f, b1 = 0.f, b2 = 0.f;
		float a1 = 0.f, a2 = 0.f;

		void setLowpass(float fc, float sampleRate) {
			float w0 = 2.0f * M_PI * clampf(fc / sampleRate, 0.0001f, 0.45f);
			float cosw0 = std::cos(w0);
			float sinw0 = std::sin(w0);
			float alpha = sinw0 / (2.0f * 0.707f);
			float a0 = 1.0f + alpha;
			b0 = ((1.0f - cosw0) / 2.0f) / a0;
			b1 = (1.0f - cosw0) / a0;
			b2 = ((1.0f - cosw0) / 2.0f) / a0;
			a1 = (-2.0f * cosw0) / a0;
			a2 = (1.0f - alpha) / a0;
		}

		void setBandpass(float fc, float Q, float sampleRate) {
			float w0 = 2.0f * M_PI * clampf(fc / sampleRate, 0.0001f, 0.45f);
			float cosw0 = std::cos(w0);
			float sinw0 = std::sin(w0);
			float alpha = sinw0 / (2.0f * Q);
			float a0 = 1.0f + alpha;
			b0 = (sinw0 / 2.0f) / a0;
			b1 = 0.0f;
			b2 = (-sinw0 / 2.0f) / a0;
			a1 = (-2.0f * cosw0) / a0;
			a2 = (1.0f - alpha) / a0;
		}

		void setHighpass(float fc, float sampleRate) {
			float w0 = 2.0f * M_PI * clampf(fc / sampleRate, 0.0001f, 0.45f);
			float cosw0 = std::cos(w0);
			float sinw0 = std::sin(w0);
			float alpha = sinw0 / (2.0f * 0.707f);
			float a0 = 1.0f + alpha;
			b0 = ((1.0f + cosw0) / 2.0f) / a0;
			b1 = (-(1.0f + cosw0)) / a0;
			b2 = ((1.0f + cosw0) / 2.0f) / a0;
			a1 = (-2.0f * cosw0) / a0;
			a2 = (1.0f - alpha) / a0;
		}

		inline float process(float in) {
			float out = b0 * in + z1;
			z1 = b1 * in - a1 * out + z2;
			z2 = b2 * in - a2 * out;
			return out;
		}
	};

	BiquadState filterLow;
	BiquadState filterLowMid;
	BiquadState filterHighMid;
	BiquadState filterHigh;
	float lastSampleRate = -1.0f;

	float calculateBaseFrequency(float coarse, float fine) {
		if (rangeMode == RANGE_VERY_SLOW) {
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float period = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			return (period > 1e-6f) ? (1.0f / period) : 0.001f;
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + 199.99f * coarse * coarse;
			return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
		} else {
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
		}
	}

	struct FreqParamQuantity : ParamQuantity {
		Randi* getRandi() {
			return dynamic_cast<Randi*>(module);
		}

		float getDisplayValue() override {
			Randi* mod = getRandi();
			if (!mod) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (mod->paramQuantities.size() > Randi::FINE_PARAM) ? mod->params[Randi::FINE_PARAM].getValue() : 0.f;

			if (mod->rangeMode == Randi::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (mod->rangeMode == Randi::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Randi* mod = getRandi();
			if (!mod) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (mod->rangeMode == Randi::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}
	};

	Randi() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency", " Hz");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		configInput(SYNC_INPUT, "Sync");

		// Column 1: 8 Stepped Randoms
		for (int i = 0; i < 8; ++i) {
			configOutput(RND_1_OUTPUT + i, "Random " + std::to_string(i + 1));
		}

		// Column 2: 8 Slewed Randoms
		static const char* slewNames[8] = {
			"Slew 20% A", "Slew 20% B",
			"Slew 40% A", "Slew 40% B",
			"Slew 60% A", "Slew 60% B",
			"Slew 80% A", "Slew 80% B"
		};
		for (int i = 0; i < 8; ++i) {
			configOutput(SLEW_20_A_OUTPUT + i, slewNames[i]);
		}

		// Column 3: 7 Filtered Distributed Randoms
		configOutput(BAND_LOW_OUTPUT, "Low band random");
		configOutput(BAND_LO_LM_OUTPUT, "Low + low-mid band random");
		configOutput(BAND_LOMID_OUTPUT, "Low-mid band random");
		configOutput(BAND_LM_HM_OUTPUT, "Low-mid + high-mid band random");
		configOutput(BAND_HIMID_OUTPUT, "High-mid band random");
		configOutput(BAND_HM_HI_OUTPUT, "High-mid + high band random");
		configOutput(BAND_HIGH_OUTPUT, "High band random");
	}

	float scaleVoltage(float normBipolar) {
		switch (voltageRange) {
			case RANGE_0_1V:
				return (normBipolar * 0.5f + 0.5f) * 1.0f;
			case RANGE_BIPOLAR_1V:
				return normBipolar * 1.0f;
			case RANGE_0_5V:
				return (normBipolar * 0.5f + 0.5f) * 5.0f;
			case RANGE_BIPOLAR_5V:
			default:
				return normBipolar * 5.0f;
			case RANGE_0_10V:
				return (normBipolar * 0.5f + 0.5f) * 10.0f;
			case RANGE_BIPOLAR_10V:
				return normBipolar * 10.0f;
		}
	}

	void stepClock() {
		// New step for Column 1 (8 unique values)
		for (int i = 0; i < 8; ++i) {
			steppedVal[i] = nextRandomFloat();
		}
		// New target for Column 2 (8 unique sources)
		for (int i = 0; i < 8; ++i) {
			slewedSourceVal[i] = nextRandomFloat();
		}
	}

	void process(const ProcessArgs& args) override {
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		// Reconfigure filter bank if sample rate changed
		if (args.sampleRate != lastSampleRate) {
			lastSampleRate = args.sampleRate;
			filterLow.setLowpass(45.0f, args.sampleRate);
			filterLowMid.setBandpass(180.0f, 1.2f, args.sampleRate);
			filterHighMid.setBandpass(720.0f, 1.2f, args.sampleRate);
			filterHigh.setHighpass(2200.0f, args.sampleRate);
		}

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam = params[FINE_PARAM].getValue();
		float f0 = calculateBaseFrequency(coarseParam, fineParam);

		// Check external sync
		bool clocked = false;
		if (inputs[SYNC_INPUT].isConnected()) {
			if (syncTrigger.process(inputs[SYNC_INPUT].getVoltage(), 0.1f, 2.0f)) {
				phase = 0.0f;
				clocked = true;
			}
		}

		// Advance phase
		phase += f0 * args.sampleTime;
		if (phase >= 1.0f) {
			phase -= std::floor(phase);
			clocked = true;
		}

		if (clocked) {
			stepClock();
		}

		// Column 1 Outputs: 8 Stepped Randoms
		for (int i = 0; i < 8; ++i) {
			outputs[RND_1_OUTPUT + i].setVoltage(scaleVoltage(steppedVal[i]));
		}

		// Column 2 Outputs: 8 Slewed Randoms
		// Slew rates proportional to clock frequency f0:
		// Slew percentage s:
		// 1+2: 20% slew (time constant tau = 0.20 / f0)
		// 3+4: 40% slew (time constant tau = 0.40 / f0)
		// 5+6: 60% slew (time constant tau = 0.60 / f0)
		// 7+8: 80% slew (time constant tau = 0.80 / f0)
		static const float slewPcts[8] = { 0.20f, 0.20f, 0.40f, 0.40f, 0.60f, 0.60f, 0.80f, 0.80f };
		for (int i = 0; i < 8; ++i) {
			float tau = std::max(1e-4f, slewPcts[i] / std::max(1e-3f, f0));
			float lambda = 1.0f - std::exp(-args.sampleTime / tau);
			slewedCurrentVal[i] += lambda * (slewedSourceVal[i] - slewedCurrentVal[i]);
			outputs[SLEW_20_A_OUTPUT + i].setVoltage(scaleVoltage(slewedCurrentVal[i]));
		}

		// Column 3 Outputs: 7 Filtered Distributed Randoms
		// Continuous wideband noise excitation from fast PRNG
		float whiteNoise = nextRandomFloat();

		// Feed into 4-band fixed filter bank
		float lowSig = filterLow.process(whiteNoise) * 3.5f;
		float loMidSig = filterLowMid.process(whiteNoise) * 4.0f;
		float hiMidSig = filterHighMid.process(whiteNoise) * 4.0f;
		float highSig = filterHigh.process(whiteNoise) * 3.5f;

		// 7 Combinations:
		// 1. LOW
		float b1 = clampf(lowSig, -1.0f, 1.0f);
		// 2. LOW + LOW-MID
		float b2 = clampf(0.6f * (lowSig + loMidSig), -1.0f, 1.0f);
		// 3. LOW-MID
		float b3 = clampf(loMidSig, -1.0f, 1.0f);
		// 4. LOW-MID + HIGH-MID
		float b4 = clampf(0.6f * (loMidSig + hiMidSig), -1.0f, 1.0f);
		// 5. HIGH-MID
		float b5 = clampf(hiMidSig, -1.0f, 1.0f);
		// 6. HIGH-MID + HIGH
		float b6 = clampf(0.6f * (hiMidSig + highSig), -1.0f, 1.0f);
		// 7. HIGH
		float b7 = clampf(highSig, -1.0f, 1.0f);

		outputs[BAND_LOW_OUTPUT].setVoltage(scaleVoltage(b1));
		outputs[BAND_LO_LM_OUTPUT].setVoltage(scaleVoltage(b2));
		outputs[BAND_LOMID_OUTPUT].setVoltage(scaleVoltage(b3));
		outputs[BAND_LM_HM_OUTPUT].setVoltage(scaleVoltage(b4));
		outputs[BAND_HIMID_OUTPUT].setVoltage(scaleVoltage(b5));
		outputs[BAND_HM_HI_OUTPUT].setVoltage(scaleVoltage(b6));
		outputs[BAND_HIGH_OUTPUT].setVoltage(scaleVoltage(b7));
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) rangeMode = (RangeMode)json_integer_value(rmJ);

		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) voltageRange = (VoltageRange)json_integer_value(vrJ);

		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);
	}
};

// Custom hardware widgets per AGENTS.md Section 7.4
struct RandiPort : app::SvgPort {
	RandiPort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};

struct RandiKnob : RoundKnob {
	RandiKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct RandiTrimpot : RoundKnob {
	RandiTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct RandiButton : app::SvgSwitch {
	RandiButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// Range button illuminated indicator ring
struct RandiRangeLightRing : ModuleLightWidget {
	RandiRangeLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));
		addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff)); // Very Slow (Warm Gold)
		addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // LFO (Teal)
		addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff)); // VCO (Magenta)
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

struct RandiWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Randi::SkinTheme currentTheme = Randi::THEME_MEDIUM;

	void setTheme(Randi::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Randi::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Randi_Light.svg");
					break;
				case Randi::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Randi_Dark.svg");
					break;
				case Randi::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Randi.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		Randi* module = dynamic_cast<Randi*>(this->module);
		if (module && module->skinTheme != currentTheme) {
			setTheme(module->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		Randi* module = dynamic_cast<Randi*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Voltage Range"));

		static const char* rangeLabels[] = {
			"0V to 1V",
			"±1V",
			"0V to 5V",
			"±5V (Default)",
			"0V to 10V",
			"±10V"
		};

		for (int i = 0; i < Randi::VOLTAGE_RANGE_LEN; ++i) {
			Randi::VoltageRange vr = (Randi::VoltageRange)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->voltageRange == vr; },
				[=]() { module->voltageRange = vr; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));

		const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			Randi::SkinTheme theme = (Randi::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return module->skinTheme == theme; },
				[=]() {
					module->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	RandiWidget(Randi* module) {
		setModule(module);
		panel = new SvgPanel();
		Randi::SkinTheme initTheme = module ? module->skinTheme : Randi::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Randi::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Randi_Light.svg");
				break;
			case Randi::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Randi_Dark.svg");
				break;
			case Randi::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Randi.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		float col0 = 7.00f;
		float col1 = 20.32f;
		float col2 = 33.64f;

		// Row 1 Controls at Y = 17.50mm
		float ctrlY = 17.50f;
		addParam(createParamCentered<RandiKnob>(mm2px(Vec(col0, ctrlY)), module, Randi::FREQ_PARAM));
		addChild(createLightCentered<RandiRangeLightRing>(mm2px(Vec(col1, ctrlY)), module, Randi::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<RandiButton>(mm2px(Vec(col1, ctrlY)), module, Randi::RANGE_PARAM));
		addParam(createParamCentered<RandiTrimpot>(mm2px(Vec(col2, ctrlY)), module, Randi::FINE_PARAM));

		// 8 Jack Rows (Y = 26.00mm down to 117.00mm, pitch 13.00mm)
		float pitch = 13.00f;
		float bottomY = 117.00f;
		float jackRows[8];
		for (int i = 0; i < 8; ++i) {
			jackRows[i] = bottomY - (7 - i) * pitch;
		}

		// Row 0 (Top Jack Row at Y = 26.00mm):
		// Col 0: RND 1 Output
		// Col 1: Slew 20% A Output
		// Col 2: SYNC Input
		addOutput(createOutputCentered<RandiPort>(mm2px(Vec(col0, jackRows[0])), module, Randi::RND_1_OUTPUT));
		addOutput(createOutputCentered<RandiPort>(mm2px(Vec(col1, jackRows[0])), module, Randi::SLEW_20_A_OUTPUT));
		addInput(createInputCentered<RandiPort>(mm2px(Vec(col2, jackRows[0])), module, Randi::SYNC_INPUT));

		// Rows 1 to 7 (Y = 39.00mm to 117.00mm):
		// Col 0: RND 2..8
		// Col 1: SLEW 20% B .. 80% B
		// Col 2: 7 Filtered Distributed Randoms (LOW .. HIGH)
		for (int r = 1; r < 8; ++r) {
			float jy = jackRows[r];
			addOutput(createOutputCentered<RandiPort>(mm2px(Vec(col0, jy)), module, Randi::RND_1_OUTPUT + r));
			addOutput(createOutputCentered<RandiPort>(mm2px(Vec(col1, jy)), module, Randi::SLEW_20_A_OUTPUT + r));
			addOutput(createOutputCentered<RandiPort>(mm2px(Vec(col2, jy)), module, Randi::BAND_LOW_OUTPUT + (r - 1)));
		}
	}
};

Model* modelRandi = createModel<Randi, RandiWidget>("Randi");
