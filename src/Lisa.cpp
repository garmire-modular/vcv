#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Lisa — Lissajous Trajectory & Harmonic Orbital Generator
//  Generate class module producing orthogonal sinusoidal oscillations
//  with harmonic frequency multipliers (1:1 to 10:10), bipolar phase
//  offset (±180°), and bipolar orbital dampening (±100%).
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
	enum ParamId {
		X_FREQ_PARAM,
		Y_FREQ_PARAM,
		PHASE_PARAM,
		DAMP_PARAM,

		X_FREQ_TRIM_PARAM,
		Y_FREQ_TRIM_PARAM,
		PHASE_TRIM_PARAM,
		DAMP_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		X_FREQ_CV_INPUT,
		Y_FREQ_CV_INPUT,
		PHASE_CV_INPUT,
		DAMP_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		X_OUTPUT,
		Y_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	enum BaseFreqMode {
		BASE_60HZ = 0,   // Standard Laser / 60 fps Video (Default)
		BASE_120HZ,      // Fast 120 fps Laser / Audio B2
		BASE_C3,         // 130.81 Hz Musical C3
		BASE_C4,         // 261.63 Hz Musical C4
		BASE_1HZ,        // 1.0 Hz LFO
		BASE_10HZ        // 10.0 Hz Sub-audio
	};

	enum DampingMode {
		DAMP_SPIRAL = 0, // Logarithmic Spiral (Harmonograph)
		DAMP_TAPER       // Smooth Orbital Taper
	};

	BaseFreqMode baseFreqMode = BASE_60HZ;
	DampingMode dampingMode = DAMP_SPIRAL;

	// Pre-allocated DSP state (MetaModule / real-time safe)
	struct VoiceState {
		float phaseX = 0.f;
		float phaseY = 0.f;
		float basePhase = 0.f;
	};

	VoiceState voices[16];

	Lisa() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Frequency Multipliers (1.0 to 10.0, default 1.0)
		configParam(X_FREQ_PARAM, 1.f, 10.f, 1.f, "X freq ratio", "×", 0.f, 1.f);
		configParam(Y_FREQ_PARAM, 1.f, 10.f, 1.f, "Y freq ratio", "×", 0.f, 1.f);

		// Phase Shift (Bipolar ±180°, default 0°)
		configParam(PHASE_PARAM, -180.f, 180.f, 0.f, "Phase offset", "°", 0.f, 1.f);

		// Orbital Dampening (Bipolar ±100%, default 0%)
		configParam(DAMP_PARAM, -1.f, 1.f, 0.f, "Orbital dampening", "%", 0.f, 100.f);

		// CV Attenuverters (Mandatory naming per AGENTS.md Section 6.5.4)
		configParam(X_FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "X freq CV depth", "%", 0.f, 100.f);
		configParam(Y_FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Y freq CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase CV depth", "%", 0.f, 100.f);
		configParam(DAMP_TRIM_PARAM, -1.f, 1.f, 0.f, "Dampening CV depth", "%", 0.f, 100.f);

		// Inputs (Rack automatically appends "input" to tooltips)
		configInput(X_FREQ_CV_INPUT, "X freq CV");
		configInput(Y_FREQ_CV_INPUT, "Y freq CV");
		configInput(PHASE_CV_INPUT, "Phase CV");
		configInput(DAMP_CV_INPUT, "Dampening CV");

		// Outputs (Rack automatically appends "output" to tooltips)
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
	}

	float getBaseFrequency() const {
		switch (baseFreqMode) {
			case BASE_60HZ:  return 60.0f;
			case BASE_120HZ: return 120.0f;
			case BASE_C3:    return 130.8128f;
			case BASE_C4:    return 261.6256f;
			case BASE_1HZ:   return 1.0f;
			case BASE_10HZ:  return 10.0f;
			default:         return 60.0f;
		}
	}

	void process(const ProcessArgs& args) override {
		int xCvCh = inputs[X_FREQ_CV_INPUT].getChannels();
		int yCvCh = inputs[Y_FREQ_CV_INPUT].getChannels();
		int pCvCh = inputs[PHASE_CV_INPUT].getChannels();
		int dCvCh = inputs[DAMP_CV_INPUT].getChannels();

		int numChannels = std::max({xCvCh, yCvCh, pCvCh, dCvCh, 1});
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float f0 = getBaseFrequency();

		float xFreqParam = params[X_FREQ_PARAM].getValue();
		float yFreqParam = params[Y_FREQ_PARAM].getValue();
		float phaseParam = params[PHASE_PARAM].getValue();
		float dampParam  = params[DAMP_PARAM].getValue();

		float xFreqTrim = params[X_FREQ_TRIM_PARAM].getValue();
		float yFreqTrim = params[Y_FREQ_TRIM_PARAM].getValue();
		float phaseTrim = params[PHASE_TRIM_PARAM].getValue();
		float dampTrim  = params[DAMP_TRIM_PARAM].getValue();

		bool yCvConnected = inputs[Y_FREQ_CV_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			VoiceState& vs = voices[c];

			// CV inputs with normalization: Y Freq CV normalizes from X Freq CV
			float xCv = inputs[X_FREQ_CV_INPUT].getPolyVoltage(c) / 5.f;
			float yCv = yCvConnected ? (inputs[Y_FREQ_CV_INPUT].getPolyVoltage(c) / 5.f) : xCv;
			float pCv = inputs[PHASE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float dCv = inputs[DAMP_CV_INPUT].getPolyVoltage(c) / 5.f;

			// Frequency ratios clamped to [0.05, 20.0]
			float rx = clampf(xFreqParam + xCv * xFreqTrim * 9.f, 0.05f, 20.f);
			float ry = clampf(yFreqParam + yCv * yFreqTrim * 9.f, 0.05f, 20.f);

			// Phase offset in radians: ±180° param + CV depth
			float phaseDeg = clampf(phaseParam + pCv * phaseTrim * 180.f, -360.f, 360.f);
			float deltaPhi = phaseDeg * (float)(M_PI / 180.0);

			// Orbital Dampening in [-1.0, 1.0]
			float damp = clampf(dampParam + dCv * dampTrim, -1.f, 1.f);

			// Advance fundamental cycle and voice phases
			vs.basePhase += f0 * args.sampleTime;
			if (vs.basePhase >= 1.f) {
				vs.basePhase -= std::floor(vs.basePhase);
			}

			vs.phaseX += (f0 * rx) * args.sampleTime;
			if (vs.phaseX >= 1.f) {
				vs.phaseX -= std::floor(vs.phaseX);
			}

			vs.phaseY += (f0 * ry) * args.sampleTime;
			if (vs.phaseY >= 1.f) {
				vs.phaseY -= std::floor(vs.phaseY);
			}

			// Compute orbital dampening envelope along the fundamental orbit
			float env = 1.0f;
			if (dampingMode == DAMP_SPIRAL) {
				// Logarithmic spiral: decays or expands smoothly along the orbit
				if (damp > 0.f) {
					env = std::exp(-2.5f * damp * vs.basePhase);
				} else if (damp < 0.f) {
					env = std::exp(2.5f * damp * (1.f - vs.basePhase));
				}
			} else {
				// Symmetrical orbital taper / pinch
				env = 1.0f - damp * 0.5f * (1.0f - std::cos(2.f * (float)M_PI * vs.basePhase));
			}

			// Orthogonal sinusoidal generation: standard Eurorack 5V nominal amplitude
			float rawX = 5.f * env * std::sin(2.f * (float)M_PI * vs.phaseX);
			float rawY = 5.f * env * std::sin(2.f * (float)M_PI * vs.phaseY + deltaPhi);

			// Hardware laser safety voltage bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clampf(rawX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(rawY, -12.f, 12.f), c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "baseFreqMode", json_integer((int)baseFreqMode));
		json_object_set_new(rootJ, "dampingMode", json_integer((int)dampingMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* bfmJ = json_object_get(rootJ, "baseFreqMode");
		if (bfmJ) {
			baseFreqMode = (BaseFreqMode)json_integer_value(bfmJ);
		}
		json_t* dmJ = json_object_get(rootJ, "dampingMode");
		if (dmJ) {
			dampingMode = (DampingMode)json_integer_value(dmJ);
		}
	}
};

struct LisaWidget : ModuleWidget {
	LisaWidget(Lisa* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Lisa.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: X Freq (7.62 mm) & Y Freq (22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Lisa::X_FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Lisa::Y_FREQ_PARAM));

		// Row 2: Phase Shift (7.62 mm) & Orbital Dampening (22.86 mm) Knobs (Center Y = 42.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 42.50)), module, Lisa::PHASE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 42.50)), module, Lisa::DAMP_PARAM));

		// Attenuverter Trimpots
		// Trimpot Row 1: X & Y Freq CV depths (Center Y = 68.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 68.00)), module, Lisa::X_FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 68.00)), module, Lisa::Y_FREQ_TRIM_PARAM));

		// Trimpot Row 2: Phase & Damp CV depths (Center Y = 80.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 80.00)), module, Lisa::PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 80.00)), module, Lisa::DAMP_TRIM_PARAM));

		// Bottom I/O Jacks
		// Row 1: Freq CV Inputs (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Lisa::X_FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Lisa::Y_FREQ_CV_INPUT));

		// Row 2: Phase CV & Damp CV Inputs (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Lisa::PHASE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Lisa::DAMP_CV_INPUT));

		// Row 3: Signal Outputs (Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Lisa::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Lisa::Y_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Lisa* module = dynamic_cast<Lisa*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Base Frequency"));

		const char* baseFreqLabels[] = {
			"60 Hz (Laser / Video 60 fps)",
			"120 Hz (Fast Laser / Audio B2)",
			"130.81 Hz (Musical C3)",
			"261.63 Hz (Musical C4)",
			"1 Hz (LFO Rate)",
			"10 Hz (Sub-audio)"
		};

		for (int i = 0; i < 6; i++) {
			Lisa::BaseFreqMode mode = (Lisa::BaseFreqMode)i;
			menu->addChild(createCheckMenuItem(baseFreqLabels[i], "",
				[=]() { return module->baseFreqMode == mode; },
				[=]() { module->baseFreqMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Dampening Profile"));

		menu->addChild(createCheckMenuItem("Logarithmic Spiral (Harmonograph)", "",
			[=]() { return module->dampingMode == Lisa::DAMP_SPIRAL; },
			[=]() { module->dampingMode = Lisa::DAMP_SPIRAL; }
		));

		menu->addChild(createCheckMenuItem("Smooth Orbital Taper", "",
			[=]() { return module->dampingMode == Lisa::DAMP_TAPER; },
			[=]() { module->dampingMode = Lisa::DAMP_TAPER; }
		));
	}
};

Model* modelLisa = createModel<Lisa, LisaWidget>("Lisa");
