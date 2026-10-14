#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Ziggy — Stepped & Serpentine Spatial Raster Generator (8 HP)
//  Generate class module producing stepped or serpentine spatial raster
//  scans across the surround sound field.
//  Controls:
//    Row 1: Coarse Freq, 3-state Range (Very Slow / LFO / VCO), Fine Freq
//    Row 2: Scanlines (2 to 10 lines), Smoothness (Stepped -> Serpentine),
//           Phase (Spatial rotation +/-180°)
//  Attenuverters & Jacks:
//    Row 1 Inputs: Sync In, Freq CV, External FM
//    Row 2 Inputs: Scanlines CV, Smoothness CV, Phase CV
//    Row 3 Outputs: Sync Out, X Out, Y Out
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Bipolar triangle wave over phase [0, 1)
inline float triWave(float phase) {
	float p = phase - std::floor(phase);
	return 4.f * std::abs(p - 0.5f) - 1.f;
}

} // namespace

struct Ziggy : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum ParamId {
		FREQ_PARAM,
		FINE_PARAM,
		RANGE_PARAM,

		LINES_PARAM,
		SMOOTH_PARAM,
		PHASE_PARAM,

		// CV Attenuverters (Zone 3)
		FREQ_TRIM_PARAM,
		FM_TRIM_PARAM,

		LINES_TRIM_PARAM,
		SMOOTH_TRIM_PARAM,
		PHASE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Row 1
		SYNC_INPUT,
		FREQ_CV_INPUT,
		FM_CV_INPUT,

		// Row 2
		LINES_CV_INPUT,
		SMOOTH_CV_INPUT,
		PHASE_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Row 3
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
		Ziggy* getZiggy() {
			return dynamic_cast<Ziggy*>(module);
		}

		float getDisplayValue() override {
			Ziggy* ziggy = getZiggy();
			if (!ziggy) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (ziggy->paramQuantities.size() > Ziggy::FINE_PARAM) ? ziggy->params[Ziggy::FINE_PARAM].getValue() : 0.f;

			if (ziggy->rangeMode == Ziggy::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (ziggy->rangeMode == Ziggy::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Ziggy* ziggy = getZiggy();
			if (!ziggy) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (ziggy->rangeMode == Ziggy::RANGE_VERY_SLOW) {
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
			Ziggy* ziggy = getZiggy();
			if (!ziggy) return;

			float fine = (ziggy->paramQuantities.size() > Ziggy::FINE_PARAM) ? ziggy->params[Ziggy::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (ziggy->rangeMode == Ziggy::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (ziggy->rangeMode == Ziggy::RANGE_LFO) {
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
			Ziggy* ziggy = getZiggy();
			if (!ziggy) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (ziggy->rangeMode == Ziggy::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Ziggy* ziggy = getZiggy();
			if (!ziggy) {
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
				isPeriod = (ziggy->rangeMode == Ziggy::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Ziggy* ziggy = getZiggy();
			if (!ziggy) return "";
			return (ziggy->rangeMode == Ziggy::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	struct ScanlinesParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int lines = (int)std::round(getValue());
			return std::to_string(lines);
		}

		void setDisplayValueString(std::string s) override {
			char* endPtr = nullptr;
			float val = std::strtof(s.c_str(), &endPtr);
			if (endPtr != s.c_str()) {
				setValue(clampf(std::round(val), getMinValue(), getMaxValue()));
			}
		}
	};

	Ziggy() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Timebase Controls
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configButton(RANGE_PARAM, "Range time-scale");

		// Row 2 & 3: Raster Parameters
		configParam<ScanlinesParamQuantity>(LINES_PARAM, 2.f, 10.f, 4.f, "Scanlines", "");
		configParam(SMOOTH_PARAM, 0.f, 1.f, 0.5f, "Smoothness", "%", 0.f, 100.f);
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°", 0.f, 1.f);

		// Zone 3: CV Attenuverters (Mandatory naming per AGENTS.md Section 7.5.4)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "Linear FM CV depth", "%", 0.f, 100.f);

		configParam(LINES_TRIM_PARAM, -1.f, 1.f, 0.f, "Scanlines CV depth", "%", 0.f, 100.f);
		configParam(SMOOTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Smoothness CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);

		// Zone 4: Inputs (Strict sentence case, no redundant "input"/"IN" per Section 7.5.1)
		configInput(SYNC_INPUT, "Sync");
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(FM_CV_INPUT, "External FM");

		configInput(LINES_CV_INPUT, "Scanlines CV");
		configInput(SMOOTH_CV_INPUT, "Smoothness CV");
		configInput(PHASE_CV_INPUT, "Phase CV");

		// Zone 4: Outputs (Strict sentence case, no redundant "output"/"OUT" per Section 7.5.1)
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

		// Polyphony channel count
		int sCh    = inputs[SYNC_INPUT].getChannels();
		int fCvCh  = inputs[FREQ_CV_INPUT].getChannels();
		int fmCvCh = inputs[FM_CV_INPUT].getChannels();
		int liCvCh = inputs[LINES_CV_INPUT].getChannels();
		int smCvCh = inputs[SMOOTH_CV_INPUT].getChannels();
		int phCvCh = inputs[PHASE_CV_INPUT].getChannels();

		int numChannels = std::max({sCh, fCvCh, fmCvCh, liCvCh, smCvCh, phCvCh, 1});
		outputs[SYNC_OUTPUT].setChannels(numChannels);
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();
		float defaultF0   = calculateBaseFrequency(coarseParam, fineParam);

		float linesParam  = params[LINES_PARAM].getValue();
		float smoothParam = params[SMOOTH_PARAM].getValue();
		float phaseParam  = params[PHASE_PARAM].getValue();

		float fTrim      = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim     = params[FM_TRIM_PARAM].getValue();
		float linesTrim  = params[LINES_TRIM_PARAM].getValue();
		float smoothTrim = params[SMOOTH_TRIM_PARAM].getValue();
		float phaseTrim  = params[PHASE_TRIM_PARAM].getValue();

		bool fmConnected   = inputs[FM_CV_INPUT].isConnected();
		bool syncConnected = inputs[SYNC_INPUT].isConnected();

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
			float liCv = inputs[LINES_CV_INPUT].getPolyVoltage(c) / 5.f;
			float smCv = inputs[SMOOTH_CV_INPUT].getPolyVoltage(c) / 5.f;
			float phCv = inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f;

			float linesMod = clampf(linesParam + liCv * linesTrim * 8.f, 2.f, 10.f);
			int numLines = (int)std::round(linesMod);
			if (numLines < 2) numLines = 2;
			if (numLines > 10) numLines = 10;

			float currentSmooth = clampf(smoothParam + smCv * smoothTrim, 0.f, 1.f);
			float currentPhaseDeg = clampf(phaseParam + phCv * phaseTrim * 180.f, -180.f, 180.f);

			float u = vs.basePhase;

			// ── Stepped & Serpentine Raster Computation ──
			// Total lines = N
			// Master phase u in [0, 1) covers N horizontal sweeps.
			float w = u * (float)numLines;
			int k = (int)std::floor(w);
			if (k >= numLines) k = numLines - 1;
			float v = w - (float)k; // intra-line phase in [0, 1)

			// Fast X Axis:
			// In serpentine (boustrophedon) mode:
			// Even lines: Left to Right (-1 -> +1)
			// Odd lines: Right to Left (+1 -> -1)
			float xLinear = (k % 2 == 0) ? (2.f * v - 1.f) : (1.f - 2.f * v);
			// Smooth sinusoid for fast axis turnaround
			float xSinusoid = (k % 2 == 0) ? (-std::cos((float)M_PI * v)) : (std::cos((float)M_PI * v));
			float rawX = (1.f - currentSmooth) * xLinear + currentSmooth * xSinusoid;

			// Slow Y Axis:
			// Line center positions uniformly spaced in [-1, 1]:
			float yStep = -1.f + 2.f * ((float)k + 0.5f) / (float)numLines;
			// S-curve rounding during turnarounds (smooth vertical step)
			float sCurve = 0.5f * (1.f - std::cos((float)M_PI * v));
			float ySmoothStep = -1.f + 2.f * ((float)k + sCurve) / (float)numLines;

			// Blend from pure stepped raster (horizontal plateaus with discrete steps)
			// to smooth curvilinear serpentine ribbon
			float rawY = (1.f - currentSmooth) * yStep + currentSmooth * ySmoothStep;

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
struct ZiggyCustomButton : app::SvgSwitch {
	ZiggyCustomButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Custom Illuminated Ring Widget (Concentric Ring around Tactile Button) ──
struct ZiggyRangeLightRing : ModuleLightWidget {
	ZiggyRangeLightRing() {
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

struct ZiggyWidget : ModuleWidget {
	ZiggyWidget(Ziggy* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ziggy.svg")));

		// Row 1: FREQ & FINE Knobs (10.82 and 29.82 mm at Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 21.59)), module, Ziggy::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 21.59)), module, Ziggy::FINE_PARAM));

		// Range Button & Concentric Illuminated Ring at Center X = 20.32 mm (Y = 21.59 mm)
		addChild(createLightCentered<ZiggyRangeLightRing>(mm2px(Vec(20.32, 21.59)), module, Ziggy::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<ZiggyCustomButton>(mm2px(Vec(20.32, 21.59)), module, Ziggy::RANGE_PARAM));

		// Row 2: LINES & SMOOTH Knobs (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 37.00)), module, Ziggy::LINES_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 37.00)), module, Ziggy::SMOOTH_PARAM));

		// Row 3: PHASE Knob (Center X = 20.32 mm, Center Y = 52.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.32, 52.50)), module, Ziggy::PHASE_PARAM));

		// Zone 3: CV Attenuverter Trimpots (3 Columns: 8.82, 20.32, 31.82 mm)
		// Row 1 Attenuverters: FREQ (20.32), FM (31.82) at Center Y = 70.00 mm
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 70.00)), module, Ziggy::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 70.00)), module, Ziggy::FM_TRIM_PARAM));

		// Row 2 Attenuverters: LINES (8.82), SMOOTH (20.32), PHASE (31.82) at Center Y = 79.50 mm
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 79.50)), module, Ziggy::LINES_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 79.50)), module, Ziggy::SMOOTH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 79.50)), module, Ziggy::PHASE_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs): SYNC (8.82), FREQ (20.32), FM (31.82) at Center Y = 94.50 mm
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 94.50)), module, Ziggy::SYNC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 94.50)), module, Ziggy::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 94.50)), module, Ziggy::FM_CV_INPUT));

		// Row 2 (Inputs): LINES (8.82), SMOOTH (20.32), PHASE (31.82) at Center Y = 106.00 mm
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 106.00)), module, Ziggy::LINES_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 106.00)), module, Ziggy::SMOOTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 106.00)), module, Ziggy::PHASE_CV_INPUT));

		// Row 3 (Outputs): SYNC (8.82), X (20.32), Y (31.82) at Center Y = 118.00 mm
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.82, 118.00)), module, Ziggy::SYNC_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.32, 118.00)), module, Ziggy::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.82, 118.00)), module, Ziggy::Y_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Ziggy* module = dynamic_cast<Ziggy*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; i++) {
			Ziggy::RangeMode mode = (Ziggy::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}
	}
};

Model* modelZiggy = createModel<Ziggy, ZiggyWidget>("Ziggy");
