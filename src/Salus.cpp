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

	enum StatusLedColor {
		STATUS_LED_GREEN = 0,
		STATUS_LED_YELLOW = 1,
		STATUS_LED_AMBER = 2
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

	// Scan-fail leaky integrator & stationary dwell timer
	float smoothedVelocity = 200.0f;
	float stationaryTimer = 0.0f;
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

	// Dual-column indicator state (0=SCAN, 1=GALVO, 2=TEMP)
	// Col 1: Status LED (Green -> Yellow -> Amber, turns off/0 while red is active)
	// Col 2: Trip LED (Red latch while active, 2.0s slow fade release to red_0)
	StatusLedColor statusLedState[3] = {STATUS_LED_GREEN, STATUS_LED_GREEN, STATUS_LED_GREEN};
	float statusCooldown[3] = {0.0f, 0.0f, 0.0f};
	float tripLedBrightness[3] = {0.0f, 0.0f, 0.0f};
	bool tripActive[3] = {false, false, false};

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

	float getDwellTimeout() const {
		switch (scanFailMode) {
			case SCANFAIL_STRICT:  return 0.010f; // 10 ms
			case SCANFAIL_RELAXED: return 0.100f; // 100 ms
			case SCANFAIL_NORMAL:
			default:               return 0.030f; // 30 ms
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

		// GALVO slew ratio vs limit
		float galvoRatio = (maxStepDistance > 1e-7f) ? (dist / maxStepDistance) : 0.0f;

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

		// 6. Instantaneous velocity and stationary beam (scan-fail) safety
		float stepX = outX - galvoVx;
		float stepY = outY - galvoVy;
		galvoVx = outX;
		galvoVy = outY;
		float instVelocity = std::sqrt(stepX * stepX + stepY * stepY) * args.sampleRate;

		// Smooth velocity with a 10ms leaky integrator for warning stability
		float alphaScan = 1.0f - std::exp(-dt / 0.010f);
		smoothedVelocity += alphaScan * (instVelocity - smoothedVelocity);

		bool isPatched = inputs[X_INPUT].isConnected() || inputs[Y_INPUT].isConnected();
		float dwellTimeout = getDwellTimeout();

		if (scanFailMode == SCANFAIL_DISABLED) {
			scanFailTripped = false;
			stationaryTimer = 0.0f;
		} else {
			float laserSum = std::fabs(inputs[R_INPUT].getVoltage()) +
			                 std::fabs(inputs[G_INPUT].getVoltage()) +
			                 std::fabs(inputs[B_INPUT].getVoltage());

			if (!isPatched) {
				// Scanner inputs not connected
				if (laserSum > 0.05f) {
					// Danger: lasers firing with unpatched scanners (stationary center hot spot)
					scanFailTripped = true;
				} else {
					// Normal unpatched standby
					scanFailTripped = false;
					stationaryTimer = 0.0f;
				}
			} else {
				// Scanners patched: check for stationary beam
				// Truly stationary beam: velocity < 0.25 V/s, OR beam parked at center (0,0) with velocity < 1.0 V/s
				bool isParkedAtZero = (std::fabs(outX) < 0.05f && std::fabs(outY) < 0.05f && instVelocity < 1.0f);
				bool isStationary = (instVelocity < 0.25f) || isParkedAtZero;

				if (isStationary) {
					stationaryTimer += dt;
					if (stationaryTimer >= dwellTimeout) {
						scanFailTripped = true;
					}
				} else {
					// Beam is moving (active scanning)
					if (stationaryTimer > 0.0f) {
						stationaryTimer -= 4.0f * dt;
						if (stationaryTimer < 0.0f) stationaryTimer = 0.0f;
					}

					if (scanFailTripped) {
						// Recover once beam has sustained motion for a few samples
						scanFailRecoveryTimer++;
						if (scanFailRecoveryTimer > (int)(0.002f * args.sampleRate)) {
							scanFailTripped = false;
							scanFailRecoveryTimer = 0;
						}
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

		// 9. Dual-Column LED State Determination (0=SCAN, 1=GALVO, 2=TEMP)
		// Col 1 (Status): Green -> Yellow -> Amber with cooldown smoothing
		// Col 2 (Trip): Red latch while tripped + 2.0s slow fade release
		StatusLedColor targetStatus[3] = {STATUS_LED_GREEN, STATUS_LED_GREEN, STATUS_LED_GREEN};
		bool tripActive[3] = {false, false, false};

		// --- Channel 0: SCAN ---
		tripActive[0] = scanFailTripped || eStopped;
		if (scanFailMode != SCANFAIL_DISABLED) {
			float dwellRatio = (dwellTimeout > 1e-5f) ? (stationaryTimer / dwellTimeout) : 0.0f;
			if (dwellRatio >= 0.90f || (isPatched && smoothedVelocity < 0.50f)) {
				targetStatus[0] = STATUS_LED_AMBER;
			} else if (dwellRatio >= 0.80f || (isPatched && smoothedVelocity < 1.00f)) {
				targetStatus[0] = STATUS_LED_YELLOW;
			} else {
				targetStatus[0] = STATUS_LED_GREEN;
			}
		}

		// --- Channel 1: GALVO ---
		tripActive[1] = slewActive || (galvoRatio >= 1.0f) || eStopped;
		if (galvoRatio >= 0.90f) {
			targetStatus[1] = STATUS_LED_AMBER;
		} else if (galvoRatio >= 0.80f) {
			targetStatus[1] = STATUS_LED_YELLOW;
		} else {
			targetStatus[1] = STATUS_LED_GREEN;
		}

		// --- Channel 2: TEMP ---
		tripActive[2] = (thermalStress >= 1.0f) || eStopped;
		if (thermalStress >= 0.90f) {
			targetStatus[2] = STATUS_LED_AMBER;
		} else if (thermalStress >= 0.80f) {
			targetStatus[2] = STATUS_LED_YELLOW;
		} else {
			targetStatus[2] = STATUS_LED_GREEN;
		}

		// Apply Cooldown Smoothing on Status LEDs and 2.0s Release Fade on Trip LEDs
		for (int ch = 0; ch < 3; ++ch) {
			StatusLedColor tgt = targetStatus[ch];
			if (tgt > statusLedState[ch]) {
				// Immediate warning upgrade
				statusLedState[ch] = tgt;
				statusCooldown[ch] = 0.25f; // 250ms hold
			} else if (tgt < statusLedState[ch]) {
				// Cooldown hold before downgrading
				if (statusCooldown[ch] > 0.0f) {
					statusCooldown[ch] -= dt;
				} else {
					statusLedState[ch] = tgt;
				}
			} else {
				if (statusLedState[ch] != STATUS_LED_GREEN) {
					statusCooldown[ch] = 0.25f;
				}
			}

			// Red Trip LED: latches full red when active, then 2.0s linear fade release
			if (tripActive[ch]) {
				tripLedBrightness[ch] = 1.0f;
			} else if (tripLedBrightness[ch] > 0.0f) {
				tripLedBrightness[ch] -= dt / 2.0f; // 2.0s linear decay
				if (tripLedBrightness[ch] < 0.0f) {
					tripLedBrightness[ch] = 0.0f;
				}
			}
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
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/btn_red_pressed.svg")));
	}
};

struct SalusStatusLed : Widget {
	Salus* module = nullptr;
	int channel = 0; // 0=SCAN, 1=GALVO, 2=TEMP
	int imgGreen = 0;
	int imgYellow = 0;
	int imgAmber = 0;
	int imgOff = 0;
	std::shared_ptr<Svg> svgGreen;
	std::shared_ptr<Svg> svgYellow;
	std::shared_ptr<Svg> svgAmber;

	SalusStatusLed(Salus* mod, int ch) : module(mod), channel(ch) {
		box.size = mm2px(Vec(4.0f, 4.0f));
		svgGreen = Svg::load(asset::plugin(pluginInstance, "res/led_4mm_green.svg"));
		svgYellow = Svg::load(asset::plugin(pluginInstance, "res/led_4mm_yellow.svg"));
		svgAmber = Svg::load(asset::plugin(pluginInstance, "res/led_4mm_amber.svg"));
	}

	void draw(const DrawArgs& args) override {
		Salus::StatusLedColor state = Salus::STATUS_LED_GREEN;
		bool isActivelyRed = false;
		if (module) {
			state = module->statusLedState[channel];
			isActivelyRed = module->tripActive[channel];
		}

		if (imgGreen <= 0) {
			imgGreen = nvgCreateImage(args.vg, asset::plugin(pluginInstance, "res/led_4mm_green.png").c_str(), 0);
			imgYellow = nvgCreateImage(args.vg, asset::plugin(pluginInstance, "res/led_4mm_yellow.png").c_str(), 0);
			imgAmber = nvgCreateImage(args.vg, asset::plugin(pluginInstance, "res/led_4mm_amber.png").c_str(), 0);
			imgOff = nvgCreateImage(args.vg, asset::plugin(pluginInstance, "res/led_4mm_0.png").c_str(), 0);
		}

		int activeImg = imgGreen;
		std::shared_ptr<Svg> activeSvg = svgGreen;

		if (isActivelyRed) {
			// Second column is actively red: Column 1 turns "off" (led_4mm_0.png)
			activeImg = imgOff;
			activeSvg = nullptr;
		} else {
			// Normal status display (green, yellow, or amber)
			switch (state) {
				case Salus::STATUS_LED_YELLOW: activeImg = imgYellow; activeSvg = svgYellow; break;
				case Salus::STATUS_LED_AMBER:  activeImg = imgAmber;  activeSvg = svgAmber;  break;
				case Salus::STATUS_LED_GREEN:
				default:                       activeImg = imgGreen;  activeSvg = svgGreen;  break;
			}
		}

		if (activeImg > 0) {
			nvgSave(args.vg);
			NVGpaint imgPaint = nvgImagePattern(args.vg, 0.0f, 0.0f, box.size.x, box.size.y, 0.0f, activeImg, 1.0f);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0.0f, 0.0f, box.size.x, box.size.y);
			nvgFillPaint(args.vg, imgPaint);
			nvgFill(args.vg);
			nvgRestore(args.vg);
		} else if (activeSvg && activeSvg->handle) {
			nvgSave(args.vg);
			float sx = box.size.x / activeSvg->handle->width;
			float sy = box.size.y / activeSvg->handle->height;
			nvgScale(args.vg, sx, sy);
			svgDraw(args.vg, activeSvg->handle);
			nvgRestore(args.vg);
		}
	}
};

struct SalusTripLed : Widget {
	Salus* module = nullptr;
	int channel = 0; // 0=SCAN, 1=GALVO, 2=TEMP
	int imgRed = 0;
	int imgRedOff = 0;
	std::shared_ptr<Svg> svgRed;

	SalusTripLed(Salus* mod, int ch) : module(mod), channel(ch) {
		box.size = mm2px(Vec(4.0f, 4.0f));
		svgRed = Svg::load(asset::plugin(pluginInstance, "res/led_4mm_red.svg"));
	}

	void draw(const DrawArgs& args) override {
		float b = module ? module->tripLedBrightness[channel] : 0.0f;

		if (imgRedOff <= 0) {
			imgRedOff = nvgCreateImage(args.vg, asset::plugin(pluginInstance, "res/led_4mm_red_0.png").c_str(), 0);
			imgRed = nvgCreateImage(args.vg, asset::plugin(pluginInstance, "res/led_4mm_red.png").c_str(), 0);
		}

		// 1. Always draw inactive base LED (led_4mm_red_0.png)
		if (imgRedOff > 0) {
			NVGpaint basePaint = nvgImagePattern(args.vg, 0.0f, 0.0f, box.size.x, box.size.y, 0.0f, imgRedOff, 1.0f);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0.0f, 0.0f, box.size.x, box.size.y);
			nvgFillPaint(args.vg, basePaint);
			nvgFill(args.vg);
		}

		// 2. When actively triggered or in the 2.0s release fade, blend bright red LED on top
		if (b > 0.001f) {
			nvgSave(args.vg);
			nvgGlobalAlpha(args.vg, clampf(b, 0.0f, 1.0f));

			if (imgRed > 0) {
				NVGpaint redPaint = nvgImagePattern(args.vg, 0.0f, 0.0f, box.size.x, box.size.y, 0.0f, imgRed, 1.0f);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 0.0f, 0.0f, box.size.x, box.size.y);
				nvgFillPaint(args.vg, redPaint);
				nvgFill(args.vg);
			} else if (svgRed && svgRed->handle) {
				float sx = box.size.x / svgRed->handle->width;
				float sy = box.size.y / svgRed->handle->height;
				nvgScale(args.vg, sx, sy);
				svgDraw(args.vg, svgRed->handle);
			}
			nvgRestore(args.vg);
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
			"Strict (10 ms)",
			"Normal (30 ms)",
			"Relaxed (100 ms)",
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

		// 3 Dual-Column LED Rows at Y = 16.50, 23.50, 30.50 mm
		// Col 1 (colIn = 5.20 mm): Status LED (Green -> Yellow -> Amber, turns off/0 while red is active)
		// Col 2 (colOut = 15.12 mm): Trip LED (Red latch while active, 2.0s slow fade release to red_0)
		float ledYs[3] = {16.50f, 23.50f, 30.50f};
		for (int ch = 0; ch < 3; ++ch) {
			auto* statusLed = new SalusStatusLed(module, ch);
			statusLed->box.pos = mm2px(Vec(colIn, ledYs[ch])).minus(statusLed->box.size.div(2.0f));
			addChild(statusLed);

			auto* tripLed = new SalusTripLed(module, ch);
			tripLed->box.pos = mm2px(Vec(colOut, ledYs[ch])).minus(tripLed->box.size.div(2.0f));
			addChild(tripLed);
		}

		// E-STOP Section: Button placed directly above GATE CV Jack (both centered at colMid = 10.16 mm)
		float colMid = 10.16f;
		float estopBtnY = 40.50f;
		float estopJackY = 50.00f;
		addParam(createParamCentered<SalusRedButton>(mm2px(Vec(colMid, estopBtnY)), module, Salus::ESTOP_PARAM));
		addInput(createInputCentered<SalusPort>(mm2px(Vec(colMid, estopJackY)), module, Salus::ESTOP_CV_INPUT));

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
