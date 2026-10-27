#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Soteria — End-of-Chain XYRGB Laser Safety & KPPS Engine (4 HP)
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

struct Soteria : Module {
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
		ESTOP_LIGHT,
		SCAN_LIGHT_R,
		SCAN_LIGHT_G,
		GALVO_LIGHT_R,
		GALVO_LIGHT_G,
		TEMP_LIGHT_R,
		TEMP_LIGHT_G,
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

	// Traffic light approach values (0.0 = normal/green, 0.85 = yellow, 0.95 = orange, 1.0 = red)
	float scanStress = 0.0f;
	float galvoStress = 0.0f;
	float tempStress = 0.0f;

	// Smoothed light values
	float scanStressSmooth = 0.0f;
	float galvoStressSmooth = 0.0f;
	float tempStressSmooth = 0.0f;

	Soteria() {
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

	void computeTrafficColor(float stress, float& outR, float& outG) const {
		// Stop-light color scheme:
		// Normal (< 0.80): Pure Green (R=0, G=1)
		// Within 20% of failover (0.80 - 0.90): Yellow (R=1, G=1)
		// Within 10% of failover (0.90 - 1.00): Orange (R=1, G=0.45)
		// Exceeded (>= 1.00): Pure Red (R=1, G=0)
		if (stress < 0.80f) {
			outR = 0.0f;
			outG = 1.0f;
		} else if (stress < 0.90f) {
			float t = (stress - 0.80f) / 0.10f;
			outR = t;
			outG = 1.0f;
		} else if (stress < 1.00f) {
			float t = (stress - 0.90f) / 0.10f;
			outR = 1.0f;
			outG = 1.0f - 0.55f * t;
		} else {
			outR = 1.0f;
			outG = 0.0f;
		}
	}

	void process(const ProcessArgs& args) override {
		// 1. E-STOP button & trigger evaluation
		// Single click toggles E-STOP blackout
		if (estopButtonTrigger.process(params[ESTOP_PARAM].getValue() > 0.5f)) {
			eStopped = !eStopped;
		}
		// External gate/trigger pulse toggles E-STOP blackout
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
		float slewRatio = 0.0f;
		if (dist > maxStepDistance && dist > 1e-7f) {
			float ratio = maxStepDistance / dist;
			galvoX += dx * ratio;
			galvoY += dy * ratio;
			slewActive = true;
			slewRatio = 1.0f;
		} else {
			galvoX = inX;
			galvoY = inY;
			slewRatio = (maxStepDistance > 1e-7f) ? (dist / maxStepDistance) : 0.0f;
		}
		galvoStress = slewActive ? 1.0f : slewRatio;

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
			scanStress = 0.0f;
		} else {
			if (smoothedVelocity < minVelocityThreshold) {
				scanFailTripped = true;
				scanFailRecoveryTimer = 0;
				scanStress = 1.0f;
			} else {
				if (scanFailTripped) {
					scanFailRecoveryTimer++;
					if (scanFailRecoveryTimer > (int)(0.002f * args.sampleRate)) {
						scanFailTripped = false;
					}
					scanStress = 1.0f;
				} else {
					// Margin ratio: how close velocity is to failover
					float margin = smoothedVelocity / (minVelocityThreshold + 1e-4f);
					if (margin >= 1.25f) {
						scanStress = 0.0f; // Normal / Green
					} else if (margin >= 1.10f) {
						scanStress = 0.85f; // Within 20% / Yellow
					} else {
						scanStress = 0.95f; // Within 10% / Orange
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
		tempStress = thermalStress;

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
		// If eStopped, complete blackout: all RGB forced to 0.0V immediately
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

		// 12. Visual indicators with smooth response
		float lightDt = clampf(15.0f * dt, 0.001f, 0.5f);
		scanStressSmooth += lightDt * (scanStress - scanStressSmooth);
		galvoStressSmooth += lightDt * (galvoStress - galvoStressSmooth);
		tempStressSmooth += lightDt * (tempStress - tempStressSmooth);

		float sr, sg, gr, gg, tr, tg;
		computeTrafficColor(scanStressSmooth, sr, sg);
		computeTrafficColor(galvoStressSmooth, gr, gg);
		computeTrafficColor(tempStressSmooth, tr, tg);

		lights[ESTOP_LIGHT].setBrightness(eStopped ? 1.0f : 0.0f);

		lights[SCAN_LIGHT_R].setBrightness(sr);
		lights[SCAN_LIGHT_G].setBrightness(sg);

		lights[GALVO_LIGHT_R].setBrightness(gr);
		lights[GALVO_LIGHT_G].setBrightness(gg);

		lights[TEMP_LIGHT_R].setBrightness(tr);
		lights[TEMP_LIGHT_G].setBrightness(tg);
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

struct SoteriaPort : app::SvgPort {
	SoteriaPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

struct SoteriaButton : app::SvgSwitch {
	SoteriaButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

struct SoteriaEstopRing : ModuleLightWidget {
	SoteriaEstopRing() {
		box.size = mm2px(Vec(8.24f, 8.24f));
	}
	void draw(const DrawArgs& args) override {
		if (!module) return;
		float b = module->lights[Soteria::ESTOP_LIGHT].getBrightness();
		if (b < 0.05f) return;
		nvgBeginPath(args.vg);
		float cx = box.size.x / 2.0f;
		float cy = box.size.y / 2.0f;
		float r = mm2px(4.120f);
		nvgCircle(args.vg, cx, cy, r);
		nvgStrokeWidth(args.vg, mm2px(1.400f));
		nvgStrokeColor(args.vg, nvgRGBAf(0.95f, 0.08f, 0.08f, 0.95f * b));
		nvgStroke(args.vg);
	}
};

struct SoteriaStatusLed : ModuleLightWidget {
	int redLightId = 0;
	int greenLightId = 0;

	SoteriaStatusLed(int rId = 0, int gId = 0) : redLightId(rId), greenLightId(gId) {
		box.size = mm2px(Vec(4.40f, 4.40f));
	}

	void draw(const DrawArgs& args) override {
		if (!module) return;
		float r = module->lights[redLightId].getBrightness();
		float g = module->lights[greenLightId].getBrightness();

		float cx = box.size.x / 2.0f;
		float cy = box.size.y / 2.0f;
		float rad = mm2px(1.85f);

		// Outer halo glow
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, rad * 1.35f);
		nvgFillColor(args.vg, nvgRGBAf(r, g, 0.0f, 0.20f));
		nvgFill(args.vg);

		// Inner illuminated LED lens
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, rad);
		nvgFillColor(args.vg, nvgRGBAf(r, g, 0.05f, 0.95f));
		nvgFill(args.vg);

		// Subtle glass reflection highlight
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx - rad * 0.25f, cy - rad * 0.25f, rad * 0.40f);
		nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.35f));
		nvgFill(args.vg);
	}
};

struct SoteriaWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Soteria::SkinTheme currentTheme = Soteria::THEME_MEDIUM;

	void setTheme(Soteria::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Soteria::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Soteria_Light.svg");
					break;
				case Soteria::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Soteria_Dark.svg");
					break;
				case Soteria::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Soteria.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Soteria*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		Soteria* mod = dynamic_cast<Soteria*>(this->module);
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

		for (int i = 0; i < Soteria::KPPS_LEN; ++i) {
			Soteria::KppsRating r = (Soteria::KppsRating)i;
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

		for (int i = 0; i < Soteria::SCANFAIL_LEN; ++i) {
			Soteria::ScanFailMode sf = (Soteria::ScanFailMode)i;
			menu->addChild(createCheckMenuItem(scanFailLabels[i], "",
				[=]() { return mod->scanFailMode == sf; },
				[=]() { mod->scanFailMode = sf; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Jump Blanking"));

		menu->addChild(createCheckMenuItem("Blank on jumps", "",
			[=]() { return mod->jumpBlanking == Soteria::JUMP_BLANK_ON; },
			[=]() { mod->jumpBlanking = Soteria::JUMP_BLANK_ON; }
		));
		menu->addChild(createCheckMenuItem("Off", "",
			[=]() { return mod->jumpBlanking == Soteria::JUMP_BLANK_OFF; },
			[=]() { mod->jumpBlanking = Soteria::JUMP_BLANK_OFF; }
		));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Trajectory Voltage"));

		static const char* trajLabels[] = {
			"±5V",
			"±10V",
			"±1V"
		};

		for (int i = 0; i < Soteria::TRAJ_LEN; ++i) {
			Soteria::TrajectoryRange tr = (Soteria::TrajectoryRange)i;
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

		for (int i = 0; i < Soteria::LASER_LEN; ++i) {
			Soteria::LaserRange lr = (Soteria::LaserRange)i;
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
			Soteria::SkinTheme theme = (Soteria::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	SoteriaWidget(Soteria* module) {
		setModule(module);
		panel = new SvgPanel();
		Soteria::SkinTheme initTheme = module ? module->skinTheme : Soteria::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Soteria::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Soteria_Light.svg");
				break;
			case Soteria::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Soteria_Dark.svg");
				break;
			case Soteria::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Soteria.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		float colIn = 5.20f;
		float colOut = 15.12f;

		// ROW 1: SCAN, GALVO, TEMP LEDs at Y = 18.20 mm
		float ledY = 18.20f;
		addChild(createLightCentered<SoteriaStatusLed>(mm2px(Vec(4.20f, ledY)), module, Soteria::SCAN_LIGHT_R));
		addChild(createLightCentered<SoteriaStatusLed>(mm2px(Vec(10.16f, ledY)), module, Soteria::GALVO_LIGHT_R));
		addChild(createLightCentered<SoteriaStatusLed>(mm2px(Vec(16.12f, ledY)), module, Soteria::TEMP_LIGHT_R));

		// ROW 2: E-STOP button with red halo when active, and E-STOP GATE CV Jack at Y = 36.00 mm
		float btnY = 36.00f;
		addChild(createLightCentered<SoteriaEstopRing>(mm2px(Vec(colIn, btnY)), module, Soteria::ESTOP_LIGHT));
		addParam(createParamCentered<SoteriaButton>(mm2px(Vec(colIn, btnY)), module, Soteria::ESTOP_PARAM));
		addInput(createInputCentered<SoteriaPort>(mm2px(Vec(colOut, btnY)), module, Soteria::ESTOP_CV_INPUT));

		// ROW 3-7: XYRGB in/outs (Row 3 at 65.00mm, Row 4 at 78.00mm, Row 5 at 91.00mm, Row 6 at 104.00mm, Row 7 at 117.00mm)
		float jackRows[5] = {65.00f, 78.00f, 91.00f, 104.00f, 117.00f};

		// Left Column: Inputs (X, Y, R, G, B)
		addInput(createInputCentered<SoteriaPort>(mm2px(Vec(colIn, jackRows[0])), module, Soteria::X_INPUT));
		addInput(createInputCentered<SoteriaPort>(mm2px(Vec(colIn, jackRows[1])), module, Soteria::Y_INPUT));
		addInput(createInputCentered<SoteriaPort>(mm2px(Vec(colIn, jackRows[2])), module, Soteria::R_INPUT));
		addInput(createInputCentered<SoteriaPort>(mm2px(Vec(colIn, jackRows[3])), module, Soteria::G_INPUT));
		addInput(createInputCentered<SoteriaPort>(mm2px(Vec(colIn, jackRows[4])), module, Soteria::B_INPUT));

		// Right Column: Outputs (X, Y, R, G, B)
		addOutput(createOutputCentered<SoteriaPort>(mm2px(Vec(colOut, jackRows[0])), module, Soteria::X_OUTPUT));
		addOutput(createOutputCentered<SoteriaPort>(mm2px(Vec(colOut, jackRows[1])), module, Soteria::Y_OUTPUT));
		addOutput(createOutputCentered<SoteriaPort>(mm2px(Vec(colOut, jackRows[2])), module, Soteria::R_OUTPUT));
		addOutput(createOutputCentered<SoteriaPort>(mm2px(Vec(colOut, jackRows[3])), module, Soteria::G_OUTPUT));
		addOutput(createOutputCentered<SoteriaPort>(mm2px(Vec(colOut, jackRows[4])), module, Soteria::B_OUTPUT));
	}
};

Model* modelSoteria = createModel<Soteria, SoteriaWidget>("Soteria");
