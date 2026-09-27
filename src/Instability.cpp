#include "plugin.hpp"
#include "dsp/Instability.hpp"

// ─────────────────────────────────────────────────────────────────────
//  Instability — Standalone 3-Axis Organic Drift Generator
// ─────────────────────────────────────────────────────────────────────

struct Instability : Module {
	enum ParamId {
		FREQ_DRIFT_PARAM,
		PHASE_DRIFT_PARAM,
		AMP_DRIFT_PARAM,
		FREQ_TRIM_PARAM,
		PHASE_TRIM_PARAM,
		AMP_TRIM_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		FREQ_INPUT,
		PHASE_INPUT,
		AMP_INPUT,
		SIGNAL_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		SIGNAL_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	struct ChannelState {
		garmire::DriftGenerator driftFreq;
		garmire::DriftGenerator driftPhase;
		garmire::DriftGenerator driftAmp;
	};

	ChannelState state[16];

	Instability() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Main Knobs
		configParam(FREQ_DRIFT_PARAM, 0.f, 1.f, 0.2f, "Frequency Drift Rate", "%", 0.f, 100.f);
		configParam(PHASE_DRIFT_PARAM, 0.f, 1.f, 0.2f, "Phase Drift Rate", "%", 0.f, 100.f);
		configParam(AMP_DRIFT_PARAM, 0.f, 1.f, 0.2f, "Amplitude Drift Rate", "%", 0.f, 100.f);

		// Attenuverters (-1 to +1, default 0.0)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Freq drift rate CV depth", "%", 0.f, 100.f);
		configParam(PHASE_TRIM_PARAM, -1.f, 1.f, 0.f, "Phase drift rate CV depth", "%", 0.f, 100.f);
		configParam(AMP_TRIM_PARAM, -1.f, 1.f, 0.f, "Amp drift rate CV depth", "%", 0.f, 100.f);

		// CV Rate Inputs
		configInput(FREQ_INPUT, "Frequency Drift Rate CV");
		configInput(PHASE_INPUT, "Phase Drift Rate CV");
		configInput(AMP_INPUT, "Amplitude Drift Rate CV");

		// Signal I/O
		configInput(SIGNAL_INPUT, "Signal Input");
		configOutput(SIGNAL_OUTPUT, "Signal / Drift CV Output");

		for (int i = 0; i < 16; i++) {
			state[i].driftFreq.init(1001u + i * 333u);
			state[i].driftPhase.init(2002u + i * 444u);
			state[i].driftAmp.init(3003u + i * 555u);
		}
	}

	float getModParam(int paramId, int attvId, int inputId, int channel) {
		float val = params[paramId].getValue();
		if (inputs[inputId].isConnected()) {
			val += (inputs[inputId].getPolyVoltage(channel) / 5.f) * params[attvId].getValue();
		}
		return val;
	}

	void process(const ProcessArgs& args) override {
		int freqCh  = inputs[FREQ_INPUT].getChannels();
		int phaseCh = inputs[PHASE_INPUT].getChannels();
		int ampCh   = inputs[AMP_INPUT].getChannels();
		int sigCh   = inputs[SIGNAL_INPUT].getChannels();

		int channels = std::max({freqCh, phaseCh, ampCh, sigCh, 1});

		outputs[SIGNAL_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			float rateFreq  = clamp(getModParam(FREQ_DRIFT_PARAM, FREQ_TRIM_PARAM, FREQ_INPUT, c), 0.f, 1.f);
			float ratePhase = clamp(getModParam(PHASE_DRIFT_PARAM, PHASE_TRIM_PARAM, PHASE_INPUT, c), 0.f, 1.f);
			float rateAmp   = clamp(getModParam(AMP_DRIFT_PARAM, AMP_TRIM_PARAM, AMP_INPUT, c), 0.f, 1.f);

			float valFreq  = state[c].driftFreq.process(args.sampleTime, rateFreq);
			float valPhase = state[c].driftPhase.process(args.sampleTime, ratePhase);
			float valAmp   = state[c].driftAmp.process(args.sampleTime, rateAmp);

			// Multi-axis organic drift combination
			float driftCombined = valAmp + 0.2f * valPhase + 0.1f * valFreq;

			if (inputs[SIGNAL_INPUT].isConnected()) {
				float inVolt = inputs[SIGNAL_INPUT].getPolyVoltage(c);
				float outVolt = inVolt * (1.0f + driftCombined * 0.5f);
				outputs[SIGNAL_OUTPUT].setVoltage(outVolt, c);
			} else {
				// Standalone drift CV (+/-5V) when signal input is unpatched
				outputs[SIGNAL_OUTPUT].setVoltage(clamp(driftCombined, -1.f, 1.f) * 5.0f, c);
			}
		}
	}
};

struct InstabilityWidget : ModuleWidget {
	InstabilityWidget(Instability* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Instability.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Knobs (X = 15.24mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 21.59)), module, Instability::FREQ_DRIFT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 40.00)), module, Instability::PHASE_DRIFT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 58.41)), module, Instability::AMP_DRIFT_PARAM));

		// Trimpots (Row Y = 83.00mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(6.00, 83.00)), module, Instability::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(15.24, 83.00)), module, Instability::PHASE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(24.48, 83.00)), module, Instability::AMP_TRIM_PARAM));

		// CV Rate Inputs (Top Jack Row Y = 105.41mm, aligned with Scale/Position Row 2)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.00, 105.41)), module, Instability::FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 105.41)), module, Instability::PHASE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24.48, 105.41)), module, Instability::AMP_INPUT));

		// Signal I/O (Bottom Jack Row Y = 116.84mm, aligned with Scale/Position Row 3)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 116.84)), module, Instability::SIGNAL_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 116.84)), module, Instability::SIGNAL_OUTPUT));
	}
};

Model* modelInstability = createModel<Instability, InstabilityWidget>("Instability");
