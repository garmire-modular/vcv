#include "plugin.hpp"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  Rose — Rhodonea Rose Curve & Limaçon Trajectory Generator
//  Generate class module producing mathematical Rhodonea rose curves
//  with harmonic petal count (k = 1 to 8), center offset Limaçon morph
//  (Cardioid / Limaçon), bidirectional frequency sync, and XY outputs.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Rose : Module {
	enum ParamId {
		PETALS_PARAM,
		OFFSET_PARAM,

		PETALS_TRIM_PARAM,
		OFFSET_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		PETALS_CV_INPUT,
		OFFSET_CV_INPUT,
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

	enum RoseFuncMode {
		FUNC_COSINE = 0, // Cosine (Standard Rhodonea, axis-aligned)
		FUNC_SINE        // Sine (Rotated)
	};

	BaseFreqMode baseFreqMode = BASE_60HZ;
	RoseFuncMode roseFuncMode = FUNC_COSINE;

	// Pre-allocated DSP state (MetaModule / real-time safe)
	struct VoiceState {
		float basePhase = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		rack::dsp::SchmittTrigger syncTrigger;
		rack::dsp::PulseGenerator syncPulse;
	};

	VoiceState voices[16];

	Rose() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Petals Multiplier (k = 1.0 to 8.0, default 3.0)
		configParam(PETALS_PARAM, 1.f, 8.f, 3.f, "Petals (k)", "", 0.f, 1.f);

		// Center Offset Limaçon (-2.0 to +2.0, default 0.0 for pure Rhodonea rose)
		configParam(OFFSET_PARAM, -2.f, 2.f, 0.f, "Center offset", "", 0.f, 1.f);

		// CV Attenuverters (Mandatory naming per AGENTS.md Section 6.5.4)
		configParam(PETALS_TRIM_PARAM, -1.f, 1.f, 0.f, "Petals CV depth", "%", 0.f, 100.f);
		configParam(OFFSET_TRIM_PARAM, -1.f, 1.f, 0.f, "Offset CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(PETALS_CV_INPUT, "Petals CV");
		configInput(OFFSET_CV_INPUT, "Offset CV");
		configInput(SYNC_INPUT, "Sync");

		// Outputs
		configOutput(SYNC_OUTPUT, "Sync");
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
		int kCvCh = inputs[PETALS_CV_INPUT].getChannels();
		int aCvCh = inputs[OFFSET_CV_INPUT].getChannels();
		int sCh   = inputs[SYNC_INPUT].getChannels();

		int numChannels = std::max({kCvCh, aCvCh, sCh, 1});
		outputs[SYNC_OUTPUT].setChannels(numChannels);
		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float defaultF0 = getBaseFrequency();

		float petalsParam = params[PETALS_PARAM].getValue();
		float offsetParam = params[OFFSET_PARAM].getValue();

		float petalsTrim = params[PETALS_TRIM_PARAM].getValue();
		float offsetTrim = params[OFFSET_TRIM_PARAM].getValue();

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

			// Frequency tracking: sync frequency if locked, else internal reference
			float f0 = (syncConnected && vs.syncPeriod > 0.f) ? (1.f / vs.syncPeriod) : defaultF0;

			// CV inputs
			float kCv = inputs[PETALS_CV_INPUT].getPolyVoltage(c) / 5.f;
			float aCv = inputs[OFFSET_CV_INPUT].getPolyVoltage(c) / 5.f;

			// Effective parameters
			float k = clampf(petalsParam + kCv * petalsTrim * 7.f, 0.1f, 16.f);
			float a = clampf(offsetParam + aCv * offsetTrim * 2.f, -4.f, 4.f);

			// Advance polar angle phase
			if (!syncTriggered) {
				vs.basePhase += f0 * args.sampleTime;
				if (vs.basePhase >= 1.f) {
					vs.basePhase -= std::floor(vs.basePhase);
					vs.syncPulse.trigger(1e-4f);
				}
			}

			// Polar angle theta in [0, 2*pi)
			float theta = 2.f * (float)M_PI * vs.basePhase;

			// Rhodonea / Limaçon radius calculation
			float rTerm = (roseFuncMode == FUNC_COSINE) ? std::cos(k * theta) : std::sin(k * theta);
			float r = a + rTerm;

			// Normalization scale factor to fit within standard Eurorack 5V bounds
			float maxExpectedRadius = std::max(1.0f, 1.0f + std::fabs(a));
			float scale = 5.0f / maxExpectedRadius;

			// Convert polar trajectory to orthogonal Cartesian X and Y
			float rawX = scale * r * std::cos(theta);
			float rawY = scale * r * std::sin(theta);

			// Sync trigger pulse out: 10V Eurorack trigger
			float syncOutV = vs.syncPulse.process(args.sampleTime) ? 10.f : 0.f;
			outputs[SYNC_OUTPUT].setVoltage(syncOutV, c);

			// Hardware laser safety voltage bounds (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clampf(rawX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(rawY, -12.f, 12.f), c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "baseFreqMode", json_integer((int)baseFreqMode));
		json_object_set_new(rootJ, "roseFuncMode", json_integer((int)roseFuncMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* bfmJ = json_object_get(rootJ, "baseFreqMode");
		if (bfmJ) {
			baseFreqMode = (BaseFreqMode)json_integer_value(bfmJ);
		}
		json_t* rfmJ = json_object_get(rootJ, "roseFuncMode");
		if (rfmJ) {
			roseFuncMode = (RoseFuncMode)json_integer_value(rfmJ);
		}
	}
};

struct RoseWidget : ModuleWidget {
	RoseWidget(Rose* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Rose.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: Petals (k) Knob (Centered at X = 15.24 mm, Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 21.59)), module, Rose::PETALS_PARAM));

		// Row 2: Offset (Limaçon) Knob (Centered at X = 15.24 mm, Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 43.00)), module, Rose::OFFSET_PARAM));

		// Attenuverter Trimpots
		// Trimpot Row: Petals (7.62 mm) & Offset (22.86 mm) CV depths (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Rose::PETALS_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, Rose::OFFSET_TRIM_PARAM));

		// Bottom I/O Jacks
		// Row 1: CV Inputs (Center Y = 99.00 mm): Petals CV (7.62), Offset CV (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Rose::PETALS_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Rose::OFFSET_CV_INPUT));

		// Row 2: Sync In (Left 7.62 mm) & Sync Out (Right 22.86 mm) (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Rose::SYNC_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Rose::SYNC_OUTPUT));

		// Row 3: Signal Outputs (Center Y = 118.00 mm): X Out (7.62), Y Out (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Rose::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Rose::Y_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Rose* module = dynamic_cast<Rose*>(this->module);
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
			Rose::BaseFreqMode mode = (Rose::BaseFreqMode)i;
			menu->addChild(createCheckMenuItem(baseFreqLabels[i], "",
				[=]() { return module->baseFreqMode == mode; },
				[=]() { module->baseFreqMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Harmonic Function"));

		menu->addChild(createCheckMenuItem("Cosine (Axis-aligned Rhodonea)", "",
			[=]() { return module->roseFuncMode == Rose::FUNC_COSINE; },
			[=]() { module->roseFuncMode = Rose::FUNC_COSINE; }
		));

		menu->addChild(createCheckMenuItem("Sine (Rotated)", "",
			[=]() { return module->roseFuncMode == Rose::FUNC_SINE; },
			[=]() { module->roseFuncMode = Rose::FUNC_SINE; }
		));
	}
};

Model* modelRose = createModel<Rose, RoseWidget>("Rose");
