#include "plugin.hpp"
#include "dsp/Oscillator.hpp"
#include <cmath>
#include <algorithm>
#include <string>

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi ? hi : v);
}

struct WaveformParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		return garmire::waveformName(getValue());
	}
};

struct IntegerParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int val = (int)std::round(getValue());
		return std::to_string(val);
	}
	void setDisplayValueString(std::string s) override {
		char* endPtr = nullptr;
		float val = std::strtof(s.c_str(), &endPtr);
		if (endPtr != s.c_str()) {
			setValue(clampf(std::round(val), getMinValue(), getMaxValue()));
		}
	}
};

struct MaudeChannel {
	float carrierPhase = 0.f;
	float subPhase = 0.f;
	dsp::SchmittTrigger syncTrigger;
	dsp::PulseGenerator syncPulse;
	float syncPeriod = 0.f;
	float timeSinceSync = 0.f;

	void reset() {
		carrierPhase = 0.f;
		subPhase = 0.f;
		syncTrigger.reset();
		syncPeriod = 0.f;
		timeSinceSync = 0.f;
	}
};

struct Maude : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0,
		RANGE_LFO = 1,
		RANGE_VCO = 2,
		RANGE_MODES_LEN
	};

	enum ParamId {
		// Row 1: Timebase & Master Phase
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,
		PHASE_PARAM,

		// Row 2: Subharmonic Engine
		DIV_PARAM,
		RATIO_PARAM,
		SHAPE_PARAM,
		DEPTH_PARAM,

		// Row 3: Rosette Transformations & Sizing
		SPLIT_PARAM,
		FOLD_PARAM,
		TWIST_PARAM,
		STRETCH_PARAM,

		// Zone 3: CV Attenuverters (3 Rows x 4 Trimpots)
		FREQ_TRIM_PARAM,
		FM_TRIM_PARAM,
		FINE_TRIM_PARAM,
		PHASE_TRIM_PARAM,

		DIV_TRIM_PARAM,
		RATIO_TRIM_PARAM,
		SHAPE_TRIM_PARAM,
		DEPTH_TRIM_PARAM,

		SPLIT_TRIM_PARAM,
		FOLD_TRIM_PARAM,
		TWIST_TRIM_PARAM,
		STRETCH_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Jack Row 1 (CV)
		FREQ_CV_INPUT,
		FM_CV_INPUT,
		FINE_CV_INPUT,
		PHASE_CV_INPUT,

		// Jack Row 2 (CV)
		DIV_CV_INPUT,
		RATIO_CV_INPUT,
		SHAPE_CV_INPUT,
		DEPTH_CV_INPUT,

		// Jack Row 3 (CV)
		SPLIT_CV_INPUT,
		FOLD_CV_INPUT,
		TWIST_CV_INPUT,
		STRETCH_CV_INPUT,

		// Jack Row 4 (Sync)
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
	dsp::BooleanTrigger rangeTrigger;
	MaudeChannel channels[16];

	struct FreqParamQuantity : ParamQuantity {
		Maude* getMaude() {
			return dynamic_cast<Maude*>(module);
		}

		float getDisplayValue() override {
			Maude* maude = getMaude();
			if (!maude) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (maude->paramQuantities.size() > Maude::FINE_PARAM) ? maude->params[Maude::FINE_PARAM].getValue() : 0.f;

			if (maude->rangeMode == Maude::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (maude->rangeMode == Maude::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Maude* maude = getMaude();
			if (!maude) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (maude->rangeMode == Maude::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Maude* maude = getMaude();
			if (!maude) return;

			float fine = (maude->paramQuantities.size() > Maude::FINE_PARAM) ? maude->params[Maude::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (maude->rangeMode == Maude::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (maude->rangeMode == Maude::RANGE_LFO) {
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
			Maude* maude = getMaude();
			if (!maude) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (maude->rangeMode == Maude::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Maude* maude = getMaude();
			if (!maude) {
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

			while (!str.empty() && (std::isalpha((unsigned char)str.back()) || std::isspace((unsigned char)str.back()))) {
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
				isPeriod = (maude->rangeMode == Maude::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Maude* maude = getMaude();
			if (!maude) return "";
			return (maude->rangeMode == Maude::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	Maude() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Timebase & Phase (Exact Generator Standard matching Lisa/Daisy/Circe/Polly)
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°", 0.f, 1.f);

		// Row 2: Subharmonic Engine
		configParam<IntegerParamQuantity>(DIV_PARAM, 1.f, 12.f, 4.f, "Subharmonic Divisions", "");
		paramQuantities[DIV_PARAM]->snapEnabled = true;

		configParam<IntegerParamQuantity>(RATIO_PARAM, 1.f, 12.f, 1.f, "Ring Mod Ratio", "");
		paramQuantities[RATIO_PARAM]->snapEnabled = true;

		configParam<WaveformParamQuantity>(SHAPE_PARAM, 0.f, 1.f, 0.f, "Carrier Waveform Morph");
		configParam(DEPTH_PARAM, 0.f, 1.f, 1.f, "Modulation Depth", "%", 0.f, 100.f);

		// Row 3: Rosette Transformations & Sizing
		configParam(SPLIT_PARAM, 0.f, 1.f, 0.f, "Split (Period Doubling)", "%", 0.f, 100.f);
		configParam(FOLD_PARAM, 0.f, 1.f, 0.f, "Fold (Concentric Waveshape)", "%", 0.f, 100.f);
		configParam(TWIST_PARAM, 0.f, 1.f, 0.f, "Twist (Quadrature Skew)", "%", 0.f, 100.f);
		configParam(STRETCH_PARAM, -1.f, 1.f, 0.f, "Stretch", "%", 0.f, 100.f);

		// Zone 3: CV Attenuverters (Mandatory naming per AGENTS.md Section 6.5.4)
		// Row 1 Attenuverters
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "Linear FM CV depth", "%", 0.f, 100.f);
		configParam(FINE_TRIM_PARAM, -1.f, 1.f, 0.f, "Fine frequency CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);

		// Row 2 Attenuverters
		configParam(DIV_TRIM_PARAM, -1.f, 1.f, 0.f, "Divisions CV depth", "%", 0.f, 100.f);
		configParam(RATIO_TRIM_PARAM, -1.f, 1.f, 0.f, "Ratio CV depth", "%", 0.f, 100.f);
		configParam(SHAPE_TRIM_PARAM, -1.f, 1.f, 0.f, "Shape CV depth", "%", 0.f, 100.f);
		configParam(DEPTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Depth CV depth", "%", 0.f, 100.f);

		// Row 3 Attenuverters
		configParam(SPLIT_TRIM_PARAM, -1.f, 1.f, 0.f, "Split CV depth", "%", 0.f, 100.f);
		configParam(FOLD_TRIM_PARAM, -1.f, 1.f, 0.f, "Fold CV depth", "%", 0.f, 100.f);
		configParam(TWIST_TRIM_PARAM, -1.f, 1.f, 0.f, "Twist CV depth", "%", 0.f, 100.f);
		configParam(STRETCH_TRIM_PARAM, -1.f, 1.f, 0.f, "Stretch CV depth", "%", 0.f, 100.f);

		// Zone 4: Inputs
		// Row 1 Inputs
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(FM_CV_INPUT, "External FM");
		configInput(FINE_CV_INPUT, "Fine Tune CV");
		configInput(PHASE_CV_INPUT, "Phase CV");

		// Row 2 Inputs
		configInput(DIV_CV_INPUT, "Divisions CV");
		configInput(RATIO_CV_INPUT, "Ratio CV");
		configInput(SHAPE_CV_INPUT, "Shape CV");
		configInput(DEPTH_CV_INPUT, "Depth CV");

		// Row 3 Inputs
		configInput(SPLIT_CV_INPUT, "Split CV");
		configInput(FOLD_CV_INPUT, "Fold CV");
		configInput(TWIST_CV_INPUT, "Twist CV");
		configInput(STRETCH_CV_INPUT, "Stretch CV");

		// Row 4 Sync
		configInput(SYNC_INPUT, "Sync");

		// Row 4 Outputs
		configOutput(SYNC_OUTPUT, "Sync");
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");

		for (int c = 0; c < 16; c++) {
			channels[c].reset();
		}
	}

	void onReset() override {
		for (int c = 0; c < 16; c++) {
			channels[c].reset();
		}
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

	void process(const ProcessArgs& args) override {
		// Handle Range Button cycling (Very Slow -> LFO -> VCO)
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % RANGE_MODES_LEN);
		}

		// Update 3-Color Range LED (Yellow = Very Slow, Teal = LFO, Magenta = VCO)
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		// Determine polyphony
		int maxChannels = 1;
		for (int i = 0; i < INPUTS_LEN; i++) {
			if (inputs[i].isConnected()) {
				maxChannels = std::max(maxChannels, inputs[i].getChannels());
			}
		}

		outputs[SYNC_OUTPUT].setChannels(maxChannels);
		outputs[X_OUTPUT].setChannels(maxChannels);
		outputs[Y_OUTPUT].setChannels(maxChannels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();
		float defaultF0   = calculateBaseFrequency(coarseParam, fineParam);

		float phaseParam   = params[PHASE_PARAM].getValue();
		float divParam     = params[DIV_PARAM].getValue();
		float ratioParam   = params[RATIO_PARAM].getValue();
		float shapeParam   = params[SHAPE_PARAM].getValue();
		float depthParam   = params[DEPTH_PARAM].getValue();
		float splitParam   = params[SPLIT_PARAM].getValue();
		float foldParam    = params[FOLD_PARAM].getValue();
		float twistParam   = params[TWIST_PARAM].getValue();
		float stretchParam = params[STRETCH_PARAM].getValue();

		float fTrim       = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim      = params[FM_TRIM_PARAM].getValue();
		float fineTrim    = params[FINE_TRIM_PARAM].getValue();
		float phaseTrim   = params[PHASE_TRIM_PARAM].getValue();
		float divTrim     = params[DIV_TRIM_PARAM].getValue();
		float ratioTrim   = params[RATIO_TRIM_PARAM].getValue();
		float shapeTrim   = params[SHAPE_TRIM_PARAM].getValue();
		float depthTrim   = params[DEPTH_TRIM_PARAM].getValue();
		float splitTrim   = params[SPLIT_TRIM_PARAM].getValue();
		float foldTrim    = params[FOLD_TRIM_PARAM].getValue();
		float twistTrim   = params[TWIST_TRIM_PARAM].getValue();
		float stretchTrim = params[STRETCH_TRIM_PARAM].getValue();

		bool syncConnected = inputs[SYNC_INPUT].isConnected();
		bool fmConnected   = inputs[FM_CV_INPUT].isConnected();

		for (int c = 0; c < maxChannels; c++) {
			auto& chan = channels[c];

			// Handle Sync Input
			bool syncTriggered = false;
			chan.timeSinceSync += args.sampleTime;
			if (syncConnected) {
				float syncVolt = inputs[SYNC_INPUT].getPolyVoltage(c);
				if (chan.syncTrigger.process(syncVolt, 0.1f, 1.5f)) {
					if (chan.timeSinceSync > 1e-5f) {
						chan.syncPeriod = chan.timeSinceSync;
					}
					chan.timeSinceSync = 0.f;
					chan.carrierPhase = 0.f;
					chan.subPhase = 0.f;
					syncTriggered = true;
					chan.syncPulse.trigger(1e-4f);
				}
			} else {
				chan.syncPeriod = 0.f;
			}

			// Base Frequency tracking
			float f0 = (syncConnected && chan.syncPeriod > 0.f) ? (1.f / chan.syncPeriod) : defaultF0;

			// Frequency 1V/Oct modulation
			float freqCv = inputs[FREQ_CV_INPUT].getPolyVoltage(c);
			float fineCv = inputs[FINE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float fineTotal = clampf(fineParam + fineCv * fineTrim, -1.f, 1.f);
			float fCarrier = f0 * std::pow(2.f, freqCv * fTrim + fineTotal * 0.10f);

			// Linear FM modulation (matching Circe/Lisa/Daisy/Polly standard)
			float fActual;
			if (!fmConnected) {
				float carrierSelfMod = std::sin(2.f * (float)M_PI * chan.carrierPhase);
				float deltaF = fCarrier * carrierSelfMod * fmTrim;
				fActual = std::max(0.0001f, fCarrier + deltaF);
			} else {
				float extFmCv = inputs[FM_CV_INPUT].getPolyVoltage(c);
				float scaleHz = (rangeMode == RANGE_VERY_SLOW) ? (fCarrier * 2.f) : (rangeMode == RANGE_LFO ? 100.f : 500.f);
				float deltaF = (extFmCv / 5.f) * fmTrim * scaleHz;
				fActual = std::max(0.0001f, fCarrier + deltaF);
			}

			// Advance carrier phase
			float deltaPhase = fActual * args.sampleTime;
			chan.carrierPhase += deltaPhase;

			// Master sync pulse trigger when carrier completes a cycle
			if (!syncTriggered && chan.carrierPhase >= 1.f) {
				chan.syncPulse.trigger(1e-4f);
			}
			chan.carrierPhase -= std::floor(chan.carrierPhase);

			// CV Modulations
			float pCv   = inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float dCv   = inputs[DIV_CV_INPUT].getPolyVoltage(c) / 5.f;
			float rCv   = inputs[RATIO_CV_INPUT].getPolyVoltage(c) / 5.f;
			float sCv   = inputs[SHAPE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float depCv = inputs[DEPTH_CV_INPUT].getPolyVoltage(c) / 5.f;
			float spCv  = inputs[SPLIT_CV_INPUT].getPolyVoltage(c) / 5.f;
			float foCv  = inputs[FOLD_CV_INPUT].getPolyVoltage(c) / 5.f;
			float twCv  = inputs[TWIST_CV_INPUT].getPolyVoltage(c) / 5.f;
			float stCv  = inputs[STRETCH_CV_INPUT].getPolyVoltage(c) / 5.f;

			float phaseDeg = clampf(phaseParam + pCv * phaseTrim * 180.f, -180.f, 180.f);
			int numDiv = (int)clampf(std::round(divParam + dCv * divTrim * 11.f), 1.f, 12.f);
			int ratio = (int)clampf(std::round(ratioParam + rCv * ratioTrim * 11.f), 1.f, 12.f);
			float shape = clampf(shapeParam + sCv * shapeTrim, 0.f, 1.f);
			float depth = clampf(depthParam + depCv * depthTrim, 0.f, 1.f);
			float split = clampf(splitParam + spCv * splitTrim, 0.f, 1.f);
			float fold = clampf(foldParam + foCv * foldTrim, 0.f, 1.f);
			float twist = clampf(twistParam + twCv * twistTrim, 0.f, 1.f);
			float stretch = clampf(stretchParam + stCv * stretchTrim, -1.f, 1.f);

			// Advance subharmonic phase spanning 2 * numDiv cycles
			int totalCycles = 2 * numDiv;
			chan.subPhase += deltaPhase / (float)totalCycles;
			chan.subPhase -= std::floor(chan.subPhase);

			// Internal Carrier Waveforms (Quadrature X and Y from Shape)
			float phiX = chan.carrierPhase;
			float phiY = chan.carrierPhase + 0.25f;
			if (phiY >= 1.f) phiY -= 1.f;

			float rawX = 5.f * garmire::waveformMorph(phiX, shape);
			float rawY = 5.f * garmire::waveformMorph(phiY, shape);

			// Subharmonic Rosette Modulation Angles
			float thetaX = 4.f * (float)M_PI * chan.subPhase * (float)ratio;
			float thetaY = thetaX + twist * (float)M_PI;

			// 1. SPLIT (subharmonic period doubling undertone)
			float splitModX = (1.f - split) * std::cos(thetaX) + split * std::cos(0.5f * thetaX);
			float splitModY = (1.f - split) * std::cos(thetaY) + split * std::cos(0.5f * thetaY);

			// 2. FOLD (multi-tier trigonometric wavefolding)
			float drive = 1.0f + 2.5f * fold;
			float modX = std::sin(drive * splitModX * (float)(M_PI * 0.5));
			float modY = std::sin(drive * splitModY * (float)(M_PI * 0.5));

			// 3. DEPTH (wet/dry ring mod crossfade)
			float wetX = rawX * modX;
			float wetY = rawY * modY;
			float xModulated = (1.f - depth) * rawX + depth * wetX;
			float yModulated = (1.f - depth) * rawY + depth * wetY;

			// 4. PHASE (orbital plane 2D rotation)
			float rad = phaseDeg * (float)(M_PI / 180.0);
			float cosP = std::cos(rad);
			float sinP = std::sin(rad);
			float rotX = xModulated * cosP - yModulated * sinP;
			float rotY = xModulated * sinP + yModulated * cosP;

			// 5. STRETCH (bipolar logarithmic aspect ratio stretch, positive outward)
			float scaleX = std::pow(2.f, stretch);
			float scaleY = std::pow(2.f, -stretch);
			float finalX = rotX * scaleX;
			float finalY = rotY * scaleY;

			// Outputs
			outputs[X_OUTPUT].setVoltage(clampf(finalX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(finalY, -12.f, 12.f), c);
			outputs[SYNC_OUTPUT].setVoltage(chan.syncPulse.process(args.sampleTime) ? 10.f : 0.f, c);
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

template <typename TBase = GrayModuleLightWidget>
struct TRangeLight : TBase {
	TRangeLight() {
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff)); // Warm Gold (Very Slow)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // Teal (LFO)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff)); // Magenta (VCO)
	}
};
struct RangeLightWidget : SmallLight<TRangeLight<>> {};

struct MaudeWidget : ModuleWidget {
	MaudeWidget(Maude* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Maude.svg")));

		// 12 HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 4 Standard Columns (7.62, 22.86, 38.10, 53.34 mm)
		const float col_x[4] = {7.62f, 22.86f, 38.10f, 53.34f};

		// ── Zone 1 & 2: Primary Parameter Knobs ──
		// Row 1: FREQ, RANGE (Button + LED), FINE, PHASE (Center Y = 18.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 18.50)), module, Maude::FREQ_PARAM));
		addChild(createLightCentered<RangeLightWidget>(mm2px(Vec(col_x[1], 13.50)), module, Maude::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<TL1105>(mm2px(Vec(col_x[1], 18.50)), module, Maude::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 18.50)), module, Maude::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 18.50)), module, Maude::PHASE_PARAM));

		// Row 2: DIV, RATIO, SHAPE, DEPTH (Center Y = 32.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 32.00)), module, Maude::DIV_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 32.00)), module, Maude::RATIO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 32.00)), module, Maude::SHAPE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 32.00)), module, Maude::DEPTH_PARAM));

		// Row 3: SPLIT, FOLD, TWIST, STRETCH (Center Y = 45.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 45.50)), module, Maude::SPLIT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 45.50)), module, Maude::FOLD_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 45.50)), module, Maude::TWIST_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 45.50)), module, Maude::STRETCH_PARAM));

		// ── Zone 3: CV Attenuverter Trimpots (3 Rows x 4 Trimpots) ──
		// Trim Row 1: FREQ, FM, FINE, PHASE (Center Y = 56.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 56.50)), module, Maude::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 56.50)), module, Maude::FM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 56.50)), module, Maude::FINE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 56.50)), module, Maude::PHASE_TRIM_PARAM));

		// Trim Row 2: DIV, RATIO, SHAPE, DEPTH (Center Y = 64.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 64.50)), module, Maude::DIV_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 64.50)), module, Maude::RATIO_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 64.50)), module, Maude::SHAPE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 64.50)), module, Maude::DEPTH_TRIM_PARAM));

		// Trim Row 3: SPLIT, FOLD, TWIST, STRETCH (Center Y = 72.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 72.50)), module, Maude::SPLIT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 72.50)), module, Maude::FOLD_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 72.50)), module, Maude::TWIST_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 72.50)), module, Maude::STRETCH_TRIM_PARAM));

		// ── Zone 4: I/O Jacks (4 Rows x 4 Jacks) ──
		// Jack Row 1 (CV): FREQ, FM, FINE, PHASE (Center Y = 85.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 85.00)), module, Maude::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 85.00)), module, Maude::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 85.00)), module, Maude::FINE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 85.00)), module, Maude::PHASE_CV_INPUT));

		// Jack Row 2 (CV): DIV, RATIO, SHAPE, DEPTH (Center Y = 96.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 96.00)), module, Maude::DIV_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 96.00)), module, Maude::RATIO_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 96.00)), module, Maude::SHAPE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 96.00)), module, Maude::DEPTH_CV_INPUT));

		// Jack Row 3 (CV): SPLIT, FOLD, TWIST, STRETCH (Center Y = 107.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 107.00)), module, Maude::SPLIT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 107.00)), module, Maude::FOLD_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 107.00)), module, Maude::TWIST_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 107.00)), module, Maude::STRETCH_CV_INPUT));

		// Jack Row 4 (Fixed Bottom Signal/Sync Row: SYNC IN, X OUT, Y OUT, SYNC OUT, Center Y = 118.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 118.00)), module, Maude::SYNC_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 118.00)), module, Maude::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 118.00)), module, Maude::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 118.00)), module, Maude::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Maude* module = dynamic_cast<Maude*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; i++) {
			Maude::RangeMode mode = (Maude::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}
	}
};

Model* modelMaude = createModel<Maude, MaudeWidget>("Maude");
