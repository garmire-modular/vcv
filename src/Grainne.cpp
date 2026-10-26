#include "plugin.hpp"
#include "dsp/ColorEngine.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────
//  Gráinne — Cyclic 3-Stop RGB Gradient Generator (8 HP)
//  Generate class module producing smooth, continuous 3-stop RGB color
//  gradients driven by the standardized Garmire Coarse/Scale/Fine
//  oscillator timebase (Very Slow / LFO / VCO), carrier-normalized or
//  external linear FM, bidirectional hard sync, 3 Hue stops with live
//  RGB halo rings (100% saturation & value), and 3 midpoint skew
//  controls with CV modulation.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Grainne : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,           // 0.01 - 200 Hz
		RANGE_VCO            // 150 Hz - 2 kHz
	};

	enum VoltageRange {
		RANGE_0_1V = 0,
		RANGE_BIPOLAR_1V,
		RANGE_0_5V,
		RANGE_BIPOLAR_5V,
		RANGE_0_10V,
		RANGE_BIPOLAR_10V,
		VOLTAGE_RANGE_LEN
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT = 1,
		THEME_DARK = 2
	};

	enum ParamId {
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,

		HUE_1_PARAM,
		HUE_2_PARAM,
		HUE_3_PARAM,

		MID_12_PARAM,
		MID_23_PARAM,
		MID_31_PARAM,

		FREQ_TRIM_PARAM,
		FM_TRIM_PARAM,
		HUE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		SYNC_INPUT,
		FREQ_CV_INPUT,
		FM_CV_INPUT,

		HUE_1_CV_INPUT,
		HUE_2_CV_INPUT,
		HUE_3_CV_INPUT,

		MID_12_CV_INPUT,
		MID_23_CV_INPUT,
		MID_31_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
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
	VoltageRange voltageRange = RANGE_0_5V;
	SkinTheme skinTheme = THEME_MEDIUM;
	dsp::BooleanTrigger rangeTrigger;

	// Live RGB values for the 3 Hue stop halo rings (default Red 0°, Green 120°, Blue 240°)
	float displayStopRgb[3][3] = {
		{1.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 1.0f}
	};

	struct VoiceState {
		float basePhase = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
	};

	VoiceState voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Grainne* getGrainne() {
			return dynamic_cast<Grainne*>(module);
		}

		float getDisplayValue() override {
			Grainne* mod = getGrainne();
			if (!mod) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (mod->paramQuantities.size() > Grainne::FINE_PARAM) ? mod->params[Grainne::FINE_PARAM].getValue() : 0.f;

			if (mod->rangeMode == Grainne::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (mod->rangeMode == Grainne::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Grainne* mod = getGrainne();
			if (!mod) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (mod->rangeMode == Grainne::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Grainne* mod = getGrainne();
			if (!mod) return;

			float fine = (mod->paramQuantities.size() > Grainne::FINE_PARAM) ? mod->params[Grainne::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (mod->rangeMode == Grainne::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (mod->rangeMode == Grainne::RANGE_LFO) {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 0.01f);
				float fCoarse = targetHz - fine * 20.0f;
				if (fCoarse <= 0.01f) {
					newCoarse = 0.0f;
				} else {
					newCoarse = std::sqrt((fCoarse - 0.01f) / 199.99f);
				}
			} else {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 100.f);
				float fineFactor = 1.0f + fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float fCoarse = targetHz / fineFactor;
				if (fCoarse <= 1.0f) fCoarse = 1.0f;

				float ratio = fCoarse / 150.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(2000.0f / 150.0f);
			}

			setValue(clampf(newCoarse, 0.0f, 1.0f));
		}

		void setDisplayValue(float displayValue) override {
			Grainne* mod = getGrainne();
			if (!mod) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (mod->rangeMode == Grainne::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Grainne* mod = getGrainne();
			if (!mod) {
				ParamQuantity::setDisplayValueString(s);
				return;
			}

			std::string str = s;
			std::string lowerStr = s;
			for (char& c : lowerStr) c = (char)std::tolower((unsigned char)c);

			enum UnitType { UNIT_NONE, UNIT_HZ, UNIT_KHZ, UNIT_MHZ, UNIT_SEC, UNIT_MS };
			UnitType unitType = UNIT_NONE;

			if (lowerStr.find("khz") != std::string::npos) {
				unitType = UNIT_KHZ;
			} else if (lowerStr.find("mhz") != std::string::npos) {
				unitType = UNIT_MHZ;
			} else if (lowerStr.find("hz") != std::string::npos) {
				unitType = UNIT_HZ;
			} else if (lowerStr.find("ms") != std::string::npos) {
				unitType = UNIT_MS;
			} else if (lowerStr.find("s") != std::string::npos || lowerStr.find("sec") != std::string::npos) {
				unitType = UNIT_SEC;
			}

			char* endPtr = nullptr;
			float rawVal = std::strtof(str.c_str(), &endPtr);
			if (endPtr == str.c_str()) return;

			bool isPeriod;
			float finalVal;

			if (unitType == UNIT_SEC) {
				isPeriod = true;
				finalVal = rawVal;
			} else if (unitType == UNIT_MS) {
				isPeriod = true;
				finalVal = rawVal * 0.001f;
			} else if (unitType == UNIT_HZ) {
				isPeriod = false;
				finalVal = rawVal;
			} else if (unitType == UNIT_KHZ) {
				isPeriod = false;
				finalVal = rawVal * 1000.0f;
			} else if (unitType == UNIT_MHZ) {
				isPeriod = false;
				finalVal = rawVal * 1000000.0f;
			} else {
				isPeriod = (mod->rangeMode == Grainne::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Grainne* mod = getGrainne();
			if (!mod) return "";
			return (mod->rangeMode == Grainne::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	Grainne() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Coarse Frequency, Scale Range Button, Fine Frequency
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.2236f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		// Row 2: Hue Stops (0° to 360°, defaulting to Red 0°, Green 120°, Blue 240°)
		configParam(HUE_1_PARAM, 0.f, 1.f, 0.0f, "Hue 1", "°", 0.f, 360.f);
		configParam(HUE_2_PARAM, 0.f, 1.f, 1.f / 3.f, "Hue 2", "°", 0.f, 360.f);
		configParam(HUE_3_PARAM, 0.f, 1.f, 2.f / 3.f, "Hue 3", "°", 0.f, 360.f);

		// Row 3: Midpoint distribution controls (5% to 95%, default 50%)
		configParam(MID_12_PARAM, 0.05f, 0.95f, 0.50f, "1-2 midpoint", "%", 0.f, 100.f);
		configParam(MID_23_PARAM, 0.05f, 0.95f, 0.50f, "2-3 midpoint", "%", 0.f, 100.f);
		configParam(MID_31_PARAM, 0.05f, 0.95f, 0.50f, "3-1 midpoint", "%", 0.f, 100.f);

		// Zone 3: CV Attenuverters (Strict sentence case "<Parameter> CV depth")
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "FM CV depth", "%", 0.f, 100.f);
		configParam(HUE_TRIM_PARAM, -1.f, 1.f, 0.f, "Hue CV depth", "%", 0.f, 100.f);

		// Zone 4: Inputs (No redundant "input"/"in" words)
		configInput(SYNC_INPUT, "Sync");
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(FM_CV_INPUT, "FM CV");

		configInput(HUE_1_CV_INPUT, "Hue 1 CV");
		configInput(HUE_2_CV_INPUT, "Hue 2 CV");
		configInput(HUE_3_CV_INPUT, "Hue 3 CV");

		configInput(MID_12_CV_INPUT, "1-2 midpoint CV");
		configInput(MID_23_CV_INPUT, "2-3 midpoint CV");
		configInput(MID_31_CV_INPUT, "3-1 midpoint CV");

		// Zone 4: Outputs (No redundant "output"/"out" words)
		configOutput(R_OUTPUT, "Red");
		configOutput(G_OUTPUT, "Green");
		configOutput(B_OUTPUT, "Blue");
		configOutput(SYNC_OUTPUT, "Sync");
	}

	float calculateBaseFrequency(float coarse, float fine) const {
		if (rangeMode == RANGE_VERY_SLOW) {
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float t = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			return 1.0f / t;
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + 199.99f * coarse * coarse;
			return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
		} else {
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
		}
	}

	float scaleVoltage(float normUnipolar) const {
		// normUnipolar is in [0.0, 1.0]
		float u = clampf(normUnipolar, 0.0f, 1.0f);
		float b = 2.0f * u - 1.0f;
		switch (voltageRange) {
			case RANGE_0_1V:
				return u * 1.0f;
			case RANGE_BIPOLAR_1V:
				return b * 1.0f;
			case RANGE_0_5V:
			default:
				return u * 5.0f;
			case RANGE_BIPOLAR_5V:
				return b * 5.0f;
			case RANGE_0_10V:
				return u * 10.0f;
			case RANGE_BIPOLAR_10V:
				return b * 10.0f;
		}
	}

	void process(const ProcessArgs& args) override {
		// Handle Range Button cycling (Very Slow -> LFO -> VCO)
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		// Update 3-Color Range Halo LED
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		// Polyphony channel count across all inputs
		int numChannels = 1;
		for (int i = 0; i < INPUTS_LEN; ++i) {
			numChannels = std::max(numChannels, inputs[i].getChannels());
		}

		outputs[R_OUTPUT].setChannels(numChannels);
		outputs[G_OUTPUT].setChannels(numChannels);
		outputs[B_OUTPUT].setChannels(numChannels);
		outputs[SYNC_OUTPUT].setChannels(numChannels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();
		float defaultF0   = calculateBaseFrequency(coarseParam, fineParam);

		float hue1Param = params[HUE_1_PARAM].getValue();
		float hue2Param = params[HUE_2_PARAM].getValue();
		float hue3Param = params[HUE_3_PARAM].getValue();

		float mid12Param = params[MID_12_PARAM].getValue();
		float mid23Param = params[MID_23_PARAM].getValue();
		float mid31Param = params[MID_31_PARAM].getValue();

		float fTrim   = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim  = params[FM_TRIM_PARAM].getValue();
		float hueTrim = params[HUE_TRIM_PARAM].getValue();

		bool fmConnected   = inputs[FM_CV_INPUT].isConnected();
		bool syncConnected = inputs[SYNC_INPUT].isConnected();

		for (int c = 0; c < numChannels; ++c) {
			VoiceState& vs = voices[c];

			// External Hard Frequency Sync
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

			// Base Frequency: track sync frequency if locked, else internal reference
			float f0 = (syncConnected && vs.syncPeriod > 0.f) ? (1.f / vs.syncPeriod) : defaultF0;

			// Frequency 1V/Oct modulation
			float freqCv = inputs[FREQ_CV_INPUT].getPolyVoltage(c);
			float fCarrier = f0 * std::pow(2.f, freqCv * fTrim);

			// Linear FM modulation (matches Circe standard)
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

			// Emit sync pulse when voice completes a master cycle
			if (!syncTriggered && vs.basePhase >= 1.f) {
				vs.syncPulse.trigger(1e-4f);
			}

			// Wrap base phase to [0.0, 1.0)
			vs.basePhase -= std::floor(vs.basePhase);

			// Compute modulated hues for the 3 stops (wrapped to [0, 1))
			float h1 = hue1Param + (inputs[HUE_1_CV_INPUT].getPolyVoltage(c) / 5.f) * hueTrim;
			float h2 = hue2Param + (inputs[HUE_2_CV_INPUT].getPolyVoltage(c) / 5.f) * hueTrim;
			float h3 = hue3Param + (inputs[HUE_3_CV_INPUT].getPolyVoltage(c) / 5.f) * hueTrim;
			h1 -= std::floor(h1);
			h2 -= std::floor(h2);
			h3 -= std::floor(h3);

			// Convert 3 Hue stops to RGB (100% Saturation, 100% Value)
			float rStop[3], gStop[3], bStop[3];
			garmire::color::hsvToRgb(h1 * 360.f, 1.f, 1.f, rStop[0], gStop[0], bStop[0]);
			garmire::color::hsvToRgb(h2 * 360.f, 1.f, 1.f, rStop[1], gStop[1], bStop[1]);
			garmire::color::hsvToRgb(h3 * 360.f, 1.f, 1.f, rStop[2], gStop[2], bStop[2]);

			// Update halo ring display from voice 0
			if (c == 0) {
				for (int s = 0; s < 3; ++s) {
					displayStopRgb[s][0] = rStop[s];
					displayStopRgb[s][1] = gStop[s];
					displayStopRgb[s][2] = bStop[s];
				}
			}

			// Compute modulated midpoints for the 3 segments ([0.05, 0.95])
			float m12 = clampf(mid12Param + (inputs[MID_12_CV_INPUT].getPolyVoltage(c) / 5.f) * 0.45f, 0.05f, 0.95f);
			float m23 = clampf(mid23Param + (inputs[MID_23_CV_INPUT].getPolyVoltage(c) / 5.f) * 0.45f, 0.05f, 0.95f);
			float m31 = clampf(mid31Param + (inputs[MID_31_CV_INPUT].getPolyVoltage(c) / 5.f) * 0.45f, 0.05f, 0.95f);

			// Determine active segment (0: Stop 1->2, 1: Stop 2->3, 2: Stop 3->1)
			float scaledPhase = vs.basePhase * 3.0f;
			int seg = (int)scaledPhase;
			if (seg > 2) seg = 2;
			float u = scaledPhase - (float)seg; // local phase in [0, 1)

			int idxA = seg;
			int idxB = (seg + 1) % 3;
			float m = (seg == 0) ? m12 : ((seg == 1) ? m23 : m31);

			// Smooth rational Mobius midpoint skew:
			// t(0, m) = 0, t(m, m) = 0.5, t(1, m) = 1
			float denom = (1.0f - 2.0f * m) * u + m;
			float t = (std::abs(denom) > 1e-6f) ? (((1.0f - m) * u) / denom) : u;
			t = clampf(t, 0.0f, 1.0f);

			// Interpolate RGB between Stop A and Stop B
			float outR = (1.0f - t) * rStop[idxA] + t * rStop[idxB];
			float outG = (1.0f - t) * gStop[idxA] + t * gStop[idxB];
			float outB = (1.0f - t) * bStop[idxA] + t * bStop[idxB];

			outputs[R_OUTPUT].setVoltage(scaleVoltage(outR), c);
			outputs[G_OUTPUT].setVoltage(scaleVoltage(outG), c);
			outputs[B_OUTPUT].setVoltage(scaleVoltage(outB), c);
			outputs[SYNC_OUTPUT].setVoltage(vs.syncPulse.process(args.sampleTime) ? 10.f : 0.f, c);
		}
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

// ── Custom Hardware Widgets per AGENTS.md Section 7.4 ──
struct GrainnePort : app::SvgPort {
	GrainnePort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};

struct GrainneKnob : RoundKnob {
	GrainneKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct GrainneTrimpot : RoundKnob {
	GrainneTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct GrainneButton : app::SvgSwitch {
	GrainneButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── SCALE Range Button Illuminated Halo Ring ──
struct GrainneRangeLightRing : ModuleLightWidget {
	GrainneRangeLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));
		addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff)); // State 0: Warm Gold (Very Slow)
		addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // State 1: Teal (LFO)
		addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff)); // State 2: Magenta (VCO)
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

// ── Hue Stop Illuminated Halo Ring (Same R = 4.12 mm, W = 1.40 mm geometry) ──
struct GrainneHueLightRing : LightWidget {
	Grainne* module = nullptr;
	int stopIndex = 0;

	GrainneHueLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));
	}

	void step() override {
		float r = 1.0f, g = 0.0f, b = 0.0f;
		if (module) {
			r = module->displayStopRgb[stopIndex][0];
			g = module->displayStopRgb[stopIndex][1];
			b = module->displayStopRgb[stopIndex][2];
		} else {
			if (stopIndex == 0) { r = 1.0f; g = 0.0f; b = 0.0f; }
			else if (stopIndex == 1) { r = 0.0f; g = 1.0f; b = 0.0f; }
			else { r = 0.0f; g = 0.0f; b = 1.0f; }
		}
		color = nvgRGBAf(r, g, b, 1.0f);
		LightWidget::step();
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

struct GrainneWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Grainne::SkinTheme currentTheme = Grainne::THEME_MEDIUM;

	void setTheme(Grainne::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Grainne::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Grainne_Light.svg");
					break;
				case Grainne::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Grainne_Dark.svg");
					break;
				case Grainne::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Grainne.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		Grainne* module = dynamic_cast<Grainne*>(this->module);
		if (module && module->skinTheme != currentTheme) {
			setTheme(module->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		Grainne* module = dynamic_cast<Grainne*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* oscRangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; ++i) {
			Grainne::RangeMode mode = (Grainne::RangeMode)i;
			menu->addChild(createCheckMenuItem(oscRangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Voltage Range"));

		static const char* voltRangeLabels[] = {
			"0V to 1V",
			"±1V",
			"0V to 5V (Default)",
			"±5V",
			"0V to 10V",
			"±10V"
		};

		for (int i = 0; i < Grainne::VOLTAGE_RANGE_LEN; ++i) {
			Grainne::VoltageRange vr = (Grainne::VoltageRange)i;
			menu->addChild(createCheckMenuItem(voltRangeLabels[i], "",
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
			Grainne::SkinTheme theme = (Grainne::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return module->skinTheme == theme; },
				[=]() {
					module->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	GrainneWidget(Grainne* module) {
		setModule(module);
		panel = new SvgPanel();
		Grainne::SkinTheme initTheme = module ? module->skinTheme : Grainne::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Grainne::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Grainne_Light.svg");
				break;
			case Grainne::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Grainne_Dark.svg");
				break;
			case Grainne::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Grainne.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// 3-Column Grid (7.00, 20.32, 33.64 mm)
		const float col0 = 7.00f;
		const float col1 = 20.32f;
		const float col2 = 33.64f;

		// Row 1 (Y = 18.00 mm): FREQ, SCALE (Range button + halo ring), FINE
		addParam(createParamCentered<GrainneKnob>(mm2px(Vec(col0, 18.00f)), module, Grainne::FREQ_PARAM));
		addChild(createLightCentered<GrainneRangeLightRing>(mm2px(Vec(col1, 18.00f)), module, Grainne::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<GrainneButton>(mm2px(Vec(col1, 18.00f)), module, Grainne::RANGE_PARAM));
		addParam(createParamCentered<GrainneKnob>(mm2px(Vec(col2, 18.00f)), module, Grainne::FINE_PARAM));

		// Row 2 (Y = 33.50 mm): HUE 1, HUE 2, HUE 3 (Trimpots + RGB Halo Rings)
		const float hueCols[3] = {col0, col1, col2};
		for (int i = 0; i < 3; ++i) {
			GrainneHueLightRing* hueRing = new GrainneHueLightRing();
			hueRing->module = module;
			hueRing->stopIndex = i;
			hueRing->box.pos = mm2px(Vec(hueCols[i], 33.50f)).minus(hueRing->box.size.div(2.f));
			addChild(hueRing);
			addParam(createParamCentered<GrainneTrimpot>(mm2px(Vec(hueCols[i], 33.50f)), module, Grainne::HUE_1_PARAM + i));
		}

		// Row 3 (Y = 46.50 mm): 1-2, 2-3, 3-1 Midpoint Trimpots
		addParam(createParamCentered<GrainneTrimpot>(mm2px(Vec(col0, 46.50f)), module, Grainne::MID_12_PARAM));
		addParam(createParamCentered<GrainneTrimpot>(mm2px(Vec(col1, 46.50f)), module, Grainne::MID_23_PARAM));
		addParam(createParamCentered<GrainneTrimpot>(mm2px(Vec(col2, 46.50f)), module, Grainne::MID_31_PARAM));

		// Zone 3 (Y = 62.50 mm): FREQ, FM, HUE CV Attenuverter Trimpots
		addParam(createParamCentered<GrainneTrimpot>(mm2px(Vec(col0, 62.50f)), module, Grainne::FREQ_TRIM_PARAM));
		addParam(createParamCentered<GrainneTrimpot>(mm2px(Vec(col1, 62.50f)), module, Grainne::FM_TRIM_PARAM));
		addParam(createParamCentered<GrainneTrimpot>(mm2px(Vec(col2, 62.50f)), module, Grainne::HUE_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Jack Row 1 (Y = 78.00 mm): SYNC, FREQ, FM
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col0, 78.00f)), module, Grainne::SYNC_INPUT));
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col1, 78.00f)), module, Grainne::FREQ_CV_INPUT));
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col2, 78.00f)), module, Grainne::FM_CV_INPUT));

		// Jack Row 2 (Y = 91.00 mm): H1, H2, H3
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col0, 91.00f)), module, Grainne::HUE_1_CV_INPUT));
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col1, 91.00f)), module, Grainne::HUE_2_CV_INPUT));
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col2, 91.00f)), module, Grainne::HUE_3_CV_INPUT));

		// Jack Row 3 (Y = 104.00 mm): 1-2, 2-3, 3-1
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col0, 104.00f)), module, Grainne::MID_12_CV_INPUT));
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col1, 104.00f)), module, Grainne::MID_23_CV_INPUT));
		addInput(createInputCentered<GrainnePort>(mm2px(Vec(col2, 104.00f)), module, Grainne::MID_31_CV_INPUT));

		// Jack Row 4 (Y = 117.00 mm): R, G, B, SYNC (4 Columns: 6.07, 15.57, 25.07, 34.57 mm)
		addOutput(createOutputCentered<GrainnePort>(mm2px(Vec(6.07f, 117.00f)), module, Grainne::R_OUTPUT));
		addOutput(createOutputCentered<GrainnePort>(mm2px(Vec(15.57f, 117.00f)), module, Grainne::G_OUTPUT));
		addOutput(createOutputCentered<GrainnePort>(mm2px(Vec(25.07f, 117.00f)), module, Grainne::B_OUTPUT));
		addOutput(createOutputCentered<GrainnePort>(mm2px(Vec(34.57f, 117.00f)), module, Grainne::SYNC_OUTPUT));
	}
};

Model* modelGrainne = createModel<Grainne, GrainneWidget>("Grainne");
