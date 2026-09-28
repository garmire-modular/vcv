#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────
//  Daisy — Rhodonea Rose Curve & Limaçon Trajectory Generator (8 HP)
//  Generate class module producing mathematical Rhodonea rose curves
//  with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO),
//  carrier-normalized linear FM (with external override), harmonic petal count
//  (k = 1 to 12), center offset Limaçon morph (Cardioid / Limaçon), bipolar phase
//  shift (±180°), bipolar bulge (±100%), and bidirectional frequency sync
//  (Sync In & Sync Out).
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Daisy : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum RoseFuncMode {
		FUNC_COSINE = 0, // Cosine (Standard Rhodonea, axis-aligned)
		FUNC_SINE        // Sine (Rotated)
	};

	enum ParamId {
		FREQ_PARAM,
		FINE_PARAM,
		RANGE_PARAM,

		COROLLA_PARAM,
		OFFSET_PARAM,

		PHASE_PARAM,
		BULGE_PARAM,

		FREQ_TRIM_PARAM,
		COROLLA_TRIM_PARAM,
		OFFSET_TRIM_PARAM,

		FM_TRIM_PARAM,
		PHASE_TRIM_PARAM,
		BULGE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		FREQ_CV_INPUT,
		COROLLA_CV_INPUT,
		OFFSET_CV_INPUT,

		FM_CV_INPUT,
		PHASE_CV_INPUT,
		BULGE_CV_INPUT,

		SYNC_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		SYNC_OUTPUT,
		X_OUTPUT,
		Y_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_ORANGE,
		RANGE_LIGHT_PURPLE,

		LIGHTS_LEN
	};

	RangeMode rangeMode = RANGE_LFO;
	RoseFuncMode roseFuncMode = FUNC_COSINE;

	dsp::BooleanTrigger rangeTrigger;

	struct VoiceState {
		float basePhase = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
	};

	VoiceState voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Daisy* getDaisy() {
			return dynamic_cast<Daisy*>(module);
		}

		float getDisplayValue() override {
			Daisy* daisy = getDaisy();
			if (!daisy) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (daisy->paramQuantities.size() > Daisy::FINE_PARAM) ? daisy->params[Daisy::FINE_PARAM].getValue() : 0.f;

			if (daisy->rangeMode == Daisy::RANGE_VERY_SLOW) {
				// Period T in seconds: 600.0s to 0.1s
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (daisy->rangeMode == Daisy::RANGE_LFO) {
				// Frequency in Hz: 0.01 to 200.00 Hz
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				// Frequency in Hz: 150.00 to 2000.00 Hz
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse + fine * 0.10f, 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Daisy* daisy = getDaisy();
			if (!daisy) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (daisy->rangeMode == Daisy::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Daisy* daisy = getDaisy();
			if (!daisy) return;

			float fine = (daisy->paramQuantities.size() > Daisy::FINE_PARAM) ? daisy->params[Daisy::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (daisy->rangeMode == Daisy::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				// tCoarse = 600.0 * (0.1 / 600.0)^coarse
				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (daisy->rangeMode == Daisy::RANGE_LFO) {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 0.01f);
				float fCoarse = targetHz - fine * 20.0f;
				if (fCoarse <= 0.01f) {
					newCoarse = 0.0f;
				} else {
					newCoarse = std::sqrt((fCoarse - 0.01f) / 199.99f);
				}
			} else { // RANGE_VCO
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 100.f);
				float fineFactor = 1.0f + fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float fCoarse = targetHz / fineFactor;
				if (fCoarse <= 1.0f) fCoarse = 1.0f;

				// fCoarse = 150.0 * (2000.0 / 150.0)^coarse
				float ratio = fCoarse / 150.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(2000.0f / 150.0f);
			}

			setValue(clampf(newCoarse, 0.0f, 1.0f));
		}

		void setDisplayValue(float displayValue) override {
			Daisy* daisy = getDaisy();
			if (!daisy) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (daisy->rangeMode == Daisy::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Daisy* daisy = getDaisy();
			if (!daisy) {
				ParamQuantity::setDisplayValueString(s);
				return;
			}

			std::string str = s;
			while (!str.empty() && std::isspace((unsigned char)str.front())) {
				str.erase(str.begin());
			}
			while (!str.empty() && std::isspace((unsigned char)str.back())) {
				str.pop_back();
			}
			if (str.empty()) return;

			std::string lowerStr = str;
			for (char& c : lowerStr) {
				c = (char)std::tolower((unsigned char)c);
			}

			enum UnitType { UNIT_DEFAULT, UNIT_HZ, UNIT_KHZ, UNIT_MHZ, UNIT_SEC, UNIT_MS };
			UnitType unitType = UNIT_DEFAULT;

			if (lowerStr.length() >= 4 && lowerStr.substr(lowerStr.length() - 4) == "secs") {
				unitType = UNIT_SEC;
				str = str.substr(0, str.length() - 4);
			} else if (lowerStr.length() >= 3 && lowerStr.substr(lowerStr.length() - 3) == "sec") {
				unitType = UNIT_SEC;
				str = str.substr(0, str.length() - 3);
			} else if (lowerStr.length() >= 3 && lowerStr.substr(lowerStr.length() - 3) == "khz") {
				unitType = UNIT_KHZ;
				str = str.substr(0, str.length() - 3);
			} else if (lowerStr.length() >= 3 && lowerStr.substr(lowerStr.length() - 3) == "mhz") {
				unitType = UNIT_MHZ;
				str = str.substr(0, str.length() - 3);
			} else if (lowerStr.length() >= 2 && lowerStr.substr(lowerStr.length() - 2) == "hz") {
				unitType = UNIT_HZ;
				str = str.substr(0, str.length() - 2);
			} else if (lowerStr.length() >= 2 && lowerStr.substr(lowerStr.length() - 2) == "ms") {
				unitType = UNIT_MS;
				str = str.substr(0, str.length() - 2);
			} else if (!lowerStr.empty() && lowerStr.back() == 's') {
				unitType = UNIT_SEC;
				str.pop_back();
			} else if (!lowerStr.empty() && lowerStr.back() == 'k') {
				unitType = UNIT_KHZ;
				str.pop_back();
			}

			while (!str.empty() && std::isspace((unsigned char)str.back())) {
				str.pop_back();
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
				isPeriod = (daisy->rangeMode == Daisy::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Daisy* daisy = getDaisy();
			if (!daisy) return "";
			return (daisy->rangeMode == Daisy::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	struct CorollaParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float val = getValue();
			int nearestInt = (int)std::round(val);
			if (std::abs(val - nearestInt) < 0.05f && nearestInt >= 1 && nearestInt <= 12) {
				int petals = (nearestInt % 2 == 0) ? (2 * nearestInt) : nearestInt;
				char buf[32];
				if (petals == 1) {
					std::snprintf(buf, sizeof(buf), "%.1f (1 lobe)", val);
				} else {
					std::snprintf(buf, sizeof(buf), "%.1f (%d petals)", val, petals);
				}
				return std::string(buf);
			}
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", val);
			return std::string(buf);
		}

		void setDisplayValueString(std::string s) override {
			std::string lowerStr = s;
			for (char& c : lowerStr) c = (char)std::tolower((unsigned char)c);
			if (lowerStr.find("petal") != std::string::npos || lowerStr.find("lobe") != std::string::npos) {
				char* endPtr = nullptr;
				float num = std::strtof(lowerStr.c_str(), &endPtr);
				if (endPtr != lowerStr.c_str()) {
					int p = (int)std::round(num);
					if (p % 2 == 0 && p >= 4) {
						setValue(clampf((float)(p / 2), getMinValue(), getMaxValue()));
						return;
					} else if (p % 2 == 1 && p >= 1) {
						setValue(clampf((float)p, getMinValue(), getMaxValue()));
						return;
					}
				}
			}
			ParamQuantity::setDisplayValueString(s);
		}
	};

	Daisy() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Coarse Frequency (0.0 to 1.0)
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");

		// Fine Frequency (-1.0 to +1.0 for +/- 10% trim)
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		// Momentary Range Button
		configButton(RANGE_PARAM, "Range time-scale");

		// Corolla Multiplier (k = 1.0 to 12.0, default 4.0 producing 8 petals)
		configParam<CorollaParamQuantity>(COROLLA_PARAM, 1.f, 12.f, 4.f, "Corolla", "");

		// Center Offset Limaçon (-2.0 to +2.0, default 0.0 for pure Rhodonea rose)
		configParam(OFFSET_PARAM, -2.f, 2.f, 0.f, "Center offset", "", 0.f, 1.f);

		// Phase Shift (Bipolar ±180°)
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°", 0.f, 1.f);

		// Bulge (Bipolar ±100%)
		configParam(BULGE_PARAM, -1.f, 1.f, 0.f, "Bulge", "%", 0.f, 100.f);

		// CV Attenuverters (Mandatory naming per AGENTS.md Section 6.5.4)
		// Row 1 Attenuverters
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(COROLLA_TRIM_PARAM, -1.f, 1.f, 0.f, "Corolla CV depth", "%", 0.f, 100.f);
		configParam(OFFSET_TRIM_PARAM, -1.f, 1.f, 0.f, "Offset CV depth", "%", 0.f, 100.f);

		// Row 2 Attenuverters
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "Linear FM CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);
		configParam(BULGE_TRIM_PARAM, -1.f, 1.f, 0.f, "Bulge CV depth", "%", 0.f, 100.f);

		// Inputs: Row 1
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(COROLLA_CV_INPUT, "Corolla CV");
		configInput(OFFSET_CV_INPUT, "Offset CV");

		// Inputs: Row 2
		configInput(FM_CV_INPUT, "External FM");
		configInput(PHASE_CV_INPUT, "Phase CV");
		configInput(BULGE_CV_INPUT, "Bulge CV");

		// Sync
		configInput(SYNC_INPUT, "Sync");

		// Outputs: Row 3
		configOutput(SYNC_OUTPUT, "Sync");
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	float calculateBaseFrequency(float coarse, float fine) const {
		if (rangeMode == RANGE_VERY_SLOW) {
			// Period T in seconds: 600.0s to 0.1s
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float t = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			return 1.0f / t;
		} else if (rangeMode == RANGE_LFO) {
			// Frequency in Hz: 0.01 to 200.0 Hz
			float fCoarse = 0.01f + 199.99f * coarse * coarse;
			return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
		} else {
			// Frequency in Hz: 150.0 to 2000.0 Hz
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse + fine * 0.10f, 100.0f, 2500.0f);
		}
	}

	void process(const ProcessArgs& args) override {
		// Handle Range Button cycling (Very Slow -> LFO -> VCO)
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		// Update 3-Color Range LED (Yellow = Very Slow, Orange = LFO, Purple = VCO)
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		// Polyphony channel count
		int fCvCh  = inputs[FREQ_CV_INPUT].getChannels();
		int fmCvCh = inputs[FM_CV_INPUT].getChannels();
		int kCvCh  = inputs[COROLLA_CV_INPUT].getChannels();
		int aCvCh  = inputs[OFFSET_CV_INPUT].getChannels();
		int pCvCh  = inputs[PHASE_CV_INPUT].getChannels();
		int bCvCh  = inputs[BULGE_CV_INPUT].getChannels();
		int sCh    = inputs[SYNC_INPUT].getChannels();

		int numChannels = std::max({fCvCh, fmCvCh, kCvCh, aCvCh, pCvCh, bCvCh, sCh, 1});
		outputs[SYNC_OUTPUT].setChannels(numChannels);
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();

		float defaultF0 = calculateBaseFrequency(coarseParam, fineParam);

		float corollaParam = params[COROLLA_PARAM].getValue();
		float offsetParam  = params[OFFSET_PARAM].getValue();
		float phaseParam   = params[PHASE_PARAM].getValue();
		float bulgeParam   = params[BULGE_PARAM].getValue();

		float fTrim       = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim      = params[FM_TRIM_PARAM].getValue();
		float corollaTrim = params[COROLLA_TRIM_PARAM].getValue();
		float offsetTrim  = params[OFFSET_TRIM_PARAM].getValue();
		float phaseTrim   = params[PHASE_TRIM_PARAM].getValue();
		float bulgeTrim   = params[BULGE_TRIM_PARAM].getValue();

		bool fmConnected      = inputs[FM_CV_INPUT].isConnected();
		bool offsetConnected  = inputs[OFFSET_CV_INPUT].isConnected();
		bool bulgeConnected   = inputs[BULGE_CV_INPUT].isConnected();
		bool syncConnected    = inputs[SYNC_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
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

			// Linear FM modulation:
			// If External FM input is unpatched, normalize to carrier frequency (+/- N% of carrier frequency)
			// If External FM input is patched, external voltage overrides that behaviour
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

			// CV inputs: Offset normalizes from Corolla if unpatched
			float kCv = inputs[COROLLA_CV_INPUT].getPolyVoltage(c) / 5.f;
			float aCv = offsetConnected ? (inputs[OFFSET_CV_INPUT].getPolyVoltage(c) / 5.f) : kCv;

			float k = clampf(corollaParam + kCv * corollaTrim * 11.f, 0.1f, 24.f);
			float a = clampf(offsetParam + aCv * offsetTrim * 2.f, -4.f, 4.f);

			// Advance base phase
			vs.basePhase += fActual * args.sampleTime;

			// Emit sync pulse when voice completes a master cycle
			if (!syncTriggered && vs.basePhase >= 1.f) {
				vs.syncPulse.trigger(1e-4f);
			}

			// Wrap base phase to [0.0, 1.0)
			vs.basePhase -= std::floor(vs.basePhase);

			// Phase shift modulation (bipolar ±180°)
			float pCv = inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float phaseDeg = clampf(phaseParam + pCv * phaseTrim * 180.f, -180.f, 180.f);
			float phaseRad = phaseDeg * (float)(M_PI / 180.0);

			// Polar angle theta in [0, 2*pi)
			float theta = 2.f * (float)M_PI * vs.basePhase;

			// Rhodonea / Limaçon radius calculation with harmonic phase offset
			float rTerm = (roseFuncMode == FUNC_COSINE) ? std::cos(k * theta + phaseRad) : std::sin(k * theta + phaseRad);
			float r = a + rTerm;

			// Normalization scale factor to fit within standard Eurorack 5V bounds
			float maxExpectedRadius = std::max(1.0f, 1.0f + std::fabs(a));
			float scale = 1.0f / maxExpectedRadius;

			// Convert polar trajectory to orthogonal Cartesian X and Y
			float rawX = scale * r * std::cos(theta);
			float rawY = scale * r * std::sin(theta);

			// Bulge modulation (bipolar ±100%): Bulge normalizes from Phase CV if unpatched
			float bCv = bulgeConnected ? (inputs[BULGE_CV_INPUT].getPolyVoltage(c) / 5.f) : pCv;
			float bulgeVal = clampf(bulgeParam + bCv * bulgeTrim, -1.f, 1.f);

			// Apply Bulge (Hardcoded Harmonograph Logarithmic Spiral with 2.72 depth)
			if (std::abs(bulgeVal) > 1e-4f) {
				float radiusNorm = std::sqrt(rawX * rawX + rawY * rawY);
				float dampFactor = 1.0f - bulgeVal * 2.72f * (1.0f - radiusNorm);
				rawX *= dampFactor;
				rawY *= dampFactor;
			}

			// Eurorack standard 10Vpp (±5V) normalized vector coordinates
			outputs[X_OUTPUT].setVoltage(clampf(rawX * 5.f, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(rawY * 5.f, -12.f, 12.f), c);

			// Sync output pulse
			outputs[SYNC_OUTPUT].setVoltage(vs.syncPulse.process(args.sampleTime) ? 10.f : 0.f, c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "roseFuncMode", json_integer((int)roseFuncMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) {
			rangeMode = (RangeMode)json_integer_value(rmJ);
		}
		json_t* rfmJ = json_object_get(rootJ, "roseFuncMode");
		if (rfmJ) {
			roseFuncMode = (RoseFuncMode)json_integer_value(rfmJ);
		}
	}
};

// ── Custom 3-Color Range Light Widget (Palette: #e1be6a, #40b0a6, #d35fb7) ──
template <typename TBase = GrayModuleLightWidget>
struct TDaisyRangeLight : TBase {
	TDaisyRangeLight() {
		// Range 0: Very Slow = #e1be6a (Warm Gold)
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff));
		// Range 1: LFO = #40b0a6 (Teal)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff));
		// Range 2: VCO = #d35fb7 (Magenta)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff));
	}
};
struct DaisyRangeLightWidget : SmallLight<TDaisyRangeLight<>> {};

struct DaisyWidget : ModuleWidget {
	DaisyWidget(Daisy* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Daisy.svg")));

		// 8 HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Row 1: FREQ & FINE Knobs (10.82 and 29.82 mm at Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 21.59)), module, Daisy::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 21.59)), module, Daisy::FINE_PARAM));

		// Row 1 Center: Range LED (15.50 mm) and Range Button (21.59 mm, horizontally aligned with knobs)
		addChild(createLightCentered<DaisyRangeLightWidget>(mm2px(Vec(20.32, 15.50)), module, Daisy::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<TL1105>(mm2px(Vec(20.32, 21.59)), module, Daisy::RANGE_PARAM));

		// Row 2: Module-Specific Parameter Knobs: COROLLA & OFFSET (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 37.00)), module, Daisy::COROLLA_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 37.00)), module, Daisy::OFFSET_PARAM));

		// Row 3: Phase & Bulge Knobs (Center Y = 52.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 52.50)), module, Daisy::PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 52.50)), module, Daisy::BULGE_PARAM));

		// Zone 3: CV Attenuverter Trimpots (3 Columns: 8.82, 20.32, 31.82 mm)
		// Row 1 Attenuverters: FREQ, COROLLA, OFFSET (Center Y = 70.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 70.00)), module, Daisy::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 70.00)), module, Daisy::COROLLA_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 70.00)), module, Daisy::OFFSET_TRIM_PARAM));

		// Row 2 Attenuverters: FM, PHASE, BULGE (Center Y = 79.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 79.50)), module, Daisy::FM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 79.50)), module, Daisy::PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 79.50)), module, Daisy::BULGE_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs): FREQ, COROLLA, OFFSET (Center Y = 94.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 94.50)), module, Daisy::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 94.50)), module, Daisy::COROLLA_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 94.50)), module, Daisy::OFFSET_CV_INPUT));

		// Row 2 (Inputs): FM, PHASE, BULGE (Center Y = 106.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 106.00)), module, Daisy::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 106.00)), module, Daisy::PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 106.00)), module, Daisy::BULGE_CV_INPUT));

		// Row 3 (Sync & Outputs): SYNC IN, X OUT, Y OUT, SYNC OUT (Center Y = 118.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.07, 118.00)), module, Daisy::SYNC_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.57, 118.00)), module, Daisy::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.07, 118.00)), module, Daisy::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(34.57, 118.00)), module, Daisy::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Daisy* module = dynamic_cast<Daisy*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; i++) {
			Daisy::RangeMode mode = (Daisy::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Harmonic Function"));

		menu->addChild(createCheckMenuItem("Cosine (Axis-aligned Rhodonea)", "",
			[=]() { return module->roseFuncMode == Daisy::FUNC_COSINE; },
			[=]() { module->roseFuncMode = Daisy::FUNC_COSINE; }
		));

		menu->addChild(createCheckMenuItem("Sine (Rotated)", "",
			[=]() { return module->roseFuncMode == Daisy::FUNC_SINE; },
			[=]() { module->roseFuncMode = Daisy::FUNC_SINE; }
		));
	}
};

Model* modelDaisy = createModel<Daisy, DaisyWidget>("Daisy");
