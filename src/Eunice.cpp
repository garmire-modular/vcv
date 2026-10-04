#include "plugin.hpp"
#include "core/EuniceEngine.hpp"
#include <cmath>

struct RateParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float val = getValue(); // 0.0 to 1.0
		float exponent = val * 15.2877f;
		float freq = 0.05f * std::pow(2.f, exponent);
		char buf[32];
		if (freq < 1.0f) {
			snprintf(buf, sizeof(buf), "%.3f", freq);
		} else if (freq < 10.0f) {
			snprintf(buf, sizeof(buf), "%.2f", freq);
		} else if (freq < 100.0f) {
			snprintf(buf, sizeof(buf), "%.1f", freq);
		} else {
			snprintf(buf, sizeof(buf), "%.0f", freq);
		}
		return std::string(buf);
	}
	std::string getUnit() override { return " Hz"; }
};

struct SlewTimeParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float val = getValue(); // 0.0 to 1.0
		float timeSec = 0.0005f * std::pow(10.0f / 0.0005f, val);
		char buf[32];
		if (timeSec < 0.01f) {
			// e.g. 0.5 ms to 10 ms
			snprintf(buf, sizeof(buf), "%.2f ms", timeSec * 1000.f);
		} else if (timeSec < 1.0f) {
			// e.g. 10 ms to 999 ms
			snprintf(buf, sizeof(buf), "%.1f ms", timeSec * 1000.f);
		} else {
			// 1.00 s to 10.00 s
			snprintf(buf, sizeof(buf), "%.2f s", timeSec);
		}
		return std::string(buf);
	}
	std::string getUnit() override { return ""; }
};

struct InAttenParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float v = getValue();
		char buf[32];
		if (std::abs(v) < 0.005f) {
			return "0%";
		} else if (v < 0.f) {
			snprintf(buf, sizeof(buf), "%.0f%% (inv)", std::abs(v) * 100.f);
			return std::string(buf);
		} else {
			snprintf(buf, sizeof(buf), "%.0f%%", v * 100.f);
			return std::string(buf);
		}
	}
	std::string getUnit() override { return ""; }
};

struct Eunice : Module {
	enum ParamId {
		RATE_PARAM,
		DIST_PARAM,
		CORR_PARAM,
		SLEW_DEST_PARAM,
		SLEW_RISE_PARAM,
		SLEW_FALL_PARAM,
		RATE_CV_ATTEN_PARAM,
		DIST_CV_ATTEN_PARAM,
		CORR_CV_ATTEN_PARAM,
		IN_ATTEN_PARAM,
		SLEW_RISE_CV_ATTEN_PARAM,
		SLEW_FALL_CV_ATTEN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		SIGNAL_INPUT,
		DIST_CV_INPUT,
		CORR_CV_INPUT,
		RATE_CV_INPUT,
		EXT_CLOCK_INPUT,
		SLEW_RISE_CV_INPUT,
		SLEW_FALL_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		SH_OUTPUT,
		TH_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		CLOCK_LIGHT,
		GATE_LIGHT,
		SIGNAL_LIGHT_GREEN,
		SIGNAL_LIGHT_RED,
		SH_LIGHT_GREEN,
		SH_LIGHT_RED,
		TH_LIGHT_GREEN,
		TH_LIGHT_RED,
		LIGHTS_LEN
	};

	eunice::Engine engine;

	Eunice() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Primary Knobs
		configParam<RateParamQuantity>(RATE_PARAM, 0.f, 1.f, 0.5f, "Rate");
		configParam(DIST_PARAM, 0.f, 1.f, 0.5f, "Distribution tilt", "%", 0.f, 100.f);
		configParam(CORR_PARAM, 0.f, 1.f, 0.0f, "Correlation crossfade", "%", 0.f, 100.f);

		// Row 2: Slew Destination Switch & Knobs
		// 0 = S&H only (Top), 1 = Both (Mid), 2 = T&H only (Bot)
		configSwitch(SLEW_DEST_PARAM, 0.f, 2.f, 1.f, "Slew destination", {"S&H only", "Both (S&H + T&H)", "T&H only"});
		configParam<SlewTimeParamQuantity>(SLEW_RISE_PARAM, 0.f, 1.f, 0.0f, "Slew rise time");
		configParam<SlewTimeParamQuantity>(SLEW_FALL_PARAM, 0.f, 1.f, 0.0f, "Slew fall time");

		// Row 3: Attenuverters
		configParam(RATE_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Rate CV depth", "%", 0.f, 100.f);
		configParam(DIST_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Distribution CV depth", "%", 0.f, 100.f);
		configParam(CORR_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Correlation CV depth", "%", 0.f, 100.f);

		// Row 4: IN Attenuverter & Slew Attenuverters
		// IN attenuverter: -1.0 to +1.0, default 1.0 (fully CW / 100%)
		configParam<InAttenParamQuantity>(IN_ATTEN_PARAM, -1.f, 1.f, 1.f, "Signal in level");
		configParam(SLEW_RISE_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Slew rise CV depth", "%", 0.f, 100.f);
		configParam(SLEW_FALL_CV_ATTEN_PARAM, -1.f, 1.f, 0.f, "Slew fall CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(SIGNAL_INPUT, "Signal");
		configInput(DIST_CV_INPUT, "Distribution CV");
		configInput(CORR_CV_INPUT, "Correlation CV");
		configInput(RATE_CV_INPUT, "Rate CV");
		configInput(EXT_CLOCK_INPUT, "External Clock / Gate");
		configInput(SLEW_RISE_CV_INPUT, "Slew rise CV");
		configInput(SLEW_FALL_CV_INPUT, "Slew fall CV");

		// Outputs
		configOutput(SH_OUTPUT, "S&H");
		configOutput(TH_OUTPUT, "T&H");

		engine.reset();
	}

	void onReset() override {
		engine.reset();
		engine.distMode = eunice::DIST_POWER_LAW;
		engine.slewProfile = eunice::SLEW_LINEAR;
	}

	void onSampleRateChange() override {
		engine.setSampleRate(APP->engine->getSampleRate());
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "distributionMode", json_integer(static_cast<int>(engine.distMode)));
		json_object_set_new(rootJ, "slewProfile", json_integer(static_cast<int>(engine.slewProfile)));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* distJ = json_object_get(rootJ, "distributionMode");
		if (distJ) {
			engine.distMode = static_cast<eunice::DistributionMode>(json_integer_value(distJ));
		}
		json_t* slewJ = json_object_get(rootJ, "slewProfile");
		if (slewJ) {
			engine.slewProfile = static_cast<eunice::SlewProfile>(json_integer_value(slewJ));
		}
	}

	void process(const ProcessArgs& args) override {
		float dt = args.sampleTime;
		engine.setSampleRate(args.sampleRate);

		// Determine polyphony channel count
		int sigChannels = inputs[SIGNAL_INPUT].getChannels();
		int clkChannels = inputs[EXT_CLOCK_INPUT].getChannels();
		int channels = std::max(sigChannels, clkChannels);
		if (channels < 1) {
			channels = 1;
		}
		if (channels > eunice::Engine::MAX_CHANNELS) {
			channels = eunice::Engine::MAX_CHANNELS;
		}

		// Calculate global internal clock frequency
		float rateKnob = params[RATE_PARAM].getValue();
		float rateAtten = params[RATE_CV_ATTEN_PARAM].getValue();
		float rateCv = inputs[RATE_CV_INPUT].getVoltage();
		// Exponential mapping: 0.05 Hz to ~2000 Hz
		float rateExponent = rateKnob * 15.2877f + rateCv * rateAtten;
		rateExponent = clamp(rateExponent, 0.0f, 16.0f);
		float clockFreq = 0.05f * std::pow(2.f, rateExponent);
		clockFreq = clamp(clockFreq, 0.01f, 10000.f);

		// Advance internal clock phase
		engine.clockPhase += clockFreq * dt;
		if (engine.clockPhase >= 1.0f) {
			engine.clockPhase -= 1.0f;
		}
		bool internalClockHigh = (engine.clockPhase < 0.5f);

		// Tracking lowpass cutoff based on clock rate (fc = 2 * f_clock)
		float fc = clamp(2.0f * clockFreq, 5.0f, 16000.0f);
		float lpfAlpha = 1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * fc * dt);
		lpfAlpha = clamp(lpfAlpha, 0.0001f, 1.0f);

		// Global parameters
		float distKnob = params[DIST_PARAM].getValue();
		float distAtten = params[DIST_CV_ATTEN_PARAM].getValue();

		float inAtten = params[IN_ATTEN_PARAM].getValue();

		float corrKnob = params[CORR_PARAM].getValue();
		float corrAtten = params[CORR_CV_ATTEN_PARAM].getValue();

		int slewDest = static_cast<int>(std::round(params[SLEW_DEST_PARAM].getValue()));
		float riseKnob = params[SLEW_RISE_PARAM].getValue();
		float riseAtten = params[SLEW_RISE_CV_ATTEN_PARAM].getValue();
		float fallKnob = params[SLEW_FALL_PARAM].getValue();
		float fallAtten = params[SLEW_FALL_CV_ATTEN_PARAM].getValue();

		float riseMod = clamp(riseKnob + riseAtten * (inputs[SLEW_RISE_CV_INPUT].getVoltage() / 5.0f), 0.0f, 1.0f);
		float fallMod = clamp(fallKnob + fallAtten * (inputs[SLEW_FALL_CV_INPUT].getVoltage() / 5.0f), 0.0f, 1.0f);
		// Exponential slew time from 0.0005s (0.5ms) to 10.0s
		float riseTime = 0.0005f * std::pow(10.0f / 0.0005f, riseMod);
		float fallTime = 0.0005f * std::pow(10.0f / 0.0005f, fallMod);

		bool anyExtClockPatched = inputs[EXT_CLOCK_INPUT].isConnected();
		float firstChanDistVoltage = 0.0f;
		float firstChanSHOut = 0.0f;
		float firstChanTHOut = 0.0f;
		bool anyGateHigh = false;

		for (int c = 0; c < channels; c++) {
			auto& ch = engine.channels[c];

			// 1. Clock evaluation
			bool clockIsHigh = false;
			bool clockTriggered = false;
			if (anyExtClockPatched) {
				float clkVolts = inputs[EXT_CLOCK_INPUT].getPolyVoltage(c);
				// Schmitt trigger with hysteresis
				if (!ch.clockHigh && clkVolts >= 1.7f) {
					ch.clockHigh = true;
					clockTriggered = true;
				} else if (ch.clockHigh && clkVolts < 0.8f) {
					ch.clockHigh = false;
				}
				clockIsHigh = ch.clockHigh;
				if (clkVolts >= 1.7f) anyGateHigh = true;
			} else {
				// Use internal clock
				clockIsHigh = internalClockHigh;
				if (!ch.clockHigh && clockIsHigh) {
					clockTriggered = true;
				}
				ch.clockHigh = clockIsHigh;
			}

			// 2. Signal evaluation (External or Internal Noise-Triangle)
			float inSignal = 0.0f;
			if (inputs[SIGNAL_INPUT].isConnected()) {
				// External signal scaled by IN attenuverter
				inSignal = inputs[SIGNAL_INPUT].getPolyVoltage(c) * inAtten;
			} else {
				// Generate internal noise-jittered ~100Hz triangle
				float noise = eunice::xorshift32_float(ch.prngState);
				// Step triangle phase (~100Hz with noise jitter)
				float phaseInc = (100.0f * dt) + (noise * 0.002f);
				ch.triPhase += phaseInc;
				if (ch.triPhase >= 1.0f) ch.triPhase -= 1.0f;
				if (ch.triPhase < 0.0f) ch.triPhase += 1.0f;

				// Triangle [-5V, +5V]
				float rawTri = 10.0f * (2.0f * std::abs(ch.triPhase - 0.5f) - 0.5f);

				// Clock-tracking lowpass
				ch.lpfState += lpfAlpha * (rawTri - ch.lpfState);
				// Scaled by IN attenuverter
				inSignal = ch.lpfState * inAtten;
			}

			// 3. Distribution Tilt Stage (Applies to both internal and external inputs; full pass-through at 50%)
			float cDistCV = inputs[DIST_CV_INPUT].getPolyVoltage(c);
			float effectiveDist = clamp(distKnob + distAtten * (cDistCV / 5.0f), 0.0f, 1.0f);
			float vDist = eunice::Engine::applyDistribution(inSignal, effectiveDist, engine.distMode);
			if (c == 0) firstChanDistVoltage = vDist;

			// 4. Correlation Crossfader Stage
			float cCorrCV = inputs[CORR_CV_INPUT].getPolyVoltage(c);
			float effectiveCorr = clamp(corrKnob + corrAtten * (cCorrCV / 5.0f), 0.0f, 1.0f);
			float vInEff = (1.0f - effectiveCorr) * vDist + effectiveCorr * ch.heldSH;

			// 5. Sample & Hold Acquisition (Edge-triggered)
			if (clockTriggered) {
				ch.heldSH = vInEff;
			}

			// 6. Track & Hold Acquisition (Level-gated)
			if (clockIsHigh) {
				ch.heldTH = vInEff;
			}

			// 7. Slew Limiter Stage
			float targetSH = ch.heldSH;
			float targetTH = ch.heldTH;

			// Slew Destination: 0 = S&H, 1 = Both, 2 = T&H
			if (slewDest == 0 || slewDest == 1) {
				ch.slewedSH = eunice::Engine::stepSlew(ch.slewedSH, targetSH, riseTime, fallTime, dt, engine.slewProfile);
			} else {
				ch.slewedSH = targetSH;
			}

			if (slewDest == 2 || slewDest == 1) {
				ch.slewedTH = eunice::Engine::stepSlew(ch.slewedTH, targetTH, riseTime, fallTime, dt, engine.slewProfile);
			} else {
				ch.slewedTH = targetTH;
			}

			float finalSH = clamp(ch.slewedSH, -12.0f, 12.0f);
			float finalTH = clamp(ch.slewedTH, -12.0f, 12.0f);

			outputs[SH_OUTPUT].setVoltage(finalSH, c);
			outputs[TH_OUTPUT].setVoltage(finalTH, c);

			if (c == 0) {
				firstChanSHOut = finalSH;
				firstChanTHOut = finalTH;
			}
		}

		outputs[SH_OUTPUT].setChannels(channels);
		outputs[TH_OUTPUT].setChannels(channels);

		// 8. Visual LED Indicators
		// Red 2mm Clock LED (shows internal clock cycle)
		lights[CLOCK_LIGHT].setBrightness(internalClockHigh ? 1.0f : 0.0f);

		// Red 2mm Gate Input LED (shows external gate activity)
		lights[GATE_LIGHT].setBrightness(anyGateHigh ? 1.0f : 0.0f);

		// Bipolar 2mm Signal In LED
		lights[SIGNAL_LIGHT_GREEN].setBrightness(std::max(0.0f, firstChanDistVoltage / 5.0f));
		lights[SIGNAL_LIGHT_RED].setBrightness(std::max(0.0f, -firstChanDistVoltage / 5.0f));

		// Bipolar 2mm S&H Out LED
		lights[SH_LIGHT_GREEN].setBrightness(std::max(0.0f, firstChanSHOut / 5.0f));
		lights[SH_LIGHT_RED].setBrightness(std::max(0.0f, -firstChanSHOut / 5.0f));

		// Bipolar 2mm T&H Out LED
		lights[TH_LIGHT_GREEN].setBrightness(std::max(0.0f, firstChanTHOut / 5.0f));
		lights[TH_LIGHT_RED].setBrightness(std::max(0.0f, -firstChanTHOut / 5.0f));
	}
};

// ── 3-Position Switch for Slew Destination (Top = S&H, Mid = Both, Bot = T&H) ───
struct EuniceCKSSThree : app::SvgSwitch {
	EuniceCKSSThree() {
		// Frame 0 (value 0.0) = TOP position (S&H only)
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThree_2.svg")));
		// Frame 1 (value 1.0) = MIDDLE position (Both S&H and T&H)
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThree_1.svg")));
		// Frame 2 (value 2.0) = BOTTOM position (T&H only)
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThree_0.svg")));
	}
};

struct EuniceWidget : ModuleWidget {
	EuniceWidget(Eunice* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Eunice.svg")));

		// 8HP Screws (Centered at x = RACK_GRID_WIDTH)
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 3 Columns: 8.13 mm, 20.32 mm, 32.51 mm
		const double col_a = 8.13;
		const double col_b = 20.32;
		const double col_c = 32.51;

		// ---------------- Row 1: Primary Knobs (Center Y = 21.59 mm) ----------------
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_a, 21.59)), module, Eunice::RATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_b, 21.59)), module, Eunice::DIST_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_c, 21.59)), module, Eunice::CORR_PARAM));

		// ---------------- Row 2: Slew Destination Switch & Knobs (Center Y = 40.00 mm) ----------------
		addParam(createParamCentered<EuniceCKSSThree>(mm2px(Vec(col_a, 40.00)), module, Eunice::SLEW_DEST_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_b, 40.00)), module, Eunice::SLEW_RISE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_c, 40.00)), module, Eunice::SLEW_FALL_PARAM));

		// ---------------- Row 3: Attenuverters (Center Y = 56.00 mm) ----------------
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_a, 56.00)), module, Eunice::RATE_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_b, 56.00)), module, Eunice::DIST_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_c, 56.00)), module, Eunice::CORR_CV_ATTEN_PARAM));

		// ---------------- Row 4: IN Attenuverter & Slew Attenuverters (Center Y = 68.00 mm) ----------------
		// Order: IN, RISE, FALL
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_a, 68.00)), module, Eunice::IN_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_b, 68.00)), module, Eunice::SLEW_RISE_CV_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_c, 68.00)), module, Eunice::SLEW_FALL_CV_ATTEN_PARAM));

		// ---------------- Row 5: Jacks (Center Y = 89.50 mm) ----------------
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_a, 89.50)), module, Eunice::SIGNAL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_b, 89.50)), module, Eunice::DIST_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_c, 89.50)), module, Eunice::CORR_CV_INPUT));

		// ---------------- Row 6: Jacks (Center Y = 103.75 mm) ----------------
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_a, 103.75)), module, Eunice::RATE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_b, 103.75)), module, Eunice::EXT_CLOCK_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_c, 103.75)), module, Eunice::SH_OUTPUT));

		// ---------------- Row 7: Jacks (Fixed Bottom Center Y = 118.00 mm) ----------------
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_a, 118.00)), module, Eunice::SLEW_RISE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_b, 118.00)), module, Eunice::SLEW_FALL_CV_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_c, 118.00)), module, Eunice::TH_OUTPUT));

		// ---------------- 2mm LEDs (Above and to the right of their knobs / jacks) ----------------
		// 1. Clock LED (Red 2mm): Above & right of Rate knob (Knob: 8.13, 21.59 -> LED: 13.50, 16.50)
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(col_a + 5.37, 16.50)), module, Eunice::CLOCK_LIGHT));

		// 2. Gate Input LED (Red 2mm): Above & right of Ext Clock jack (Jack: 20.32, 103.75 -> LED: 24.80, 98.75)
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(col_b + 4.50, 98.75)), module, Eunice::GATE_LIGHT));

		// 3. Signal In / Internal Random LED (Bipolar Green/Red 2mm): Above & right of Signal In jack (Jack: 8.13, 89.50 -> LED: 12.60, 84.50)
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(col_a + 4.50, 84.50)), module, Eunice::SIGNAL_LIGHT_GREEN));

		// 4. S&H Out LED (Bipolar Green/Red 2mm): Above & right of S&H Out jack (Jack: 32.51, 103.75 -> LED: 37.00, 98.75)
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(col_c + 4.50, 98.75)), module, Eunice::SH_LIGHT_GREEN));

		// 5. T&H Out LED (Bipolar Green/Red 2mm): Above & right of T&H Out jack (Jack: 32.51, 118.00 -> LED: 37.00, 113.00)
		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(col_c + 4.50, 113.00)), module, Eunice::TH_LIGHT_GREEN));
	}

	void appendContextMenu(Menu* menu) override {
		Eunice* module = dynamic_cast<Eunice*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Distribution Tilt Mode"));

		struct DistModeItem : MenuItem {
			Eunice* module;
			eunice::DistributionMode mode;
			void onAction(const event::Action& e) override {
				module->engine.distMode = mode;
			}
		};

		const char* distLabels[] = {
			"Power-Law Skew (Default)",
			"Sigmoid Bias",
			"Diode Saturation"
		};

		for (int m = 0; m <= 2; m++) {
			auto item = createMenuItem<DistModeItem>(distLabels[m]);
			item->module = module;
			item->mode = static_cast<eunice::DistributionMode>(m);
			item->rightText = (module->engine.distMode == m) ? "\u2713" : "";
			menu->addChild(item);
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Slew Limiting Profile"));

		struct SlewProfileItem : MenuItem {
			Eunice* module;
			eunice::SlewProfile profile;
			void onAction(const event::Action& e) override {
				module->engine.slewProfile = profile;
			}
		};

		const char* slewLabels[] = {
			"Linear Ramp (Default)",
			"Exponential RC"
		};

		for (int s = 0; s <= 1; s++) {
			auto item = createMenuItem<SlewProfileItem>(slewLabels[s]);
			item->module = module;
			item->profile = static_cast<eunice::SlewProfile>(s);
			item->rightText = (module->engine.slewProfile == s) ? "\u2713" : "";
			menu->addChild(item);
		}
	}
};

Model* modelEunice = createModel<Eunice, EuniceWidget>("Eunice");
