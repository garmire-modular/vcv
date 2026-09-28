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

		// Row 2: Distribution Parameters (SPLIT, PAIR, TRIO, GROUP, BUNCH)
		SPLIT_PARAM,
		PAIR_PARAM,
		TRIO_PARAM,
		GROUP_PARAM,
		BUNCH_PARAM,

		// Row 3: Secondary Geometry & Warping (PINCH, TWIST, BEVEL, PHASE, BULGE)
		PINCH_PARAM,
		TWIST_PARAM,
		BEVEL_PARAM,
		PHASE_PARAM,
		BULGE_PARAM,

		// Zone 3: CV Attenuverters (Trimpots)
		// Row 1 Attenuverters: FREQ, SIDES, ANGLE, PINCH, TWIST
		FREQ_TRIM_PARAM,
		SIDES_TRIM_PARAM,
		ANGLE_TRIM_PARAM,
		PINCH_TRIM_PARAM,
		TWIST_TRIM_PARAM,

		// Row 2 Attenuverters: FM, DIST, BEVEL, PHASE, BULGE
		FM_TRIM_PARAM,
		DIST_TRIM_PARAM,
		BEVEL_TRIM_PARAM,
		PHASE_TRIM_PARAM,
		BULGE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputIds {
		// Jack Row 1 (Inputs): FREQ, SIDES, ANGLE, PINCH, TWIST
		FREQ_CV_INPUT,
		SIDES_CV_INPUT,
		ANGLE_CV_INPUT,
		PINCH_CV_INPUT,
		TWIST_CV_INPUT,

		// Jack Row 2 (Inputs): FM, DIST, BEVEL, PHASE, BULGE
		FM_CV_INPUT,
		DIST_CV_INPUT,
		BEVEL_CV_INPUT,
		PHASE_CV_INPUT,
		BULGE_CV_INPUT,

		// Jack Row 3 (Sync In)
		SYNC_INPUT,

		INPUTS_LEN
	};

	enum OutputIds {
		X_OUTPUT,
		Y_OUTPUT,
		SYNC_OUTPUT,
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

		// Row 1: FREQ, RANGE, FINE, SIDES, ANGLE
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configParam(SIDES_PARAM, 3.f, 32.f, 4.f, "Sides", "");
		configParam(ANGLE_PARAM, -180.f, 180.f, 0.f, "Angle", "°");

		// Row 2: Distribution Parameters (SPLIT, PAIR, TRIO, GROUP, BUNCH)
		configParam(SPLIT_PARAM, -1.f, 1.f, 0.f, "Split", "%", 0.f, 100.f);
		configParam(PAIR_PARAM, -1.f, 1.f, 0.f, "Pair", "%", 0.f, 100.f);
		configParam(TRIO_PARAM, -1.f, 1.f, 0.f, "Trio", "%", 0.f, 100.f);
		configParam(GROUP_PARAM, -1.f, 1.f, 0.f, "Group", "%", 0.f, 100.f);
		configParam(BUNCH_PARAM, -1.f, 1.f, 0.f, "Bunch", "%", 0.f, 100.f);

		// Row 3: PINCH, TWIST, BEVEL, PHASE, BULGE
		configParam(PINCH_PARAM, -1.f, 1.f, 0.f, "Pinch", "%", 0.f, 100.f);
		configParam(TWIST_PARAM, -1.f, 1.f, 0.f, "Twist", "%", 0.f, 100.f);
		configParam(BEVEL_PARAM, -1.f, 1.f, 0.f, "Bevel", "%", 0.f, 100.f);
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°");
		configParam(BULGE_PARAM, -1.f, 1.f, 0.f, "Bulge", "%", 0.f, 100.f);

		// Zone 3: CV Attenuverters (Mandatory naming per AGENTS.md Rule 6.5.4)
		// Row 1 Attenuverters
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(SIDES_TRIM_PARAM, -1.f, 1.f, 0.f, "Sides CV depth", "%", 0.f, 100.f);
		configParam(ANGLE_TRIM_PARAM, -1.f, 1.f, 0.f, "Angle CV depth", "%", 0.f, 100.f);
		configParam(PINCH_TRIM_PARAM, -1.f, 1.f, 0.f, "Pinch CV depth", "%", 0.f, 100.f);
		configParam(TWIST_TRIM_PARAM, -1.f, 1.f, 0.f, "Twist CV depth", "%", 0.f, 100.f);

		// Row 2 Attenuverters
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "Linear FM CV depth", "%", 0.f, 100.f);
		configParam(DIST_TRIM_PARAM, -1.f, 1.f, 0.f, "Distribution CV depth", "%", 0.f, 100.f);
		configParam(BEVEL_TRIM_PARAM, -1.f, 1.f, 0.f, "Bevel CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);
		configParam(BULGE_TRIM_PARAM, -1.f, 1.f, 0.f, "Bulge CV depth", "%", 0.f, 100.f);

		// Zone 4: I/O Jacks
		// Row 1
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(SIDES_CV_INPUT, "Sides CV");
		configInput(ANGLE_CV_INPUT, "Angle CV");
		configInput(PINCH_CV_INPUT, "Pinch CV");
		configInput(TWIST_CV_INPUT, "Twist CV");

		// Row 2
		configInput(FM_CV_INPUT, "External FM");
		configInput(DIST_CV_INPUT, "Distribution CV");
		configInput(BEVEL_CV_INPUT, "Bevel CV");
		configInput(PHASE_CV_INPUT, "Phase CV");
		configInput(BULGE_CV_INPUT, "Bulge CV");

		// Row 3
		configInput(SYNC_INPUT, "Sync");
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
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

		// Polyphony
		int maxCh = 1;
		for (int i = 0; i < INPUTS_LEN; i++) {
			maxCh = std::max(maxCh, inputs[i].getChannels());
		}
		int numChannels = std::min(maxCh, 16);
		outputs[SYNC_OUTPUT].setChannels(numChannels);
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		// Base Frequency
		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();
		float defaultF0   = calculateBaseFrequency(coarseParam, fineParam);

		// Geometry parameters
		float sidesParam  = params[SIDES_PARAM].getValue();
		float angleParam  = params[ANGLE_PARAM].getValue();
		float pinchParam  = params[PINCH_PARAM].getValue();
		float twistParam  = params[TWIST_PARAM].getValue();
		float bevelParam  = params[BEVEL_PARAM].getValue();
		float phaseParam  = params[PHASE_PARAM].getValue();
		float bulgeParam  = params[BULGE_PARAM].getValue();

		// Distribution knobs
		float splitParam  = params[SPLIT_PARAM].getValue();
		float pairParam   = params[PAIR_PARAM].getValue();
		float trioParam   = params[TRIO_PARAM].getValue();
		float groupParam  = params[GROUP_PARAM].getValue();
		float bunchParam  = params[BUNCH_PARAM].getValue();

		// Trimpots
		float fTrim      = params[FREQ_TRIM_PARAM].getValue();
		float sidesTrim  = params[SIDES_TRIM_PARAM].getValue();
		float angleTrim  = params[ANGLE_TRIM_PARAM].getValue();
		float pinchTrim  = params[PINCH_TRIM_PARAM].getValue();
		float twistTrim  = params[TWIST_TRIM_PARAM].getValue();

		float fmTrim     = params[FM_TRIM_PARAM].getValue();
		float distTrim   = params[DIST_TRIM_PARAM].getValue();
		float bevelTrim  = params[BEVEL_TRIM_PARAM].getValue();
		float phaseTrim  = params[PHASE_TRIM_PARAM].getValue();
		float bulgeTrim  = params[BULGE_TRIM_PARAM].getValue();

		bool fmConnected   = inputs[FM_CV_INPUT].isConnected();
		bool distConnected = inputs[DIST_CV_INPUT].isConnected();
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

			float pinchVal = clampf(pinchParam + (inputs[PINCH_CV_INPUT].getPolyVoltage(c) / 5.f) * pinchTrim, -1.f, 1.f);
			float twistVal = clampf(twistParam + (inputs[TWIST_CV_INPUT].getPolyVoltage(c) / 5.f) * twistTrim, -1.f, 1.f);
			float bevelVal = clampf(bevelParam + (inputs[BEVEL_CV_INPUT].getPolyVoltage(c) / 5.f) * bevelTrim, -1.f, 1.f);

			float phaseVal = (phaseParam + (inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f) * phaseTrim * 180.f) * (float)(M_PI / 180.0);
			float bulgeVal = clampf(bulgeParam + (inputs[BULGE_CV_INPUT].getPolyVoltage(c) / 5.f) * bulgeTrim, -1.f, 1.f);

			// Unified Distribution Modulation:
			// Modulates all 5 distribution parameters simultaneously in a bipolar fashion around current knob values
			float distMod = distConnected ? ((inputs[DIST_CV_INPUT].getPolyVoltage(c) / 5.f) * distTrim) : 0.f;
			float splitVal = clampf(splitParam + distMod, -1.f, 1.f);
			float pairVal  = clampf(pairParam  + distMod, -1.f, 1.f);
			float trioVal  = clampf(trioParam  + distMod, -1.f, 1.f);
			float groupVal = clampf(groupParam + distMod, -1.f, 1.f);
			float bunchVal = clampf(bunchParam + distMod, -1.f, 1.f);

			// Master phase traversal across polygon sides
			float p = vs.basePhase;
			float sideProgress = p * (float)N;
			int sideIdx = (int)sideProgress;
			if (sideIdx >= N) sideIdx = 0;
			float t = sideProgress - sideIdx;
			int nextIdx = (sideIdx + 1) % N;

			// Base Vertex Coordinates with Proposal 1 Bilateral Harmonic Star Adaptation:
			auto computeVertex = [&](int idx) -> std::pair<float, float> {
				float baseTheta = (float)idx * (2.f * (float)M_PI / (float)N);
				float theta = baseAngleRad + baseTheta;
				float step = (float)M_PI / (float)N;

				// 1. Bunch (k=1 dipole: bunches toward one pole)
				if (std::abs(bunchVal) > 0.001f) {
					theta += bunchVal * step * std::sin(theta);
				}
				// 2. Group (k=2 quadrupole: bilateral compression/expansion)
				if (std::abs(groupVal) > 0.001f) {
					theta += groupVal * step * std::sin(2.f * theta);
				}
				// 3. Trio (discrete 3-grouping: pulls triplets together ••• —— •••)
				if (std::abs(trioVal) > 0.001f) {
					int trioMod = idx % 3;
					float trioShift = (trioMod == 0) ? 1.0f : ((trioMod == 2) ? -1.0f : 0.0f);
					theta += trioVal * step * trioShift;
				}
				// 4. Pair (alternating adjacent edges: •• — •• — ••)
				if (std::abs(pairVal) > 0.001f) {
					theta += pairVal * step * ((idx % 2 == 0) ? 1.f : -1.f);
				}

				// 5. Split (Proposal 1: Bilateral harmonic star adaptation)
				// For even N: M = N/2, cos(k*pi) = (-1)^k (exact alternating star)
				// For odd N: M = (N-1)/2, seamlessly closes with bilateral shield/crest symmetry
				float r = 1.f;
				if (std::abs(splitVal) > 0.001f) {
					int M = N / 2;
					float splitWave = std::cos((float)M * (float)idx * (2.f * (float)M_PI / (float)N));
					r += splitVal * 0.40f * splitWave;
				}
				return {r * std::cos(theta), r * std::sin(theta)};
			};

			auto v1 = computeVertex(sideIdx);
			auto v2 = computeVertex(nextIdx);

			// Compute Secondary Vertex (Pinch & Twist)
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

			// Secondary vertex insertion with Pinch displacement
			float secX = baseMidX + (pinchVal * 0.75f) * normX;
			float secY = baseMidY + (pinchVal * 0.75f) * normY;

			// Interpolate position along sub-segments
			float curX, curY;
			bool useSecondary = (std::abs(pinchVal) > 0.001f || std::abs(twistVal) > 0.001f);
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

			// Corner Bevel: 3x depth!
			// Threshold extended to 0.40f (covering up to 40% of the half-edge) with 3x concave depth
			if (std::abs(bevelVal) > 0.001f) {
				float cornerDist = (t < 0.5f) ? t : (1.f - t);
				if (cornerDist < 0.40f) {
					float blend = (0.40f - cornerDist) / 0.40f;
					blend = blend * blend;
					if (bevelVal > 0.f) {
						// Convex bevel: 3x deeper arc curvature rounding
						float circR = 0.85f;
						float rad = std::sqrt(curX * curX + curY * curY);
						if (rad > 1e-5f) {
							float targetX = (curX / rad) * circR;
							float targetY = (curY / rad) * circR;
							curX = (1.f - blend * bevelVal) * curX + (blend * bevelVal) * targetX;
							curY = (1.f - blend * bevelVal) * curY + (blend * bevelVal) * targetY;
						}
					} else {
						// Concave cusp: 3x deeper inward notch (multiplier 2.1f vs previous 0.7f)
						float pull = clampf(blend * std::abs(bevelVal) * 2.1f, 0.f, 0.95f);
						curX *= (1.f - pull);
						curY *= (1.f - pull);
					}
				}
			}

			// Phase happens AFTER all vertex transformations, secondary vertices, and bevels
			float curR = std::sqrt(curX * curX + curY * curY);
			float curTheta = std::atan2(curY, curX);

			// Apply Global Phase Offset
			float finalX = curR * std::cos(curTheta + phaseVal);
			float finalY = curR * std::sin(curTheta + phaseVal);

			// Bulge happens LAST (Hardcoded Harmonograph Logarithmic Spiral with Depth Factor 4.0 for Polly)
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

		// 14 HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 5 Columns (X coordinates matching generator script)
		const float col_x[5] = { 8.56f, 22.06f, 35.56f, 49.06f, 62.56f };

		// 4 Symmetrically Spaced Columns for Bottom Row 3 Jacks
		const float jack3_x[4] = { 11.06f, 27.39f, 43.73f, 60.06f };

		// Row 1 Knobs: FREQ, RANGE, FINE, SIDES, ANGLE (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 21.59)), module, Polly::FREQ_PARAM));
		addChild(createLightCentered<PollyRangeLightWidget>(mm2px(Vec(col_x[1], 15.50)), module, Polly::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<TL1105>(mm2px(Vec(col_x[1], 21.59)), module, Polly::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 21.59)), module, Polly::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 21.59)), module, Polly::SIDES_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[4], 21.59)), module, Polly::ANGLE_PARAM));

		// Row 2 Knobs: Distribution (SPLIT, PAIR, TRIO, GROUP, BUNCH) (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 37.00)), module, Polly::SPLIT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 37.00)), module, Polly::PAIR_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 37.00)), module, Polly::TRIO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 37.00)), module, Polly::GROUP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[4], 37.00)), module, Polly::BUNCH_PARAM));

		// Row 3 Knobs: PINCH, TWIST, BEVEL, PHASE, BULGE (Center Y = 52.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 52.50)), module, Polly::PINCH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 52.50)), module, Polly::TWIST_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 52.50)), module, Polly::BEVEL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 52.50)), module, Polly::PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[4], 52.50)), module, Polly::BULGE_PARAM));

		// Zone 3: CV Attenuverter Trimpots (Center Y = 70.00 and 79.50 mm, matching Lisa)
		// Row 1 Attenuverters: FREQ, SIDES, ANGLE, PINCH, TWIST
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 70.00)), module, Polly::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 70.00)), module, Polly::SIDES_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 70.00)), module, Polly::ANGLE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 70.00)), module, Polly::PINCH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[4], 70.00)), module, Polly::TWIST_TRIM_PARAM));

		// Row 2 Attenuverters: FM, DIST, BEVEL, PHASE, BULGE
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 79.50)), module, Polly::FM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 79.50)), module, Polly::DIST_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 79.50)), module, Polly::BEVEL_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 79.50)), module, Polly::PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[4], 79.50)), module, Polly::BULGE_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs): FREQ, SIDES, ANGLE, PINCH, TWIST (Center Y = 94.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 94.50)), module, Polly::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 94.50)), module, Polly::SIDES_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 94.50)), module, Polly::ANGLE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 94.50)), module, Polly::PINCH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[4], 94.50)), module, Polly::TWIST_CV_INPUT));

		// Row 2 (Inputs): FM, DIST, BEVEL, PHASE, BULGE (Center Y = 106.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 106.00)), module, Polly::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 106.00)), module, Polly::DIST_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 106.00)), module, Polly::BEVEL_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 106.00)), module, Polly::PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[4], 106.00)), module, Polly::BULGE_CV_INPUT));

		// Row 3 (Sync & Outputs): SYNC, X, Y, SYNC (Center Y = 118.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jack3_x[0], 118.00)), module, Polly::SYNC_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack3_x[1], 118.00)), module, Polly::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack3_x[2], 118.00)), module, Polly::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(jack3_x[3], 118.00)), module, Polly::SYNC_OUTPUT));
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
