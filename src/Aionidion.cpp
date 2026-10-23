#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Aionidion — High-Speed Event & Function Generator (8 HP)
//  Short timebase version of Aion with seconds (up to 99) and
//  milliseconds (up to 999), 88"888ms 7-segment display, stacked step buttons,
//  latched run timing (editing time or CV prepares next trigger without
//  disturbing current cycle), snug 88"888 countdown LCD, selectable voltage
//  range, start trigger/gate input, active high/low gates, trigger & EOC pulses,
//  and synchronous continuous functions (Ramp+, Ramp-, Tri+, Tri-, Sin+, Sin-, Pulse).
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Aionidion : Module {
	enum VoltageRange {
		RANGE_BIPOLAR_1V = 0,
		RANGE_UNIPOLAR_1V,
		RANGE_BIPOLAR_5V,
		RANGE_UNIPOLAR_5V,
		RANGE_BIPOLAR_10V,
		RANGE_UNIPOLAR_10V,
		NUM_RANGES
	};

	enum ParamId {
		START_PARAM,
		PWM_PARAM,
		SEC_ATTEN_PARAM,
		MS_ATTEN_PARAM,
		PWM_ATTEN_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		SEC_CV_INPUT,
		MS_CV_INPUT,
		PWM_CV_INPUT,
		START_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		TRIG_OUTPUT,
		PULSE_OUTPUT,
		HIGH_OUTPUT,
		SIN_PLUS_OUTPUT,
		TRI_PLUS_OUTPUT,
		LOW_OUTPUT,
		SIN_MINUS_OUTPUT,
		TRI_MINUS_OUTPUT,
		RAMP_PLUS_OUTPUT,
		RAMP_MINUS_OUTPUT,
		EOC_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	// ── Base Parameters ──
	int seconds = 1;       // 0 to 99
	int milliseconds = 500; // 0 to 999
	VoltageRange voltageRange = RANGE_UNIPOLAR_10V;

	// ── Prepared Display Values (Tracks base + CV inputs) ──
	int displaySeconds = 1;
	int displayMs = 500;

	// ── Latched DSP State ──
	bool running = false;
	float activeDuration = 1.500f; // Latched at trigger; immune to subsequent CV/time changes
	float elapsedSec = 0.0f;
	float currentProgress = 0.0f;

	// ── Remaining Time for Countdown LCD ──
	int remSeconds = 0;
	int remMs = 0;

	dsp::SchmittTrigger startBtnTrigger;
	dsp::SchmittTrigger startGateTrigger;
	dsp::PulseGenerator startPulse;
	dsp::PulseGenerator eocPulse;

	Aionidion() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters
		configParam(START_PARAM, 0.0f, 1.0f, 0.0f, "Start");
		configParam(PWM_PARAM, 0.01f, 0.99f, 0.50f, "Pulse width", "%", 0.0f, 100.0f);
		configParam(SEC_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Seconds CV depth", "%", 0.0f, 100.0f);
		configParam(MS_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Milliseconds CV depth", "%", 0.0f, 100.0f);
		configParam(PWM_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Pulse width CV depth", "%", 0.0f, 100.0f);

		// Inputs
		configInput(SEC_CV_INPUT, "Seconds CV");
		configInput(MS_CV_INPUT, "Milliseconds CV");
		configInput(PWM_CV_INPUT, "Pulse width CV");
		configInput(START_INPUT, "Start gate");

		// Outputs
		configOutput(TRIG_OUTPUT, "Trigger pulse");
		configOutput(PULSE_OUTPUT, "Pulse width modulated wave");
		configOutput(HIGH_OUTPUT, "High gate");
		configOutput(SIN_PLUS_OUTPUT, "Sine");
		configOutput(TRI_PLUS_OUTPUT, "Triangle");
		configOutput(LOW_OUTPUT, "Low gate");
		configOutput(SIN_MINUS_OUTPUT, "Inverted sine");
		configOutput(TRI_MINUS_OUTPUT, "Inverted triangle");
		configOutput(RAMP_PLUS_OUTPUT, "Ramp up");
		configOutput(RAMP_MINUS_OUTPUT, "Ramp down");
		configOutput(EOC_OUTPUT, "End of cycle gate");
	}

	void getRangeVoltages(float& vMin, float& vMax) const {
		switch (voltageRange) {
			case RANGE_BIPOLAR_1V:   vMin = -1.0f; vMax = 1.0f;  break;
			case RANGE_UNIPOLAR_1V:  vMin =  0.0f; vMax = 1.0f;  break;
			case RANGE_BIPOLAR_5V:   vMin = -5.0f; vMax = 5.0f;  break;
			case RANGE_UNIPOLAR_5V:  vMin =  0.0f; vMax = 5.0f;  break;
			case RANGE_BIPOLAR_10V:  vMin = -10.0f; vMax = 10.0f; break;
			case RANGE_UNIPOLAR_10V:
			default:                 vMin =  0.0f; vMax = 10.0f; break;
		}
	}

	bool isBipolar() const {
		return voltageRange == RANGE_BIPOLAR_1V ||
		       voltageRange == RANGE_BIPOLAR_5V ||
		       voltageRange == RANGE_BIPOLAR_10V;
	}

	void parseTimeString(const std::string& str) {
		int s = 0;
		int ms = 0;
		// Check for formats like "5s500ms", "5s 500ms", "5.5s", "5s", "500ms"
		size_t sPos = str.find_first_of("sS\"");
		size_t msPos = str.find("ms");
		if (msPos == std::string::npos) msPos = str.find("MS");
		size_t dotPos = str.find('.');

		if (dotPos != std::string::npos) {
			float val = (float)std::atof(str.c_str());
			s = (int)val;
			ms = (int)std::round((val - (float)s) * 1000.0f);
		}
		else if (msPos != std::string::npos && (sPos == std::string::npos || sPos >= msPos)) {
			// e.g. "500ms"
			int totalMs = std::atoi(str.c_str());
			s = totalMs / 1000;
			ms = totalMs % 1000;
		}
		else if (sPos != std::string::npos) {
			s = std::atoi(str.substr(0, sPos).c_str());
			if (sPos + 1 < str.size()) {
				ms = std::atoi(str.substr(sPos + 1).c_str());
			}
		}
		else {
			float val = (float)std::atof(str.c_str());
			s = (int)val;
			ms = (int)std::round((val - (float)s) * 1000.0f);
		}

		seconds = (int)clampf((float)s, 0.0f, 99.0f);
		milliseconds = (int)clampf((float)ms, 0.0f, 999.0f);
	}

	void process(const ProcessArgs& args) override {
		// 1. Calculate prepared time for the NEXT event
		// Seconds CV modulates integer seconds, clamped to [0, 99]
		int secCvDelta = 0;
		if (inputs[SEC_CV_INPUT].isConnected()) {
			secCvDelta = (int)std::round(inputs[SEC_CV_INPUT].getVoltage() * params[SEC_ATTEN_PARAM].getValue() * 10.0f);
		}
		displaySeconds = (int)clampf((float)(seconds + secCvDelta), 0.0f, 99.0f);

		// Milliseconds CV modulates integer milliseconds, clamped to [0, 999]
		int msCvDelta = 0;
		if (inputs[MS_CV_INPUT].isConnected()) {
			msCvDelta = (int)std::round(inputs[MS_CV_INPUT].getVoltage() * params[MS_ATTEN_PARAM].getValue() * 100.0f);
		}
		displayMs = (int)clampf((float)(milliseconds + msCvDelta), 0.0f, 999.0f);

		float preparedDuration = (float)displaySeconds + (float)displayMs * 0.001f;
		preparedDuration = std::max(0.001f, preparedDuration);

		// 2. Detect Start Trigger
		bool startTriggered = false;
		if (startBtnTrigger.process(params[START_PARAM].getValue())) {
			startTriggered = true;
		}
		if (inputs[START_INPUT].isConnected() && startGateTrigger.process(inputs[START_INPUT].getVoltage())) {
			startTriggered = true;
		}

		if (startTriggered) {
			running = true;
			elapsedSec = 0.0f;
			activeDuration = preparedDuration; // Latched! Immune to subsequent time/CV changes
			startPulse.trigger(0.001f);
		}

		// 3. Update Progress & Timing of Current Active Event
		float phase = 0.0f;
		if (running) {
			elapsedSec += args.sampleTime;
			if (elapsedSec >= activeDuration) {
				phase = 1.0f;
				running = false;
				elapsedSec = 0.0f;
				remSeconds = 0;
				remMs = 0;
				eocPulse.trigger(0.001f);
			}
			else {
				phase = elapsedSec / activeDuration;
				float remainingSec = std::max(0.0f, activeDuration - elapsedSec);
				remSeconds = (int)remainingSec;
				remMs = (int)std::round((remainingSec - (float)remSeconds) * 1000.0f);
				if (remMs >= 1000) {
					remMs = 0;
					remSeconds++;
				}
			}
		}
		else {
			remSeconds = 0;
			remMs = 0;
		}
		currentProgress = phase;

		// 4. Calculate Pulse Width
		float pw = params[PWM_PARAM].getValue();
		if (inputs[PWM_CV_INPUT].isConnected()) {
			pw += inputs[PWM_CV_INPUT].getVoltage() * 0.1f * params[PWM_ATTEN_PARAM].getValue();
		}
		pw = clampf(pw, 0.01f, 0.99f);

		// 5. Output Voltages
		float vMin = 0.0f;
		float vMax = 10.0f;
		getRangeVoltages(vMin, vMax);
		float vSpan = vMax - vMin;

		float uRampUp = running ? phase : 0.0f;
		float uRampDn = running ? (1.0f - phase) : 0.0f;
		float uTri = 0.0f;
		if (running) {
			uTri = (phase < 0.5f) ? (2.0f * phase) : (2.0f * (1.0f - phase));
		}
		float uInvTri = running ? (1.0f - uTri) : 0.0f;

		// Row 2: TRIG & PULSE
		bool trigActive = startPulse.process(args.sampleTime);
		outputs[TRIG_OUTPUT].setVoltage(trigActive ? vMax : 0.0f);
		bool pwmHigh = running && (phase < pw);
		outputs[PULSE_OUTPUT].setVoltage(pwmHigh ? vMax : vMin);

		// Row 3: HIGH, SIN+, TRI+
		outputs[HIGH_OUTPUT].setVoltage(running ? vMax : 0.0f);

		float sinVal = running ? std::sin(2.0f * (float)M_PI * phase) : 0.0f;
		float invSinVal = -sinVal;

		if (isBipolar()) {
			outputs[SIN_PLUS_OUTPUT].setVoltage(vMax * sinVal);
			outputs[SIN_MINUS_OUTPUT].setVoltage(vMax * invSinVal);
		}
		else {
			outputs[SIN_PLUS_OUTPUT].setVoltage(vMin + 0.5f * (1.0f + sinVal) * vSpan);
			outputs[SIN_MINUS_OUTPUT].setVoltage(vMin + 0.5f * (1.0f + invSinVal) * vSpan);
		}

		outputs[TRI_PLUS_OUTPUT].setVoltage(vMin + uTri * vSpan);

		// Row 4: LOW, SIN-, TRI-
		outputs[LOW_OUTPUT].setVoltage(running ? vMin : vMax);
		outputs[TRI_MINUS_OUTPUT].setVoltage(vMin + uInvTri * vSpan);

		// Row 5: RAMP+, RAMP-, EOC
		outputs[RAMP_PLUS_OUTPUT].setVoltage(vMin + uRampUp * vSpan);
		outputs[RAMP_MINUS_OUTPUT].setVoltage(vMin + uRampDn * vSpan);
		bool eocActive = eocPulse.process(args.sampleTime);
		outputs[EOC_OUTPUT].setVoltage(eocActive ? vMax : 0.0f);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "seconds", json_integer(seconds));
		json_object_set_new(rootJ, "milliseconds", json_integer(milliseconds));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* secJ = json_object_get(rootJ, "seconds");
		if (secJ) {
			seconds = json_integer_value(secJ);
		}
		json_t* msJ = json_object_get(rootJ, "milliseconds");
		if (msJ) {
			milliseconds = json_integer_value(msJ);
		}
		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) {
			voltageRange = (VoltageRange)json_integer_value(vrJ);
		}
	}
};

// ── Custom Hardware Widgets ──
struct AionidionPort : app::SvgPort {
	AionidionPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

struct AionidionKnob : RoundKnob {
	AionidionKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct AionidionTrimpot : RoundKnob {
	AionidionTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct AionidionButton : app::SvgSwitch {
	AionidionButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Context Menu Time Text Input ──
struct AionidionTimeTextField : ui::TextField {
	Aionidion* module = nullptr;

	AionidionTimeTextField(Aionidion* m) : module(m) {
		box.size.x = 160.0f;
		if (m) {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%ds%03dms", m->seconds, m->milliseconds);
			setText(buf);
		}
	}

	void onAction(const ActionEvent& e) override {
		if (module) {
			module->parseTimeString(getText());
		}
	}

	void onSelectKey(const SelectKeyEvent& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			if (module) {
				module->parseTimeString(getText());
			}
			ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
			if (overlay) {
				overlay->requestDelete();
			}
			e.consume(this);
			return;
		}
		ui::TextField::onSelectKey(e);
	}
};

static void appendAionidionMenuItems(ui::Menu* menu, Aionidion* module) {
	if (!module) return;

	menu->addChild(createMenuLabel("Set Event Time"));
	AionidionTimeTextField* textField = new AionidionTimeTextField(module);
	menu->addChild(textField);
	if (APP && APP->event) {
		APP->event->setSelectedWidget(textField);
	}

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("Voltage Range"));

	const char* rangeLabels[] = {
		"+/-1V (-1V to +1V)",
		"0-1V (0V to +1V)",
		"+/-5V (-5V to +5V)",
		"0-5V (0V to +5V)",
		"+/-10V (-10V to +10V)",
		"0-10V (0V to +10V)"
	};

	for (int i = 0; i < Aionidion::NUM_RANGES; i++) {
		Aionidion::VoltageRange mode = (Aionidion::VoltageRange)i;
		menu->addChild(createCheckMenuItem(rangeLabels[i], "",
			[=]() { return module ? module->voltageRange == mode : false; },
			[=]() { if (module) module->voltageRange = mode; }
		));
	}
}

// ── Shared 7-Segment Drawing Utility ──
static const uint8_t gSegmentTableIdion[10] = {
	0b00111111, // 0
	0b00000110, // 1
	0b01011011, // 2
	0b01001111, // 3
	0b01100110, // 4
	0b01101101, // 5
	0b01111101, // 6
	0b00000111, // 7
	0b01111111, // 8
	0b01101111  // 9
};

static void drawSegmentIdion(NVGcontext* vg, float x, float y, float w, float h, bool on, NVGcolor onCol, NVGcolor offCol) {
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, w, h, 0.3f);
	nvgFillColor(vg, on ? onCol : offCol);
	nvgFill(vg);
}

static void draw7SegmentIdion(NVGcontext* vg, float dx, float dy, float dw, float dh, float st, int digit, NVGcolor onCol, NVGcolor offCol) {
	if (digit < 0 || digit > 9) digit = 0;
	uint8_t mask = gSegmentTableIdion[digit];
	float halfH = dh * 0.5f;

	drawSegmentIdion(vg, dx + st * 0.7f, dy, dw - st * 1.4f, st, (mask & 0x01), onCol, offCol);
	drawSegmentIdion(vg, dx + dw - st, dy + st * 0.7f, st, halfH - st * 0.9f, (mask & 0x02), onCol, offCol);
	drawSegmentIdion(vg, dx + dw - st, dy + halfH + st * 0.2f, st, halfH - st * 0.9f, (mask & 0x04), onCol, offCol);
	drawSegmentIdion(vg, dx + st * 0.7f, dy + dh - st, dw - st * 1.4f, st, (mask & 0x08), onCol, offCol);
	drawSegmentIdion(vg, dx, dy + halfH + st * 0.2f, st, halfH - st * 0.9f, (mask & 0x10), onCol, offCol);
	drawSegmentIdion(vg, dx, dy + st * 0.7f, st, halfH - st * 0.9f, (mask & 0x20), onCol, offCol);
	drawSegmentIdion(vg, dx + st * 0.7f, dy + halfH - st * 0.5f, dw - st * 1.4f, st, (mask & 0x40), onCol, offCol);
}

// ── Primary Time LCD Display Widget (34.0 x 8.5 mm, 88"888 format) ──
struct AionidionDisplayWidget : OpaqueWidget {
	Aionidion* module = nullptr;

	AionidionDisplayWidget() {
		box.size = mm2px(Vec(34.0f, 8.5f));
	}

	void drawArrow(NVGcontext* vg, float cx, float cy, bool up, NVGcolor col) {
		nvgBeginPath(vg);
		if (up) {
			nvgMoveTo(vg, cx, cy - 2.6f);
			nvgLineTo(vg, cx + 3.0f, cy + 1.8f);
			nvgLineTo(vg, cx - 3.0f, cy + 1.8f);
		}
		else {
			nvgMoveTo(vg, cx, cy + 2.6f);
			nvgLineTo(vg, cx + 3.0f, cy - 1.8f);
			nvgLineTo(vg, cx - 3.0f, cy - 1.8f);
		}
		nvgClosePath(vg);
		nvgFillColor(vg, col);
		nvgFill(vg);
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float w = box.size.x;
		float h = box.size.y;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, 2.0f);
		nvgFillColor(args.vg, nvgRGBA(0x10, 0x14, 0x12, 0xff));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 1.6f);
		nvgStrokeColor(args.vg, nvgRGBA(0x2c, 0x35, 0x30, 0xff));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);

		NVGcolor onColor = nvgRGBA(0x38, 0xf8, 0x70, 0xff);
		NVGcolor offColor = nvgRGBA(0x15, 0x27, 0x1a, 0x48);
		NVGcolor btnColor = nvgRGBA(0x72, 0x8a, 0x7a, 0xff);

		int sVal = module ? module->displaySeconds : 1;
		int msVal = module ? module->displayMs : 500;

		drawArrow(args.vg, 8.5f, h * 0.30f, true, btnColor);
		drawArrow(args.vg, 8.5f, h * 0.70f, false, btnColor);

		drawArrow(args.vg, w - 8.5f, h * 0.30f, true, btnColor);
		drawArrow(args.vg, w - 8.5f, h * 0.70f, false, btnColor);

		// Digits: 2 seconds digits + " + 3 milliseconds digits
		int s10 = (sVal / 10) % 10;
		int s1 = sVal % 10;
		int ms100 = (msVal / 100) % 10;
		int ms10 = (msVal / 10) % 10;
		int ms1 = msVal % 10;

		float dw = 5.4f;
		float dh = 12.5f;
		float st = 1.15f;
		float digitPitch = 7.5f;
		// 2 sec digits (2*7.5) + double prime tick (3.8) + 3 ms digits (3*7.5) = 41.3px
		float contentW = 2.0f * digitPitch + 3.8f + 3.0f * digitPitch;
		float startX = (w - contentW) * 0.5f;
		float digitY = (h - dh) * 0.5f;

		// Seconds (8 8)
		draw7SegmentIdion(args.vg, startX, digitY, dw, dh, st, s10, onColor, offColor);
		draw7SegmentIdion(args.vg, startX + digitPitch, digitY, dw, dh, st, s1, onColor, offColor);

		// Double prime tick "
		float dprimeX = startX + digitPitch * 2.0f - 0.5f;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, dprimeX, digitY + 1.0f);
		nvgLineTo(args.vg, dprimeX + 1.2f, digitY + 4.0f);
		nvgMoveTo(args.vg, dprimeX + 2.5f, digitY + 1.0f);
		nvgLineTo(args.vg, dprimeX + 3.7f, digitY + 4.0f);
		nvgStrokeColor(args.vg, onColor);
		nvgStrokeWidth(args.vg, 1.1f);
		nvgStroke(args.vg);

		// Milliseconds (8 8 8)
		float msStartX = dprimeX + 5.0f;
		draw7SegmentIdion(args.vg, msStartX, digitY, dw, dh, st, ms100, onColor, offColor);
		draw7SegmentIdion(args.vg, msStartX + digitPitch, digitY, dw, dh, st, ms10, onColor, offColor);
		draw7SegmentIdion(args.vg, msStartX + digitPitch * 2.0f, digitY, dw, dh, st, ms1, onColor, offColor);

		nvgRestore(args.vg);
	}

	void onButton(const ButtonEvent& e) override {
		if (!module) {
			OpaqueWidget::onButton(e);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			appendAionidionMenuItems(menu, module);
			e.consume(this);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			float x = e.pos.x;
			float y = e.pos.y;
			float w = box.size.x;
			float h = box.size.y;
			int stepSec = (e.mods & GLFW_MOD_SHIFT) ? 10 : 1;
			int stepMs = (e.mods & GLFW_MOD_SHIFT) ? 100 : 10;

			// Left side: Seconds buttons (x < 18.0)
			if (x < 18.0f) {
				if (y < h * 0.5f) {
					module->seconds = std::min(99, module->seconds + stepSec);
				}
				else {
					module->seconds = std::max(0, module->seconds - stepSec);
				}
				e.consume(this);
				return;
			}

			// Right side: Milliseconds buttons (x > w - 18.0)
			if (x > w - 18.0f) {
				if (y < h * 0.5f) {
					module->milliseconds = std::min(999, module->milliseconds + stepMs);
				}
				else {
					module->milliseconds = std::max(0, module->milliseconds - stepMs);
				}
				e.consume(this);
				return;
			}
		}

		OpaqueWidget::onButton(e);
	}
};

// ── Countdown LCD Display Widget (Snug 14.0 x 7.5 mm, 88"888 with leading zeroes) ──
struct AionidionCountdownWidget : OpaqueWidget {
	Aionidion* module = nullptr;

	AionidionCountdownWidget() {
		box.size = mm2px(Vec(14.0f, 7.5f));
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float w = box.size.x;
		float h = box.size.y;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, 2.0f);
		nvgFillColor(args.vg, nvgRGBA(0x10, 0x14, 0x12, 0xff));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 1.5f);
		nvgStrokeColor(args.vg, nvgRGBA(0x2c, 0x35, 0x30, 0xff));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);

		bool isRun = module ? module->running : false;
		int sVal = module ? module->remSeconds : 0;
		int msVal = module ? module->remMs : 0;

		NVGcolor onColor = isRun ? nvgRGBA(0xff, 0xb8, 0x33, 0xff) : nvgRGBA(0x60, 0x50, 0x20, 0x60);
		NVGcolor offColor = nvgRGBA(0x25, 0x1e, 0x10, 0x40);

		int s10 = (sVal / 10) % 10;
		int s1 = sVal % 10;
		int ms100 = (msVal / 100) % 10;
		int ms10 = (msVal / 10) % 10;
		int ms1 = msVal % 10;

		float dw = 3.6f;
		float dh = 9.5f;
		float st = 0.85f;
		float digitPitch = 4.8f;
		// 2 sec digits (2*4.8) + double tick (2.5) + 3 ms digits (3*4.8) = 26.5px
		float contentW = 2.0f * digitPitch + 2.5f + 3.0f * digitPitch;
		float startX = (w - contentW) * 0.5f;
		float digitY = (h - dh) * 0.5f;

		// Seconds (8 8)
		draw7SegmentIdion(args.vg, startX, digitY, dw, dh, st, s10, onColor, offColor);
		draw7SegmentIdion(args.vg, startX + digitPitch, digitY, dw, dh, st, s1, onColor, offColor);

		// Double prime tick "
		float dprimeX = startX + digitPitch * 2.0f - 0.4f;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, dprimeX, digitY + 1.0f);
		nvgLineTo(args.vg, dprimeX + 1.0f, digitY + 3.2f);
		nvgMoveTo(args.vg, dprimeX + 2.0f, digitY + 1.0f);
		nvgLineTo(args.vg, dprimeX + 3.0f, digitY + 3.2f);
		nvgStrokeColor(args.vg, onColor);
		nvgStrokeWidth(args.vg, 0.9f);
		nvgStroke(args.vg);

		// Milliseconds (8 8 8)
		float msStartX = dprimeX + 3.4f;
		draw7SegmentIdion(args.vg, msStartX, digitY, dw, dh, st, ms100, onColor, offColor);
		draw7SegmentIdion(args.vg, msStartX + digitPitch, digitY, dw, dh, st, ms10, onColor, offColor);
		draw7SegmentIdion(args.vg, msStartX + digitPitch * 2.0f, digitY, dw, dh, st, ms1, onColor, offColor);

		nvgRestore(args.vg);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			appendAionidionMenuItems(menu, module);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
};

// ── Module Widget ──
struct AionidionWidget : ModuleWidget {
	AionidionWidget(Aionidion* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Aionidion.svg")));

		float col0 = 7.000f;
		float col1 = 20.320f;
		float col2 = 33.640f;

		// ── Zone 2: LCD Displays & Primary Controls ──
		AionidionDisplayWidget* display = new AionidionDisplayWidget();
		display->module = module;
		display->box.pos = mm2px(Vec(col1 - 17.0f, 12.25f));
		addChild(display);

		addParam(createParamCentered<AionidionButton>(mm2px(Vec(col0, 31.00f)), module, Aionidion::START_PARAM));

		AionidionCountdownWidget* countdown = new AionidionCountdownWidget();
		countdown->module = module;
		countdown->box.pos = mm2px(Vec(col1 - 7.0f, 27.25f));
		addChild(countdown);

		addParam(createParamCentered<AionidionKnob>(mm2px(Vec(col2, 31.00f)), module, Aionidion::PWM_PARAM));

		// ── Zone 3: CV Attenuverters (Center Y = 49.00 mm) ──
		addParam(createParamCentered<AionidionTrimpot>(mm2px(Vec(col0, 49.00f)), module, Aionidion::SEC_ATTEN_PARAM));
		addParam(createParamCentered<AionidionTrimpot>(mm2px(Vec(col1, 49.00f)), module, Aionidion::MS_ATTEN_PARAM));
		addParam(createParamCentered<AionidionTrimpot>(mm2px(Vec(col2, 49.00f)), module, Aionidion::PWM_ATTEN_PARAM));

		// ── Zone 4: I/O Jacks (Exact User Order: 5 Rows x 3 Columns) ──
		// Row 1 (Center Y = 65.00 mm): SEC, MS, PWM
		addInput(createInputCentered<AionidionPort>(mm2px(Vec(col0, 65.00f)), module, Aionidion::SEC_CV_INPUT));
		addInput(createInputCentered<AionidionPort>(mm2px(Vec(col1, 65.00f)), module, Aionidion::MS_CV_INPUT));
		addInput(createInputCentered<AionidionPort>(mm2px(Vec(col2, 65.00f)), module, Aionidion::PWM_CV_INPUT));

		// Row 2 (Center Y = 78.00 mm): START, TRIG, PULSE
		addInput(createInputCentered<AionidionPort>(mm2px(Vec(col0, 78.00f)), module, Aionidion::START_INPUT));
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col1, 78.00f)), module, Aionidion::TRIG_OUTPUT));
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col2, 78.00f)), module, Aionidion::PULSE_OUTPUT));

		// Row 3 (Center Y = 91.00 mm): HIGH, SIN+, TRI+
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col0, 91.00f)), module, Aionidion::HIGH_OUTPUT));
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col1, 91.00f)), module, Aionidion::SIN_PLUS_OUTPUT));
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col2, 91.00f)), module, Aionidion::TRI_PLUS_OUTPUT));

		// Row 4 (Center Y = 104.00 mm): LOW, SIN-, TRI-
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col0, 104.00f)), module, Aionidion::LOW_OUTPUT));
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col1, 104.00f)), module, Aionidion::SIN_MINUS_OUTPUT));
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col2, 104.00f)), module, Aionidion::TRI_MINUS_OUTPUT));

		// Row 5 (Center Y = 117.00 mm): RAMP+, RAMP-, EOC
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col0, 117.00f)), module, Aionidion::RAMP_PLUS_OUTPUT));
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col1, 117.00f)), module, Aionidion::RAMP_MINUS_OUTPUT));
		addOutput(createOutputCentered<AionidionPort>(mm2px(Vec(col2, 117.00f)), module, Aionidion::EOC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Aionidion* module = dynamic_cast<Aionidion*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		appendAionidionMenuItems(menu, module);
	}
};

Model* modelAionidion = createModel<Aionidion, AionidionWidget>("Aionidion");
