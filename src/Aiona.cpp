#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Aiona — Macro-Timescale Event & Function Generator (8 HP)
//  Long timebase version of Aion with hours (up to 999) and
//  minutes (up to 59), 888h88m 7-segment display, stacked step buttons,
//  latched run timing (editing time or CV prepares next trigger without
//  disturbing current cycle), snug 888h88m countdown LCD, selectable voltage
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

struct Aiona : Module {
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
		HRS_ATTEN_PARAM,
		MIN_ATTEN_PARAM,
		PWM_ATTEN_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		HRS_CV_INPUT,
		MIN_CV_INPUT,
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
	int hours = 1;         // 0 to 999
	int minutes = 0;       // 0 to 59
	VoltageRange voltageRange = RANGE_UNIPOLAR_10V;

	// ── Prepared Display Values (Tracks base + CV inputs) ──
	int displayHours = 1;
	int displayMinutes = 0;

	// ── Latched DSP State ──
	bool running = false;
	float activeDuration = 3600.0f; // Latched at trigger; immune to subsequent CV/time changes
	float elapsedSec = 0.0f;
	float currentProgress = 0.0f;

	// ── Remaining Time for Countdown LCD ──
	int remHours = 0;
	int remMinutes = 0;

	dsp::SchmittTrigger startBtnTrigger;
	dsp::SchmittTrigger startGateTrigger;
	dsp::PulseGenerator startPulse;
	dsp::PulseGenerator eocPulse;

	Aiona() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters
		configParam(START_PARAM, 0.0f, 1.0f, 0.0f, "Start");
		configParam(PWM_PARAM, 0.01f, 0.99f, 0.50f, "Pulse width", "%", 0.0f, 100.0f);
		configParam(HRS_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Hours CV depth", "%", 0.0f, 100.0f);
		configParam(MIN_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Minutes CV depth", "%", 0.0f, 100.0f);
		configParam(PWM_ATTEN_PARAM, -1.0f, 1.0f, 0.0f, "Pulse width CV depth", "%", 0.0f, 100.0f);

		// Inputs
		configInput(HRS_CV_INPUT, "Hours CV");
		configInput(MIN_CV_INPUT, "Minutes CV");
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
		int h = 0;
		int m = 0;
		// Check for formats like "12h30m", "12h 30m", "12h", "30m", "12:30"
		size_t hPos = str.find_first_of("hH");
		size_t mPos = str.find_first_of("mM'");
		size_t colonPos = str.find(':');

		if (colonPos != std::string::npos) {
			h = std::atoi(str.substr(0, colonPos).c_str());
			m = std::atoi(str.substr(colonPos + 1).c_str());
		}
		else if (hPos != std::string::npos) {
			h = std::atoi(str.substr(0, hPos).c_str());
			if (mPos != std::string::npos && mPos > hPos) {
				m = std::atoi(str.substr(hPos + 1, mPos - hPos - 1).c_str());
			}
			else if (hPos + 1 < str.size()) {
				m = std::atoi(str.substr(hPos + 1).c_str());
			}
		}
		else if (mPos != std::string::npos) {
			m = std::atoi(str.substr(0, mPos).c_str());
		}
		else {
			float val = (float)std::atof(str.c_str());
			h = (int)val;
			m = (int)std::round((val - (float)h) * 60.0f);
		}

		hours = (int)clampf((float)h, 0.0f, 999.0f);
		minutes = (int)clampf((float)m, 0.0f, 59.0f);
	}

	void process(const ProcessArgs& args) override {
		// 1. Calculate prepared time for the NEXT event
		// Hours CV modulates integer hours, clamped to [0, 999]
		int hrsCvDelta = 0;
		if (inputs[HRS_CV_INPUT].isConnected()) {
			hrsCvDelta = (int)std::round(inputs[HRS_CV_INPUT].getVoltage() * params[HRS_ATTEN_PARAM].getValue() * 10.0f);
		}
		displayHours = (int)clampf((float)(hours + hrsCvDelta), 0.0f, 999.0f);

		// Minutes CV modulates integer minutes, clamped to [0, 59]
		int minCvDelta = 0;
		if (inputs[MIN_CV_INPUT].isConnected()) {
			minCvDelta = (int)std::round(inputs[MIN_CV_INPUT].getVoltage() * params[MIN_ATTEN_PARAM].getValue() * 6.0f);
		}
		displayMinutes = (int)clampf((float)(minutes + minCvDelta), 0.0f, 59.0f);

		float preparedDuration = (float)displayHours * 3600.0f + (float)displayMinutes * 60.0f;
		preparedDuration = std::max(1.0f, preparedDuration);

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
				remHours = 0;
				remMinutes = 0;
				eocPulse.trigger(0.001f);
			}
			else {
				phase = elapsedSec / activeDuration;
				float remainingSec = std::max(0.0f, activeDuration - elapsedSec);
				int remInt = (int)std::ceil(remainingSec);
				remHours = remInt / 3600;
				remMinutes = (remInt % 3600) / 60;
			}
		}
		else {
			remHours = 0;
			remMinutes = 0;
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
		json_object_set_new(rootJ, "hours", json_integer(hours));
		json_object_set_new(rootJ, "minutes", json_integer(minutes));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* hrsJ = json_object_get(rootJ, "hours");
		if (hrsJ) {
			hours = json_integer_value(hrsJ);
		}
		json_t* minJ = json_object_get(rootJ, "minutes");
		if (minJ) {
			minutes = json_integer_value(minJ);
		}
		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) {
			voltageRange = (VoltageRange)json_integer_value(vrJ);
		}
	}
};

// ── Custom Hardware Widgets ──
struct AionaPort : app::SvgPort {
	AionaPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

struct AionaKnob : RoundKnob {
	AionaKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct AionaTrimpot : RoundKnob {
	AionaTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct AionaButton : app::SvgSwitch {
	AionaButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Context Menu Time Text Input ──
struct AionaTimeTextField : ui::TextField {
	Aiona* module = nullptr;

	AionaTimeTextField(Aiona* m) : module(m) {
		box.size.x = 160.0f;
		if (m) {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%dh%02dm", m->hours, m->minutes);
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

static void appendAionaMenuItems(ui::Menu* menu, Aiona* module) {
	if (!module) return;

	menu->addChild(createMenuLabel("Set Event Time"));
	AionaTimeTextField* textField = new AionaTimeTextField(module);
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

	for (int i = 0; i < Aiona::NUM_RANGES; i++) {
		Aiona::VoltageRange mode = (Aiona::VoltageRange)i;
		menu->addChild(createCheckMenuItem(rangeLabels[i], "",
			[=]() { return module ? module->voltageRange == mode : false; },
			[=]() { if (module) module->voltageRange = mode; }
		));
	}
}

// ── Shared 7-Segment Drawing Utility ──
static const uint8_t gSegmentTableAiona[10] = {
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

static void drawSegmentAiona(NVGcontext* vg, float x, float y, float w, float h, bool on, NVGcolor onCol, NVGcolor offCol) {
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, w, h, 0.3f);
	nvgFillColor(vg, on ? onCol : offCol);
	nvgFill(vg);
}

static void draw7SegmentAiona(NVGcontext* vg, float dx, float dy, float dw, float dh, float st, int digit, NVGcolor onCol, NVGcolor offCol) {
	if (digit < 0 || digit > 9) digit = 0;
	uint8_t mask = gSegmentTableAiona[digit];
	float halfH = dh * 0.5f;

	drawSegmentAiona(vg, dx + st * 0.7f, dy, dw - st * 1.4f, st, (mask & 0x01), onCol, offCol);
	drawSegmentAiona(vg, dx + dw - st, dy + st * 0.7f, st, halfH - st * 0.9f, (mask & 0x02), onCol, offCol);
	drawSegmentAiona(vg, dx + dw - st, dy + halfH + st * 0.2f, st, halfH - st * 0.9f, (mask & 0x04), onCol, offCol);
	drawSegmentAiona(vg, dx + st * 0.7f, dy + dh - st, dw - st * 1.4f, st, (mask & 0x08), onCol, offCol);
	drawSegmentAiona(vg, dx, dy + halfH + st * 0.2f, st, halfH - st * 0.9f, (mask & 0x10), onCol, offCol);
	drawSegmentAiona(vg, dx, dy + st * 0.7f, st, halfH - st * 0.9f, (mask & 0x20), onCol, offCol);
	drawSegmentAiona(vg, dx + st * 0.7f, dy + halfH - st * 0.5f, dw - st * 1.4f, st, (mask & 0x40), onCol, offCol);
}

// ── Primary Time LCD Display Widget (34.0 x 8.5 mm, 888h88m format) ──
struct AionaDisplayWidget : OpaqueWidget {
	Aiona* module = nullptr;

	AionaDisplayWidget() {
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

		int hVal = module ? module->displayHours : 1;
		int mVal = module ? module->displayMinutes : 0;

		drawArrow(args.vg, 8.5f, h * 0.30f, true, btnColor);
		drawArrow(args.vg, 8.5f, h * 0.70f, false, btnColor);

		drawArrow(args.vg, w - 8.5f, h * 0.30f, true, btnColor);
		drawArrow(args.vg, w - 8.5f, h * 0.70f, false, btnColor);

		// Digits: 3 hours digits + h tick + 2 minutes digits + m tick
		int h100 = (hVal / 100) % 10;
		int h10 = (hVal / 10) % 10;
		int h1 = hVal % 10;
		int m10 = (mVal / 10) % 10;
		int m1 = mVal % 10;

		float dw = 5.4f;
		float dh = 12.5f;
		float st = 1.15f;
		float digitPitch = 7.5f;
		float contentW = 3.0f * digitPitch + 4.5f + 2.0f * digitPitch + 3.8f;
		float startX = (w - contentW) * 0.5f;
		float digitY = (h - dh) * 0.5f;

		// Hours (8 8 8)
		draw7SegmentAiona(args.vg, startX, digitY, dw, dh, st, h100, onColor, offColor);
		draw7SegmentAiona(args.vg, startX + digitPitch, digitY, dw, dh, st, h10, onColor, offColor);
		draw7SegmentAiona(args.vg, startX + digitPitch * 2.0f, digitY, dw, dh, st, h1, onColor, offColor);

		// 'h' tick
		float hTickX = startX + digitPitch * 3.0f - 1.2f;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, hTickX, digitY + 1.0f);
		nvgLineTo(args.vg, hTickX + 1.4f, digitY + 4.0f);
		nvgStrokeColor(args.vg, onColor);
		nvgStrokeWidth(args.vg, 1.2f);
		nvgStroke(args.vg);

		// Minutes (8 8)
		float minStartX = hTickX + 4.8f;
		draw7SegmentAiona(args.vg, minStartX, digitY, dw, dh, st, m10, onColor, offColor);
		draw7SegmentAiona(args.vg, minStartX + digitPitch, digitY, dw, dh, st, m1, onColor, offColor);

		// 'm' tick
		float mTickX = minStartX + digitPitch * 2.0f - 0.8f;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, mTickX, digitY + 1.0f);
		nvgLineTo(args.vg, mTickX + 1.2f, digitY + 4.0f);
		nvgMoveTo(args.vg, mTickX + 2.5f, digitY + 1.0f);
		nvgLineTo(args.vg, mTickX + 3.7f, digitY + 4.0f);
		nvgStrokeColor(args.vg, onColor);
		nvgStrokeWidth(args.vg, 1.1f);
		nvgStroke(args.vg);

		nvgRestore(args.vg);
	}

	void onButton(const ButtonEvent& e) override {
		if (!module) {
			OpaqueWidget::onButton(e);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			appendAionaMenuItems(menu, module);
			e.consume(this);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			float x = e.pos.x;
			float y = e.pos.y;
			float w = box.size.x;
			float h = box.size.y;
			int step = (e.mods & GLFW_MOD_SHIFT) ? 10 : 1;

			// Left side: Hours buttons (x < 18.0)
			if (x < 18.0f) {
				if (y < h * 0.5f) {
					module->hours = std::min(999, module->hours + step);
				}
				else {
					module->hours = std::max(0, module->hours - step);
				}
				e.consume(this);
				return;
			}

			// Right side: Minutes buttons (x > w - 18.0)
			if (x > w - 18.0f) {
				if (y < h * 0.5f) {
					module->minutes = (module->minutes + step) % 60;
				}
				else {
					module->minutes = (module->minutes - step + 60) % 60;
				}
				e.consume(this);
				return;
			}
		}

		OpaqueWidget::onButton(e);
	}
};

// ── Countdown LCD Display Widget (Snug 14.0 x 7.5 mm, 888h88m with leading zeroes) ──
struct AionaCountdownWidget : OpaqueWidget {
	Aiona* module = nullptr;

	AionaCountdownWidget() {
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
		int hVal = module ? module->remHours : 0;
		int mVal = module ? module->remMinutes : 0;

		NVGcolor onColor = isRun ? nvgRGBA(0xff, 0xb8, 0x33, 0xff) : nvgRGBA(0x60, 0x50, 0x20, 0x60);
		NVGcolor offColor = nvgRGBA(0x25, 0x1e, 0x10, 0x40);

		int h100 = (hVal / 100) % 10;
		int h10 = (hVal / 10) % 10;
		int h1 = hVal % 10;
		int m10 = (mVal / 10) % 10;
		int m1 = mVal % 10;

		float dw = 3.6f;
		float dh = 9.5f;
		float st = 0.85f;
		float digitPitch = 4.8f;
		float contentW = 3.0f * digitPitch + 3.2f + 2.0f * digitPitch + 2.5f;
		float startX = (w - contentW) * 0.5f;
		float digitY = (h - dh) * 0.5f;

		// Hours (8 8 8)
		draw7SegmentAiona(args.vg, startX, digitY, dw, dh, st, h100, onColor, offColor);
		draw7SegmentAiona(args.vg, startX + digitPitch, digitY, dw, dh, st, h10, onColor, offColor);
		draw7SegmentAiona(args.vg, startX + digitPitch * 2.0f, digitY, dw, dh, st, h1, onColor, offColor);

		// 'h' tick
		float primeX = startX + digitPitch * 3.0f - 0.8f;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, primeX, digitY + 1.0f);
		nvgLineTo(args.vg, primeX + 1.2f, digitY + 3.2f);
		nvgStrokeColor(args.vg, onColor);
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);

		// Minutes (8 8)
		float minStartX = primeX + 3.2f;
		draw7SegmentAiona(args.vg, minStartX, digitY, dw, dh, st, m10, onColor, offColor);
		draw7SegmentAiona(args.vg, minStartX + digitPitch, digitY, dw, dh, st, m1, onColor, offColor);

		// 'm' tick
		float dprimeX = minStartX + digitPitch * 2.0f - 0.6f;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, dprimeX, digitY + 1.0f);
		nvgLineTo(args.vg, dprimeX + 1.0f, digitY + 3.2f);
		nvgMoveTo(args.vg, dprimeX + 2.0f, digitY + 1.0f);
		nvgLineTo(args.vg, dprimeX + 3.0f, digitY + 3.2f);
		nvgStrokeColor(args.vg, onColor);
		nvgStrokeWidth(args.vg, 0.9f);
		nvgStroke(args.vg);

		nvgRestore(args.vg);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			appendAionaMenuItems(menu, module);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
};

// ── Module Widget ──
struct AionaWidget : ModuleWidget {
	AionaWidget(Aiona* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Aiona.svg")));

		float col0 = 7.000f;
		float col1 = 20.320f;
		float col2 = 33.640f;

		// ── Zone 2: LCD Displays & Primary Controls ──
		AionaDisplayWidget* display = new AionaDisplayWidget();
		display->module = module;
		display->box.pos = mm2px(Vec(col1 - 17.0f, 12.25f));
		addChild(display);

		addParam(createParamCentered<AionaButton>(mm2px(Vec(col0, 31.00f)), module, Aiona::START_PARAM));

		AionaCountdownWidget* countdown = new AionaCountdownWidget();
		countdown->module = module;
		countdown->box.pos = mm2px(Vec(col1 - 7.0f, 27.25f));
		addChild(countdown);

		addParam(createParamCentered<AionaKnob>(mm2px(Vec(col2, 31.00f)), module, Aiona::PWM_PARAM));

		// ── Zone 3: CV Attenuverters (Center Y = 49.00 mm) ──
		addParam(createParamCentered<AionaTrimpot>(mm2px(Vec(col0, 49.00f)), module, Aiona::HRS_ATTEN_PARAM));
		addParam(createParamCentered<AionaTrimpot>(mm2px(Vec(col1, 49.00f)), module, Aiona::MIN_ATTEN_PARAM));
		addParam(createParamCentered<AionaTrimpot>(mm2px(Vec(col2, 49.00f)), module, Aiona::PWM_ATTEN_PARAM));

		// ── Zone 4: I/O Jacks (Exact User Order: 5 Rows x 3 Columns) ──
		// Row 1 (Center Y = 65.00 mm): HRS, MIN, PWM
		addInput(createInputCentered<AionaPort>(mm2px(Vec(col0, 65.00f)), module, Aiona::HRS_CV_INPUT));
		addInput(createInputCentered<AionaPort>(mm2px(Vec(col1, 65.00f)), module, Aiona::MIN_CV_INPUT));
		addInput(createInputCentered<AionaPort>(mm2px(Vec(col2, 65.00f)), module, Aiona::PWM_CV_INPUT));

		// Row 2 (Center Y = 78.00 mm): START, TRIG, PULSE
		addInput(createInputCentered<AionaPort>(mm2px(Vec(col0, 78.00f)), module, Aiona::START_INPUT));
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col1, 78.00f)), module, Aiona::TRIG_OUTPUT));
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col2, 78.00f)), module, Aiona::PULSE_OUTPUT));

		// Row 3 (Center Y = 91.00 mm): HIGH, SIN+, TRI+
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col0, 91.00f)), module, Aiona::HIGH_OUTPUT));
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col1, 91.00f)), module, Aiona::SIN_PLUS_OUTPUT));
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col2, 91.00f)), module, Aiona::TRI_PLUS_OUTPUT));

		// Row 4 (Center Y = 104.00 mm): LOW, SIN-, TRI-
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col0, 104.00f)), module, Aiona::LOW_OUTPUT));
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col1, 104.00f)), module, Aiona::SIN_MINUS_OUTPUT));
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col2, 104.00f)), module, Aiona::TRI_MINUS_OUTPUT));

		// Row 5 (Center Y = 117.00 mm): RAMP+, RAMP-, EOC
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col0, 117.00f)), module, Aiona::RAMP_PLUS_OUTPUT));
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col1, 117.00f)), module, Aiona::RAMP_MINUS_OUTPUT));
		addOutput(createOutputCentered<AionaPort>(mm2px(Vec(col2, 117.00f)), module, Aiona::EOC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Aiona* module = dynamic_cast<Aiona*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		appendAionaMenuItems(menu, module);
	}
};

Model* modelAiona = createModel<Aiona, AionaWidget>("Aiona");
