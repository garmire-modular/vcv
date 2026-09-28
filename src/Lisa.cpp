#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Lisa — Lissajous Trajectory & Harmonic Orbital Generator (8 HP)
//  Generate class module producing orthogonal sinusoidal oscillations
//  with Coarse/Fine timebase, 3-state Range oscillator (Very Slow / LFO / VCO),
//  carrier-normalized linear FM (with external override), harmonic frequency
//  multipliers (1:1 to 10:10), bipolar phase shift (±180°), bipolar orbital
//  dampening (±100%), and bidirectional frequency sync (Sync In & Sync Out).
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Lisa : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum DampingMode {
		DAMP_SPIRAL = 0, // Logarithmic Spiral (Harmonograph)
		DAMP_TAPER       // Smooth Orbital Taper
	};

	enum ParamId {
		FREQ_PARAM,
		FINE_PARAM,
		RANGE_PARAM,

		X_RATIO_PARAM,
		Y_RATIO_PARAM,

		PHASE_PARAM,
		DAMP_PARAM,

		FREQ_TRIM_PARAM,
		X_RATIO_TRIM_PARAM,
		Y_RATIO_TRIM_PARAM,

		FM_TRIM_PARAM,
		PHASE_TRIM_PARAM,
		DAMP_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		FREQ_CV_INPUT,
		X_RATIO_CV_INPUT,
		Y_RATIO_CV_INPUT,

		FM_CV_INPUT,
		PHASE_CV_INPUT,
		DAMP_CV_INPUT,

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
	DampingMode dampingMode = DAMP_SPIRAL;

	dsp::BooleanTrigger rangeTrigger;

	struct VoiceState {
		float phaseX = 0.f;
		float phaseY = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
	};

	VoiceState voices[16];

	struct FreqParamQuantity : ParamQuantity {
		Lisa* getLisa() {
			return dynamic_cast<Lisa*>(module);
		}

		std::string getDisplayValueString() override {
			Lisa* lisa = getLisa();
			if (!lisa) return ParamQuantity::getDisplayValueString();

			float coarse = getValue();
			float fine = (lisa->paramQuantities.size() > Lisa::FINE_PARAM) ? lisa->params[Lisa::FINE_PARAM].getValue() : 0.f;

			char buf[32];
			if (lisa->rangeMode == Lisa::RANGE_VERY_SLOW) {
				// Period T in seconds: 600.0s to 0.1s
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				float t = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
				std::snprintf(buf, sizeof(buf), "%.1f", t);
			} else if (lisa->rangeMode == Lisa::RANGE_LFO) {
				// Frequency in Hz: 0.00 to 200.00 Hz
				float fCoarse = 200.0f * coarse * coarse;
				float f = clampf(fCoarse + fine * 20.0f, 0.0f, 250.0f);
				std::snprintf(buf, sizeof(buf), "%.2f", f);
			} else {
				// Frequency in Hz: 150.00 to 2000.00 Hz
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				float f = clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
				std::snprintf(buf, sizeof(buf), "%.2f", f);
			}
			return std::string(buf);
		}

		std::string getUnit() override {
			Lisa* lisa = getLisa();
			if (!lisa) return "";
			return (lisa->rangeMode == Lisa::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	Lisa() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Coarse Frequency (0.0 to 1.0)
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");

		// Fine Frequency (-1.0 to +1.0 for +/- 10% trim)
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		// Momentary Range Button
		configButton(RANGE_PARAM, "Range time-scale");

		// Harmonic Ratios (1.0 to 10.0)
		configParam(X_RATIO_PARAM, 1.f, 10.f, 1.f, "X ratio", "×", 0.f, 1.f);
		configParam(Y_RATIO_PARAM, 1.f, 10.f, 1.f, "Y ratio", "×", 0.f, 1.f);

		// Phase Shift (Bipolar ±180°)
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°", 0.f, 1.f);

		// Orbital Dampening (Bipolar ±100%)
		configParam(DAMP_PARAM, -1.f, 1.f, 0.f, "Orbital dampening", "%", 0.f, 100.f);

		// CV Attenuverters (Mandatory naming per AGENTS.md Section 6.5.4)
		// Row 1 Attenuverters
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(X_RATIO_TRIM_PARAM, -1.f, 1.f, 0.f, "X ratio CV depth", "%", 0.f, 100.f);
		configParam(Y_RATIO_TRIM_PARAM, -1.f, 1.f, 0.f, "Y ratio CV depth", "%", 0.f, 100.f);

		// Row 2 Attenuverters
		configParam(FM_TRIM_PARAM, -1.f, 1.f, 0.f, "Linear FM CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);
		configParam(DAMP_TRIM_PARAM, -1.f, 1.f, 0.f, "Damp CV depth", "%", 0.f, 100.f);

		// Inputs: Row 1
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(X_RATIO_CV_INPUT, "X ratio CV");
		configInput(Y_RATIO_CV_INPUT, "Y ratio CV");

		// Inputs: Row 2
		configInput(FM_CV_INPUT, "External FM");
		configInput(PHASE_CV_INPUT, "Phase CV");
		configInput(DAMP_CV_INPUT, "Damp CV");

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
			// Frequency in Hz: 0.0 to 200.0 Hz
			float fCoarse = 200.0f * coarse * coarse;
			return clampf(fCoarse + fine * 20.0f, 0.001f, 250.0f);
		} else {
			// Frequency in Hz: 150.0 to 2000.0 Hz
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
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
		int xCvCh  = inputs[X_RATIO_CV_INPUT].getChannels();
		int yCvCh  = inputs[Y_RATIO_CV_INPUT].getChannels();
		int pCvCh  = inputs[PHASE_CV_INPUT].getChannels();
		int dCvCh  = inputs[DAMP_CV_INPUT].getChannels();
		int sCh    = inputs[SYNC_INPUT].getChannels();

		int numChannels = std::max({fCvCh, fmCvCh, xCvCh, yCvCh, pCvCh, dCvCh, sCh, 1});
		outputs[SYNC_OUTPUT].setChannels(numChannels);
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();

		float defaultF0 = calculateBaseFrequency(coarseParam, fineParam);

		float xRatioParam = params[X_RATIO_PARAM].getValue();
		float yRatioParam = params[Y_RATIO_PARAM].getValue();
		float phaseParam  = params[PHASE_PARAM].getValue();
		float dampParam   = params[DAMP_PARAM].getValue();

		float fTrim      = params[FREQ_TRIM_PARAM].getValue();
		float fmTrim     = params[FM_TRIM_PARAM].getValue();
		float xRatioTrim = params[X_RATIO_TRIM_PARAM].getValue();
		float yRatioTrim = params[Y_RATIO_TRIM_PARAM].getValue();
		float phaseTrim  = params[PHASE_TRIM_PARAM].getValue();
		float dampTrim   = params[DAMP_TRIM_PARAM].getValue();

		bool fmConnected  = inputs[FM_CV_INPUT].isConnected();
		bool yCvConnected = inputs[Y_RATIO_CV_INPUT].isConnected();
		bool dCvConnected = inputs[DAMP_CV_INPUT].isConnected();
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
					vs.phaseX = 0.f;
					vs.phaseY = 0.f;
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
				float carrierSelfMod = std::sin(2.f * (float)M_PI * vs.phaseX);
				float deltaF = fCarrier * carrierSelfMod * fmTrim;
				fActual = std::max(0.0001f, fCarrier + deltaF);
			} else {
				float extFmCv = inputs[FM_CV_INPUT].getPolyVoltage(c);
				float scaleHz = (rangeMode == RANGE_VERY_SLOW) ? (fCarrier * 2.f) : (rangeMode == RANGE_LFO ? 100.f : 500.f);
				float deltaF = (extFmCv / 5.f) * fmTrim * scaleHz;
				fActual = std::max(0.0001f, fCarrier + deltaF);
			}

			// Ratio CV inputs: Y Ratio normalizes from X Ratio
			float xCv = inputs[X_RATIO_CV_INPUT].getPolyVoltage(c) / 5.f;
			float yCv = yCvConnected ? (inputs[Y_RATIO_CV_INPUT].getPolyVoltage(c) / 5.f) : xCv;

			float rx = clampf(xRatioParam + xCv * xRatioTrim * 9.f, 0.05f, 20.f);
			float ry = clampf(yRatioParam + yCv * yRatioTrim * 9.f, 0.05f, 20.f);

			float fx = fActual * rx;
			float fy = fActual * ry;

			// Advance phases
			vs.phaseX += fx * args.sampleTime;
			vs.phaseY += fy * args.sampleTime;

			// Emit sync pulse when voice completes a master cycle
			if (!syncTriggered && vs.phaseY >= 1.f) {
				vs.syncPulse.trigger(1e-4f);
			}

			// Wrap phases to [0.0, 1.0)
			vs.phaseX -= std::floor(vs.phaseX);
			vs.phaseY -= std::floor(vs.phaseY);

			// Phase shift modulation (bipolar ±180°)
			float pCv = inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float phaseDeg = clampf(phaseParam + pCv * phaseTrim * 180.f, -180.f, 180.f);
			float phaseRad = phaseDeg * (float)(M_PI / 180.0);

			// Orbital Dampening modulation (bipolar ±100%): Damp normalizes from Phase CV if unpatched
			float dCv = dCvConnected ? (inputs[DAMP_CV_INPUT].getPolyVoltage(c) / 5.f) : pCv;
			float dampVal = clampf(dampParam + dCv * dampTrim, -1.f, 1.f);

			// Orthogonal Sinusoidal Oscillations
			float angX = 2.f * (float)M_PI * vs.phaseX + phaseRad;
			float angY = 2.f * (float)M_PI * vs.phaseY;

			float rawX = std::sin(angX);
			float rawY = std::sin(angY);

			// Apply Orbital Dampening
			if (std::abs(dampVal) > 1e-4f) {
				if (dampingMode == DAMP_SPIRAL) {
					// Harmonograph logarithmic spiral dampening
					float r = std::sqrt(rawX * rawX + rawY * rawY);
					float dampFactor = 1.0f - dampVal * 0.45f * (1.0f - r);
					rawX *= dampFactor;
					rawY *= dampFactor;
				} else {
					// Smooth orbital taper
					float decay = std::exp(-dampVal * vs.phaseY * 1.5f);
					rawX *= decay;
					rawY *= decay;
				}
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
		json_object_set_new(rootJ, "dampingMode", json_integer((int)dampingMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) {
			rangeMode = (RangeMode)json_integer_value(rmJ);
		}
		json_t* dmJ = json_object_get(rootJ, "dampingMode");
		if (dmJ) {
			dampingMode = (DampingMode)json_integer_value(dmJ);
		}
	}
};

// ── Custom 3-Color Range Light Widget (Okabe-Ito Palette) ───────────
template <typename TBase = GrayModuleLightWidget>
struct TRangeLight : TBase {
	TRangeLight() {
		// Range 0: Very Slow = Yellow (#F0E442)
		this->addBaseColor(nvgRGBA(0xf0, 0xe4, 0x42, 0xff));
		// Range 1: LFO = Orange (#E69F00)
		this->addBaseColor(nvgRGBA(0xe6, 0x9f, 0x00, 0xff));
		// Range 2: VCO = Reddish Purple (#CC79A7)
		this->addBaseColor(nvgRGBA(0xcc, 0x79, 0xa7, 0xff));
	}
};
struct RangeLightWidget : SmallLight<TRangeLight<>> {};

struct LisaWidget : ModuleWidget {
	LisaWidget(Lisa* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Lisa.svg")));

		// 8 HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Row 1: FREQ & FINE Knobs (10.82 and 29.82 mm at Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 21.59)), module, Lisa::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 21.59)), module, Lisa::FINE_PARAM));

		// Row 1 Center: Range LED (16.50 mm) and Range Button (24.50 mm)
		addChild(createLightCentered<RangeLightWidget>(mm2px(Vec(20.32, 16.50)), module, Lisa::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<TL1105>(mm2px(Vec(20.32, 24.50)), module, Lisa::RANGE_PARAM));

		// Row 2: Harmonic Ratio Knobs (Center Y = 37.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 37.00)), module, Lisa::X_RATIO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 37.00)), module, Lisa::Y_RATIO_PARAM));

		// Row 3: Phase & Damp Knobs (Center Y = 52.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.82, 52.50)), module, Lisa::PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(29.82, 52.50)), module, Lisa::DAMP_PARAM));

		// Zone 3: CV Attenuverter Trimpots (3 Columns: 8.82, 20.32, 31.82 mm)
		// Row 1 Attenuverters: FREQ, X, Y (Center Y = 70.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 70.00)), module, Lisa::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 70.00)), module, Lisa::X_RATIO_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 70.00)), module, Lisa::Y_RATIO_TRIM_PARAM));

		// Row 2 Attenuverters: FM, PHASE, DAMP (Center Y = 79.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.82, 79.50)), module, Lisa::FM_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.32, 79.50)), module, Lisa::PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(31.82, 79.50)), module, Lisa::DAMP_TRIM_PARAM));

		// Zone 4: I/O Jacks
		// Row 1 (Inputs): FREQ, X, Y (Center Y = 94.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 94.50)), module, Lisa::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 94.50)), module, Lisa::X_RATIO_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 94.50)), module, Lisa::Y_RATIO_CV_INPUT));

		// Row 2 (Inputs): FM, PHASE, DAMP (Center Y = 106.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.82, 106.00)), module, Lisa::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 106.00)), module, Lisa::PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.82, 106.00)), module, Lisa::DAMP_CV_INPUT));

		// Row 3 (Sync & Outputs): SYNC IN, X OUT, Y OUT, SYNC OUT (Center Y = 118.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.07, 118.00)), module, Lisa::SYNC_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.57, 118.00)), module, Lisa::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.07, 118.00)), module, Lisa::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(34.57, 118.00)), module, Lisa::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Lisa* module = dynamic_cast<Lisa*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));

		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};

		for (int i = 0; i < 3; i++) {
			Lisa::RangeMode mode = (Lisa::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Orbital Damping"));

		menu->addChild(createCheckMenuItem("Logarithmic Spiral (Harmonograph)", "",
			[=]() { return module->dampingMode == Lisa::DAMP_SPIRAL; },
			[=]() { module->dampingMode = Lisa::DAMP_SPIRAL; }
		));

		menu->addChild(createCheckMenuItem("Orbital Taper", "",
			[=]() { return module->dampingMode == Lisa::DAMP_TAPER; },
			[=]() { module->dampingMode = Lisa::DAMP_TAPER; }
		));
	}
};

Model* modelLisa = createModel<Lisa, LisaWidget>("Lisa");
