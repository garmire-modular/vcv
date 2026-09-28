#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
inline float clampf(float v, float lo, float hi) {
	return std::max(lo, std::min(v, hi));
}
} // namespace

struct Polly : Module {
	enum ParamIds {
		// Row 1: Frequency & Primary Geometry
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,
		SIDES_PARAM,
		ANGLE_PARAM,
		TEETH_PARAM,
		OFFSET_PARAM,
		TWIST_PARAM,

		// Row 2: Vertex Bias, Curves & Ripple
		DISTRIB_PARAM,
		PATTERN_PARAM,
		FILLET_PARAM,
		BOW_PARAM,
		RIP_AMT_PARAM,
		RIP_ORD_PARAM,
		RIP_PHS_PARAM,
		RIP_SHP_PARAM,

		// Row 3: Symmetry, Warp, Traversal & Bulge
		SYMM_PARAM,
		ALTERN_PARAM,
		WARP_PARAM,
		TRAV_PARAM,
		DWELL_PARAM,
		CURVE_PARAM,
		PHASE_PARAM,
		BULGE_PARAM,

		// Zone 3: CV Attenuverters (Trimpots)
		// Row 1 Attenuverters
		FREQ_TRIM_PARAM,
		SIDES_TRIM_PARAM,
		ANGLE_TRIM_PARAM,
		TEETH_TRIM_PARAM,
		OFFSET_TRIM_PARAM,
		TWIST_TRIM_PARAM,
		FILLET_TRIM_PARAM,
		BOW_TRIM_PARAM,

		// Row 2 Attenuverters
		FM_TRIM_PARAM,
		DISTRIB_TRIM_PARAM,
		RIPPLE_TRIM_PARAM,
		SYMM_TRIM_PARAM,
		ALTERN_TRIM_PARAM,
		WARP_TRIM_PARAM,
		PHASE_TRIM_PARAM,
		BULGE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputIds {
		// Jack Row 1 (Inputs)
		FREQ_CV_INPUT,
		SIDES_CV_INPUT,
		ANGLE_CV_INPUT,
		TEETH_CV_INPUT,
		OFFSET_CV_INPUT,
		TWIST_CV_INPUT,
		FILLET_CV_INPUT,
		BOW_CV_INPUT,

		// Jack Row 2 (Inputs)
		FM_CV_INPUT,
		DISTRIB_CV_INPUT,
		RIPPLE_CV_INPUT,
		SYMM_CV_INPUT,
		ALTERN_CV_INPUT,
		WARP_CV_INPUT,
		PHASE_CV_INPUT,
		BULGE_CV_INPUT,

		// Jack Row 3 (Sync & Extra CV Inputs)
		SYNC_INPUT,
		TRAV_CV_INPUT,
		DWELL_CV_INPUT,
		ORDER_CV_INPUT,
		PATT_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputIds {
		SYNC_OUTPUT,
		X_OUTPUT,
		Y_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightIds {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_ORANGE,
		RANGE_LIGHT_PURPLE,
		LIGHTS_LEN
	};

	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600.0s to 0.1s (Period)
		RANGE_LFO = 1,       // 0.01 Hz to 200.0 Hz
		RANGE_VCO = 2        // 150.0 Hz to 2000.0 Hz
	};

	RangeMode rangeMode = RANGE_LFO;
	dsp::SchmittTrigger rangeTrigger;

	struct VoiceState {
		float basePhase = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
		float syncPeriod = 0.f;
		float timeSinceSync = 0.f;
	};

	VoiceState voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Polly* getPolly() {
			return dynamic_cast<Polly*>(this->module);
		}

		std::string getDisplayValueString() override {
			Polly* polly = getPolly();
			if (!polly) return "0.0";
			float coarse = getValue();
			float fine = polly->params[FINE_PARAM].getValue();
			float freq = polly->calculateBaseFrequency(coarse, fine);

			char buf[32];
			if (polly->rangeMode == RANGE_VERY_SLOW) {
				float period = (freq > 1e-6f) ? (1.0f / freq) : 600.0f;
				std::snprintf(buf, sizeof(buf), "%.2f", period);
			} else {
				if (freq < 10.0f) {
					std::snprintf(buf, sizeof(buf), "%.3f", freq);
				} else if (freq < 100.0f) {
					std::snprintf(buf, sizeof(buf), "%.2f", freq);
				} else {
					std::snprintf(buf, sizeof(buf), "%.1f", freq);
				}
			}
			return std::string(buf);
		}

		void setFrequencyValue(float val, bool isPeriod) {
			Polly* polly = getPolly();
			if (!polly) return;
			float fine = polly->params[FINE_PARAM].getValue();

			if (polly->rangeMode == RANGE_VERY_SLOW) {
				float period = isPeriod ? val : ((val > 1e-6f) ? (1.0f / val) : 600.0f);
				period = clampf(period, 0.05f, 1000.0f);
				float tCoarse = period / (1.0f - fine * 0.10f);
				tCoarse = clampf(tCoarse, 0.1f, 600.0f);
				float coarse = std::log(tCoarse / 600.0f) / std::log(0.1f / 600.0f);
				setValue(clampf(coarse, 0.0f, 1.0f));
			} else if (polly->rangeMode == RANGE_LFO) {
				float freq = isPeriod ? ((val > 1e-6f) ? (1.0f / val) : 0.01f) : val;
				freq = clampf(freq, 0.01f, 250.0f);
				float fCoarse = freq - fine * 20.0f;
				fCoarse = clampf(fCoarse, 0.01f, 200.0f);
				float coarse = std::sqrt((fCoarse - 0.01f) / 199.99f);
				setValue(clampf(coarse, 0.0f, 1.0f));
			} else {
				float freq = isPeriod ? ((val > 1e-6f) ? (1.0f / val) : 150.0f) : val;
				freq = clampf(freq, 100.0f, 2500.0f);
				float fCoarse = freq - fine * 0.10f;
				fCoarse = clampf(fCoarse, 150.0f, 2000.0f);
				float coarse = std::log(fCoarse / 150.0f) / std::log(2000.0f / 150.0f);
				setValue(clampf(coarse, 0.0f, 1.0f));
			}
		}

		void setDisplayValueString(std::string s) override {
			Polly* polly = getPolly();
			if (!polly) return;

			std::string lowerStr = s;
			for (char& c : lowerStr) c = (char)std::tolower((unsigned char)c);

			enum UnitType { UNIT_DEFAULT, UNIT_SEC, UNIT_MS, UNIT_HZ, UNIT_KHZ, UNIT_MHZ };
			UnitType unitType = UNIT_DEFAULT;

			if (lowerStr.find("mhz") != std::string::npos) unitType = UNIT_MHZ;
			else if (lowerStr.find("khz") != std::string::npos) unitType = UNIT_KHZ;
			else if (lowerStr.find("hz") != std::string::npos) unitType = UNIT_HZ;
			else if (lowerStr.find("ms") != std::string::npos) unitType = UNIT_MS;
			else if (lowerStr.find("s") != std::string::npos || lowerStr.find("sec") != std::string::npos) unitType = UNIT_SEC;

			char* endPtr = nullptr;
			float rawVal = std::strtof(lowerStr.c_str(), &endPtr);
			if (endPtr == lowerStr.c_str()) return;

			float finalVal = rawVal;
			bool isPeriod = false;

			if (unitType == UNIT_SEC) {
				isPeriod = true;
				finalVal = rawVal;
			} else if (unitType == UNIT_MS) {
				isPeriod = true;
				finalVal = rawVal / 1000.0f;
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
				isPeriod = (polly->rangeMode == Polly::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Polly* polly = getPolly();
			if (!polly) return "";
			return (polly->rangeMode == Polly::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	Polly() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: FREQ, RANGE, FINE, SIDES, ANGLE, TEETH, OFFSET, TWIST
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configParam(SIDES_PARAM, 3.f, 32.f, 4.f, "Sides", "");
		configParam(ANGLE_PARAM, -180.f, 180.f, 0.f, "Angle", "°");
		configParam(TEETH_PARAM, -1.f, 1.f, 0.f, "Teeth", "%", 0.f, 100.f);
		configParam(OFFSET_PARAM, -1.f, 1.f, 0.f, "Offset", "%", 0.f, 100.f);
		configParam(TWIST_PARAM, -1.f, 1.f, 0.f, "Twist", "%", 0.f, 100.f);

		// Row 2: DISTRIB, PATTERN, FILLET, BOW, RIP_AMT, RIP_ORD, RIP_PHS, RIP_SHP
		configParam(DISTRIB_PARAM, -1.f, 1.f, 0.f, "Distribution", "%", 0.f, 100.f);
		configSwitch(PATTERN_PARAM, 1.f, 4.f, 1.f, "Pattern", {"Cluster", "Alternate", "Triad", "Harmonic"});
		configParam(FILLET_PARAM, -1.f, 1.f, 0.f, "Fillet", "%", 0.f, 100.f);
		configParam(BOW_PARAM, -1.f, 1.f, 0.f, "Bow", "%", 0.f, 100.f);
		configParam(RIP_AMT_PARAM, 0.f, 1.f, 0.f, "Ripple depth", "%", 0.f, 100.f);
		configParam(RIP_ORD_PARAM, 1.f, 16.f, 1.f, "Ripple order", "");
		configParam(RIP_PHS_PARAM, -180.f, 180.f, 0.f, "Ripple phase", "°");
		configSwitch(RIP_SHP_PARAM, 0.f, 2.f, 0.f, "Ripple shape", {"Sine", "Triangle", "Square"});

		// Row 3: SYMM, ALTERN, WARP, TRAV, DWELL, CURVE, PHASE, BULGE
		configParam(SYMM_PARAM, 1.f, 12.f, 1.f, "Symmetry", "");
		configParam(ALTERN_PARAM, -1.f, 1.f, 0.f, "Alternation", "%", 0.f, 100.f);
		configParam(WARP_PARAM, -1.f, 1.f, 0.f, "Warp", "%", 0.f, 100.f);
		configSwitch(TRAV_PARAM, 0.f, 3.f, 0.f, "Traversal mode", {"Angular", "Arc Length", "Vertex", "Easing"});
		configParam(DWELL_PARAM, 0.f, 1.f, 0.f, "Dwell", "%", 0.f, 100.f);
		configParam(CURVE_PARAM, 0.f, 1.f, 0.f, "Dwell curve", "%", 0.f, 100.f);
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°");
		configParam(BULGE_PARAM, -1.f, 1.f, 0.f, "Bulge", "%", 0.f, 100.f);

		// Zone 3: CV Attenuverters (Mandatory naming per AGENTS.md Rule 6.5.4)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(SIDES_TRIM_PARAM, -1.f, 1.f, 0.f, "Sides CV depth", "%", 0.f, 100.f);
		configParam(ANGLE_TRIM_PARAM, -1.f, 1.f, 0.f, "Angle CV depth", "%", 0.f, 100.f);
		configParam(TEETH_TRIM_PARAM, -1.f, 1.f, 0.f, "Teeth CV depth", "%", 0.f, 100.f);
		configParam(OFFSET_TRIM_PARAM, -1.f, 1.f, 0.f, "Offset CV depth", "%", 0.f, 100.f);
		configParam(TWIST_TRIM_PARAM, -1.f, 1.f, 0.f, "Twist CV depth", "%", 0.f, 100.f);
		configParam(FILLET_TRIM_PARAM, -1.f, 1.f, 0.f, "Fillet CV depth", "%", 0.f, 100.f);
		configParam(BOW_TRIM_PARAM, -1.f, 1.f, 0.f, "Bow CV depth", "%", 0.f, 100.f);

		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "Linear FM CV depth", "%", 0.f, 100.f);
		configParam(DISTRIB_TRIM_PARAM, -1.f, 1.f, 0.f, "Distribution CV depth", "%", 0.f, 100.f);
		configParam(RIPPLE_TRIM_PARAM, -1.f, 1.f, 0.f, "Ripple CV depth", "%", 0.f, 100.f);
		configParam(SYMM_TRIM_PARAM, -1.f, 1.f, 0.f, "Symmetry CV depth", "%", 0.f, 100.f);
		configParam(ALTERN_TRIM_PARAM, -1.f, 1.f, 0.f, "Alternation CV depth", "%", 0.f, 100.f);
		configParam(WARP_TRIM_PARAM, -1.f, 1.f, 0.f, "Warp CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);
		configParam(BULGE_TRIM_PARAM, -1.f, 1.f, 0.f, "Bulge CV depth", "%", 0.f, 100.f);

		// Zone 4: I/O Jacks
		// Row 1
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(SIDES_CV_INPUT, "Sides CV");
		configInput(ANGLE_CV_INPUT, "Angle CV");
		configInput(TEETH_CV_INPUT, "Teeth CV");
		configInput(OFFSET_CV_INPUT, "Offset CV");
		configInput(TWIST_CV_INPUT, "Twist CV");
		configInput(FILLET_CV_INPUT, "Fillet CV");
		configInput(BOW_CV_INPUT, "Bow CV");

		// Row 2
		configInput(FM_CV_INPUT, "External FM");
		configInput(DISTRIB_CV_INPUT, "Distribution CV");
		configInput(RIPPLE_CV_INPUT, "Ripple CV");
		configInput(SYMM_CV_INPUT, "Symmetry CV");
		configInput(ALTERN_CV_INPUT, "Alternation CV");
		configInput(WARP_CV_INPUT, "Warp CV");
		configInput(PHASE_CV_INPUT, "Phase CV");
		configInput(BULGE_CV_INPUT, "Bulge CV");

		// Row 3
		configInput(SYNC_INPUT, "Sync");
		configInput(TRAV_CV_INPUT, "Traversal mode CV");
		configInput(DWELL_CV_INPUT, "Dwell CV");
		configInput(ORDER_CV_INPUT, "Ripple order CV");
		configInput(PATT_CV_INPUT, "Pattern CV");

		configOutput(SYNC_OUTPUT, "Sync");
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
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
			return clampf(fCoarse + fine * 0.10f, 100.0f, 2500.0f);
		}
	}

	void process(const ProcessArgs& args) override {
		// Handle Range Button cycling (Very Slow -> LFO -> VCO)
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		// Update 3-Color Range LED (Yellow = Very Slow, Teal = LFO, Magenta = VCO)
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		// Channels determination
		int maxCh = 1;
		for (int i = 0; i < INPUTS_LEN; i++) {
			maxCh = std::max(maxCh, inputs[i].getChannels());
		}
		int numChannels = std::min(maxCh, 16);
		outputs[SYNC_OUTPUT].setChannels(numChannels);
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		// Read continuous module-level parameters
		float coarseParam  = params[FREQ_PARAM].getValue();
		float fineParam    = params[FINE_PARAM].getValue();
		float defaultF0    = calculateBaseFrequency(coarseParam, fineParam);

		float sidesParam   = params[SIDES_PARAM].getValue();
		float angleParam   = params[ANGLE_PARAM].getValue();
		float teethParam   = params[TEETH_PARAM].getValue();
		float offsetParam  = params[OFFSET_PARAM].getValue();
		float twistParam   = params[TWIST_PARAM].getValue();

		float distribParam = params[DISTRIB_PARAM].getValue();
		float patternParam = params[PATTERN_PARAM].getValue();
		float filletParam  = params[FILLET_PARAM].getValue();
		float bowParam     = params[BOW_PARAM].getValue();
		float ripAmtParam  = params[RIP_AMT_PARAM].getValue();
		float ripOrdParam  = params[RIP_ORD_PARAM].getValue();
		float ripPhsParam  = params[RIP_PHS_PARAM].getValue();
		float ripShpParam  = params[RIP_SHP_PARAM].getValue();

		float symmParam    = params[SYMM_PARAM].getValue();
		float alternParam  = params[ALTERN_PARAM].getValue();
		float warpParam    = params[WARP_PARAM].getValue();
		float travParam    = params[TRAV_PARAM].getValue();
		float dwellParam   = params[DWELL_PARAM].getValue();
		float curveParam   = params[CURVE_PARAM].getValue();
		float phaseParam   = params[PHASE_PARAM].getValue();
		float bulgeParam   = params[BULGE_PARAM].getValue();

		// Trimpots
		float fTrim       = params[FREQ_TRIM_PARAM].getValue();
		float sidesTrim   = params[SIDES_TRIM_PARAM].getValue();
		float angleTrim   = params[ANGLE_TRIM_PARAM].getValue();
		float teethTrim   = params[TEETH_TRIM_PARAM].getValue();
		float offsetTrim  = params[OFFSET_TRIM_PARAM].getValue();
		float twistTrim   = params[TWIST_TRIM_PARAM].getValue();
		float filletTrim  = params[FILLET_TRIM_PARAM].getValue();
		float bowTrim     = params[BOW_TRIM_PARAM].getValue();

		float fmTrim      = params[FM_TRIM_PARAM].getValue();
		float distribTrim = params[DISTRIB_TRIM_PARAM].getValue();
		float rippleTrim  = params[RIPPLE_TRIM_PARAM].getValue();
		float symmTrim    = params[SYMM_TRIM_PARAM].getValue();
		float alternTrim  = params[ALTERN_TRIM_PARAM].getValue();
		float warpTrim    = params[WARP_TRIM_PARAM].getValue();
		float phaseTrim   = params[PHASE_TRIM_PARAM].getValue();
		float bulgeTrim   = params[BULGE_TRIM_PARAM].getValue();

		bool fmConnected   = inputs[FM_CV_INPUT].isConnected();
		bool syncConnected = inputs[SYNC_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			VoiceState& vs = voices[c];

			// Hard sync tracking
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

			// Base Frequency
			float f0 = (syncConnected && vs.syncPeriod > 0.f) ? (1.f / vs.syncPeriod) : defaultF0;
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

			// Evaluate Per-Voice Parameters with CV
			float sidesVal = clampf(sidesParam + (inputs[SIDES_CV_INPUT].getPolyVoltage(c) / 5.f) * sidesTrim * 29.f, 3.f, 32.f);
			int N = (int)std::round(sidesVal);
			N = std::max(3, std::min(N, 32));

			float angleVal = angleParam + (inputs[ANGLE_CV_INPUT].getPolyVoltage(c) / 5.f) * angleTrim * 180.f;
			float baseAngleRad = angleVal * (float)(M_PI / 180.0);

			float teethVal = clampf(teethParam + (inputs[TEETH_CV_INPUT].getPolyVoltage(c) / 5.f) * teethTrim, -1.f, 1.f);
			float offsetVal = clampf(offsetParam + (inputs[OFFSET_CV_INPUT].getPolyVoltage(c) / 5.f) * offsetTrim, -1.f, 1.f);
			float twistVal = clampf(twistParam + (inputs[TWIST_CV_INPUT].getPolyVoltage(c) / 5.f) * twistTrim, -1.f, 1.f);

			float distribVal = clampf(distribParam + (inputs[DISTRIB_CV_INPUT].getPolyVoltage(c) / 5.f) * distribTrim, -1.f, 1.f);
			int pattVal = (int)std::round(patternParam + (inputs[PATT_CV_INPUT].getPolyVoltage(c) / 5.f) * 3.f);
			pattVal = clampf(pattVal, 1, 4);

			float filletVal = clampf(filletParam + (inputs[FILLET_CV_INPUT].getPolyVoltage(c) / 5.f) * filletTrim, -1.f, 1.f);
			float bowVal = clampf(bowParam + (inputs[BOW_CV_INPUT].getPolyVoltage(c) / 5.f) * bowTrim, -1.f, 1.f);

			float ripAmtVal = clampf(ripAmtParam + (inputs[RIPPLE_CV_INPUT].getPolyVoltage(c) / 5.f) * rippleTrim, 0.f, 1.f);
			float ripOrdVal = clampf(ripOrdParam + (inputs[ORDER_CV_INPUT].getPolyVoltage(c) / 5.f) * 8.f, 1.f, 16.f);
			float ripPhsVal = (ripPhsParam + (inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f) * 180.f) * (float)(M_PI / 180.0);
			int ripShpVal = (int)std::round(ripShpParam);

			int symmVal = (int)std::round(symmParam + (inputs[SYMM_CV_INPUT].getPolyVoltage(c) / 5.f) * symmTrim * 6.f);
			symmVal = std::max(1, std::min(symmVal, 12));

			float alternVal = clampf(alternParam + (inputs[ALTERN_CV_INPUT].getPolyVoltage(c) / 5.f) * alternTrim, -1.f, 1.f);
			float warpVal = clampf(warpParam + (inputs[WARP_CV_INPUT].getPolyVoltage(c) / 5.f) * warpTrim, -1.f, 1.f);

			int travMode = (int)std::round(travParam + (inputs[TRAV_CV_INPUT].getPolyVoltage(c) / 5.f) * 2.f);
			travMode = clampf(travMode, 0, 3);

			float dwellVal = clampf(dwellParam + (inputs[DWELL_CV_INPUT].getPolyVoltage(c) / 5.f), 0.f, 1.f);
			float curveVal = clampf(curveParam, 0.f, 1.f);

			float phaseVal = (phaseParam + (inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f) * phaseTrim * 180.f) * (float)(M_PI / 180.0);
			float bulgeVal = clampf(bulgeParam + (inputs[BULGE_CV_INPUT].getPolyVoltage(c) / 5.f) * bulgeTrim, -1.f, 1.f);

			// Traversal Phase Modulation
			float p = vs.basePhase;
			if (travMode == 1) {
				// Arc Length proxy
				float modP = p * (float)N;
				int sIdx = (int)modP;
				float sFrac = modP - sIdx;
				sFrac = 0.5f - 0.5f * std::cos((float)M_PI * sFrac);
				p = ((float)sIdx + sFrac) / (float)N;
			} else if (travMode == 2) {
				// Vertex-Biased (slow near corners, fast mid-edge)
				float modP = p * (float)N;
				int sIdx = (int)modP;
				float sFrac = modP - sIdx;
				sFrac = sFrac - 0.15f * std::sin(2.f * (float)M_PI * sFrac);
				p = ((float)sIdx + clampf(sFrac, 0.f, 1.f)) / (float)N;
			} else if (travMode == 3) {
				// Smooth Eased
				float modP = p * (float)N;
				int sIdx = (int)modP;
				float sFrac = modP - sIdx;
				sFrac = sFrac * sFrac * (3.f - 2.f * sFrac);
				p = ((float)sIdx + sFrac) / (float)N;
			}

			// Vertex Dwell: linger around integer side boundaries
			if (dwellVal > 0.001f) {
				float modP = p * (float)N;
				int sIdx = (int)modP;
				float sFrac = modP - sIdx;
				float dwellRange = dwellVal * 0.35f;
				if (sFrac < dwellRange) {
					float factor = std::pow(sFrac / dwellRange, 1.f + curveVal * 2.f);
					sFrac = factor * dwellRange;
				} else if (sFrac > 1.f - dwellRange) {
					float delta = 1.f - sFrac;
					float factor = std::pow(delta / dwellRange, 1.f + curveVal * 2.f);
					sFrac = 1.f - factor * dwellRange;
				}
				p = ((float)sIdx + sFrac) / (float)N;
			}
			p = p - std::floor(p);

			// Determine which side we are traversing
			float sideProgress = p * (float)N;
			int sideIdx = (int)sideProgress;
			if (sideIdx >= N) sideIdx = 0;
			float t = sideProgress - sideIdx;
			int nextIdx = (sideIdx + 1) % N;

			// Fold vertices through Symmetry order if applicable
			int v1Symm = (symmVal > 1) ? (sideIdx % symmVal) : sideIdx;
			int v2Symm = (symmVal > 1) ? (nextIdx % symmVal) : nextIdx;

			// Base Vertex Coordinates
			auto computeVertex = [&](int idx, int symmIdx) -> std::pair<float, float> {
				float baseTheta = (float)idx * (2.f * (float)M_PI / (float)N);

				// Distribution bias
				float dTheta = 0.f;
				if (std::abs(distribVal) > 0.001f) {
					float step = (float)M_PI / (float)N;
					if (pattVal == 1) {
						dTheta = distribVal * step * std::sin(baseTheta);
					} else if (pattVal == 2) {
						dTheta = distribVal * step * ((symmIdx % 2 == 0) ? 1.f : -1.f);
					} else if (pattVal == 3) {
						dTheta = distribVal * step * std::cos(3.f * baseTheta);
					} else {
						dTheta = distribVal * step * std::sin(2.f * baseTheta);
					}
				}
				float theta = baseAngleRad + baseTheta + dTheta;

				// Alternation radius
				float r = 1.f;
				if (std::abs(alternVal) > 0.001f) {
					r += alternVal * 0.35f * ((symmIdx % 2 == 0) ? 1.f : -1.f);
				}
				return {r * std::cos(theta), r * std::sin(theta)};
			};

			auto v1 = computeVertex(sideIdx, v1Symm);
			auto v2 = computeVertex(nextIdx, v2Symm);

			// Compute Secondary Vertex (Offset & Twist)
			float sPos = clampf(0.5f + twistVal * 0.38f, 0.05f, 0.95f);
			float baseMidX = (1.f - sPos) * v1.first + sPos * v2.first;
			float baseMidY = (1.f - sPos) * v1.second + sPos * v2.second;

			// Outward edge normal
			float edgX = v2.first - v1.first;
			float edgY = v2.second - v1.second;
			float edgLen = std::sqrt(edgX * edgX + edgY * edgY);
			float normX = 0.f, normY = 0.f;
			if (edgLen > 1e-5f) {
				normX = -edgY / edgLen;
				normY = edgX / edgLen;
			}

			// Secondary vertex insertion with Offset and Teeth skew
			float secX = baseMidX + (offsetVal * 0.7f + teethVal * 0.35f) * normX;
			float secY = baseMidY + (offsetVal * 0.7f + teethVal * 0.35f) * normY;

			// Interpolate position along sub-segments
			float curX, curY;
			bool useSecondary = (std::abs(offsetVal) > 0.001f || std::abs(twistVal) > 0.001f || std::abs(teethVal) > 0.001f);
			if (useSecondary) {
				if (t < sPos) {
					float u = t / sPos;
					curX = (1.f - u) * v1.first + u * secX;
					curY = (1.f - u) * v1.second + u * secY;
				} else {
					float u = (t - sPos) / (1.f - sPos);
					curX = (1.f - u) * secX + u * v2.first;
					curY = (1.f - u) * secY + u * v2.second;
				}
			} else {
				curX = (1.f - t) * v1.first + t * v2.first;
				curY = (1.f - t) * v1.second + t * v2.second;
			}

			// Edge Bow (parabolic arch)
			if (std::abs(bowVal) > 0.001f) {
				float bowAmt = bowVal * 0.4f * 4.f * t * (1.f - t);
				curX += bowAmt * normX;
				curY += bowAmt * normY;
			}

			// Corner Fillet (convex rounding or concave inward notch)
			if (std::abs(filletVal) > 0.001f) {
				float cornerDist = (t < 0.5f) ? t : (1.f - t);
				if (cornerDist < 0.25f) {
					float blend = (0.25f - cornerDist) / 0.25f;
					blend = blend * blend;
					if (filletVal > 0.f) {
						// Convex fillet: round towards smoothed circular midpoint
						float circR = 0.85f;
						float rad = std::sqrt(curX * curX + curY * curY);
						if (rad > 1e-5f) {
							float targetX = (curX / rad) * circR;
							float targetY = (curY / rad) * circR;
							curX = (1.f - blend * filletVal) * curX + (blend * filletVal) * targetX;
							curY = (1.f - blend * filletVal) * curY + (blend * filletVal) * targetY;
						}
					} else {
						// Concave cusp: pull towards origin
						curX *= (1.f + blend * filletVal * 0.7f);
						curY *= (1.f + blend * filletVal * 0.7f);
					}
				}
			}

			// Convert to Polar for Radial Ripple and Angular Warp
			float curR = std::sqrt(curX * curX + curY * curY);
			float curTheta = std::atan2(curY, curX);

			// Radial Harmonic Ripple: r(theta) = r0 * [1 + A * Wave(k*theta + phi)]
			if (ripAmtVal > 0.001f) {
				float rippleAngle = ripOrdVal * curTheta + ripPhsVal;
				float wave = 0.f;
				if (ripShpVal == 0) {
					// Sine
					wave = std::cos(rippleAngle);
				} else if (ripShpVal == 1) {
					// Triangle
					float normAngle = rippleAngle / (2.f * (float)M_PI);
					normAngle -= std::floor(normAngle);
					wave = 2.f * std::abs(2.f * normAngle - 1.f) - 1.f;
				} else {
					// Square
					wave = (std::cos(rippleAngle) >= 0.f) ? 1.f : -1.f;
				}
				curR *= (1.f + ripAmtVal * 0.45f * wave);
			}

			// Angular Warp / Shear
			if (std::abs(warpVal) > 0.001f) {
				curTheta += warpVal * 0.45f * std::sin(2.f * curTheta);
			}

			// Reconvert to Cartesian with Global Phase Offset
			float finalX = curR * std::cos(curTheta + phaseVal);
			float finalY = curR * std::sin(curTheta + phaseVal);

			// Apply Bulge (Hardcoded Harmonograph Logarithmic Spiral with Depth Factor 4.0 for Polly)
			if (std::abs(bulgeVal) > 1e-4f) {
				float rNorm = std::sqrt(finalX * finalX + finalY * finalY);
				float dampFactor = 1.0f - bulgeVal * 4.0f * (1.0f - rNorm);
				finalX *= dampFactor;
				finalY *= dampFactor;
			}

			// Standard Eurorack 10Vpp (±5V) Output
			outputs[X_OUTPUT].setVoltage(clampf(finalX * 5.f, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(finalY * 5.f, -12.f, 12.f), c);
			outputs[SYNC_OUTPUT].setVoltage(vs.syncPulse.process(args.sampleTime) ? 10.f : 0.f, c);
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
struct TPollyRangeLight : TBase {
	TPollyRangeLight() {
		// Range 0: Very Slow = #e1be6a (Warm Gold)
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff));
		// Range 1: LFO = #40b0a6 (Teal)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff));
		// Range 2: VCO = #d35fb7 (Magenta)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff));
	}
};
struct PollyRangeLightWidget : SmallLight<TPollyRangeLight<>> {};

struct PollyWidget : ModuleWidget {
	PollyWidget(Polly* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Polly.svg")));

		// 24 HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 8 Columns (X coordinates matching generator script)
		const float col_x[8] = { 9.20f, 23.60f, 38.00f, 52.40f, 66.80f, 81.20f, 95.60f, 110.00f };

		// Row 1 Knobs: FREQ, RANGE, FINE, SIDES, ANGLE, TEETH, OFFSET, TWIST (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 21.59)), module, Polly::FREQ_PARAM));
		addChild(createLightCentered<PollyRangeLightWidget>(mm2px(Vec(col_x[1], 15.50)), module, Polly::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<TL1105>(mm2px(Vec(col_x[1], 21.59)), module, Polly::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 21.59)), module, Polly::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 21.59)), module, Polly::SIDES_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[4], 21.59)), module, Polly::ANGLE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[5], 21.59)), module, Polly::TEETH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[6], 21.59)), module, Polly::OFFSET_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[7], 21.59)), module, Polly::TWIST_PARAM));

		// Row 2 Knobs: DISTRIB, PATTERN, FILLET, BOW, RIP_AMT, RIP_ORD, RIP_PHS, RIP_SHP (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 37.00)), module, Polly::DISTRIB_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 37.00)), module, Polly::PATTERN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 37.00)), module, Polly::FILLET_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 37.00)), module, Polly::BOW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[4], 37.00)), module, Polly::RIP_AMT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[5], 37.00)), module, Polly::RIP_ORD_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[6], 37.00)), module, Polly::RIP_PHS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[7], 37.00)), module, Polly::RIP_SHP_PARAM));

		// Row 3 Knobs: SYMM, ALTERN, WARP, TRAV, DWELL, CURVE, PHASE, BULGE (Center Y = 52.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 52.50)), module, Polly::SYMM_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 52.50)), module, Polly::ALTERN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 52.50)), module, Polly::WARP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 52.50)), module, Polly::TRAV_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[4], 52.50)), module, Polly::DWELL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[5], 52.50)), module, Polly::CURVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[6], 52.50)), module, Polly::PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[7], 52.50)), module, Polly::BULGE_PARAM));

		// Zone 3: CV Attenuverter Trimpots (Y = 69.50 and 79.50 mm)
		// Row 1 Attenuverters
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 69.50)), module, Polly::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 69.50)), module, Polly::SIDES_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 69.50)), module, Polly::ANGLE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 69.50)), module, Polly::TEETH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[4], 69.50)), module, Polly::OFFSET_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[5], 69.50)), module, Polly::TWIST_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[6], 69.50)), module, Polly::FILLET_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[7], 69.50)), module, Polly::BOW_TRIM_PARAM));

		// Row 2 Attenuverters
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 79.50)), module, Polly::FM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 79.50)), module, Polly::DISTRIB_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 79.50)), module, Polly::RIPPLE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 79.50)), module, Polly::SYMM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[4], 79.50)), module, Polly::ALTERN_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[5], 79.50)), module, Polly::WARP_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[6], 79.50)), module, Polly::PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[7], 79.50)), module, Polly::BULGE_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs) (Center Y = 93.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 93.50)), module, Polly::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 93.50)), module, Polly::SIDES_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 93.50)), module, Polly::ANGLE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 93.50)), module, Polly::TEETH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[4], 93.50)), module, Polly::OFFSET_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[5], 93.50)), module, Polly::TWIST_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[6], 93.50)), module, Polly::FILLET_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[7], 93.50)), module, Polly::BOW_CV_INPUT));

		// Row 2 (Inputs) (Center Y = 104.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 104.50)), module, Polly::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 104.50)), module, Polly::DISTRIB_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 104.50)), module, Polly::RIPPLE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 104.50)), module, Polly::SYMM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[4], 104.50)), module, Polly::ALTERN_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[5], 104.50)), module, Polly::WARP_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[6], 104.50)), module, Polly::PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[7], 104.50)), module, Polly::BULGE_CV_INPUT));

		// Row 3 (Sync & Outputs) (Center Y = 117.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 117.50)), module, Polly::SYNC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 117.50)), module, Polly::TRAV_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 117.50)), module, Polly::DWELL_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 117.50)), module, Polly::ORDER_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[4], 117.50)), module, Polly::PATT_CV_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[5], 117.50)), module, Polly::SYNC_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[6], 117.50)), module, Polly::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[7], 117.50)), module, Polly::Y_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Polly* module = dynamic_cast<Polly*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; i++) {
			Polly::RangeMode mode = (Polly::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}
	}
};

Model* modelPolly = createModel<Polly, PollyWidget>("Polly");
