#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Persephone — End-of-Chain XYRGB Laser Safety Interlock & KPPS Engine
//  Utility class module providing galvanometer mechanical protection,
//  vector slew rate limiting, scan-fail optical extinction, blanking on
//  jumps, thermal coil overload protection, and diode reverse-bias safety.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}
} // namespace

struct Persephone : Module {
	enum KppsRating {
		KPPS_12 = 0,
		KPPS_20,
		KPPS_30, // Default ILDA reference
		KPPS_40,
		KPPS_60,
		KPPS_90,
		KPPS_LEN
	};

	enum ScanFailMode {
		SCANFAIL_STRICT = 0, // 1 ms threshold
		SCANFAIL_NORMAL,     // 3 ms threshold (Default)
		SCANFAIL_RELAXED,    // 10 ms threshold
		SCANFAIL_DISABLED,   // Bypass (Warning: Laser burn hazard)
		SCANFAIL_LEN
	};

	enum TrajectoryRange {
		TRAJ_BIPOLAR_5V = 0, // ±5V (Default ILDA standard)
		TRAJ_BIPOLAR_10V,    // ±10V (Eurorack standard)
		TRAJ_BIPOLAR_1V,     // ±1V
		TRAJ_LEN
	};

	enum LaserRange {
		LASER_0_5V = 0, // 0V to +5V (Default ILDA analog modulation)
		LASER_0_1V,     // 0V to +1V
		LASER_0_10V,    // 0V to +10V
		LASER_LEN
	};

	enum JumpBlankingMode {
		JUMP_BLANK_ON = 0, // Default: blanks laser during large transit jumps
		JUMP_BLANK_OFF,
		JUMP_BLANK_LEN
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};

	enum ParamId {
		ARM_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		ARM_CV_INPUT,
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
		ARM_LIGHT,
		SAFE_LIGHT,
		FAIL_LIGHT,
		SLEW_LIGHT,
		TEMP_LIGHT,
		LIGHTS_LEN
	};

	// Configuration state with JSON persistence
	KppsRating kppsRating = KPPS_30;
	ScanFailMode scanFailMode = SCANFAIL_NORMAL;
	TrajectoryRange trajRange = TRAJ_BIPOLAR_5V;
	LaserRange laserRange = LASER_0_5V;
	JumpBlankingMode jumpBlanking = JUMP_BLANK_ON;
	SkinTheme skinTheme = THEME_MEDIUM;

	// Internal DSP state
	bool armed = true;
	dsp::BooleanTrigger armTrigger;

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

	// Light smoothing
	float safeLightSmooth = 0.0f;
	float failLightSmooth = 0.0f;
	float slewLightSmooth = 0.0f;
	float tempLightSmooth = 0.0f;

	Persephone() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(ARM_PARAM, 0.0f, 1.0f, 1.0f, "Arm safety shutter");

		configInput(ARM_CV_INPUT, "Arm gate");
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
		// 1. Arm button & gate evaluation
		if (armTrigger.process(params[ARM_PARAM].getValue() > 0.5f)) {
			armed = !armed;
		}

		bool gateHigh = inputs[ARM_CV_INPUT].isConnected()
			? (inputs[ARM_CV_INPUT].getVoltage() >= 1.0f)
			: true;
		bool systemArmed = armed && gateHigh;

		// 2. Physical & mathematical constants for the selected KPPS & Sample Rate
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

		// 5. Galvo Mechanical Inertia (Critically damped 2nd-order lowpass filter)
		// Cutoff frequency fc ~ 0.5 * pps
		float fc = clampf(0.5f * pps, 100.0f, 0.45f * args.sampleRate);
		float omega = 2.0f * (float)M_PI * fc;
		float alpha = clampf(omega * dt, 0.001f, 0.95f);

		// Critical damping: filter1 tracks galvo, filter2 tracks filter1
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
		} else {
			if (smoothedVelocity < minVelocityThreshold) {
				scanFailTripped = true;
				scanFailRecoveryTimer = 0;
			} else if (scanFailTripped) {
				// Require continuous safe motion for ~2ms before re-enabling laser
				scanFailRecoveryTimer++;
				if (scanFailRecoveryTimer > (int)(0.002f * args.sampleRate)) {
					scanFailTripped = false;
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
		// Coil current force is proportional to acceleration: d(vel)/dt
		float instAccel = std::fabs(instVelocity - prevVelocity) * args.sampleRate;
		prevVelocity = instVelocity;

		// Normalized heating power (referenced to safe continuous max acceleration)
		float maxAccelRef = maxSlewRate * pps * 0.15f;
		float normStress = (instAccel / (maxAccelRef + 1e-4f));
		normStress = normStress * normStress;

		float tauThermal = 0.5f; // 500 ms thermal time constant
		float alphaThermal = dt / tauThermal;
		thermalStress += alphaThermal * (normStress - thermalStress);
		if (thermalStress < 0.0f) thermalStress = 0.0f;

		// 9. Color delay alignment buffer
		// Real galvos have mechanical group delay ~ 2.5 / pps
		float delaySec = 2.5f / pps;
		int delaySamples = (int)(delaySec * args.sampleRate + 0.5f);
		delaySamples = clampf(delaySamples, 1, DELAY_BUF_SIZE - 1);

		// Store incoming RGB in ring buffer
		rBuf[delayWriteIndex] = inputs[R_INPUT].getVoltage();
		gBuf[delayWriteIndex] = inputs[G_INPUT].getVoltage();
		bBuf[delayWriteIndex] = inputs[B_INPUT].getVoltage();

		int delayReadIndex = (delayWriteIndex - delaySamples + DELAY_BUF_SIZE) % DELAY_BUF_SIZE;
		float delayedR = rBuf[delayReadIndex];
		float delayedG = gBuf[delayReadIndex];
		float delayedB = bBuf[delayReadIndex];

		delayWriteIndex = (delayWriteIndex + 1) % DELAY_BUF_SIZE;

		// 10. Shutter, Interlock & Safety Gating
		bool emissionPermitted = systemArmed && !scanFailTripped && !jumpActive;

		float outR = 0.0f;
		float outG = 0.0f;
		float outB = 0.0f;

		if (emissionPermitted) {
			// Normalize incoming 0..5V standard to selected output laser voltage standard
			// Clamping prevents reverse bias and overdrive
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

		// 12. Visual indicators with smooth falloff
		float lightDt = clampf(15.0f * dt, 0.001f, 0.5f);

		bool isSafe = systemArmed && !scanFailTripped;
		safeLightSmooth += lightDt * ((isSafe ? 1.0f : 0.0f) - safeLightSmooth);
		failLightSmooth += lightDt * ((scanFailTripped ? 1.0f : 0.0f) - failLightSmooth);
		slewLightSmooth += lightDt * ((slewActive ? 1.0f : 0.0f) - slewLightSmooth);
		tempLightSmooth += lightDt * ((thermalStress > 0.8f ? 1.0f : 0.0f) - tempLightSmooth);

		lights[ARM_LIGHT].setBrightness(armed ? 1.0f : 0.1f);
		lights[SAFE_LIGHT].setBrightness(safeLightSmooth);
		lights[FAIL_LIGHT].setBrightness(failLightSmooth);
		lights[SLEW_LIGHT].setBrightness(slewLightSmooth);
		lights[TEMP_LIGHT].setBrightness(tempLightSmooth);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "kppsRating", json_integer((int)kppsRating));
		json_object_set_new(rootJ, "scanFailMode", json_integer((int)scanFailMode));
		json_object_set_new(rootJ, "trajRange", json_integer((int)trajRange));
		json_object_set_new(rootJ, "laserRange", json_integer((int)laserRange));
		json_object_set_new(rootJ, "jumpBlanking", json_integer((int)jumpBlanking));
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "armed", json_boolean(armed));
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

		json_t* armJ = json_object_get(rootJ, "armed");
		if (armJ) armed = json_is_true(armJ);
	}
};

struct PersephonePort : app::SvgPort {
	PersephonePort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

struct PersephoneButton : app::SvgSwitch {
	PersephoneButton() {
		momentary = false;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

struct PersephoneArmLightRing : ModuleLightWidget {
	PersephoneArmLightRing() {
		box.size = mm2px(Vec(8.24f, 8.24f));
	}
	void draw(const DrawArgs& args) override {
		if (!module) return;
		float b = module->lights[Persephone::ARM_LIGHT].getBrightness();
		nvgBeginPath(args.vg);
		float cx = box.size.x / 2.0f;
		float cy = box.size.y / 2.0f;
		float r = mm2px(4.120f);
		nvgCircle(args.vg, cx, cy, r);
		nvgStrokeWidth(args.vg, mm2px(1.400f));
		NVGcolor c = nvgRGBAf(0.1f + 0.9f * b, 0.7f * b, 0.1f * b, 0.25f + 0.75f * b);
		nvgStrokeColor(args.vg, c);
		nvgStroke(args.vg);
	}
};

struct PersephoneWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Persephone::SkinTheme currentTheme = Persephone::THEME_MEDIUM;

	void setTheme(Persephone::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Persephone::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Persephone_Light.svg");
					break;
				case Persephone::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Persephone_Dark.svg");
					break;
				case Persephone::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Persephone.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Persephone*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		Persephone* mod = dynamic_cast<Persephone*>(this->module);
		if (!mod) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Scanner Speed (KPPS)"));

		static const char* kppsLabels[] = {
			"12 kpps (Budget / Entry Scanners)",
			"20 kpps (Club Scanners)",
			"30 kpps (ILDA Standard Reference)",
			"40 kpps (High Speed Scanners)",
			"60 kpps (Cambridge / ScannerMAX)",
			"90 kpps (Ultra-Fast Graphic Scanners)"
		};

		for (int i = 0; i < Persephone::KPPS_LEN; ++i) {
			Persephone::KppsRating r = (Persephone::KppsRating)i;
			menu->addChild(createCheckMenuItem(kppsLabels[i], "",
				[=]() { return mod->kppsRating == r; },
				[=]() { mod->kppsRating = r; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Scan-Fail Protection"));

		static const char* scanFailLabels[] = {
			"Strict (1 ms Cutoff)",
			"Normal (3 ms Cutoff - Default)",
			"Relaxed (10 ms Cutoff)",
			"Disabled (Warning: Laser Burn Hazard)"
		};

		for (int i = 0; i < Persephone::SCANFAIL_LEN; ++i) {
			Persephone::ScanFailMode sf = (Persephone::ScanFailMode)i;
			menu->addChild(createCheckMenuItem(scanFailLabels[i], "",
				[=]() { return mod->scanFailMode == sf; },
				[=]() { mod->scanFailMode = sf; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Jump Blanking"));

		menu->addChild(createCheckMenuItem("Blank Laser On Large Transit Jumps", "",
			[=]() { return mod->jumpBlanking == Persephone::JUMP_BLANK_ON; },
			[=]() { mod->jumpBlanking = Persephone::JUMP_BLANK_ON; }
		));
		menu->addChild(createCheckMenuItem("Draw Through Jumps (Unblanked)", "",
			[=]() { return mod->jumpBlanking == Persephone::JUMP_BLANK_OFF; },
			[=]() { mod->jumpBlanking = Persephone::JUMP_BLANK_OFF; }
		));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Trajectory Voltage Standard"));

		static const char* trajLabels[] = {
			"±5V (ILDA Standard Reference)",
			"±10V (Eurorack Standard)",
			"±1V"
		};

		for (int i = 0; i < Persephone::TRAJ_LEN; ++i) {
			Persephone::TrajectoryRange tr = (Persephone::TrajectoryRange)i;
			menu->addChild(createCheckMenuItem(trajLabels[i], "",
				[=]() { return mod->trajRange == tr; },
				[=]() { mod->trajRange = tr; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Laser Modulation Standard"));

		static const char* laserLabels[] = {
			"0V to 5V (ILDA Analog Modulation)",
			"0V to 1V (Diode Standard)",
			"0V to 10V (Eurorack CV Standard)"
		};

		for (int i = 0; i < Persephone::LASER_LEN; ++i) {
			Persephone::LaserRange lr = (Persephone::LaserRange)i;
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
			Persephone::SkinTheme theme = (Persephone::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	PersephoneWidget(Persephone* module) {
		setModule(module);
		panel = new SvgPanel();
		Persephone::SkinTheme initTheme = module ? module->skinTheme : Persephone::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Persephone::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Persephone_Light.svg");
				break;
			case Persephone::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Persephone_Dark.svg");
				break;
			case Persephone::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Persephone.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		float col0 = 7.00f;
		float col1 = 20.32f;
		float col2 = 33.64f;

		// Zone 2: ARM Button & Halo at Y = 17.50 mm
		float btnY = 17.50f;
		addChild(createLightCentered<PersephoneArmLightRing>(mm2px(Vec(col1, btnY)), module, Persephone::ARM_LIGHT));
		addParam(createParamCentered<PersephoneButton>(mm2px(Vec(col1, btnY)), module, Persephone::ARM_PARAM));

		// Status LEDs at Y = 28.50 mm
		float ledY = 28.50f;
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(10.50f, ledY)), module, Persephone::SAFE_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(17.00f, ledY)), module, Persephone::FAIL_LIGHT));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(23.64f, ledY)), module, Persephone::SLEW_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(30.14f, ledY)), module, Persephone::TEMP_LIGHT));

		// Interlock Arm Gate Jack at Y = 39.50 mm
		float gateY = 39.50f;
		addInput(createInputCentered<PersephonePort>(mm2px(Vec(col1, gateY)), module, Persephone::ARM_CV_INPUT));

		// Zone 4: 5 Jack Rows (Row 0 at 65.00mm, Row 1 at 78.00mm, Row 2 at 91.00mm, Row 3 at 104.00mm, Row 4 at 117.00mm)
		float jackRows[5] = {65.00f, 78.00f, 91.00f, 104.00f, 117.00f};

		// Left Column: Inputs (X, Y, R, G, B)
		addInput(createInputCentered<PersephonePort>(mm2px(Vec(col0, jackRows[0])), module, Persephone::X_INPUT));
		addInput(createInputCentered<PersephonePort>(mm2px(Vec(col0, jackRows[1])), module, Persephone::Y_INPUT));
		addInput(createInputCentered<PersephonePort>(mm2px(Vec(col0, jackRows[2])), module, Persephone::R_INPUT));
		addInput(createInputCentered<PersephonePort>(mm2px(Vec(col0, jackRows[3])), module, Persephone::G_INPUT));
		addInput(createInputCentered<PersephonePort>(mm2px(Vec(col0, jackRows[4])), module, Persephone::B_INPUT));

		// Right Column: Outputs (X, Y, R, G, B)
		addOutput(createOutputCentered<PersephonePort>(mm2px(Vec(col2, jackRows[0])), module, Persephone::X_OUTPUT));
		addOutput(createOutputCentered<PersephonePort>(mm2px(Vec(col2, jackRows[1])), module, Persephone::Y_OUTPUT));
		addOutput(createOutputCentered<PersephonePort>(mm2px(Vec(col2, jackRows[2])), module, Persephone::R_OUTPUT));
		addOutput(createOutputCentered<PersephonePort>(mm2px(Vec(col2, jackRows[3])), module, Persephone::G_OUTPUT));
		addOutput(createOutputCentered<PersephonePort>(mm2px(Vec(col2, jackRows[4])), module, Persephone::B_OUTPUT));
	}
};

Model* modelPersephone = createModel<Persephone, PersephoneWidget>("Persephone");
