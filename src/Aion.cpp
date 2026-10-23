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
//  selectable voltage range, start trigger/gate input, active high/low
//  gates, trigger & EOC pulses, and 7 continuous synchronous functions
//  (Ramp Up, Ramp Down, Triangle, Inverted Triangle, Sine, Cosine, PWM).
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
		HIGH_OUTPUT,
		LOW_OUTPUT,
		TRIG_OUTPUT,
		EOC_OUTPUT,
		PWM_OUTPUT,
		RAMP_UP_OUTPUT,
		RAMP_DN_OUTPUT,
		TRI_OUTPUT,
		INV_TRI_OUTPUT,
		SINE_OUTPUT,
		COSINE_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		RUN_LIGHT_G,
		RUN_LIGHT_R,
		LIGHTS_LEN
	};

	// ── State Variables ──
	int minutes = 0;       // 0 to 999
	int seconds = 10;      // 0 to 59 (default 10s)
	VoltageRange voltageRange = RANGE_UNIPOLAR_10V;

	bool running = false;
	float elapsedSec = 0.0f;
	float currentProgress = 0.0f; // 0.0 to 1.0 (for display)

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
		configOutput(HIGH_OUTPUT, "High gate");
		configOutput(LOW_OUTPUT, "Low gate");
		configOutput(TRIG_OUTPUT, "Trigger");
		configOutput(EOC_OUTPUT, "End of cycle gate");
		configOutput(PWM_OUTPUT, "Pulse width modulated wave");
		configOutput(RAMP_UP_OUTPUT, "Ramp up");
		configOutput(RAMP_DN_OUTPUT, "Ramp down");
		configOutput(TRI_OUTPUT, "Triangle");
		configOutput(INV_TRI_OUTPUT, "Inverted triangle");
		configOutput(SINE_OUTPUT, "Sine");
		configOutput(COSINE_OUTPUT, "Cosine");
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
		// Check for formats like "12m34s", "12m 34s", "12m", "34s", "12:34"
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
			// Plain number: treat as seconds
			int totalSec = std::atoi(str.c_str());
			m = totalSec / 60;
			s = totalSec % 60;
		}

		minutes = clampf((float)m, 0.0f, 999.0f);
		seconds = clampf((float)s, 0.0f, 59.0f);
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

		// 3. Update Progress & Timing
		float phase = 0.0f;
		if (running) {
			elapsedSec += args.sampleTime;
			if (elapsedSec >= totalDuration) {
				phase = 1.0f;
				running = false;
				elapsedSec = 0.0f;
				eocPulse.trigger(0.001f); // 1 ms EOC pulse
			}
			else {
				phase = elapsedSec / totalDuration;
			}
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

		// Output 1 & 2: Gates
		outputs[HIGH_OUTPUT].setVoltage(running ? vMax : 0.0f);
		outputs[LOW_OUTPUT].setVoltage(running ? vMin : vMax);

		// Output 3 & 4: Pulses
		bool trigActive = startPulse.process(args.sampleTime);
		bool eocActive = eocPulse.process(args.sampleTime);
		outputs[TRIG_OUTPUT].setVoltage(trigActive ? vMax : 0.0f);
		outputs[EOC_OUTPUT].setVoltage(eocActive ? vMax : 0.0f);

		// Output 5: PWM
		bool pwmHigh = running && (phase < pw);
		outputs[PWM_OUTPUT].setVoltage(pwmHigh ? vMax : vMin);

		// Output 6: Ramp Up
		outputs[RAMP_UP_OUTPUT].setVoltage(vMin + uRampUp * vSpan);

		// Output 7: Ramp Down
		outputs[RAMP_DN_OUTPUT].setVoltage(vMin + uRampDn * vSpan);

		// Output 8: Triangle
		outputs[TRI_OUTPUT].setVoltage(vMin + uTri * vSpan);

		// Output 9: Inverted Triangle
		outputs[INV_TRI_OUTPUT].setVoltage(vMin + uInvTri * vSpan);

		// Output 10 & 11: Sine & Cosine
		float sinVal = running ? std::sin(2.0f * (float)M_PI * phase) : 0.0f;
		float cosVal = running ? std::cos(2.0f * (float)M_PI * phase) : 0.0f;

		if (isBipolar()) {
			outputs[SINE_OUTPUT].setVoltage(vMax * sinVal);
			outputs[COSINE_OUTPUT].setVoltage(vMax * cosVal);
		}
		else {
			outputs[SINE_OUTPUT].setVoltage(vMin + 0.5f * (1.0f + sinVal) * vSpan);
			outputs[COSINE_OUTPUT].setVoltage(vMin + 0.5f * (1.0f + cosVal) * vSpan);
		}

		// 6. Running Status LED (Green when running, dim when idle)
		lights[RUN_LIGHT_G].setBrightness(running ? 1.0f : 0.0f);
		lights[RUN_LIGHT_R].setBrightness(0.0f);
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
			e.consume(this);
			return;
		}
		ui::TextField::onSelectKey(e);
	}
};

// ── 7-Segment LCD Display Widget ──
struct AionDisplayWidget : OpaqueWidget {
	Aion* module = nullptr;

	// Segment map for digits 0-9: segments a,b,c,d,e,f,g (bit 0 to bit 6)
	// a: bit 0 (top)
	// b: bit 1 (top right)
	// c: bit 2 (bottom right)
	// d: bit 3 (bottom)
	// e: bit 4 (bottom left)
	// f: bit 5 (top left)
	// g: bit 6 (middle)
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

		float dw = 6.2f;   // digit width
		float dh = 15.0f;  // digit height
		float st = 1.35f;  // segment thickness
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

		// Colors
		NVGcolor onColor = nvgRGBA(0x38, 0xf8, 0x70, 0xff);  // High contrast vibrant LCD green
		NVGcolor offColor = nvgRGBA(0x15, 0x27, 0x1a, 0x55); // Dim unlit segment mesh
		NVGcolor btnColor = nvgRGBA(0x72, 0x8a, 0x7a, 0xff); // Stacked arrow buttons

		int mVal = module ? module->minutes : 0;
		int sVal = module ? module->seconds : 10;

		// 2. Left Stacked Arrows (Minutes Up / Down)
		// Up arrow center at (11.0, h * 0.30), Down arrow at (11.0, h * 0.70)
		drawArrow(args.vg, 10.0f, h * 0.28f, true, btnColor);
		drawArrow(args.vg, 10.0f, h * 0.72f, false, btnColor);

		// 3. Right Stacked Arrows (Seconds Up / Down)
		drawArrow(args.vg, w - 10.0f, h * 0.28f, true, btnColor);
		drawArrow(args.vg, w - 10.0f, h * 0.72f, false, btnColor);

		// 4. Center 7-Segment Digits: 3 Minutes + ' + 2 Seconds + "
		// Compute individual digits
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

// ── Module Widget ──
struct AionWidget : ModuleWidget {
	AionWidget(Aion* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/Aion.svg")));

		// Strict Ban on Virtual Screws (AGENTS.md Section 7.5.6) - ZERO screws added

		// Column coordinates per Section 7.1
		float col0 = 7.000f;
		float col1 = 20.320f;
		float col2 = 33.640f;

		// ── Zone 2: LCD Display & Primary Controls ──
		// 7-Segment LCD Display (Center X = 20.32 mm, Center Y = 17.50 mm, W = 34.00 mm, H = 11.00 mm)
		AionDisplayWidget* display = new AionDisplayWidget();
		display->module = module;
		display->box.pos = mm2px(Vec(col1 - 17.0f, 17.50f - 5.5f));
		addChild(display);

		// Row at Y = 32.00 mm: Start Button, Run Indicator LED, PWM Knob
		addParam(createParamCentered<GarmireButton>(mm2px(Vec(col0, 32.00f)), module, Aion::START_PARAM));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(col1, 32.00f)), module, Aion::RUN_LIGHT_G));
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(col2, 32.00f)), module, Aion::PWM_PARAM));

		// ── Zone 3: CV Attenuverters (Center Y = 49.00 mm) ──
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col0, 49.00f)), module, Aion::MIN_ATTEN_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col1, 49.00f)), module, Aion::SEC_ATTEN_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col2, 49.00f)), module, Aion::PWM_ATTEN_PARAM));

		// ── Zone 4: I/O Jacks (5 Rows from Y = 65.00 mm to Y = 117.00 mm) ──
		// Jack Row 1 (Center Y = 65.00 mm): MIN CV, SEC CV, PWM CV
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col0, 65.00f)), module, Aion::MIN_CV_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col1, 65.00f)), module, Aion::SEC_CV_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col2, 65.00f)), module, Aion::PWM_CV_INPUT));

		// Jack Row 2 (Center Y = 78.00 mm): START Gate, HIGH Gate, LOW Gate
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col0, 78.00f)), module, Aion::START_INPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, 78.00f)), module, Aion::HIGH_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, 78.00f)), module, Aion::LOW_OUTPUT));

		// Jack Row 3 (Center Y = 91.00 mm): TRIG, EOC, PWM
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col0, 91.00f)), module, Aion::TRIG_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, 91.00f)), module, Aion::EOC_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, 91.00f)), module, Aion::PWM_OUTPUT));

		// Jack Row 4 (Center Y = 104.00 mm): RAMP +, RAMP -, TRI
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col0, 104.00f)), module, Aion::RAMP_UP_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, 104.00f)), module, Aion::RAMP_DN_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, 104.00f)), module, Aion::TRI_OUTPUT));

		// Jack Row 5 (Center Y = 117.00 mm): INV, SIN, COS
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col0, 117.00f)), module, Aion::INV_TRI_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, 117.00f)), module, Aion::SINE_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, 117.00f)), module, Aion::COSINE_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Aion* module = dynamic_cast<Aion*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Set Event Time"));

		AionTimeTextField* textField = new AionTimeTextField(module);
		menu->addChild(textField);

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
				[=]() { return module->voltageRange == mode; },
				[=]() { module->voltageRange = mode; }
			));
		}
	}
};

Model* modelAion = createModel<Aion, AionWidget>("Aion");
