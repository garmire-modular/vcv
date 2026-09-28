#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────
//  Circe — Circle, Ellipse & Crescent Vector Generator (8 HP)
//  Generate class module producing geometric circular, elliptical,
//  and crescent vector trajectories with Coarse/Fine timebase, 3-state
//  Range oscillator (Very Slow / LFO / VCO), carrier-normalized linear FM
//  (with external override), knife-edge Crop (half-circles), intersecting
//  Circle Eclipse (C-shapes/crescents), bipolar Stretch (aspect ratio),
//  bipolar Phase (rotational orientation), and bidirectional frequency sync.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Circe : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum ParamId {
		FREQ_PARAM,
		FINE_PARAM,
		RANGE_PARAM,

		CROP_PARAM,
		ECLIPSE_PARAM,

		PHASE_PARAM,
		STRETCH_PARAM,

		FREQ_TRIM_PARAM,
		CROP_TRIM_PARAM,
		ECLIPSE_TRIM_PARAM,

		FM_TRIM_PARAM,
		PHASE_TRIM_PARAM,
		STRETCH_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		FREQ_CV_INPUT,
		CROP_CV_INPUT,
		ECLIPSE_CV_INPUT,

		FM_CV_INPUT,
		PHASE_CV_INPUT,
		STRETCH_CV_INPUT,

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

	struct VoiceState {
		float basePhase = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
	};

	VoiceState voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Circe* getCirce() {
			return dynamic_cast<Circe*>(module);
		}

		float getDisplayValue() override {
			Circe* circe = getCirce();
			if (!circe) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (circe->paramQuantities.size() > Circe::FINE_PARAM) ? circe->params[Circe::FINE_PARAM].getValue() : 0.f;

			if (circe->rangeMode == Circe::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (circe->rangeMode == Circe::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Circe* circe = getCirce();
			if (!circe) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (circe->rangeMode == Circe::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Circe* circe = getCirce();
			if (!circe) return;

			float fine = (circe->paramQuantities.size() > Circe::FINE_PARAM) ? circe->params[Circe::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (circe->rangeMode == Circe::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (circe->rangeMode == Circe::RANGE_LFO) {
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
			Circe* circe = getCirce();
			if (!circe) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (circe->rangeMode == Circe::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Circe* circe = getCirce();
			if (!circe) {
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
				isPeriod = (circe->rangeMode == Circe::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Circe* circe = getCirce();
			if (!circe) return "";
			return (circe->rangeMode == Circe::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	Circe() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Coarse Frequency (0.0 to 1.0)
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");

		// Fine Frequency (-1.0 to +1.0 for +/- 10% trim)
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		// Momentary Range Button
		configButton(RANGE_PARAM, "Range time-scale");

		// Crop (Bipolar ±100%)
		configParam(CROP_PARAM, -1.f, 1.f, 0.f, "Crop", "%", 0.f, 100.f);

		// Eclipse (Bipolar ±100%)
		configParam(ECLIPSE_PARAM, -1.f, 1.f, 0.f, "Eclipse", "%", 0.f, 100.f);

		// Phase Shift (Bipolar ±180°)
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°", 0.f, 1.f);

		// Stretch (Bipolar ±100%)
		configParam(STRETCH_PARAM, -1.f, 1.f, 0.f, "Stretch", "%", 0.f, 100.f);

		// CV Attenuverters (Mandatory naming per AGENTS.md Section 6.5.4)
		// Row 1 Attenuverters
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(CROP_TRIM_PARAM, -1.f, 1.f, 0.f, "Crop CV depth", "%", 0.f, 100.f);
		configParam(ECLIPSE_TRIM_PARAM, -1.f, 1.f, 0.f, "Eclipse CV depth", "%", 0.f, 100.f);

		// Row 2 Attenuverters
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "Linear FM CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);
		configParam(STRETCH_TRIM_PARAM, -1.f, 1.f, 0.f, "Stretch CV depth", "%", 0.f, 100.f);

		// Inputs: Row 1
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(CROP_CV_INPUT, "Crop CV");
		configInput(ECLIPSE_CV_INPUT, "Eclipse CV");

		// Inputs: Row 2
		configInput(FM_CV_INPUT, "External FM");
		configInput(PHASE_CV_INPUT, "Phase CV");
		configInput(STRETCH_CV_INPUT, "Stretch CV");

		// Sync
		configInput(SYNC_INPUT, "Sync");

		// Outputs: Row 3
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
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
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
		int cCvCh  = inputs[CROP_CV_INPUT].getChannels();
		int eCvCh  = inputs[ECLIPSE_CV_INPUT].getChannels();
		int pCvCh  = inputs[PHASE_CV_INPUT].getChannels();
		int sCvCh  = inputs[STRETCH_CV_INPUT].getChannels();
		int sCh    = inputs[SYNC_INPUT].getChannels();

		int numChannels = std::max({fCvCh, fmCvCh, cCvCh, eCvCh, pCvCh, sCvCh, sCh, 1});
		outputs[SYNC_OUTPUT].setChannels(numChannels);
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();

		float defaultF0 = calculateBaseFrequency(coarseParam, fineParam);

		float cropParam    = params[CROP_PARAM].getValue();
		float eclipseParam = params[ECLIPSE_PARAM].getValue();
		float phaseParam   = params[PHASE_PARAM].getValue();
		float stretchParam = params[STRETCH_PARAM].getValue();

		float fTrim       = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim      = params[FM_TRIM_PARAM].getValue();
		float cropTrim    = params[CROP_TRIM_PARAM].getValue();
		float eclipseTrim = params[ECLIPSE_TRIM_PARAM].getValue();
		float phaseTrim   = params[PHASE_TRIM_PARAM].getValue();
		float stretchTrim = params[STRETCH_TRIM_PARAM].getValue();

		bool fmConnected      = inputs[FM_CV_INPUT].isConnected();
		bool eclipseConnected = inputs[ECLIPSE_CV_INPUT].isConnected();
		bool stretchConnected = inputs[STRETCH_CV_INPUT].isConnected();
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

			// Advance base phase
			vs.basePhase += fActual * args.sampleTime;

			// Emit sync pulse when voice completes a master cycle
			if (!syncTriggered && vs.basePhase >= 1.f) {
				vs.syncPulse.trigger(1e-4f);
			}

			// Wrap base phase to [0.0, 1.0)
			vs.basePhase -= std::floor(vs.basePhase);

			// Modulations:
			// Eclipse normalizes from Crop CV if unpatched
			float cCv = inputs[CROP_CV_INPUT].getPolyVoltage(c) / 5.f;
			float eCv = eclipseConnected ? (inputs[ECLIPSE_CV_INPUT].getPolyVoltage(c) / 5.f) : cCv;
			float cropVal = clampf(cropParam + cCv * cropTrim, -1.f, 1.f);
			float eclipseVal = clampf(eclipseParam + eCv * eclipseTrim, -1.f, 1.f);

			// Stretch normalizes from Phase CV if unpatched
			float pCv = inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float sCv = stretchConnected ? (inputs[STRETCH_CV_INPUT].getPolyVoltage(c) / 5.f) : pCv;
			float stretchVal = clampf(stretchParam + sCv * stretchTrim, -1.f, 1.f);

			// Phase shift modulation (bipolar ±180°)
			float phaseDeg = clampf(phaseParam + pCv * phaseTrim * 180.f, -180.f, 180.f);
			float phaseRad = phaseDeg * (float)(M_PI / 180.0);

			// ── 1. Base Geometry & Eclipse ──
			// Smooth closed crescent / C-shape parametrization with uniform traversal velocity
			float rawX, rawY;
			if (std::abs(eclipseVal) < 1e-4f) {
				// Pure unit circle
				float th = 2.f * (float)M_PI * vs.basePhase;
				rawX = std::cos(th);
				rawY = std::sin(th);
			} else {
				float sign = (eclipseVal > 0.f) ? 1.0f : -1.0f;
				float val = std::abs(eclipseVal);
				float d = 2.0f - val * 1.85f;
				float alpha = std::acos(clampf(d * 0.5f, -1.0f, 1.0f));
				float pSplit = 1.0f - alpha * (float)(1.0 / M_PI);

				if (vs.basePhase < pSplit) {
					float th = alpha + 2.f * (float)M_PI * vs.basePhase;
					rawX = std::cos(th);
					rawY = std::sin(th);
				} else {
					float u = (vs.basePhase - pSplit) / std::max(1e-6f, alpha * (float)(1.0 / M_PI));
					float phi = ((float)M_PI + alpha) - u * 2.f * alpha;
					rawX = d + std::cos(phi);
					rawY = std::sin(phi);
				}

				if (sign < 0.f) {
					rawX = -rawX;
				}
			}

			// ── 2. Knife-edge Crop ──
			// Bipolar: >0 crops from right edge (+X inward), <0 crops from left edge (-X inward)
			// At ±1.0 produces exact straight-edge half-circles
			if (cropVal > 1e-4f) {
				float xCut = 1.0f - cropVal * 1.0f;
				if (rawX > xCut) rawX = xCut;
			} else if (cropVal < -1e-4f) {
				float xCut = -1.0f - cropVal * 1.0f;
				if (rawX < xCut) rawX = xCut;
			}

			// ── 3. Bipolar Stretch (Aspect Ratio / Ellipse Morph) ──
			// >0 stretches horizontal ellipse (compresses Y), <0 stretches vertical ellipse (compresses X)
			// Preserves 5V peak bounds without rail clipping
			if (std::abs(stretchVal) > 1e-4f) {
				float sx = (stretchVal >= 0.f) ? 1.0f : std::max(0.05f, 1.0f + stretchVal * 0.95f);
				float sy = (stretchVal <= 0.f) ? 1.0f : std::max(0.05f, 1.0f - stretchVal * 0.95f);
				rawX *= sx;
				rawY *= sy;
			}

			// ── 4. Rotational Phase Orientation ──
			// Rotates the final 2D shape smoothly around origin
			if (std::abs(phaseRad) > 1e-4f) {
				float cosP = std::cos(phaseRad);
				float sinP = std::sin(phaseRad);
				float rx = rawX * cosP - rawY * sinP;
				float ry = rawX * sinP + rawY * cosP;
				rawX = rx;
				rawY = ry;
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
struct TRangeLight : TBase {
	TRangeLight() {
		// Range 0: Very Slow = #e1be6a (Warm Gold)
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff));
		// Range 1: LFO = #40b0a6 (Teal)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff));
		// Range 2: VCO = #d35fb7 (Magenta)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff));
	}
};
struct RangeLightWidget : SmallLight<TRangeLight<>> {};

struct CirceWidget : ModuleWidget {
	CirceWidget(Circe* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Circe.svg")));

		// 8 HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Row 1: FREQ & FINE Knobs (10.82 and 29.82 mm at Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 21.59)), module, Circe::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 21.59)), module, Circe::FINE_PARAM));

		// Row 1 Center: Range LED (15.50 mm) and Range Button (21.59 mm, horizontally aligned with knobs)
		addChild(createLightCentered<RangeLightWidget>(mm2px(Vec(20.32, 15.50)), module, Circe::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<TL1105>(mm2px(Vec(20.32, 21.59)), module, Circe::RANGE_PARAM));

		// Row 2: CROP & ECLIPSE Knobs (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 37.00)), module, Circe::CROP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 37.00)), module, Circe::ECLIPSE_PARAM));

		// Row 3: PHASE & STRETCH Knobs (Center Y = 52.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 52.50)), module, Circe::PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 52.50)), module, Circe::STRETCH_PARAM));

		// Zone 3: CV Attenuverter Trimpots (3 Columns: 8.82, 20.32, 31.82 mm)
		// Row 1 Attenuverters: FREQ, CROP, ECLIPSE (Center Y = 70.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 70.00)), module, Circe::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 70.00)), module, Circe::CROP_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 70.00)), module, Circe::ECLIPSE_TRIM_PARAM));

		// Row 2 Attenuverters: FM, PHASE, STRETCH (Center Y = 79.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 79.50)), module, Circe::FM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 79.50)), module, Circe::PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 79.50)), module, Circe::STRETCH_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs): FREQ, CROP, ECLIPSE (Center Y = 94.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 94.50)), module, Circe::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 94.50)), module, Circe::CROP_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 94.50)), module, Circe::ECLIPSE_CV_INPUT));

		// Row 2 (Inputs): FM, PHASE, STRETCH (Center Y = 106.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 106.00)), module, Circe::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 106.00)), module, Circe::PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 106.00)), module, Circe::STRETCH_CV_INPUT));

		// Row 3 (Sync & Outputs): SYNC IN, X OUT, Y OUT, SYNC OUT (Center Y = 118.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.07, 118.00)), module, Circe::SYNC_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.57, 118.00)), module, Circe::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.07, 118.00)), module, Circe::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(34.57, 118.00)), module, Circe::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Circe* module = dynamic_cast<Circe*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; i++) {
			Circe::RangeMode mode = (Circe::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}
	}
};

Model* modelCirce = createModel<Circe, CirceWidget>("Circe");
