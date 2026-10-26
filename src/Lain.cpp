#include "plugin.hpp"
#include "dsp/ColorEngine.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────
//  Lain — Repeating 3-Segment RGB Line Generator (8 HP, v1.0.0)
//  Generate class module producing 3 discrete, solid-color RGB segments
//  (no gradients) driven by the standardized Garmire Coarse/Scale/Fine
//  oscillator timebase (Very Slow / LFO / VCO), external linear FM,
//  bidirectional hard sync, 3 Hue stops with live RGB halo rings,
//  3 proportional segment Length controls, 3 bipolar Blink Rate
//  controls (Off -> /128../1.5 -> 1:1 -> x1.5..x128 -> On), and a
//  live 1.50 mm 3-segment RGB preview bar.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Lain : Module {
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

		LEN_1_PARAM,
		LEN_2_PARAM,
		LEN_3_PARAM,

		BLINK_1_PARAM,
		BLINK_2_PARAM,
		BLINK_3_PARAM,

		FREQ_TRIM_PARAM,
		HUE_TRIM_PARAM,
		LEN_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		SYNC_INPUT,
		FREQ_CV_INPUT,
		FM_CV_INPUT,

		HUE_1_CV_INPUT,
		HUE_2_CV_INPUT,
		HUE_3_CV_INPUT,

		LEN_1_CV_INPUT,
		LEN_2_CV_INPUT,
		LEN_3_CV_INPUT,

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
	VoltageRange voltageRange = RANGE_0_10V;
	SkinTheme skinTheme = THEME_MEDIUM;
	dsp::BooleanTrigger rangeTrigger;

	// Live RGB values for the 3 Hue stop halo rings & segment preview bar
	float displayStopRgb[3][3] = {
		{1.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 1.0f}
	};

	// Live normalized segment fractions (sum to 1.0 when any length > 0)
	float displaySegFracs[3] = {1.f / 3.f, 1.f / 3.f, 1.f / 3.f};
	int displayBlinkSteps[3] = {0, 0, 0};
	uint64_t displayCycleCount = 0;

	struct VoiceState {
		float basePhase = 0.f;
		uint64_t cycleCount = 0;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
	};

	VoiceState voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Lain* getLain() {
			return dynamic_cast<Lain*>(module);
		}

		float getDisplayValue() override {
			Lain* mod = getLain();
			if (!mod) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (mod->paramQuantities.size() > Lain::FINE_PARAM) ? mod->params[Lain::FINE_PARAM].getValue() : 0.f;

			if (mod->rangeMode == Lain::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (mod->rangeMode == Lain::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Lain* mod = getLain();
			if (!mod) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (mod->rangeMode == Lain::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Lain* mod = getLain();
			if (!mod) return;

			float fine = (mod->paramQuantities.size() > Lain::FINE_PARAM) ? mod->params[Lain::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (mod->rangeMode == Lain::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (mod->rangeMode == Lain::RANGE_LFO) {
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
			Lain* mod = getLain();
			if (!mod) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (mod->rangeMode == Lain::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Lain* mod = getLain();
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
				isPeriod = (mod->rangeMode == Lain::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Lain* mod = getLain();
			if (!mod) return "";
			return (mod->rangeMode == Lain::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	// Bipolar Blink Rate ParamQuantity:
	// Discrete step index k in [-255, +255]:
	//   k = -255        : Off (0V / muted)
	//   k in [-254, -1] : /128.0, /127.5, ..., /1.5  (div = 1.0 + 0.5 * |k|)
	//   k = 0           : 1:1
	//   k in [+1, +254] : x1.5, x2.0, ..., x128.0    (mult = 1.0 + 0.5 * k)
	//   k = +255        : On (solid segment, default)
	struct BlinkRateParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int k = (int)std::round(getValue());
			if (k <= -255) {
				return "Off";
			} else if (k >= 255) {
				return "On";
			} else if (k == 0) {
				return "1:1";
			} else if (k < 0) {
				float div = 1.0f + 0.5f * (float)(-k);
				char buf[32];
				if (std::abs(div - std::round(div)) < 0.01f) {
					std::snprintf(buf, sizeof(buf), "/%.0f", div);
				} else {
					std::snprintf(buf, sizeof(buf), "/%.1f", div);
				}
				return std::string(buf);
			} else {
				float mult = 1.0f + 0.5f * (float)k;
				char buf[32];
				if (std::abs(mult - std::round(mult)) < 0.01f) {
					std::snprintf(buf, sizeof(buf), "x%.0f", mult);
				} else {
					std::snprintf(buf, sizeof(buf), "x%.1f", mult);
				}
				return std::string(buf);
			}
		}

		void setDisplayValueString(std::string s) override {
			std::string str = s;
			while (!str.empty() && std::isspace((unsigned char)str.front())) str.erase(str.begin());
			while (!str.empty() && std::isspace((unsigned char)str.back())) str.pop_back();
			if (str.empty()) return;

			std::string lowerStr = str;
			for (char& c : lowerStr) c = (char)std::tolower((unsigned char)c);

			if (lowerStr == "off") {
				setValue(-255.f);
				return;
			}
			if (lowerStr == "on") {
				setValue(255.f);
				return;
			}
			if (lowerStr == "1:1" || lowerStr == "1") {
				setValue(0.f);
				return;
			}

			bool isDiv = false;
			if (lowerStr[0] == '/') {
				isDiv = true;
				str = str.substr(1);
			} else if (lowerStr[0] == 'x' || lowerStr[0] == '*') {
				isDiv = false;
				str = str.substr(1);
			}

			char* endPtr = nullptr;
			float v = std::strtof(str.c_str(), &endPtr);
			if (endPtr == str.c_str()) return;

			if (v < 0.f) {
				isDiv = true;
				v = -v;
			}

			if (v < 1.25f) {
				setValue(0.f);
				return;
			}

			int k = (int)std::round((v - 1.0f) * 2.0f);
			k = std::max(1, std::min(254, k));
			setValue(isDiv ? (float)(-k) : (float)k);
		}
	};

	Lain() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Coarse Frequency (default 0.5477f matching Circe: 60.00 Hz LFO / 5.1s Very Slow / 619.22 Hz VCO)
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		// Row 2: Hue Stops (0° to 360°, defaulting to Red 0°, Green 120°, Blue 240°)
		configParam(HUE_1_PARAM, 0.f, 1.f, 0.0f, "Hue 1", "°", 0.f, 360.f);
		configParam(HUE_2_PARAM, 0.f, 1.f, 1.f / 3.f, "Hue 2", "°", 0.f, 360.f);
		configParam(HUE_3_PARAM, 0.f, 1.f, 2.f / 3.f, "Hue 3", "°", 0.f, 360.f);

		// Row 3: Relative Segment Lengths (0% to 100%, default 50% each = equal 1/3 segments)
		configParam(LEN_1_PARAM, 0.f, 1.f, 0.50f, "Length 1", "%", 0.f, 100.f);
		configParam(LEN_2_PARAM, 0.f, 1.f, 0.50f, "Length 2", "%", 0.f, 100.f);
		configParam(LEN_3_PARAM, 0.f, 1.f, 0.50f, "Length 3", "%", 0.f, 100.f);

		// Row 4: Blink Rates (-255 = Off, -254..-1 = /128../1.5, 0 = 1:1 default, +1..+254 = x1.5..x128, +255 = On)
		for (int i = 0; i < 3; ++i) {
			std::string name = "Blink rate " + std::to_string(i + 1);
			configParam<BlinkRateParamQuantity>(BLINK_1_PARAM + i, -255.f, 255.f, 0.f, name, "");
			paramQuantities[BLINK_1_PARAM + i]->snapEnabled = true;
		}

		// Zone 3: CV Attenuverters (Strict sentence case "<Parameter> CV depth")
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(HUE_TRIM_PARAM, -1.f, 1.f, 0.f, "Hue CV depth", "%", 0.f, 100.f);
		configParam(LEN_TRIM_PARAM, -1.f, 1.f, 0.f, "Length CV depth", "%", 0.f, 100.f);

		// Zone 4: Inputs (No redundant "input"/"in" words)
		configInput(SYNC_INPUT, "Sync");
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(FM_CV_INPUT, "FM CV");

		configInput(HUE_1_CV_INPUT, "Hue 1 CV");
		configInput(HUE_2_CV_INPUT, "Hue 2 CV");
		configInput(HUE_3_CV_INPUT, "Hue 3 CV");

		configInput(LEN_1_CV_INPUT, "Length 1 CV");
		configInput(LEN_2_CV_INPUT, "Length 2 CV");
		configInput(LEN_3_CV_INPUT, "Length 3 CV");

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
				return u * 5.0f;
			case RANGE_BIPOLAR_5V:
				return b * 5.0f;
			case RANGE_0_10V:
			default:
				return u * 10.0f;
			case RANGE_BIPOLAR_10V:
				return b * 10.0f;
		}
	}

	static inline bool evaluateBlinkActive(
		int blinkStep,
		uint64_t cycleCount,
		float basePhase,
		float segStart
	) {
		if (blinkStep <= -255) return false; // Off
		if (blinkStep >= 255) return true;   // On

		float relPhase = std::max(0.0f, basePhase - segStart);
		if (blinkStep == 0) {
			// 1:1 (1x master frequency aligned to segment start, or alternating half-cycle)
			double p = (double)relPhase * 1.0;
			p -= std::floor(p);
			return p < 0.5;
		} else if (blinkStep > 0) {
			// Multiplication of master frequency (x1.5 .. x128)
			double mult = 1.0 + 0.5 * (double)blinkStep;
			double p = (double)relPhase * mult;
			p -= std::floor(p);
			return p < 0.5;
		} else {
			// Division of master frequency (/1.5 .. /128)
			double div = 1.0 + 0.5 * (double)(-blinkStep);
			double totalP = ((double)cycleCount + (double)relPhase) / div;
			totalP -= std::floor(totalP);
			return totalP < 0.5;
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

		float len1Param = params[LEN_1_PARAM].getValue();
		float len2Param = params[LEN_2_PARAM].getValue();
		float len3Param = params[LEN_3_PARAM].getValue();

		int blinkSteps[3] = {
			(int)std::round(params[BLINK_1_PARAM].getValue()),
			(int)std::round(params[BLINK_2_PARAM].getValue()),
			(int)std::round(params[BLINK_3_PARAM].getValue())
		};

		float fTrim   = params[FREQ_TRIM_PARAM].getValue();
		float hueTrim = params[HUE_TRIM_PARAM].getValue();
		float lenTrim = params[LEN_TRIM_PARAM].getValue();

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
					vs.cycleCount++;
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

			// External Linear FM (only applied when FM CV input is connected)
			float fActual = fCarrier;
			if (fmConnected) {
				float extFmCv = inputs[FM_CV_INPUT].getPolyVoltage(c);
				float scaleHz = (rangeMode == RANGE_VERY_SLOW) ? (fCarrier * 2.f) : (rangeMode == RANGE_LFO ? 100.f : 500.f);
				float deltaF = (extFmCv / 5.f) * scaleHz;
				fActual = std::max(0.0001f, fCarrier + deltaF);
			}

			// Advance base phase
			vs.basePhase += fActual * args.sampleTime;

			// Emit sync pulse when voice completes a master cycle
			if (vs.basePhase >= 1.f) {
				uint64_t wraps = (uint64_t)std::floor(vs.basePhase);
				vs.cycleCount += wraps;
				vs.basePhase -= std::floor(vs.basePhase);
				if (!syncTriggered) {
					vs.syncPulse.trigger(1e-4f);
				}
			}

			// Compute modulated hues for the 3 stops (wrapped to [0, 1))
			float h1 = hue1Param + (inputs[HUE_1_CV_INPUT].getPolyVoltage(c) / 5.f) * hueTrim;
			float h2 = hue2Param + (inputs[HUE_2_CV_INPUT].getPolyVoltage(c) / 5.f) * hueTrim;
			float h3 = hue3Param + (inputs[HUE_3_CV_INPUT].getPolyVoltage(c) / 5.f) * hueTrim;
			h1 -= std::floor(h1);
			h2 -= std::floor(h2);
			h3 -= std::floor(h3);

			// Convert 3 Hue stops to RGB (100% Saturation, 100% Value)
			float stopRgb[3][3];
			garmire::color::hsvToRgb(h1 * 360.f, 1.f, 1.f, stopRgb[0][0], stopRgb[0][1], stopRgb[0][2]);
			garmire::color::hsvToRgb(h2 * 360.f, 1.f, 1.f, stopRgb[1][0], stopRgb[1][1], stopRgb[1][2]);
			garmire::color::hsvToRgb(h3 * 360.f, 1.f, 1.f, stopRgb[2][0], stopRgb[2][1], stopRgb[2][2]);

			// Compute modulated proportional lengths for the 3 segments ([0, 1])
			float w0 = clampf(len1Param + (inputs[LEN_1_CV_INPUT].getPolyVoltage(c) / 5.f) * lenTrim, 0.f, 1.f);
			float w1 = clampf(len2Param + (inputs[LEN_2_CV_INPUT].getPolyVoltage(c) / 5.f) * lenTrim, 0.f, 1.f);
			float w2 = clampf(len3Param + (inputs[LEN_3_CV_INPUT].getPolyVoltage(c) / 5.f) * lenTrim, 0.f, 1.f);
			float sumW = w0 + w1 + w2;

			float fracs[3] = {0.f, 0.f, 0.f};
			if (sumW > 1e-6f) {
				fracs[0] = w0 / sumW;
				fracs[1] = w1 / sumW;
				fracs[2] = w2 / sumW;
			}

			// Update halo rings & segment preview bar from voice 0
			if (c == 0) {
				for (int s = 0; s < 3; ++s) {
					displayStopRgb[s][0] = stopRgb[s][0];
					displayStopRgb[s][1] = stopRgb[s][1];
					displayStopRgb[s][2] = stopRgb[s][2];
					displaySegFracs[s] = fracs[s];
					displayBlinkSteps[s] = blinkSteps[s];
				}
				displayCycleCount = vs.cycleCount;
			}

			// Determine active discrete segment and blink state (no gradient interpolation)
			float outR = 0.f, outG = 0.f, outB = 0.f;
			if (sumW > 1e-6f) {
				float b0 = fracs[0];
				float b1 = fracs[0] + fracs[1];
				int seg = 2;
				float segStart = b1;
				if (vs.basePhase < b0 && fracs[0] > 1e-6f) {
					seg = 0;
					segStart = 0.f;
				} else if (vs.basePhase < b1 && fracs[1] > 1e-6f) {
					seg = 1;
					segStart = b0;
				} else if (fracs[2] <= 1e-6f) {
					if (fracs[1] > 1e-6f) {
						seg = 1;
						segStart = b0;
					} else {
						seg = 0;
						segStart = 0.f;
					}
				}

				if (evaluateBlinkActive(blinkSteps[seg], vs.cycleCount, vs.basePhase, segStart)) {
					outR = stopRgb[seg][0];
					outG = stopRgb[seg][1];
					outB = stopRgb[seg][2];
				}
			}

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
struct LainPort : app::SvgPort {
	LainPort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};

struct LainKnob : RoundKnob {
	LainKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct LainTrimpot : RoundKnob {
	LainTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct LainButton : app::SvgSwitch {
	LainButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── SCALE Range Button Illuminated Halo Ring ──
struct LainRangeLightRing : ModuleLightWidget {
	LainRangeLightRing() {
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
struct LainHueLightRing : LightWidget {
	Lain* module = nullptr;
	int stopIndex = 0;

	LainHueLightRing() {
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

// ── Live 1.50 mm 3-Segment RGB Preview Bar at Center Y = 56.25 mm ──
struct LainSegmentPreview : TransparentWidget {
	Lain* module = nullptr;

	LainSegmentPreview() {
		// Width = 80% of 40.64 mm = 32.512 mm, Height = 1.50 mm
		box.size = mm2px(Vec(32.512f, 1.500f));
	}

	void renderSegmentBar(const DrawArgs& args) {
		float defaultStops[3][3] = {
			{1.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 1.0f}
		};
		float defaultFracs[3] = {1.f / 3.f, 1.f / 3.f, 1.f / 3.f};
		int defaultBlinks[3] = {0, 0, 0};

		const float (*stops)[3] = module ? module->displayStopRgb : defaultStops;
		const float* fracs = module ? module->displaySegFracs : defaultFracs;
		const int* blinks = module ? module->displayBlinkSteps : defaultBlinks;
		uint64_t cyc = module ? module->displayCycleCount : 0;

		float w = box.size.x;
		float h = box.size.y;

		// Dark background backing when segments are off or blinking
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.0f, 0.0f, w, h);
		nvgFillColor(args.vg, nvgRGBA(0x18, 0x18, 0x18, 0xff));
		nvgFill(args.vg);

		const int numSlices = 120;
		float sliceW = w / (float)numSlices;
		float b0 = fracs[0];
		float b1 = fracs[0] + fracs[1];
		float sumF = fracs[0] + fracs[1] + fracs[2];
		if (sumF <= 1e-6f) return;

		for (int i = 0; i < numSlices; ++i) {
			float p = ((float)i + 0.5f) / (float)numSlices;
			int seg = 2;
			float segStart = b1;
			if (p < b0 && fracs[0] > 1e-6f) {
				seg = 0;
				segStart = 0.f;
			} else if (p < b1 && fracs[1] > 1e-6f) {
				seg = 1;
				segStart = b0;
			} else if (fracs[2] <= 1e-6f) {
				if (fracs[1] > 1e-6f) {
					seg = 1;
					segStart = b0;
				} else {
					seg = 0;
					segStart = 0.f;
				}
			}

			if (Lain::evaluateBlinkActive(blinks[seg], cyc, p, segStart)) {
				float x0 = i * sliceW;
				float drawW = sliceW + ((i < numSlices - 1) ? 0.4f : 0.0f);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, x0, 0.0f, drawW, h);
				nvgFillColor(args.vg, nvgRGBAf(stops[seg][0], stops[seg][1], stops[seg][2], 1.0f));
				nvgFill(args.vg);
			}
		}
	}

	void draw(const DrawArgs& args) override {
		renderSegmentBar(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			renderSegmentBar(args);
		}
		TransparentWidget::drawLayer(args, layer);
	}
};

struct LainWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Lain::SkinTheme currentTheme = Lain::THEME_MEDIUM;

	void setTheme(Lain::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Lain::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Lain_Light.svg");
					break;
				case Lain::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Lain_Dark.svg");
					break;
				case Lain::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Lain.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		Lain* module = dynamic_cast<Lain*>(this->module);
		if (module && module->skinTheme != currentTheme) {
			setTheme(module->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		Lain* module = dynamic_cast<Lain*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* oscRangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; ++i) {
			Lain::RangeMode mode = (Lain::RangeMode)i;
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
			"0V to 5V",
			"±5V",
			"0V to 10V (Default)",
			"±10V"
		};

		for (int i = 0; i < Lain::VOLTAGE_RANGE_LEN; ++i) {
			Lain::VoltageRange vr = (Lain::VoltageRange)i;
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
			Lain::SkinTheme theme = (Lain::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return module->skinTheme == theme; },
				[=]() {
					module->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	LainWidget(Lain* module) {
		setModule(module);
		panel = new SvgPanel();
		Lain::SkinTheme initTheme = module ? module->skinTheme : Lain::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Lain::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Lain_Light.svg");
				break;
			case Lain::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Lain_Dark.svg");
				break;
			case Lain::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Lain.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// 3-Column Grid (7.00, 20.32, 33.64 mm)
		const float col0 = 7.00f;
		const float col1 = 20.32f;
		const float col2 = 33.64f;

		// Row 1 (Y = 17.50 mm): FREQ, SCALE (Range button + halo ring), FINE
		addParam(createParamCentered<LainKnob>(mm2px(Vec(col0, 17.50f)), module, Lain::FREQ_PARAM));
		addChild(createLightCentered<LainRangeLightRing>(mm2px(Vec(col1, 17.50f)), module, Lain::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<LainButton>(mm2px(Vec(col1, 17.50f)), module, Lain::RANGE_PARAM));
		addParam(createParamCentered<LainKnob>(mm2px(Vec(col2, 17.50f)), module, Lain::FINE_PARAM));

		// Row 2 (Y = 30.50 mm): HUE 1, HUE 2, HUE 3 (Trimpots + RGB Halo Rings)
		const float hueCols[3] = {col0, col1, col2};
		for (int i = 0; i < 3; ++i) {
			LainHueLightRing* hueRing = new LainHueLightRing();
			hueRing->module = module;
			hueRing->stopIndex = i;
			hueRing->box.pos = mm2px(Vec(hueCols[i], 30.50f)).minus(hueRing->box.size.div(2.f));
			addChild(hueRing);
			addParam(createParamCentered<LainTrimpot>(mm2px(Vec(hueCols[i], 30.50f)), module, Lain::HUE_1_PARAM + i));
		}

		// Row 3 (Y = 41.25 mm): LENGTH 1, LENGTH 2, LENGTH 3 Trimpots
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col0, 41.25f)), module, Lain::LEN_1_PARAM));
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col1, 41.25f)), module, Lain::LEN_2_PARAM));
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col2, 41.25f)), module, Lain::LEN_3_PARAM));

		// Row 4 (Y = 50.75 mm): BLINK 1, BLINK 2, BLINK 3 Trimpots
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col0, 50.75f)), module, Lain::BLINK_1_PARAM));
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col1, 50.75f)), module, Lain::BLINK_2_PARAM));
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col2, 50.75f)), module, Lain::BLINK_3_PARAM));

		// Live 1.50 mm 3-Segment RGB Preview Bar at Center Y = 56.25 mm (X = 8.128 mm, W = 32.512 mm, H = 1.500 mm)
		LainSegmentPreview* segPreview = new LainSegmentPreview();
		segPreview->module = module;
		segPreview->box.pos = mm2px(Vec(8.128f, 55.500f));
		addChild(segPreview);

		// Zone 3 (Y = 64.25 mm): FREQ, HUE, LENGTH CV Attenuverter Trimpots
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col0, 64.25f)), module, Lain::FREQ_TRIM_PARAM));
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col1, 64.25f)), module, Lain::HUE_TRIM_PARAM));
		addParam(createParamCentered<LainTrimpot>(mm2px(Vec(col2, 64.25f)), module, Lain::LEN_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Jack Row 1 (Y = 78.00 mm): SYNC, FREQ, FM
		addInput(createInputCentered<LainPort>(mm2px(Vec(col0, 78.00f)), module, Lain::SYNC_INPUT));
		addInput(createInputCentered<LainPort>(mm2px(Vec(col1, 78.00f)), module, Lain::FREQ_CV_INPUT));
		addInput(createInputCentered<LainPort>(mm2px(Vec(col2, 78.00f)), module, Lain::FM_CV_INPUT));

		// Jack Row 2 (Y = 91.00 mm): H1, H2, H3
		addInput(createInputCentered<LainPort>(mm2px(Vec(col0, 91.00f)), module, Lain::HUE_1_CV_INPUT));
		addInput(createInputCentered<LainPort>(mm2px(Vec(col1, 91.00f)), module, Lain::HUE_2_CV_INPUT));
		addInput(createInputCentered<LainPort>(mm2px(Vec(col2, 91.00f)), module, Lain::HUE_3_CV_INPUT));

		// Jack Row 3 (Y = 104.00 mm): L1, L2, L3
		addInput(createInputCentered<LainPort>(mm2px(Vec(col0, 104.00f)), module, Lain::LEN_1_CV_INPUT));
		addInput(createInputCentered<LainPort>(mm2px(Vec(col1, 104.00f)), module, Lain::LEN_2_CV_INPUT));
		addInput(createInputCentered<LainPort>(mm2px(Vec(col2, 104.00f)), module, Lain::LEN_3_CV_INPUT));

		// Jack Row 4 (Y = 117.00 mm): R, G, B, SYNC (4 Columns: 6.07, 15.57, 25.07, 34.57 mm)
		addOutput(createOutputCentered<LainPort>(mm2px(Vec(6.07f, 117.00f)), module, Lain::R_OUTPUT));
		addOutput(createOutputCentered<LainPort>(mm2px(Vec(15.57f, 117.00f)), module, Lain::G_OUTPUT));
		addOutput(createOutputCentered<LainPort>(mm2px(Vec(25.07f, 117.00f)), module, Lain::B_OUTPUT));
		addOutput(createOutputCentered<LainPort>(mm2px(Vec(34.57f, 117.00f)), module, Lain::SYNC_OUTPUT));
	}
};

Model* modelLain = createModel<Lain, LainWidget>("Lain");
