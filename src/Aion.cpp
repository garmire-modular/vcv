#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Aion — Timed Event & Function Generator (8 HP)
//  Control class module providing precision timed events up to 999m59s
//  with 7-segment display, stacked minutes/seconds step buttons,
//  real-time CV display tracking, remaining-seconds countdown LCD,
//  selectable voltage range, start trigger/gate input, active high/low
//  gates, trigger & EOC pulses, and synchronous continuous functions
//  (Ramp Up, Ramp Down, Triangle +, Triangle -, Sine +, Sine -, PWM Pulse).
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

} // namespace

struct Aion : Module {
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
		MIN_ATTEN_PARAM,
		SEC_ATTEN_PARAM,
		PWM_ATTEN_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		MIN_CV_INPUT,
		SEC_CV_INPUT,
		PWM_CV_INPUT,
		START_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		// Row 2
		TRIG_OUTPUT,
		PULSE_OUTPUT,
		// Row 3
		HIGH_OUTPUT,
		SIN_PLUS_OUTPUT,
		TRI_PLUS_OUTPUT,
		// Row 4
		LOW_OUTPUT,
		SIN_MINUS_OUTPUT,
		TRI_MINUS_OUTPUT,
		// Row 5
		RAMP_PLUS_OUTPUT,
		RAMP_MINUS_OUTPUT,
		EOC_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	// ── Base Parameters ──
	int minutes = 0;       // 0 to 999
	int seconds = 10;      // 0 to 59 (default 10s)
	VoltageRange voltageRange = RANGE_UNIPOLAR_10V;

	// ── Dynamic Display Values (Updated with CV modulation) ──
	int displayMinutes = 0;
	int displaySeconds = 10;
	float remainingSeconds = 0.0f;

	// ── DSP State ──
	bool running = false;
	float elapsedSec = 0.0f;
	float currentProgress = 0.0f;

	dsp::SchmittTrigger startBtnTrigger;
	dsp::SchmittTrigger startGateTrigger;
	dsp::PulseGenerator startPulse;
	dsp::PulseGenerator eocPulse;

	Aion() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters
		configParam(START_PARAM, 0.0f, 1.0f, 0.0f, "Start");
		configParam(PWM_PARAM, 0.01f, 0.99f, 0.50f, "Pulse width", "%", 0.0f, 100.0f);
		configParam(MIN_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Minutes CV depth", "%", 0.0f, 100.0f);
		configParam(SEC_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Seconds CV depth", "%", 0.0f, 100.0f);
		configParam(PWM_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Pulse width CV depth", "%", 0.0f, 100.0f);

		// Inputs (Strictly no redundant "input" or "in" per Section 7.5.1)
		configInput(MIN_CV_INPUT, "Minutes CV");
		configInput(SEC_CV_INPUT, "Seconds CV");
		configInput(PWM_CV_INPUT, "Pulse width CV");
		configInput(START_INPUT, "Start gate");

		// Outputs (Strictly no redundant "output" or "out" per Section 7.5.1)
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
		int m = 0;
		int s = 0;
		size_t mPos = str.find_first_of("mM'");
		size_t sPos = str.find_first_of("sS\"");
		size_t colonPos = str.find(':');

		if (colonPos != std::string::npos) {
			m = std::atoi(str.substr(0, colonPos).c_str());
			s = std::atoi(str.substr(colonPos + 1).c_str());
		}
		else if (mPos != std::string::npos) {
			m = std::atoi(str.substr(0, mPos).c_str());
			if (sPos != std::string::npos && sPos > mPos) {
				s = std::atoi(str.substr(mPos + 1, sPos - mPos - 1).c_str());
			}
			else if (mPos + 1 < str.size()) {
				s = std::atoi(str.substr(mPos + 1).c_str());
			}
		}
		else if (sPos != std::string::npos) {
			s = std::atoi(str.substr(0, sPos).c_str());
		}
		else {
			int totalSec = std::atoi(str.c_str());
			m = totalSec / 60;
			s = totalSec % 60;
		}

		minutes = (int)clampf((float)m, 0.0f, 999.0f);
		seconds = (int)clampf((float)s, 0.0f, 59.0f);
	}

	void process(const ProcessArgs& args) override {
		// 1. Detect Start Trigger (Button or Gate Input)
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
			startPulse.trigger(0.001f); // 1 ms trigger pulse
		}

		// 2. Compute effective duration with CV modulation
		float baseSec = (float)minutes * 60.0f + (float)seconds;

		// Minutes CV: 1V = 1 minute (60s)
		float minCvDelta = 0.0f;
		if (inputs[MIN_CV_INPUT].isConnected()) {
			minCvDelta = inputs[MIN_CV_INPUT].getVoltage() * params[MIN_ATTEN_PARAM].getValue() * 60.0f;
		}

		// Seconds CV: 1V = 10s (0-6V = 60s)
		float secCvDelta = 0.0f;
		if (inputs[SEC_CV_INPUT].isConnected()) {
			secCvDelta = inputs[SEC_CV_INPUT].getVoltage() * params[SEC_ATTEN_PARAM].getValue() * 10.0f;
		}

		float totalDuration = clampf(baseSec + minCvDelta + secCvDelta, 0.001f, 59999.0f);

		// Dynamically update displayed minutes and seconds to reflect CV modulation
		int effTotalInt = (int)std::round(totalDuration);
		displayMinutes = clampf((float)(effTotalInt / 60), 0.0f, 999.0f);
		displaySeconds = clampf((float)(effTotalInt % 60), 0.0f, 59.0f);

		// 3. Update Progress & Timing
		float phase = 0.0f;
		if (running) {
			elapsedSec += args.sampleTime;
			if (elapsedSec >= totalDuration) {
				phase = 1.0f;
				running = false;
				elapsedSec = 0.0f;
				remainingSeconds = 0.0f;
				eocPulse.trigger(0.001f); // 1 ms EOC pulse
			}
			else {
				phase = elapsedSec / totalDuration;
				remainingSeconds = std::max(0.0f, totalDuration - elapsedSec);
			}
		}
		else {
			remainingSeconds = 0.0f;
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

		// Continuous Waveforms (0.0 to 1.0 normalized)
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
		json_object_set_new(rootJ, "minutes", json_integer(minutes));
		json_object_set_new(rootJ, "seconds", json_integer(seconds));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* minJ = json_object_get(rootJ, "minutes");
		if (minJ) {
			minutes = json_integer_value(minJ);
		}
		json_t* secJ = json_object_get(rootJ, "seconds");
		if (secJ) {
			seconds = json_integer_value(secJ);
		}
		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) {
			voltageRange = (VoltageRange)json_integer_value(vrJ);
		}
	}
};

// ── Custom Hardware Widgets per Section 7.4 ──
struct GarmirePort : app::SvgPort {
	GarmirePort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

struct GarmireKnob : RoundKnob {
	GarmireKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct GarmireTrimpot : RoundKnob {
	GarmireTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct GarmireButton : app::SvgSwitch {
	GarmireButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Context Menu Time Text Input ──
struct AionTimeTextField : ui::TextField {
	Aion* module = nullptr;

	AionTimeTextField(Aion* m) : module(m) {
		box.size.x = 160.0f;
		if (m) {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%dm%02ds", m->minutes, m->seconds);
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

// Helper function to build the Aion context menu items
static void appendAionMenuItems(ui::Menu* menu, Aion* module) {
	if (!module) return;

	menu->addChild(createMenuLabel("Set Event Time"));
	AionTimeTextField* textField = new AionTimeTextField(module);
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

	for (int i = 0; i < Aion::NUM_RANGES; i++) {
		Aion::VoltageRange mode = (Aion::VoltageRange)i;
		menu->addChild(createCheckMenuItem(rangeLabels[i], "",
			[=]() { return module ? module->voltageRange == mode : false; },
			[=]() { if (module) module->voltageRange = mode; }
		));
	}
}

// ── 7-Segment LCD Display Widget ──
struct AionDisplayWidget : OpaqueWidget {
	Aion* module = nullptr;

	const uint8_t segmentTable[10] = {
		0b00111111, // 0: a b c d e f
		0b00000110, // 1: b c
		0b01011011, // 2: a b d e g
		0b01001111, // 3: a b c d g
		0b01100110, // 4: b c f g
		0b01101101, // 5: a c d f g
		0b01111101, // 6: a c d e f g
		0b00000111, // 7: a b c
		0b01111111, // 8: a b c d e f g
		0b01101111  // 9: a b c d f g
	};

	AionDisplayWidget() {
		box.size = mm2px(Vec(34.0f, 11.0f));
	}

	void drawSegment(NVGcontext* vg, float x, float y, float w, float h, bool on, NVGcolor onCol, NVGcolor offCol) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, x, y, w, h, 0.4f);
		nvgFillColor(vg, on ? onCol : offCol);
		nvgFill(vg);
	}

	void draw7SegmentDigit(NVGcontext* vg, float dx, float dy, int digit, NVGcolor onCol, NVGcolor offCol) {
		if (digit < 0 || digit > 9) digit = 0;
		uint8_t mask = segmentTable[digit];

		float dw = 6.2f;
		float dh = 15.0f;
		float st = 1.35f;
		float halfH = dh * 0.5f;

		// a: top horizontal
		drawSegment(vg, dx + st * 0.7f, dy, dw - st * 1.4f, st, (mask & 0x01), onCol, offCol);
		// b: top right vertical
		drawSegment(vg, dx + dw - st, dy + st * 0.7f, st, halfH - st * 0.9f, (mask & 0x02), onCol, offCol);
		// c: bottom right vertical
		drawSegment(vg, dx + dw - st, dy + halfH + st * 0.2f, st, halfH - st * 0.9f, (mask & 0x04), onCol, offCol);
		// d: bottom horizontal
		drawSegment(vg, dx + st * 0.7f, dy + dh - st, dw - st * 1.4f, st, (mask & 0x08), onCol, offCol);
		// e: bottom left vertical
		drawSegment(vg, dx, dy + halfH + st * 0.2f, st, halfH - st * 0.9f, (mask & 0x10), onCol, offCol);
		// f: top left vertical
		drawSegment(vg, dx, dy + st * 0.7f, st, halfH - st * 0.9f, (mask & 0x20), onCol, offCol);
		// g: middle horizontal
		drawSegment(vg, dx + st * 0.7f, dy + halfH - st * 0.5f, dw - st * 1.4f, st, (mask & 0x40), onCol, offCol);
	}

	void drawArrow(NVGcontext* vg, float cx, float cy, bool up, NVGcolor col) {
		nvgBeginPath(vg);
		if (up) {
			nvgMoveTo(vg, cx, cy - 3.2f);
			nvgLineTo(vg, cx + 3.8f, cy + 2.2f);
			nvgLineTo(vg, cx - 3.8f, cy + 2.2f);
		}
		else {
			nvgMoveTo(vg, cx, cy + 3.2f);
			nvgLineTo(vg, cx + 3.8f, cy - 2.2f);
			nvgLineTo(vg, cx - 3.8f, cy - 2.2f);
		}
		nvgClosePath(vg);
		nvgFillColor(vg, col);
		nvgFill(vg);
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float w = box.size.x;
		float h = box.size.y;

		// 1. LCD Screen Housing & Background
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, 3.0f);
		nvgFillColor(args.vg, nvgRGBA(0x10, 0x14, 0x12, 0xff));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 2.5f);
		nvgStrokeColor(args.vg, nvgRGBA(0x2c, 0x35, 0x30, 0xff));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);

		NVGcolor onColor = nvgRGBA(0x38, 0xf8, 0x70, 0xff);
		NVGcolor offColor = nvgRGBA(0x15, 0x27, 0x1a, 0x55);
		NVGcolor btnColor = nvgRGBA(0x72, 0x8a, 0x7a, 0xff);

		// Read dynamically modulated display minutes & seconds
		int mVal = module ? module->displayMinutes : 0;
		int sVal = module ? module->displaySeconds : 10;

		// 2. Left Stacked Arrows (Minutes Up / Down)
		drawArrow(args.vg, 10.0f, h * 0.28f, true, btnColor);
		drawArrow(args.vg, 10.0f, h * 0.72f, false, btnColor);

		// 3. Right Stacked Arrows (Seconds Up / Down)
		drawArrow(args.vg, w - 10.0f, h * 0.28f, true, btnColor);
		drawArrow(args.vg, w - 10.0f, h * 0.72f, false, btnColor);

		// 4. Center 7-Segment Digits: 3 Minutes + ' + 2 Seconds + "
		int m100 = (mVal / 100) % 10;
		int m10 = (mVal / 10) % 10;
		int m1 = mVal % 10;
		int s10 = (sVal / 10) % 10;
		int s1 = sVal % 10;

		float digitY = (h - 15.0f) * 0.5f;
		float startX = 20.0f;
		float digitPitch = 8.5f;

		// Minute digits (8 8 8)
		draw7SegmentDigit(args.vg, startX, digitY, m100, onColor, offColor);
		draw7SegmentDigit(args.vg, startX + digitPitch, digitY, m10, onColor, offColor);
		draw7SegmentDigit(args.vg, startX + digitPitch * 2.0f, digitY, m1, onColor, offColor);

		// Prime tick / apostrophe '
		float primeX = startX + digitPitch * 3.0f - 1.5f;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, primeX, digitY + 1.0f);
		nvgLineTo(args.vg, primeX + 1.6f, digitY + 4.5f);
		nvgStrokeColor(args.vg, onColor);
		nvgStrokeWidth(args.vg, 1.3f);
		nvgStroke(args.vg);

		// Second digits (8 8)
		float secStartX = primeX + 5.5f;
		draw7SegmentDigit(args.vg, secStartX, digitY, s10, onColor, offColor);
		draw7SegmentDigit(args.vg, secStartX + digitPitch, digitY, s1, onColor, offColor);

		// Double prime tick "
		float dprimeX = secStartX + digitPitch * 2.0f - 1.0f;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, dprimeX, digitY + 1.0f);
		nvgLineTo(args.vg, dprimeX + 1.4f, digitY + 4.5f);
		nvgMoveTo(args.vg, dprimeX + 2.8f, digitY + 1.0f);
		nvgLineTo(args.vg, dprimeX + 4.2f, digitY + 4.5f);
		nvgStrokeColor(args.vg, onColor);
		nvgStrokeWidth(args.vg, 1.2f);
		nvgStroke(args.vg);

		nvgRestore(args.vg);
	}

	void onButton(const ButtonEvent& e) override {
		if (!module) {
			OpaqueWidget::onButton(e);
			return;
		}

		// Right click on display: open time entry context menu
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			appendAionMenuItems(menu, module);
			e.consume(this);
			return;
		}

		// Left click on display: check stacked arrow buttons
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			float x = e.pos.x;
			float y = e.pos.y;
			float w = box.size.x;
			float h = box.size.y;
			int step = (e.mods & GLFW_MOD_SHIFT) ? 10 : 1;

			// Left side: Minutes buttons (x < 18.0)
			if (x < 18.0f) {
				if (y < h * 0.5f) {
					module->minutes = std::min(999, module->minutes + step);
				}
				else {
					module->minutes = std::max(0, module->minutes - step);
				}
				e.consume(this);
				return;
			}

			// Right side: Seconds buttons (x > w - 18.0)
			if (x > w - 18.0f) {
				if (y < h * 0.5f) {
					module->seconds = (module->seconds + step) % 60;
				}
				else {
					module->seconds = (module->seconds - step + 60) % 60;
				}
				e.consume(this);
				return;
			}
		}

		OpaqueWidget::onButton(e);
	}
};

// ── Smaller Countdown LCD Display Widget ──
struct AionCountdownWidget : OpaqueWidget {
	Aion* module = nullptr;

	const uint8_t segmentTable[10] = {
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

	AionCountdownWidget() {
		box.size = mm2px(Vec(14.0f, 8.0f));
	}

	void drawSegment(NVGcontext* vg, float x, float y, float w, float h, bool on, NVGcolor onCol, NVGcolor offCol) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, x, y, w, h, 0.3f);
		nvgFillColor(vg, on ? onCol : offCol);
		nvgFill(vg);
	}

	void drawMiniDigit(NVGcontext* vg, float dx, float dy, int digit, NVGcolor onCol, NVGcolor offCol) {
		if (digit < 0 || digit > 9) digit = 0;
		uint8_t mask = segmentTable[digit];

		float dw = 4.2f;
		float dh = 10.0f;
		float st = 1.0f;
		float halfH = dh * 0.5f;

		// a
		drawSegment(vg, dx + st * 0.7f, dy, dw - st * 1.4f, st, (mask & 0x01), onCol, offCol);
		// b
		drawSegment(vg, dx + dw - st, dy + st * 0.7f, st, halfH - st * 0.9f, (mask & 0x02), onCol, offCol);
		// c
		drawSegment(vg, dx + dw - st, dy + halfH + st * 0.2f, st, halfH - st * 0.9f, (mask & 0x04), onCol, offCol);
		// d
		drawSegment(vg, dx + st * 0.7f, dy + dh - st, dw - st * 1.4f, st, (mask & 0x08), onCol, offCol);
		// e
		drawSegment(vg, dx, dy + halfH + st * 0.2f, st, halfH - st * 0.9f, (mask & 0x10), onCol, offCol);
		// f
		drawSegment(vg, dx, dy + st * 0.7f, st, halfH - st * 0.9f, (mask & 0x20), onCol, offCol);
		// g
		drawSegment(vg, dx + st * 0.7f, dy + halfH - st * 0.5f, dw - st * 1.4f, st, (mask & 0x40), onCol, offCol);
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float w = box.size.x;
		float h = box.size.y;

		// LCD screen housing
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, 2.0f);
		nvgFillColor(args.vg, nvgRGBA(0x10, 0x14, 0x12, 0xff));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 1.5f);
		nvgStrokeColor(args.vg, nvgRGBA(0x2c, 0x35, 0x30, 0xff));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);

		// Countdown digits
		bool isRun = module ? module->running : false;
		float remSec = module ? module->remainingSeconds : 0.0f;
		int secInt = isRun ? (int)std::ceil(remSec) : 0;

		NVGcolor onColor = isRun ? nvgRGBA(0xff, 0xb8, 0x33, 0xff) : nvgRGBA(0x60, 0x50, 0x20, 0x60); // Warm amber
		NVGcolor offColor = nvgRGBA(0x25, 0x1e, 0x10, 0x40);

		// Format integer seconds into up to 5 digits (right-aligned)
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%5d", secInt);

		float digitPitch = 5.8f;
		float startX = (w - (5 * digitPitch - 1.6f)) * 0.5f;
		float digitY = (h - 10.0f) * 0.5f;

		for (int i = 0; i < 5; i++) {
			char c = buf[i];
			float dx = startX + i * digitPitch;
			if (c >= '0' && c <= '9') {
				drawMiniDigit(args.vg, dx, digitY, c - '0', onColor, offColor);
			}
			else {
				// Blank space: draw off segments
				drawMiniDigit(args.vg, dx, digitY, 8, offColor, offColor);
			}
		}

		nvgRestore(args.vg);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			appendAionMenuItems(menu, module);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
};

// ── Module Widget ──
struct AionWidget : ModuleWidget {
	AionWidget(Aion* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Aion.svg")));

		// Zero virtual screws added (AGENTS.md Section 7.5.6)

		float col0 = 7.000f;
		float col1 = 20.320f;
		float col2 = 33.640f;

		// ── Zone 2: LCD Display & Controls ──
		// Main 7-Segment LCD Display (Center X = 20.32 mm, Center Y = 16.50 mm [moved up 1mm], W = 34.00 mm, H = 11.00 mm)
		AionDisplayWidget* display = new AionDisplayWidget();
		display->module = module;
		display->box.pos = mm2px(Vec(col1 - 17.0f, 11.00f));
		addChild(display);

		// Row at Y = 31.00 mm: Start Button (X = 7.00), Countdown LCD (X = 20.32), PWM Knob (X = 33.64)
		addParam(createParamCentered<GarmireButton>(mm2px(Vec(col0, 31.00f)), module, Aion::START_PARAM));

		// Countdown LCD Display (Center X = 20.32 mm, Center Y = 31.00 mm, W = 14.00 mm, H = 8.00 mm)
		AionCountdownWidget* countdown = new AionCountdownWidget();
		countdown->module = module;
		countdown->box.pos = mm2px(Vec(col1 - 7.0f, 27.00f));
		addChild(countdown);

		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(col2, 31.00f)), module, Aion::PWM_PARAM));

		// ── Zone 3: CV Attenuverters (Center Y = 49.00 mm) ──
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col0, 49.00f)), module, Aion::MIN_ATTEN_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col1, 49.00f)), module, Aion::SEC_ATTEN_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col2, 49.00f)), module, Aion::PWM_ATTEN_PARAM));

		// ── Zone 4: I/O Jacks (Exact User Order: 5 Rows x 3 Columns) ──
		// Row 1 (Center Y = 65.00 mm): MIN, SEC, PWM
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col0, 65.00f)), module, Aion::MIN_CV_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col1, 65.00f)), module, Aion::SEC_CV_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col2, 65.00f)), module, Aion::PWM_CV_INPUT));

		// Row 2 (Center Y = 78.00 mm): START, TRIG, PULSE
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col0, 78.00f)), module, Aion::START_INPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, 78.00f)), module, Aion::TRIG_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, 78.00f)), module, Aion::PULSE_OUTPUT));

		// Row 3 (Center Y = 91.00 mm): HIGH, SIN+, TRI+
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col0, 91.00f)), module, Aion::HIGH_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, 91.00f)), module, Aion::SIN_PLUS_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, 91.00f)), module, Aion::TRI_PLUS_OUTPUT));

		// Row 4 (Center Y = 104.00 mm): LOW, SIN-, TRI-
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col0, 104.00f)), module, Aion::LOW_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, 104.00f)), module, Aion::SIN_MINUS_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, 104.00f)), module, Aion::TRI_MINUS_OUTPUT));

		// Row 5 (Center Y = 117.00 mm): RAMP+, RAMP-, EOC
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col0, 117.00f)), module, Aion::RAMP_PLUS_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, 117.00f)), module, Aion::RAMP_MINUS_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, 117.00f)), module, Aion::EOC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Aion* module = dynamic_cast<Aion*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		appendAionMenuItems(menu, module);
	}
};

Model* modelAion = createModel<Aion, AionWidget>("Aion");
