#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Crissy — Diagonal-Criss Cross Vector Generator (8 HP)
//  Generate class module producing rapid spatial crossings cutting
//  straight through the acoustic center, with adjustable center pinch dwell.
//  Morphable patterns: X, Bow-tie, and Asterisk.
//  Controls:
//    Row 1: Coarse Freq, 3-state Range (Very Slow / LFO / VCO), Fine Freq
//    Row 2: Shape (X -> Bow-tie -> Asterisk), Pinch (Center dwell/snap)
//    Row 3: Phase (Rotational orientation +/-180°)
//  Attenuverters & Jacks:
//    Row 1: Freq CV, External FM, Sync In
//    Row 2: Shape CV, Pinch CV, Phase CV
//    Row 3 (Outputs): X, Y, Sync Out
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Crissy : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum ParamId {
		FREQ_PARAM,
		FINE_PARAM,
		RANGE_PARAM,

		SHAPE_PARAM,
		PINCH_PARAM,
		PHASE_PARAM,

		// CV Attenuverters (Zone 3)
		FREQ_TRIM_PARAM,
		FM_TRIM_PARAM,

		SHAPE_TRIM_PARAM,
		PINCH_TRIM_PARAM,
		PHASE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Row 1
		FREQ_CV_INPUT,
		FM_CV_INPUT,
		SYNC_INPUT,

		// Row 2
		SHAPE_CV_INPUT,
		PINCH_CV_INPUT,
		PHASE_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Row 3
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

	struct VoiceState {
		float basePhase = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
	};

	VoiceState voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Crissy* getCrissy() {
			return dynamic_cast<Crissy*>(module);
		}

		float getDisplayValue() override {
			Crissy* crissy = getCrissy();
			if (!crissy) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (crissy->paramQuantities.size() > Crissy::FINE_PARAM) ? crissy->params[Crissy::FINE_PARAM].getValue() : 0.f;

			if (crissy->rangeMode == Crissy::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (crissy->rangeMode == Crissy::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Crissy* crissy = getCrissy();
			if (!crissy) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (crissy->rangeMode == Crissy::RANGE_VERY_SLOW) {
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

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Crissy* crissy = getCrissy();
			if (!crissy) return;

			float fine = (crissy->paramQuantities.size() > Crissy::FINE_PARAM) ? crissy->params[Crissy::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (crissy->rangeMode == Crissy::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (crissy->rangeMode == Crissy::RANGE_LFO) {
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
			Crissy* crissy = getCrissy();
			if (!crissy) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (crissy->rangeMode == Crissy::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Crissy* crissy = getCrissy();
			if (!crissy) {
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
				isPeriod = (crissy->rangeMode == Crissy::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Crissy* crissy = getCrissy();
			if (!crissy) return "";
			return (crissy->rangeMode == Crissy::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	struct ShapeParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float val = getValue();
			char buf[64];
			if (val < 0.05f) {
				return "X";
			} else if (val < 0.95f) {
				std::snprintf(buf, sizeof(buf), "X -> Bow-tie (%.0f%%)", val * 100.f);
				return std::string(buf);
			} else if (val <= 1.05f) {
				return "Bow-tie";
			} else if (val < 1.95f) {
				std::snprintf(buf, sizeof(buf), "Bow-tie -> Asterisk (%.0f%%)", (val - 1.f) * 100.f);
				return std::string(buf);
			} else {
				return "Asterisk";
			}
		}
	};

	Crissy() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Timebase Controls
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configButton(RANGE_PARAM, "Range time-scale");

		// Row 2 & 3: Pattern & Orientation Controls
		configParam<ShapeParamQuantity>(SHAPE_PARAM, 0.f, 2.f, 0.f, "Shape", "");
		configParam(PINCH_PARAM, -1.f, 1.f, 0.f, "Pinch", "%", 0.f, 100.f);
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°", 0.f, 1.f);

		// Zone 3: CV Attenuverters (Mandatory naming per AGENTS.md Section 7.5.4)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "Linear FM CV depth", "%", 0.f, 100.f);

		configParam(SHAPE_TRIM_PARAM, -1.f, 1.f, 0.f, "Shape CV depth", "%", 0.f, 100.f);
		configParam(PINCH_TRIM_PARAM, -1.f, 1.f, 0.f, "Pinch CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);

		// Zone 4: Inputs (Strict sentence case, no redundant "input"/"IN" per Section 7.5.1)
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(FM_CV_INPUT, "External FM");
		configInput(SYNC_INPUT, "Sync");

		configInput(SHAPE_CV_INPUT, "Shape CV");
		configInput(PINCH_CV_INPUT, "Pinch CV");
		configInput(PHASE_CV_INPUT, "Phase CV");

		// Zone 4: Outputs (Strict sentence case, no redundant "output"/"OUT" per Section 7.5.1)
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

		// Polyphony channel count
		int fCvCh  = inputs[FREQ_CV_INPUT].getChannels();
		int fmCvCh = inputs[FM_CV_INPUT].getChannels();
		int shCvCh = inputs[SHAPE_CV_INPUT].getChannels();
		int piCvCh = inputs[PINCH_CV_INPUT].getChannels();
		int phCvCh = inputs[PHASE_CV_INPUT].getChannels();
		int sCh    = inputs[SYNC_INPUT].getChannels();

		int numChannels = std::max({fCvCh, fmCvCh, shCvCh, piCvCh, phCvCh, sCh, 1});
		outputs[SYNC_OUTPUT].setChannels(numChannels);
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();
		float defaultF0   = calculateBaseFrequency(coarseParam, fineParam);

		float shapeParam = params[SHAPE_PARAM].getValue();
		float pinchParam = params[PINCH_PARAM].getValue();
		float phaseParam = params[PHASE_PARAM].getValue();

		float fTrim     = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim    = params[FM_TRIM_PARAM].getValue();
		float shapeTrim = params[SHAPE_TRIM_PARAM].getValue();
		float pinchTrim = params[PINCH_TRIM_PARAM].getValue();
		float phaseTrim = params[PHASE_TRIM_PARAM].getValue();

		bool fmConnected    = inputs[FM_CV_INPUT].isConnected();
		bool syncConnected  = inputs[SYNC_INPUT].isConnected();

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
			// Unpatched: Carrier normalized self-modulation
			// Patched: External FM input override
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

			// Emit sync pulse when completing a master cycle
			if (!syncTriggered && vs.basePhase >= 1.f) {
				vs.syncPulse.trigger(1e-4f);
			}

			// Wrap base phase to [0.0, 1.0)
			vs.basePhase -= std::floor(vs.basePhase);

			// Parameter modulations
			float shCv = inputs[SHAPE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float piCv = inputs[PINCH_CV_INPUT].getPolyVoltage(c) / 5.f;
			float phCv = inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f;

			float currentShape = clampf(shapeParam + shCv * shapeTrim, 0.f, 2.f);
			float currentPinch = clampf(pinchParam + piCv * pinchTrim, -1.f, 1.f);
			float currentPhaseDeg = clampf(phaseParam + phCv * phaseTrim * 180.f, -180.f, 180.f);

			float u = vs.basePhase;

			// Helper for center pinch dwell:
			// s in [-1, 1], with s = 0 at the acoustic center.
			// Pinch > 0: Dwell at origin (phase velocity slows down near s = 0)
			// Pinch < 0: Rapid spatial crossing (velocity snaps through origin)
			auto applyPinchDwell = [](float s, float p) -> float {
				float sign = (s >= 0.f) ? 1.0f : -1.0f;
				float absS = std::abs(s);
				if (absS < 1e-7f) return 0.f;
				if (p > 0.f) {
					// Exponent > 1: Linger at center
					float exponent = 1.0f + 3.0f * p;
					return sign * std::pow(absS, exponent);
				} else if (p < 0.f) {
					// Exponent < 1: Slingshot through center
					float exponent = 1.0f / (1.0f + 3.0f * (-p));
					return sign * std::pow(absS, exponent);
				}
				return s;
			};

			// ── 1. Pattern: X (Diagonal Criss-Cross) ──
			// Two crossing diagonal strokes: Stroke 1 (0..0.5) from (-1,-1) to (1,1);
			// Stroke 2 (0.5..1.0) from (-1,1) to (1,-1). Both slice straight through (0,0).
			float x_X = 0.f, y_X = 0.f;
			if (u < 0.5f) {
				float w = u * 2.f; // 0 to 1
				float s = applyPinchDwell(w * 2.f - 1.f, currentPinch);
				x_X = s;
				y_X = s;
			} else {
				float w = (u - 0.5f) * 2.f; // 0 to 1
				float s = applyPinchDwell(w * 2.f - 1.f, currentPinch);
				x_X = s;
				y_X = -s;
			}

			// ── 2. Pattern: Bow-tie (Hourglass / Lemniscate) ──
			// Smooth closed figure-8 trajectory slicing through origin at u = 0.25 and u = 0.75
			float thetaBt = 2.f * (float)M_PI * u;
			float sxBt = std::cos(thetaBt);
			float syBt = std::sin(2.f * thetaBt); // crosses (0,0) twice per cycle

			// Apply pinch dwell to distance from origin
			float rBt = std::sqrt(sxBt * sxBt + syBt * syBt);
			float rBtMod = rBt;
			if (rBt > 1e-6f) {
				if (currentPinch > 0.f) {
					rBtMod = std::pow(rBt, 1.0f + 2.0f * currentPinch);
				} else if (currentPinch < 0.f) {
					rBtMod = std::pow(rBt, 1.0f / (1.0f + 2.0f * (-currentPinch)));
				}
			}
			float scaleBt = (rBt > 1e-6f) ? (rBtMod / rBt) : 0.f;
			float x_Bt = sxBt * scaleBt;
			float y_Bt = syBt * scaleBt;

			// ── 3. Pattern: Asterisk (Radial Star) ──
			// 4 crossing axes (8 rays: 0°, 45°, 90°, 135°), all cutting through center (0,0)
			float wAst = u * 4.f;
			int k = (int)std::floor(wAst);
			if (k > 3) k = 3;
			float localWAst = wAst - (float)k;
			float sAst = applyPinchDwell(localWAst * 2.f - 1.f, currentPinch);
			float angleK = (float)k * ((float)M_PI * 0.25f);
			float x_Ast = sAst * std::cos(angleK);
			float y_Ast = sAst * std::sin(angleK);

			// Morph continuously across the 3 patterns:
			// 0..1: X -> Bow-tie
			// 1..2: Bow-tie -> Asterisk
			float rawX = 0.f, rawY = 0.f;
			if (currentShape <= 1.0f) {
				float m = currentShape;
				rawX = (1.f - m) * x_X + m * x_Bt;
				rawY = (1.f - m) * y_X + m * y_Bt;
			} else {
				float m = currentShape - 1.0f;
				rawX = (1.f - m) * x_Bt + m * x_Ast;
				rawY = (1.f - m) * y_Bt + m * y_Ast;
			}

			// ── Phase Rotation (Bipolar ±180°) ──
			float phaseRad = currentPhaseDeg * (float)(M_PI / 180.0);
			float cosP = std::cos(phaseRad);
			float sinP = std::sin(phaseRad);
			float rotX = rawX * cosP - rawY * sinP;
			float rotY = rawX * sinP + rawY * cosP;

			// Eurorack standard 10Vpp (±5V) normalized vector coordinates
			outputs[X_OUTPUT].setVoltage(clampf(rotX * 5.f, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(rotY * 5.f, -12.f, 12.f), c);

			// Sync output pulse
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

// ── Custom Hardware Tactile Button Widget ──
struct CrissyCustomButton : app::SvgSwitch {
	CrissyCustomButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Custom Illuminated Ring Widget (Concentric Ring around Tactile Button) ──
struct CrissyRangeLightRing : ModuleLightWidget {
	CrissyRangeLightRing() {
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

		// 50% Visibility Rule
		haloColor.a *= 0.175f;

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, haloColor);
		nvgStrokeWidth(args.vg, mm2px(1.40f) + haloW);
		nvgStroke(args.vg);
	}
};

struct CrissyWidget : ModuleWidget {
	CrissyWidget(Crissy* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Crissy.svg")));

		// Row 1: FREQ & FINE Knobs (10.82 and 29.82 mm at Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 21.59)), module, Crissy::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 21.59)), module, Crissy::FINE_PARAM));

		// Range Button & Concentric Illuminated Ring at Center X = 20.32 mm (Y = 21.59 mm)
		addChild(createLightCentered<CrissyRangeLightRing>(mm2px(Vec(20.32, 21.59)), module, Crissy::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<CrissyCustomButton>(mm2px(Vec(20.32, 21.59)), module, Crissy::RANGE_PARAM));

		// Row 2: SHAPE & PINCH Knobs (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 37.00)), module, Crissy::SHAPE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 37.00)), module, Crissy::PINCH_PARAM));

		// Row 3: PHASE Knob (Center X = 20.32 mm, Center Y = 52.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.32, 52.50)), module, Crissy::PHASE_PARAM));

		// Zone 3: CV Attenuverter Trimpots (3 Columns: 8.82, 20.32, 31.82 mm)
		// Row 1 Attenuverters: FREQ, FM (Center Y = 70.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 70.00)), module, Crissy::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 70.00)), module, Crissy::FM_TRIM_PARAM));

		// Row 2 Attenuverters: SHAPE, PINCH, PHASE (Center Y = 79.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 79.50)), module, Crissy::SHAPE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 79.50)), module, Crissy::PINCH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 79.50)), module, Crissy::PHASE_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs): FREQ, FM, SYNC IN (Center Y = 94.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 94.50)), module, Crissy::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 94.50)), module, Crissy::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 94.50)), module, Crissy::SYNC_INPUT));

		// Row 2 (Inputs): SHAPE, PINCH, PHASE (Center Y = 106.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 106.00)), module, Crissy::SHAPE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 106.00)), module, Crissy::PINCH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 106.00)), module, Crissy::PHASE_CV_INPUT));

		// Row 3 (Outputs): X, Y, SYNC OUT (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.82, 118.00)), module, Crissy::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.32, 118.00)), module, Crissy::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.82, 118.00)), module, Crissy::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Crissy* module = dynamic_cast<Crissy*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; i++) {
			Crissy::RangeMode mode = (Crissy::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}
	}
};

Model* modelCrissy = createModel<Crissy, CrissyWidget>("Crissy");
