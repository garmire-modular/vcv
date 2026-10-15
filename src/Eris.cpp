#include "plugin.hpp"
#include "ErisEngine.hpp"
#include <cmath>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Eris — Stochastic & Chaos Trajectory Engine (14 HP)
//  Generate class module based on 18 chaotic strange attractors & drift.
//  Controls:
//    Row 1: Coarse Freq, 3-state Range (Very Slow / LFO / VCO), Fine Freq, Model
//    Row 2: Complex, Orbit (Bipolar 3D tumble speed), Reseed trigger button, Reseed Rate knob
//  Attenuverters & Jacks:
//    Trimpot Row 1: Freq CV, FM CV, Model CV
//    Trimpot Row 2: Complex CV, Orbit CV, Reseed Rate CV
//    Jack Row 1 (Inputs): Freq CV, FM CV, Model CV, Complex CV
//    Jack Row 2 (Inputs): Orbit CV, Reseed Trig In, Reseed Rate CV, Sync In
//    Jack Row 3 (Outputs): X, Y, Z (Color blanking/depth VCA CV), Sync Out
// ─────────────────────────────────────────────────────────────────────

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Eris : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum ZOutputMode {
		Z_UNIPOLAR_10V = 0,   // 0V to +10V (ideal for 3xVCA / color depth)
		Z_BIPOLAR_5V          // -5V to +5V
	};

	enum ParamId {
		// Row 1
		FREQ_PARAM,
		FINE_PARAM,
		RANGE_PARAM,
		MODEL_PARAM,

		// Row 2
		COMPLEX_PARAM,
		ORBIT_PARAM,
		RESEED_BUTTON_PARAM,
		RATE_PARAM,

		// CV Attenuverters (Zone 3)
		FREQ_TRIM_PARAM,
		FM_TRIM_PARAM,
		MODEL_TRIM_PARAM,

		COMPLEX_TRIM_PARAM,
		ORBIT_TRIM_PARAM,
		RATE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Jack Row 1 (Inputs)
		FREQ_CV_INPUT,
		FM_CV_INPUT,
		MODEL_CV_INPUT,
		COMPLEX_CV_INPUT,

		// Jack Row 2 (Inputs)
		ORBIT_CV_INPUT,
		RESEED_TRIG_INPUT,
		RATE_CV_INPUT,
		SYNC_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Jack Row 3 (Outputs)
		X_OUTPUT,
		Y_OUTPUT,
		Z_OUTPUT,
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
	ZOutputMode zMode = Z_UNIPOLAR_10V;
	dsp::BooleanTrigger rangeTrigger;
	dsp::BooleanTrigger reseedButtonTrigger;

	struct VoiceState {
		float basePhase = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::SchmittTrigger extReseedTrigger;
		dsp::PulseGenerator syncPulse;

		// 3D Orbit Tumbler Angles
		float thetaX = 0.f;
		float thetaY = 0.f;
		float thetaZ = 0.f;

		eris::OrbitBuffer activeOrbit;
		eris::OrbitBuffer targetOrbit;
		int currentSeed = 100;
		float lastComplexity = -1.f;
		int lastModel = -1;

		float crossfadeTau = 1.0f;
		bool isCrossfading = false;
		float reseedAccum = 0.f;
		bool initialized = false;
	};

	VoiceState voices[16];

	float calculateBaseFrequency(float coarse, float fine) {
		if (rangeMode == RANGE_VERY_SLOW) {
			float periodCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float period = periodCoarse * (1.0f - fine * 0.10f);
			if (period < 0.001f) period = 0.001f;
			return 1.0f / period;
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + (200.0f - 0.01f) * (coarse * coarse);
			return clampf(fCoarse + fine * 20.0f, 0.001f, 250.0f);
		} else {
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
		}
	}

	struct FreqParamQuantity : ParamQuantity {
		Eris* getEris() {
			return dynamic_cast<Eris*>(this->module);
		}

		float getDisplayValue() override {
			Eris* eris = getEris();
			if (!eris) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (eris->paramQuantities.size() > Eris::FINE_PARAM) ? eris->params[Eris::FINE_PARAM].getValue() : 0.f;
			float freq = eris->calculateBaseFrequency(coarse, fine);

			if (eris->rangeMode == Eris::RANGE_VERY_SLOW) {
				return (freq > 1e-6f) ? (1.0f / freq) : 600.0f;
			}
			return freq;
		}

		std::string getUnit() override {
			Eris* eris = getEris();
			if (!eris) return " Hz";
			return (eris->rangeMode == Eris::RANGE_VERY_SLOW) ? " s" : " Hz";
		}

		std::string getDisplayValueString() override {
			Eris* eris = getEris();
			if (!eris) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (eris->rangeMode == Eris::RANGE_VERY_SLOW) {
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
	};

	struct ModelParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int idx = eris::clampVal((int)std::round(getValue()), 0, (int)eris::NUM_MODELS - 1);
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%d: %s", idx + 1, eris::METADATA[idx].name);
			return buf;
		}
	};

	struct OrbitParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue(); // -1.0 to +1.0 Hz
			if (std::abs(v) < 0.005f) {
				return "0.00 Hz (halted)";
			} else if (v > 0.0f) {
				char buf[32];
				std::snprintf(buf, sizeof(buf), "+%.2f Hz (CW)", v);
				return buf;
			} else {
				char buf[32];
				std::snprintf(buf, sizeof(buf), "%.2f Hz (CCW)", v);
				return buf;
			}
		}
	};

	struct ReseedRateParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float r = getValue();
			if (std::abs(r) < 0.05f) {
				return "Off (does not repeat)";
			} else if (r > 0.0f) {
				float mult = std::pow(2.0f, r);
				char buf[32];
				std::snprintf(buf, sizeof(buf), "x%.2f", mult);
				return buf;
			} else {
				float div = std::pow(2.0f, -r);
				char buf[32];
				std::snprintf(buf, sizeof(buf), "/%.2f", div);
				return buf;
			}
		}
	};

	Eris() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: FREQ, RANGE, FINE, MODEL
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency", " Hz");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configParam<ModelParamQuantity>(MODEL_PARAM, 0.f, 17.f, 0.f, "Model");

		// Row 2: COMPLEX, ORBIT, RESEED BUTTON, RESEED RATE
		configParam(COMPLEX_PARAM, 0.f, 1.f, 0.5f, "Macro complexity", "%", 0.f, 100.f);
		configParam<OrbitParamQuantity>(ORBIT_PARAM, -1.f, 1.f, 0.f, "Orbit speed", " Hz");
		configButton(RESEED_BUTTON_PARAM, "Manual reseed");
		configParam<ReseedRateParamQuantity>(RATE_PARAM, -7.f, 7.f, 0.f, "Reseed rate");

		// Zone 3: CV Attenuverters (Mandatory naming per AGENTS.md Section 7.5.4)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 1.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "External FM CV depth", "%", 0.f, 100.f);
		configParam(MODEL_TRIM_PARAM, -1.f, 1.f, 0.f, "Model CV depth", "%", 0.f, 100.f);

		configParam(COMPLEX_TRIM_PARAM, -1.f, 1.f, 0.f, "Macro complexity CV depth", "%", 0.f, 100.f);
		configParam(ORBIT_TRIM_PARAM, -1.f, 1.f, 0.f, "Orbit speed CV depth", "%", 0.f, 100.f);
		configParam(RATE_TRIM_PARAM, -1.f, 1.f, 0.f, "Reseed rate CV depth", "%", 0.f, 100.f);

		// Zone 4: Inputs (Mandatory sentence case, no redundant "input")
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(FM_CV_INPUT, "External FM");
		configInput(MODEL_CV_INPUT, "Model CV");
		configInput(COMPLEX_CV_INPUT, "Macro complexity CV");

		configInput(ORBIT_CV_INPUT, "Orbit CV");
		configInput(RESEED_TRIG_INPUT, "Reseed trigger");
		configInput(RATE_CV_INPUT, "Reseed rate CV");
		configInput(SYNC_INPUT, "Sync");

		// Zone 4: Outputs (Mandatory sentence case, no redundant "output")
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(Z_OUTPUT, "Z");
		configOutput(SYNC_OUTPUT, "Sync");
	}

	void triggerReseed(VoiceState& vs, int targetModel, float targetComplexity) {
		// Mutate seed to branch into a fresh, distinct chaotic path
		vs.currentSeed = ((vs.currentSeed * 1103515245 + 12345) & 0x7fffffff) % 1000 + 1;
		eris::generateOrbit(vs.targetOrbit, targetModel, vs.currentSeed, targetComplexity);
		vs.crossfadeTau = 0.0f;
		vs.isCrossfading = true;
		vs.syncPulse.trigger(1e-4f);
	}

	void process(const ProcessArgs& args) override {
		// Handle 3-state Range Button Trigger
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		bool manualReseedPressed = reseedButtonTrigger.process(params[RESEED_BUTTON_PARAM].getValue() > 0.5f);

		// Update 3-State LED Ring Indicator
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		int channels = std::max(1, inputs[FREQ_CV_INPUT].getChannels());
		if (inputs[FM_CV_INPUT].isConnected()) channels = std::max(channels, inputs[FM_CV_INPUT].getChannels());
		if (inputs[SYNC_INPUT].isConnected()) channels = std::max(channels, inputs[SYNC_INPUT].getChannels());
		if (inputs[MODEL_CV_INPUT].isConnected()) channels = std::max(channels, inputs[MODEL_CV_INPUT].getChannels());
		if (inputs[COMPLEX_CV_INPUT].isConnected()) channels = std::max(channels, inputs[COMPLEX_CV_INPUT].getChannels());
		if (inputs[ORBIT_CV_INPUT].isConnected()) channels = std::max(channels, inputs[ORBIT_CV_INPUT].getChannels());
		if (inputs[RESEED_TRIG_INPUT].isConnected()) channels = std::max(channels, inputs[RESEED_TRIG_INPUT].getChannels());
		if (inputs[RATE_CV_INPUT].isConnected()) channels = std::max(channels, inputs[RATE_CV_INPUT].getChannels());
		channels = std::min(channels, 16);

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
		outputs[Z_OUTPUT].setChannels(channels);
		outputs[SYNC_OUTPUT].setChannels(channels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam = params[FINE_PARAM].getValue();
		float defaultF0 = calculateBaseFrequency(coarseParam, fineParam);

		float fTrim = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim = params[FM_TRIM_PARAM].getValue();
		bool fmConnected = inputs[FM_CV_INPUT].isConnected();
		bool syncConnected = inputs[SYNC_INPUT].isConnected();

		float modelParam = params[MODEL_PARAM].getValue();
		float modelTrim = params[MODEL_TRIM_PARAM].getValue();

		float complexParam = params[COMPLEX_PARAM].getValue();
		float complexTrim = params[COMPLEX_TRIM_PARAM].getValue();

		float orbitParam = params[ORBIT_PARAM].getValue();
		float orbitTrim = params[ORBIT_TRIM_PARAM].getValue();

		float rateParam = params[RATE_PARAM].getValue();
		float rateTrim = params[RATE_TRIM_PARAM].getValue();

		const float twoPi = 2.0f * (float)M_PI;

		for (int c = 0; c < channels; ++c) {
			VoiceState& vs = voices[c];

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

			float f0 = (syncConnected && vs.syncPeriod > 0.f) ? (1.f / vs.syncPeriod) : defaultF0;

			// Frequency modulation (1V/Oct)
			float freqCv = inputs[FREQ_CV_INPUT].getPolyVoltage(c);
			float fCarrier = f0 * std::pow(2.f, freqCv * fTrim);

			// Linear FM modulation
			float fActual;
			if (!fmConnected) {
				float carrierSelfMod = std::sin(twoPi * vs.basePhase);
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

			// Model selection
			float mCv = inputs[MODEL_CV_INPUT].getPolyVoltage(c) / 5.f;
			float mVal = clampf(modelParam + mCv * modelTrim * 17.f, 0.f, 17.f);
			int currentModel = eris::clampVal((int)std::round(mVal), 0, (int)eris::NUM_MODELS - 1);

			// Complexity modulation
			float cCv = inputs[COMPLEX_CV_INPUT].getPolyVoltage(c) / 5.f;
			float currentComplexity = clampf(complexParam + cCv * complexTrim, 0.f, 1.f);

			// First-time initialization
			if (!vs.initialized) {
				vs.lastModel = currentModel;
				vs.lastComplexity = currentComplexity;
				eris::generateOrbit(vs.activeOrbit, currentModel, vs.currentSeed, currentComplexity);
				vs.initialized = true;
			} else {
				// Detect model or parameter change requiring orbit update
				bool modelChanged = (currentModel != vs.lastModel);
				bool complexityChanged = std::abs(currentComplexity - vs.lastComplexity) > 0.05f;
				if (modelChanged || complexityChanged) {
					vs.lastModel = currentModel;
					vs.lastComplexity = currentComplexity;
					eris::generateOrbit(vs.targetOrbit, currentModel, vs.currentSeed, currentComplexity);
					vs.crossfadeTau = 0.0f;
					vs.isCrossfading = true;
				}
			}

			// Orbit 3D Tumbling Speed & Polarities:
			// CW  (> 0): X +, Y -, Z +
			// CCW (< 0): X -, Y +, Z -
			// Max 1.0 Hz
			float orbCv = inputs[ORBIT_CV_INPUT].getPolyVoltage(c) / 5.f;
			float orbitSpeedHz = clampf(orbitParam + orbCv * orbitTrim, -1.f, 1.f);
			float omega = orbitSpeedHz * twoPi;

			vs.thetaX += omega * 1.000f * args.sampleTime;
			vs.thetaY += omega * (-0.618f) * args.sampleTime;
			vs.thetaZ += omega * 1.414f * args.sampleTime;

			vs.thetaX -= std::floor(vs.thetaX / twoPi) * twoPi;
			vs.thetaY -= std::floor(vs.thetaY / twoPi) * twoPi;
			vs.thetaZ -= std::floor(vs.thetaZ / twoPi) * twoPi;

			// Reseed clock & triggers
			float rCv = inputs[RATE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float currentRate = clampf(rateParam + rCv * rateTrim * 7.f, -7.f, 7.f);

			bool extReseedTrig = false;
			if (inputs[RESEED_TRIG_INPUT].isConnected()) {
				extReseedTrig = vs.extReseedTrigger.process(inputs[RESEED_TRIG_INPUT].getPolyVoltage(c), 0.1f, 2.0f);
			}

			bool clockReseed = false;
			if (std::abs(currentRate) >= 0.05f) {
				float rateMultiplier = (currentRate > 0.f) ? std::pow(2.0f, currentRate) : (1.0f / std::pow(2.0f, -currentRate));
				float reseedFreq = std::max(1e-5f, fActual * rateMultiplier);
				vs.reseedAccum += reseedFreq * args.sampleTime;
				if (vs.reseedAccum >= 1.0f) {
					vs.reseedAccum -= std::floor(vs.reseedAccum);
					clockReseed = true;
				}
			} else {
				vs.reseedAccum = 0.f;
			}

			if (manualReseedPressed || extReseedTrig || clockReseed) {
				triggerReseed(vs, currentModel, currentComplexity);
			}

			// Sample active / target orbits with 5ms equal-power crossfade
			eris::OrbitPoint ptActive = eris::sampleOrbit(vs.activeOrbit, vs.basePhase);
			eris::OrbitPoint outPt = ptActive;

			if (vs.isCrossfading) {
				vs.crossfadeTau += args.sampleTime / 0.005f;
				if (vs.crossfadeTau >= 1.0f) {
					vs.crossfadeTau = 1.0f;
					vs.activeOrbit = vs.targetOrbit;
					vs.isCrossfading = false;
					outPt = eris::sampleOrbit(vs.activeOrbit, vs.basePhase);
				} else {
					eris::OrbitPoint ptTarget = eris::sampleOrbit(vs.targetOrbit, vs.basePhase);
					float angle = vs.crossfadeTau * (float)(M_PI * 0.5);
					float wOld = std::cos(angle);
					float wNew = std::sin(angle);
					outPt.x = wOld * ptActive.x + wNew * ptTarget.x;
					outPt.y = wOld * ptActive.y + wNew * ptTarget.y;
					outPt.z = wOld * ptActive.z + wNew * ptTarget.z;
				}
			}

			// 3D Euler Rotation around X, Y, Z:
			float rz0 = 2.0f * outPt.z - 1.0f; // Centered to [-1, 1]

			// 1. Rotate around X axis by thetaX:
			float cosX = std::cos(vs.thetaX);
			float sinX = std::sin(vs.thetaX);
			float x1 = outPt.x;
			float y1 = outPt.y * cosX - rz0 * sinX;
			float z1 = outPt.y * sinX + rz0 * cosX;

			// 2. Rotate around Y axis by thetaY:
			float cosY = std::cos(vs.thetaY);
			float sinY = std::sin(vs.thetaY);
			float x2 = x1 * cosY + z1 * sinY;
			float y2 = y1;
			float z2 = -x1 * sinY + z1 * cosY;

			// 3. Rotate around Z axis by thetaZ:
			float cosZ = std::cos(vs.thetaZ);
			float sinZ = std::sin(vs.thetaZ);
			float rotX = x2 * cosZ - y2 * sinZ;
			float rotY = x2 * sinZ + y2 * cosZ;
			float rotZ = z2;

			// Output scaling: Eurorack 10Vpp (±5V) for X and Y
			outputs[X_OUTPUT].setVoltage(clampf(rotX * 5.f, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(rotY * 5.f, -12.f, 12.f), c);

			// Z Output: 0V to +10V (ideal for 3xVCA / color depth) or ±5V bipolar
			float zNorm = eris::clampVal(0.5f + 0.5f * rotZ, 0.0f, 1.0f);
			float zVolts = (zMode == Z_UNIPOLAR_10V) ? (zNorm * 10.0f) : (rotZ * 5.0f);
			outputs[Z_OUTPUT].setVoltage(clampf(zVolts, -12.f, 12.f), c);

			// Sync output pulse
			outputs[SYNC_OUTPUT].setVoltage(vs.syncPulse.process(args.sampleTime) ? 10.f : 0.f, c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "zMode", json_integer((int)zMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) {
			rangeMode = (RangeMode)json_integer_value(rmJ);
		}
		json_t* zmJ = json_object_get(rootJ, "zMode");
		if (zmJ) {
			zMode = (ZOutputMode)json_integer_value(zmJ);
		}
	}
};

// ── Custom Hardware Tactile Button Widget ──
struct ErisCustomButton : app::SvgSwitch {
	ErisCustomButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Custom Illuminated Ring Widget (Concentric Ring around Range Button) ──
struct ErisRangeLightRing : ModuleLightWidget {
	ErisRangeLightRing() {
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
		haloColor.a *= 0.175f;

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, haloColor);
		nvgStrokeWidth(args.vg, mm2px(1.40f) + haloW);
		nvgStroke(args.vg);
	}
};

struct ErisWidget : ModuleWidget {
	ErisWidget(Eris* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Eris.svg")));

		// 4 Columns across 14 HP: [8.56, 26.23, 44.89, 62.56] mm
		const float col_x[4] = { 8.56f, 26.23f, 44.89f, 62.56f };

		// Row 1: FREQ, RANGE, FINE, MODEL (Center Y = 20.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 20.00f)), module, Eris::FREQ_PARAM));
		addChild(createLightCentered<ErisRangeLightRing>(mm2px(Vec(col_x[1], 20.00f)), module, Eris::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<ErisCustomButton>(mm2px(Vec(col_x[1], 20.00f)), module, Eris::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 20.00f)), module, Eris::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 20.00f)), module, Eris::MODEL_PARAM));

		// Row 2: COMPLEX, ORBIT, RESEED BUTTON, RATE (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 37.00f)), module, Eris::COMPLEX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 37.00f)), module, Eris::ORBIT_PARAM));
		addParam(createParamCentered<ErisCustomButton>(mm2px(Vec(col_x[2], 37.00f)), module, Eris::RESEED_BUTTON_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 37.00f)), module, Eris::RATE_PARAM));

		// Zone 3: CV Attenuverter Trimpots
		// Row 1: FREQ, FM, MODEL (Center Y = 66.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 66.00f)), module, Eris::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 66.00f)), module, Eris::FM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 66.00f)), module, Eris::MODEL_TRIM_PARAM));

		// Row 2: COMPLEX, ORBIT, RATE (Center Y = 77.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 77.00f)), module, Eris::COMPLEX_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 77.00f)), module, Eris::ORBIT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 77.00f)), module, Eris::RATE_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs): FREQ, FM, MODEL, COMPLEX (Center Y = 93.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 93.00f)), module, Eris::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 93.00f)), module, Eris::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 93.00f)), module, Eris::MODEL_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 93.00f)), module, Eris::COMPLEX_CV_INPUT));

		// Row 2 (Inputs): ORBIT, RESEED, RATE, SYNC (Center Y = 106.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 106.50f)), module, Eris::ORBIT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 106.50f)), module, Eris::RESEED_TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 106.50f)), module, Eris::RATE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 106.50f)), module, Eris::SYNC_INPUT));

		// Row 3 (Outputs): X, Y, Z, SYNC (Center Y = 120.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 120.00f)), module, Eris::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 120.00f)), module, Eris::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 120.00f)), module, Eris::Z_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 120.00f)), module, Eris::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Eris* module = dynamic_cast<Eris*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator);

		// Frequency Range sub-menu
		MenuLabel* rangeLabel = new MenuLabel();
		rangeLabel->text = "Frequency Range";
		menu->addChild(rangeLabel);

		static const char* rangeNames[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 Hz - 200 Hz)",
			"VCO (150 Hz - 2000 Hz)"
		};

		for (int r = 0; r < 3; ++r) {
			menu->addChild(createCheckMenuItem(rangeNames[r], "",
				[module, r]() { return module->rangeMode == r; },
				[module, r]() { module->rangeMode = (Eris::RangeMode)r; }
			));
		}

		menu->addChild(new MenuSeparator);

		// Z Output Range sub-menu
		MenuLabel* zLabel = new MenuLabel();
		zLabel->text = "Z Output Range";
		menu->addChild(zLabel);

		menu->addChild(createCheckMenuItem("0V to +10V (Unipolar / VCA Depth)", "",
			[module]() { return module->zMode == Eris::Z_UNIPOLAR_10V; },
			[module]() { module->zMode = Eris::Z_UNIPOLAR_10V; }
		));
		menu->addChild(createCheckMenuItem("-5V to +5V (Bipolar)", "",
			[module]() { return module->zMode == Eris::Z_BIPOLAR_5V; },
			[module]() { module->zMode = Eris::Z_BIPOLAR_5V; }
		));
	}
};

Model* modelEris = createModel<Eris, ErisWidget>("Eris");
