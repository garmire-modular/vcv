#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Salus — End-of-Chain XYRGB Laser Safety & KPPS Engine (4 HP)
//  Utility class module providing galvanometer mechanical protection,
//  vector slew rate limiting, scan-fail optical extinction, blanking on
//  jumps, thermal coil overload protection, and manual/CV E-STOP blackout.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}
} // namespace

struct Salus : Module {
	enum KppsRating {
		KPPS_12 = 0,
		KPPS_20,
		KPPS_30,
		KPPS_40,
		KPPS_60,
		KPPS_90,
		KPPS_LEN
	};

	enum ScanFailMode {
		SCANFAIL_STRICT = 0,
		SCANFAIL_NORMAL,
		SCANFAIL_RELAXED,
		SCANFAIL_DISABLED,
		SCANFAIL_LEN
	};

	enum TrajectoryRange {
		TRAJ_BIPOLAR_5V = 0,
		TRAJ_BIPOLAR_10V,
		TRAJ_BIPOLAR_1V,
		TRAJ_LEN
	};

	enum LaserRange {
		LASER_0_5V = 0,
		LASER_0_1V,
		LASER_0_10V,
		LASER_LEN
	};

	enum JumpBlankingMode {
		JUMP_BLANK_ON = 0,
		JUMP_BLANK_OFF,
		JUMP_BLANK_LEN
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};

	enum LedColorState {
		LED_GREEN = 0,
		LED_YELLOW = 1,
		LED_AMBER = 2,
		LED_RED = 3
	};

	enum ParamId {
		ESTOP_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		ESTOP_CV_INPUT,
		X_INPUT,
		Y_INPUT,
		R_INPUT,
		G_INPUT,
		B_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		X_OUTPUT,
		Y_OUTPUT,
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	// Configuration state with JSON persistence
	KppsRating kppsRating = KPPS_30;
	ScanFailMode scanFailMode = SCANFAIL_NORMAL;
	TrajectoryRange trajRange = TRAJ_BIPOLAR_5V;
	LaserRange laserRange = LASER_0_5V;
	JumpBlankingMode jumpBlanking = JUMP_BLANK_ON;
	SkinTheme skinTheme = THEME_MEDIUM;

	// E-Stop state
	bool eStopped = false;
	dsp::BooleanTrigger estopButtonTrigger;
	dsp::SchmittTrigger estopGateTrigger;

	// Physical Galvo state
	float galvoX = 0.0f;
	float galvoY = 0.0f;
	float galvoVx = 0.0f;
	float galvoVy = 0.0f;

	// Inertia filter state (2nd-order critically damped)
	float filterX1 = 0.0f;
	float filterX2 = 0.0f;
	float filterY1 = 0.0f;
	float filterY2 = 0.0f;

	// Scan-fail leaky integrator
	float smoothedVelocity = 200.0f;
	bool scanFailTripped = false;
	int scanFailRecoveryTimer = 0;

	// Thermal coil stress integrator
	float prevVelocity = 0.0f;
	float thermalStress = 0.0f;

	// Jump blanking active state
	bool jumpActive = false;

	// Color delay compensation buffer (fixed 64 samples)
	static const int DELAY_BUF_SIZE = 64;
	float rBuf[DELAY_BUF_SIZE] = {0.0f};
	float gBuf[DELAY_BUF_SIZE] = {0.0f};
	float bBuf[DELAY_BUF_SIZE] = {0.0f};
	int delayWriteIndex = 0;

	// 4-state stop-light indicators (GREEN, YELLOW, AMBER, RED)
	LedColorState scanLedState = LED_GREEN;
	LedColorState galvoLedState = LED_GREEN;
	LedColorState tempLedState = LED_GREEN;

	Salus() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(ESTOP_PARAM, 0.0f, 1.0f, 0.0f, "E-stop blackout toggle");

		configInput(ESTOP_CV_INPUT, "E-stop toggle");
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(R_INPUT, "Red");
		configInput(G_INPUT, "Green");
		configInput(B_INPUT, "Blue");

		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(R_OUTPUT, "Red");
		configOutput(G_OUTPUT, "Green");
		configOutput(B_OUTPUT, "Blue");
	}

	float getPointsPerSecond() const {
		switch (kppsRating) {
			case KPPS_12: return 12000.0f;
			case KPPS_20: return 20000.0f;
			case KPPS_40: return 40000.0f;
			case KPPS_60: return 60000.0f;
			case KPPS_90: return 90000.0f;
			case KPPS_30:
			default:      return 30000.0f;
		}
	}

	float getTrajMaxVoltage() const {
		switch (trajRange) {
			case TRAJ_BIPOLAR_10V: return 10.0f;
			case TRAJ_BIPOLAR_1V:  return 1.0f;
			case TRAJ_BIPOLAR_5V:
			default:               return 5.0f;
		}
	}

	float getLaserMaxVoltage() const {
		switch (laserRange) {
			case LASER_0_1V:  return 1.0f;
			case LASER_0_10V: return 10.0f;
			case LASER_0_5V:
			default:          return 5.0f;
		}
	}

	void process(const ProcessArgs& args) override {
		// 1. E-STOP button & trigger evaluation
		if (estopButtonTrigger.process(params[ESTOP_PARAM].getValue() > 0.5f)) {
			eStopped = !eStopped;
		}
		if (inputs[ESTOP_CV_INPUT].isConnected()) {
			if (estopGateTrigger.process(inputs[ESTOP_CV_INPUT].getVoltage())) {
				eStopped = !eStopped;
			}
		}

		// 2. Physical & mathematical constants for selected KPPS & Sample Rate
		float pps = getPointsPerSecond();
		float maxTrajV = getTrajMaxVoltage();
		float maxLaserV = getLaserMaxVoltage();
		float vpp = 2.0f * maxTrajV;
		float dt = args.sampleTime;

		// Maximum safe physical slew rate (V/s):
		// Scanners traverse full scale in ~15 test pattern sample points at rated kpps
		float maxSlewRate = (vpp * pps) / 15.0f;
		float maxStepDistance = maxSlewRate * dt;

		// 3. Read target inputs with trajectory clamping
		float inX = clampf(inputs[X_INPUT].getVoltage(), -maxTrajV, maxTrajV);
		float inY = clampf(inputs[Y_INPUT].getVoltage(), -maxTrajV, maxTrajV);

		// Thermal throttle scaling (backs off trajectory amplitude if coils overheat)
		float thermalScale = 1.0f;
		if (thermalStress > 1.0f) {
			thermalScale = 1.0f / std::sqrt(thermalStress);
		}
		inX *= thermalScale;
		inY *= thermalScale;

		// 4. Vector distance and Euclidean slew limiting
		float dx = inX - galvoX;
		float dy = inY - galvoY;
		float dist = std::sqrt(dx * dx + dy * dy);

		bool slewActive = false;
		if (dist > maxStepDistance && dist > 1e-7f) {
			float ratio = maxStepDistance / dist;
			galvoX += dx * ratio;
			galvoY += dy * ratio;
			slewActive = true;
		} else {
			galvoX = inX;
			galvoY = inY;
		}

		// GALVO LED evaluation (Slew / Step distance vs limit)
		// Green: normal (< 80%)
		// Yellow: within 20% (80% - 90%)
		// Amber: within 10% (90% - 100%)
		// Red: exceeded (>= 100% / slew limiter active)
		float galvoRatio = (maxStepDistance > 1e-7f) ? (dist / maxStepDistance) : 0.0f;
		if (slewActive || galvoRatio >= 1.0f) {
			galvoLedState = LED_RED;
		} else if (galvoRatio >= 0.90f) {
			galvoLedState = LED_AMBER;
		} else if (galvoRatio >= 0.80f) {
			galvoLedState = LED_YELLOW;
		} else {
			galvoLedState = LED_GREEN;
		}

		// 5. Galvo Mechanical Inertia (Critically damped 2nd-order lowpass filter)
		float fc = clampf(0.5f * pps, 100.0f, 0.45f * args.sampleRate);
		float omega = 2.0f * (float)M_PI * fc;
		float alpha = clampf(omega * dt, 0.001f, 0.95f);

		filterX1 += alpha * (galvoX - filterX1);
		filterX2 += alpha * (filterX1 - filterX2);
		filterY1 += alpha * (galvoY - filterY1);
		filterY2 += alpha * (filterY1 - filterY2);

		float outX = clampf(filterX2, -maxTrajV, maxTrajV);
		float outY = clampf(filterY2, -maxTrajV, maxTrajV);

		// 6. Instantaneous velocity and scan-fail evaluation
		float stepX = outX - galvoVx;
		float stepY = outY - galvoVy;
		galvoVx = outX;
		galvoVy = outY;
		float instVelocity = std::sqrt(stepX * stepX + stepY * stepY) * args.sampleRate;

		// Scan-fail leaky integration
		float tauScanFail = 0.003f;
		if (scanFailMode == SCANFAIL_STRICT) {
			tauScanFail = 0.001f;
		} else if (scanFailMode == SCANFAIL_RELAXED) {
			tauScanFail = 0.010f;
		}
		float alphaScan = 1.0f - std::exp(-dt / tauScanFail);
		smoothedVelocity += alphaScan * (instVelocity - smoothedVelocity);

		// Velocity threshold for stationary beam safety (~3% full scale per ms at 30k)
		float minVelocityThreshold = 0.03f * vpp * 1000.0f * (pps / 30000.0f);
		if (scanFailMode == SCANFAIL_DISABLED) {
			scanFailTripped = false;
			scanLedState = LED_GREEN;
		} else {
			bool isPatched = inputs[X_INPUT].isConnected() || inputs[Y_INPUT].isConnected();
			if (!isPatched) {
				// Unpatched / no signal: stationary beam protection active (RED)
				scanFailTripped = true;
				scanLedState = LED_RED;
			} else if (smoothedVelocity < minVelocityThreshold) {
				scanFailTripped = true;
				scanFailRecoveryTimer = 0;
				scanLedState = LED_RED;
			} else {
				if (scanFailTripped) {
					scanFailRecoveryTimer++;
					if (scanFailRecoveryTimer > (int)(0.002f * args.sampleRate)) {
						scanFailTripped = false;
					}
					scanLedState = LED_RED;
				} else {
					// Velocity margin approaching stall
					float margin = smoothedVelocity / (minVelocityThreshold + 1e-4f);
					if (margin < 1.10f) {
						scanLedState = LED_AMBER; // within 10%
					} else if (margin < 1.25f) {
						scanLedState = LED_YELLOW; // within 20%
					} else {
						scanLedState = LED_GREEN; // normal
					}
				}
			}
		}

		// 7. Blanking on Jump (Large repositioning movements)
		if (jumpBlanking == JUMP_BLANK_ON) {
			float jumpThreshold = 0.08f * vpp;
			if (dist > jumpThreshold && slewActive) {
				jumpActive = true;
			} else if (jumpActive) {
				if (dist < 0.02f * vpp) {
					jumpActive = false;
				}
			}
		} else {
			jumpActive = false;
		}

		// 8. Thermal coil stress integrator
		float instAccel = std::fabs(instVelocity - prevVelocity) * args.sampleRate;
		prevVelocity = instVelocity;

		float maxAccelRef = maxSlewRate * pps * 0.15f;
		float normStress = (instAccel / (maxAccelRef + 1e-4f));
		normStress = normStress * normStress;

		float tauThermal = 0.5f;
		float alphaThermal = dt / tauThermal;
		thermalStress += alphaThermal * (normStress - thermalStress);
		if (thermalStress < 0.0f) thermalStress = 0.0f;

		// TEMP LED evaluation (Thermal load vs 1.0 limit)
		if (thermalStress >= 1.0f) {
			tempLedState = LED_RED;
		} else if (thermalStress >= 0.90f) {
			tempLedState = LED_AMBER;
		} else if (thermalStress >= 0.80f) {
			tempLedState = LED_YELLOW;
		} else {
			tempLedState = LED_GREEN;
		}

		// 9. Color delay alignment buffer
		float delaySec = 2.5f / pps;
		int delaySamples = (int)(delaySec * args.sampleRate + 0.5f);
		delaySamples = clampf(delaySamples, 1, DELAY_BUF_SIZE - 1);

		rBuf[delayWriteIndex] = inputs[R_INPUT].getVoltage();
		gBuf[delayWriteIndex] = inputs[G_INPUT].getVoltage();
		bBuf[delayWriteIndex] = inputs[B_INPUT].getVoltage();

		int delayReadIndex = (delayWriteIndex - delaySamples + DELAY_BUF_SIZE) % DELAY_BUF_SIZE;
		float delayedR = rBuf[delayReadIndex];
		float delayedG = gBuf[delayReadIndex];
		float delayedB = bBuf[delayReadIndex];

		delayWriteIndex = (delayWriteIndex + 1) % DELAY_BUF_SIZE;

		// 10. Laser Blackout & Emission Decision
		// E-STOP active forces complete blackout
		bool emissionPermitted = !eStopped && !scanFailTripped && !jumpActive;

		float outR = 0.0f;
		float outG = 0.0f;
		float outB = 0.0f;

		if (emissionPermitted) {
			float scaleR = clampf(delayedR / 5.0f, 0.0f, 1.0f);
			float scaleG = clampf(delayedG / 5.0f, 0.0f, 1.0f);
			float scaleB = clampf(delayedB / 5.0f, 0.0f, 1.0f);

			outR = scaleR * maxLaserV;
			outG = scaleG * maxLaserV;
			outB = scaleB * maxLaserV;
		}

		// 11. Write outputs
		outputs[X_OUTPUT].setVoltage(outX);
		outputs[Y_OUTPUT].setVoltage(outY);
		outputs[R_OUTPUT].setVoltage(outR);
		outputs[G_OUTPUT].setVoltage(outG);
		outputs[B_OUTPUT].setVoltage(outB);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "kppsRating", json_integer((int)kppsRating));
		json_object_set_new(rootJ, "scanFailMode", json_integer((int)scanFailMode));
		json_object_set_new(rootJ, "trajRange", json_integer((int)trajRange));
		json_object_set_new(rootJ, "laserRange", json_integer((int)laserRange));
		json_object_set_new(rootJ, "jumpBlanking", json_integer((int)jumpBlanking));
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "eStopped", json_boolean(eStopped));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* kpJ = json_object_get(rootJ, "kppsRating");
		if (kpJ) kppsRating = (KppsRating)json_integer_value(kpJ);

		json_t* sfJ = json_object_get(rootJ, "scanFailMode");
		if (sfJ) scanFailMode = (ScanFailMode)json_integer_value(sfJ);

		json_t* trJ = json_object_get(rootJ, "trajRange");
		if (trJ) trajRange = (TrajectoryRange)json_integer_value(trJ);

		json_t* lrJ = json_object_get(rootJ, "laserRange");
		if (lrJ) laserRange = (LaserRange)json_integer_value(lrJ);

		json_t* jbJ = json_object_get(rootJ, "jumpBlanking");
		if (jbJ) jumpBlanking = (JumpBlankingMode)json_integer_value(jbJ);

		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);

		json_t* esJ = json_object_get(rootJ, "eStopped");
		if (esJ) eStopped = json_is_true(esJ);
	}
};

struct SalusPort : app::SvgPort {
	SalusPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

struct SalusRedButton : app::SvgSwitch {
	SalusRedButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/btn_red.svg")));
	}
};

struct Salus4mmLed : Widget {
	Salus* module = nullptr;
	int channel = 0; // 0=SCAN, 1=GALVO, 2=TEMP
	std::shared_ptr<Svg> svgGreen;
	std::shared_ptr<Svg> svgYellow;
	std::shared_ptr<Svg> svgAmber;
	std::shared_ptr<Svg> svgRed;

	Salus4mmLed(Salus* mod, int ch) : module(mod), channel(ch) {
		svgGreen = Svg::load(asset::plugin(pluginInstance, "res/led_4mm_green.svg"));
		svgYellow = Svg::load(asset::plugin(pluginInstance, "res/led_4mm_yellow.svg"));
		svgAmber = Svg::load(asset::plugin(pluginInstance, "res/led_4mm_amber.svg"));
		svgRed = Svg::load(asset::plugin(pluginInstance, "res/led_4mm_red.svg"));
		box.size = Vec(11.34f, 11.34f);
	}

	void draw(const DrawArgs& args) override {
		Salus::LedColorState state = Salus::LED_GREEN;
		if (module) {
			if (channel == 0) state = module->scanLedState;
			else if (channel == 1) state = module->galvoLedState;
			else if (channel == 2) state = module->tempLedState;
		}
		std::shared_ptr<Svg> s = svgGreen;
		switch (state) {
			case Salus::LED_YELLOW: s = svgYellow; break;
			case Salus::LED_AMBER:  s = svgAmber;  break;
			case Salus::LED_RED:    s = svgRed;    break;
			case Salus::LED_GREEN:
			default:                s = svgGreen;  break;
		}
		if (s && s->handle) {
			svgDraw(args.vg, s->handle);
		}
	}
};

struct SalusWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Salus::SkinTheme currentTheme = Salus::THEME_MEDIUM;

	void setTheme(Salus::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Salus::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Salus_Light.svg");
					break;
				case Salus::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Salus_Dark.svg");
					break;
				case Salus::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Salus.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Salus*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		Salus* mod = dynamic_cast<Salus*>(this->module);
		if (!mod) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Scanner Speed"));

		static const char* kppsLabels[] = {
			"12 kpps",
			"20 kpps",
			"30 kpps",
			"40 kpps",
			"60 kpps",
			"90 kpps"
		};

		for (int i = 0; i < Salus::KPPS_LEN; ++i) {
			Salus::KppsRating r = (Salus::KppsRating)i;
			menu->addChild(createCheckMenuItem(kppsLabels[i], "",
				[=]() { return mod->kppsRating == r; },
				[=]() { mod->kppsRating = r; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Scan-Fail Protection"));

		static const char* scanFailLabels[] = {
			"Strict (1 ms)",
			"Normal (3 ms)",
			"Relaxed (10 ms)",
			"Disabled"
		};

		for (int i = 0; i < Salus::SCANFAIL_LEN; ++i) {
			Salus::ScanFailMode sf = (Salus::ScanFailMode)i;
			menu->addChild(createCheckMenuItem(scanFailLabels[i], "",
				[=]() { return mod->scanFailMode == sf; },
				[=]() { mod->scanFailMode = sf; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Jump Blanking"));

		menu->addChild(createCheckMenuItem("Blank on jumps", "",
			[=]() { return mod->jumpBlanking == Salus::JUMP_BLANK_ON; },
			[=]() { mod->jumpBlanking = Salus::JUMP_BLANK_ON; }
		));
		menu->addChild(createCheckMenuItem("Off", "",
			[=]() { return mod->jumpBlanking == Salus::JUMP_BLANK_OFF; },
			[=]() { mod->jumpBlanking = Salus::JUMP_BLANK_OFF; }
		));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Trajectory Voltage"));

		static const char* trajLabels[] = {
			"±5V",
			"±10V",
			"±1V"
		};

		for (int i = 0; i < Salus::TRAJ_LEN; ++i) {
			Salus::TrajectoryRange tr = (Salus::TrajectoryRange)i;
			menu->addChild(createCheckMenuItem(trajLabels[i], "",
				[=]() { return mod->trajRange == tr; },
				[=]() { mod->trajRange = tr; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Laser Modulation"));

		static const char* laserLabels[] = {
			"0V to 5V",
			"0V to 1V",
			"0V to 10V"
		};

		for (int i = 0; i < Salus::LASER_LEN; ++i) {
			Salus::LaserRange lr = (Salus::LaserRange)i;
			menu->addChild(createCheckMenuItem(laserLabels[i], "",
				[=]() { return mod->laserRange == lr; },
				[=]() { mod->laserRange = lr; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));

		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			Salus::SkinTheme theme = (Salus::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	SalusWidget(Salus* module) {
		setModule(module);
		panel = new SvgPanel();
		Salus::SkinTheme initTheme = module ? module->skinTheme : Salus::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Salus::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Salus_Light.svg");
				break;
			case Salus::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Salus_Dark.svg");
				break;
			case Salus::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Salus.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		float colIn = 5.20f;
		float colOut = 15.12f;

		// 3 LED Layers (Layout: LABEL on left, LED on right at Y = 17.50, 24.50, 31.50 mm)
		float ledYs[3] = {17.50f, 24.50f, 31.50f};
		for (int ch = 0; ch < 3; ++ch) {
			Vec posPx = mm2px(Vec(colOut, ledYs[ch]));
			auto* led = new Salus4mmLed(module, ch);
			led->box.pos = posPx.minus(led->box.size.div(2.0f));
			addChild(led);
		}

		// E-STOP Section at Y = 45.00 mm: Red button (colIn) + GATE CV Jack (colOut)
		float estopY = 45.00f;
		addParam(createParamCentered<SalusRedButton>(mm2px(Vec(colIn, estopY)), module, Salus::ESTOP_PARAM));
		addInput(createInputCentered<SalusPort>(mm2px(Vec(colOut, estopY)), module, Salus::ESTOP_CV_INPUT));

		// ROW 3-7: XYRGB in/outs (Row 3 at 65.00mm, Row 4 at 78.00mm, Row 5 at 91.00mm, Row 6 at 104.00mm, Row 7 at 117.00mm)
		float jackRows[5] = {65.00f, 78.00f, 91.00f, 104.00f, 117.00f};

		// Left Column: Inputs (X, Y, R, G, B)
		addInput(createInputCentered<SalusPort>(mm2px(Vec(colIn, jackRows[0])), module, Salus::X_INPUT));
		addInput(createInputCentered<SalusPort>(mm2px(Vec(colIn, jackRows[1])), module, Salus::Y_INPUT));
		addInput(createInputCentered<SalusPort>(mm2px(Vec(colIn, jackRows[2])), module, Salus::R_INPUT));
		addInput(createInputCentered<SalusPort>(mm2px(Vec(colIn, jackRows[3])), module, Salus::G_INPUT));
		addInput(createInputCentered<SalusPort>(mm2px(Vec(colIn, jackRows[4])), module, Salus::B_INPUT));

		// Right Column: Outputs (X, Y, R, G, B)
		addOutput(createOutputCentered<SalusPort>(mm2px(Vec(colOut, jackRows[0])), module, Salus::X_OUTPUT));
		addOutput(createOutputCentered<SalusPort>(mm2px(Vec(colOut, jackRows[1])), module, Salus::Y_OUTPUT));
		addOutput(createOutputCentered<SalusPort>(mm2px(Vec(colOut, jackRows[2])), module, Salus::R_OUTPUT));
		addOutput(createOutputCentered<SalusPort>(mm2px(Vec(colOut, jackRows[3])), module, Salus::G_OUTPUT));
		addOutput(createOutputCentered<SalusPort>(mm2px(Vec(colOut, jackRows[4])), module, Salus::B_OUTPUT));
	}
};

Model* modelSalus = createModel<Salus, SalusWidget>("Salus");
