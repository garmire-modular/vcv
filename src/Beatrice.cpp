#include "plugin.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

struct Beatrice : Module {
	enum ParamId {
		BPM_PARAM,
		START_PARAM,
		RESET_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		BPM_CV_INPUT,
		START_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		// Row 2: 1, 3/2, 2/3
		DIV_1_OUTPUT,
		DIV_3_2_OUTPUT,
		DIV_2_3_OUTPUT,
		// Row 3: 1/2, 3/4, 1/3
		DIV_1_2_OUTPUT,
		DIV_3_4_OUTPUT,
		DIV_1_3_OUTPUT,
		// Row 4: 1/4, 3/8, 1/6
		DIV_1_4_OUTPUT,
		DIV_3_8_OUTPUT,
		DIV_1_6_OUTPUT,
		// Row 5: 1/8, 3/16, 1/12
		DIV_1_8_OUTPUT,
		DIV_3_16_OUTPUT,
		DIV_1_12_OUTPUT,
		// Row 6: 1/16, 3/32, 1/24
		DIV_1_16_OUTPUT,
		DIV_3_32_OUTPUT,
		DIV_1_24_OUTPUT,
		// Row 7: 1/32, 3/64, 1/48
		DIV_1_32_OUTPUT,
		DIV_3_64_OUTPUT,
		DIV_1_48_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		START_LIGHT,
		RESET_LIGHT,
		// 18 division LEDs
		DIV_1_LIGHT,
		DIV_3_2_LIGHT,
		DIV_2_3_LIGHT,
		DIV_1_2_LIGHT,
		DIV_3_4_LIGHT,
		DIV_1_3_LIGHT,
		DIV_1_4_LIGHT,
		DIV_3_8_LIGHT,
		DIV_1_6_LIGHT,
		DIV_1_8_LIGHT,
		DIV_3_16_LIGHT,
		DIV_1_12_LIGHT,
		DIV_1_16_LIGHT,
		DIV_3_32_LIGHT,
		DIV_1_24_LIGHT,
		DIV_1_32_LIGHT,
		DIV_3_64_LIGHT,
		DIV_1_48_LIGHT,
		LIGHTS_LEN
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT = 1,
		THEME_DARK = 2
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum OutputMode {
		OUTPUT_MODE_TRIGGER = 0,
		OUTPUT_MODE_GATE = 1
	};
	OutputMode outputMode = OUTPUT_MODE_TRIGGER;

	enum BpmCvMode {
		BPM_CV_CURVED = 0,
		BPM_CV_LINEAR = 1
	};
	BpmCvMode bpmCvMode = BPM_CV_CURVED;

	struct DivisionDef {
		int num; // Numerator of duration in whole notes
		int den; // Denominator of duration in whole notes
		const char* tooltip;
	};

	static constexpr int NUM_DIVS = 18;
	static const DivisionDef divisions[NUM_DIVS];

	// Engine state
	bool running = true;
	float bpm = 120.0f;
	float displayBpm = 120.0f;

	float phase[NUM_DIVS] = {};
	dsp::PulseGenerator pulseGen[NUM_DIVS];
	dsp::PulseGenerator ledPulseGen[NUM_DIVS];
	dsp::PulseGenerator resetLedPulse;

	dsp::BooleanTrigger startButtonTrigger;
	dsp::SchmittTrigger startCvTrigger;
	dsp::BooleanTrigger resetButtonTrigger;
	dsp::SchmittTrigger resetCvTrigger;

	// Invertible non-linear BPM mapping prioritizing 30-180 across center 80%
	static float uToBpm(float u) {
		u = clamp(u, 0.0f, 1.0f);
		if (u < 0.1f) {
			return -1025.0f * u * u + 392.5f * u + 1.0f;
		} else if (u <= 0.9f) {
			return 30.0f + 187.5f * (u - 0.1f);
		} else {
			float t = u - 0.9f;
			return 20125.0f * t * t + 187.5f * t + 180.0f;
		}
	}

	static float bpmToU(float b) {
		b = clamp(b, 1.0f, 400.0f);
		if (b < 30.0f) {
			float disc = 392.5f * 392.5f - 4.0f * 1025.0f * (b - 1.0f);
			return (392.5f - std::sqrt(std::max(0.0f, disc))) / 2050.0f;
		} else if (b <= 180.0f) {
			return 0.1f + (b - 30.0f) / 187.5f;
		} else {
			float disc = 187.5f * 187.5f - 4.0f * 20125.0f * (180.0f - b);
			float t = (-187.5f + std::sqrt(std::max(0.0f, disc))) / 40250.0f;
			return 0.9f + t;
		}
	}

	Beatrice() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Default: u = 0.58 corresponds to exactly 120.00 BPM
		configParam(BPM_PARAM, 0.0f, 1.0f, 0.58f, "BPM");
		configParam(START_PARAM, 0.0f, 1.0f, 0.0f, "Start");
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");

		configInput(BPM_CV_INPUT, "BPM CV");
		configInput(START_INPUT, "Start");
		configInput(RESET_INPUT, "Reset");

		for (int i = 0; i < NUM_DIVS; ++i) {
			configOutput(DIV_1_OUTPUT + i, divisions[i].tooltip);
		}

		for (int i = 0; i < NUM_DIVS; ++i) {
			phase[i] = 0.0f;
		}
	}

	void parseBpmString(const std::string& str) {
		try {
			float val = std::stof(str);
			if (val >= 1.0f && val <= 400.0f) {
				bpm = val;
				params[BPM_PARAM].setValue(bpmToU(val));
			}
		} catch (...) {
		}
	}

	void stepBpm(float delta) {
		float newBpm = std::round((displayBpm + delta) * 100.0f) / 100.0f;
		newBpm = clamp(newBpm, 1.0f, 400.0f);
		bpm = newBpm;
		params[BPM_PARAM].setValue(bpmToU(newBpm));
	}

	void resetClock() {
		for (int i = 0; i < NUM_DIVS; ++i) {
			phase[i] = 0.0f;
			pulseGen[i].trigger(1e-3f);
			ledPulseGen[i].trigger(0.025f);
		}
		resetLedPulse.trigger(0.050f);
	}

	void process(const ProcessArgs& args) override {
		// 1. Handle Start button and CV gate input
		if (startButtonTrigger.process(params[START_PARAM].getValue())) {
			running = !running;
		}
		if (inputs[START_INPUT].isConnected()) {
			if (startCvTrigger.process(inputs[START_INPUT].getVoltage())) {
				running = !running;
			}
		}
		lights[START_LIGHT].setBrightness(running ? 1.0f : 0.0f);

		// 2. Handle Reset button and CV input
		bool doReset = false;
		if (resetButtonTrigger.process(params[RESET_PARAM].getValue())) {
			doReset = true;
		}
		if (inputs[RESET_INPUT].isConnected()) {
			if (resetCvTrigger.process(inputs[RESET_INPUT].getVoltage())) {
				doReset = true;
			}
		}
		if (doReset) {
			resetClock();
		}
		lights[RESET_LIGHT].setBrightness(resetLedPulse.process(args.sampleTime) ? 1.0f : 0.0f);

		// 3. Compute effective BPM
		float uKnob = params[BPM_PARAM].getValue();
		float effBpm = 120.0f;

		if (inputs[BPM_CV_INPUT].isConnected()) {
			float cv = inputs[BPM_CV_INPUT].getVoltage();
			if (bpmCvMode == BPM_CV_CURVED) {
				float uMod = clamp(uKnob + cv * 0.1f, 0.0f, 1.0f);
				effBpm = uToBpm(uMod);
			} else {
				float baseBpm = uToBpm(uKnob);
				effBpm = clamp(baseBpm + cv * 20.0f, 1.0f, 400.0f);
			}
		} else {
			effBpm = uToBpm(uKnob);
		}

		bpm = effBpm;
		displayBpm = effBpm;

		// 4. Update division phases if running
		if (running) {
			// Whole note frequency: BPM / 240.0 Hz
			float fWhole = effBpm / 240.0f;

			for (int i = 0; i < NUM_DIVS; ++i) {
				float ratio = (float)divisions[i].den / (float)divisions[i].num;
				float fDiv = ratio * fWhole;
				phase[i] += fDiv * args.sampleTime;
				if (phase[i] >= 1.0f) {
					phase[i] -= 1.0f;
					pulseGen[i].trigger(1e-3f);
					ledPulseGen[i].trigger(0.025f);
				}

				if (outputMode == OUTPUT_MODE_TRIGGER) {
					bool high = pulseGen[i].process(args.sampleTime);
					outputs[DIV_1_OUTPUT + i].setVoltage(high ? 10.0f : 0.0f);
					lights[DIV_1_LIGHT + i].setBrightness(ledPulseGen[i].process(args.sampleTime) ? 1.0f : 0.0f);
				} else {
					// 50% duty cycle gate
					bool high = (phase[i] < 0.5f);
					outputs[DIV_1_OUTPUT + i].setVoltage(high ? 10.0f : 0.0f);
					lights[DIV_1_LIGHT + i].setBrightness(high ? 1.0f : 0.0f);
				}
			}
		} else {
			for (int i = 0; i < NUM_DIVS; ++i) {
				outputs[DIV_1_OUTPUT + i].setVoltage(0.0f);
				lights[DIV_1_LIGHT + i].setBrightness(0.0f);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "outputMode", json_integer((int)outputMode));
		json_object_set_new(rootJ, "bpmCvMode", json_integer((int)bpmCvMode));
		json_object_set_new(rootJ, "running", json_boolean(running));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* themeJ = json_object_get(rootJ, "skinTheme");
		if (themeJ) {
			skinTheme = (SkinTheme)json_integer_value(themeJ);
		}
		json_t* modeJ = json_object_get(rootJ, "outputMode");
		if (modeJ) {
			outputMode = (OutputMode)json_integer_value(modeJ);
		}
		json_t* cvModeJ = json_object_get(rootJ, "bpmCvMode");
		if (cvModeJ) {
			bpmCvMode = (BpmCvMode)json_integer_value(cvModeJ);
		}
		json_t* runJ = json_object_get(rootJ, "running");
		if (runJ) {
			running = json_is_true(runJ);
		}
	}
};

const Beatrice::DivisionDef Beatrice::divisions[Beatrice::NUM_DIVS] = {
	// Row 2: 1, 3/2, 2/3
	{1, 1, "𝅝 (1)"},
	{3, 2, "𝅝. (3/2)"},
	{2, 3, "𝅝³ (2/3)"},
	// Row 3: 1/2, 3/4, 1/3
	{1, 2, "𝅗𝅥 (1/2)"},
	{3, 4, "𝅗𝅥. (3/4)"},
	{1, 3, "𝅗𝅥³ (1/3)"},
	// Row 4: 1/4, 3/8, 1/6
	{1, 4, "𝅘𝅥 (1/4)"},
	{3, 8, "𝅘𝅥. (3/8)"},
	{1, 6, "𝅘𝅥³ (1/6)"},
	// Row 5: 1/8, 3/16, 1/12
	{1, 8, "𝅘𝅥𝅮 (1/8)"},
	{3, 16, "𝅘𝅥𝅮. (3/16)"},
	{1, 12, "𝅘𝅥𝅮³ (1/12)"},
	// Row 6: 1/16, 3/32, 1/24
	{1, 16, "𝅘𝅥𝅯 (1/16)"},
	{3, 32, "𝅘𝅥𝅯. (3/32)"},
	{1, 24, "𝅘𝅥𝅯³ (1/24)"},
	// Row 7: 1/32, 3/64, 1/48
	{1, 32, "𝅘𝅥𝅰 (1/32)"},
	{3, 64, "𝅘𝅥𝅰. (3/64)"},
	{1, 48, "𝅘𝅥𝅰³ (1/48)"}
};

// Garmire Custom Hardware Widgets
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

struct GarmireButton : app::SvgSwitch {
	GarmireButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// TextField for entering BPM directly
struct BeatriceBpmTextField : ui::TextField {
	Beatrice* module = nullptr;

	BeatriceBpmTextField(Beatrice* m) : module(m) {
		box.size.x = 160.0f;
		if (m) {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.2f", m->displayBpm);
			setText(buf);
		}
	}

	void onAction(const ActionEvent& e) override {
		if (module) {
			module->parseBpmString(getText());
		}
	}

	void onSelectKey(const SelectKeyEvent& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			if (module) {
				module->parseBpmString(getText());
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

static void appendBeatriceMenuItems(ui::Menu* menu, Beatrice* module);

// Beatrice LCD Display Widget
struct BeatriceDisplayWidget : OpaqueWidget {
	Beatrice* module = nullptr;

	BeatriceDisplayWidget() {
		box.size = mm2px(Vec(34.0f, 7.5f));
	}

	static void draw7Segment(NVGcontext* vg, float x, float y, float w, float h, float s, int digit, NVGcolor onColor, NVGcolor offColor) {
		static const uint8_t segs[10] = {
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

		uint8_t mask = (digit >= 0 && digit <= 9) ? segs[digit] : 0;
		float halfH = h * 0.5f;

		// a: top horizontal
		nvgBeginPath(vg);
		nvgMoveTo(vg, x + s, y);
		nvgLineTo(vg, x + w - s, y);
		nvgStrokeColor(vg, (mask & 0x01) ? onColor : offColor);
		nvgStrokeWidth(vg, s);
		nvgStroke(vg);

		// b: top right vertical
		nvgBeginPath(vg);
		nvgMoveTo(vg, x + w, y + s);
		nvgLineTo(vg, x + w, y + halfH - s * 0.5f);
		nvgStrokeColor(vg, (mask & 0x02) ? onColor : offColor);
		nvgStrokeWidth(vg, s);
		nvgStroke(vg);

		// c: bottom right vertical
		nvgBeginPath(vg);
		nvgMoveTo(vg, x + w, y + halfH + s * 0.5f);
		nvgLineTo(vg, x + w, y + h - s);
		nvgStrokeColor(vg, (mask & 0x04) ? onColor : offColor);
		nvgStrokeWidth(vg, s);
		nvgStroke(vg);

		// d: bottom horizontal
		nvgBeginPath(vg);
		nvgMoveTo(vg, x + s, y + h);
		nvgLineTo(vg, x + w - s, y + h);
		nvgStrokeColor(vg, (mask & 0x08) ? onColor : offColor);
		nvgStrokeWidth(vg, s);
		nvgStroke(vg);

		// e: bottom left vertical
		nvgBeginPath(vg);
		nvgMoveTo(vg, x, y + halfH + s * 0.5f);
		nvgLineTo(vg, x, y + h - s);
		nvgStrokeColor(vg, (mask & 0x10) ? onColor : offColor);
		nvgStrokeWidth(vg, s);
		nvgStroke(vg);

		// f: top left vertical
		nvgBeginPath(vg);
		nvgMoveTo(vg, x, y + s);
		nvgLineTo(vg, x, y + halfH - s * 0.5f);
		nvgStrokeColor(vg, (mask & 0x20) ? onColor : offColor);
		nvgStrokeWidth(vg, s);
		nvgStroke(vg);

		// g: middle horizontal
		nvgBeginPath(vg);
		nvgMoveTo(vg, x + s, y + halfH);
		nvgLineTo(vg, x + w - s, y + halfH);
		nvgStrokeColor(vg, (mask & 0x40) ? onColor : offColor);
		nvgStrokeWidth(vg, s);
		nvgStroke(vg);
	}

	void draw(const DrawArgs& args) override {
		float w = box.size.x;
		float h = box.size.y;

		nvgSave(args.vg);

		// Frame background
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, 2.5f);
		nvgFillColor(args.vg, nvgRGBA(16, 20, 18, 255));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(44, 53, 48, 255));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);

		// Arrows: Left (integer BPM)
		float arrowXLeft = 19.0f;
		float arrowXRight = w - 19.0f;
		float arrowYMid = h * 0.5f;

		// Left Up Arrow
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, arrowXLeft - 3.5f, arrowYMid - 2.0f);
		nvgLineTo(args.vg, arrowXLeft, arrowYMid - 6.5f);
		nvgLineTo(args.vg, arrowXLeft + 3.5f, arrowYMid - 2.0f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGBA(56, 212, 48, 220));
		nvgFill(args.vg);

		// Left Down Arrow
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, arrowXLeft - 3.5f, arrowYMid + 2.0f);
		nvgLineTo(args.vg, arrowXLeft, arrowYMid + 6.5f);
		nvgLineTo(args.vg, arrowXLeft + 3.5f, arrowYMid + 2.0f);
		nvgClosePath(args.vg);
		nvgFill(args.vg);

		// Right Up Arrow
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, arrowXRight - 3.5f, arrowYMid - 2.0f);
		nvgLineTo(args.vg, arrowXRight, arrowYMid - 6.5f);
		nvgLineTo(args.vg, arrowXRight + 3.5f, arrowYMid - 2.0f);
		nvgClosePath(args.vg);
		nvgFill(args.vg);

		// Right Down Arrow
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, arrowXRight - 3.5f, arrowYMid + 2.0f);
		nvgLineTo(args.vg, arrowXRight, arrowYMid + 6.5f);
		nvgLineTo(args.vg, arrowXRight + 3.5f, arrowYMid + 2.0f);
		nvgClosePath(args.vg);
		nvgFill(args.vg);

		// Digits display (e.g. 120.00)
		float curBpm = module ? module->displayBpm : 120.0f;
		int totalCents = (int)std::round(curBpm * 100.0f);
		totalCents = clamp(totalCents, 100, 40000);

		int intBpm = totalCents / 100;
		int centBpm = totalCents % 100;

		int d100 = intBpm / 100;
		int d10 = (intBpm / 10) % 10;
		int d1 = intBpm % 10;
		int c10 = centBpm / 10;
		int c1 = centBpm % 10;

		NVGcolor onColor = nvgRGBA(68, 255, 68, 255);
		NVGcolor offColor = nvgRGBA(24, 48, 28, 120);

		float dw = 4.8f;
		float dh = 11.5f;
		float st = 1.3f;
		float digitPitch = 7.0f;

		// 3 integer digits + dot + 2 decimal digits
		float contentW = 3.0f * digitPitch + 3.5f + 2.0f * digitPitch;
		float startX = (w - contentW) * 0.5f;
		float digitY = (h - dh) * 0.5f;

		// Integer digits
		draw7Segment(args.vg, startX, digitY, dw, dh, st, (d100 > 0 ? d100 : -1), onColor, offColor);
		draw7Segment(args.vg, startX + digitPitch, digitY, dw, dh, st, (intBpm >= 10 ? d10 : -1), onColor, offColor);
		draw7Segment(args.vg, startX + digitPitch * 2.0f, digitY, dw, dh, st, d1, onColor, offColor);

		// Decimal point
		float dotX = startX + digitPitch * 3.0f + 0.8f;
		float dotY = digitY + dh - 1.2f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, dotX, dotY, 1.1f);
		nvgFillColor(args.vg, onColor);
		nvgFill(args.vg);

		// Decimal digits
		float centStartX = dotX + 3.5f;
		draw7Segment(args.vg, centStartX, digitY, dw, dh, st, c10, onColor, offColor);
		draw7Segment(args.vg, centStartX + digitPitch, digitY, dw, dh, st, c1, onColor, offColor);

		nvgRestore(args.vg);
	}

	void onButton(const ButtonEvent& e) override {
		if (!module) {
			OpaqueWidget::onButton(e);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			appendBeatriceMenuItems(menu, module);
			e.consume(this);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			float x = e.pos.x;
			float y = e.pos.y;
			float w = box.size.x;
			float h = box.size.y;
			float halfH = h * 0.5f;

			// Left arrows (Integer BPM, x < 28.0)
			if (x < 28.0f) {
				float step = (e.mods & GLFW_MOD_SHIFT) ? 10.0f : 1.0f;
				if (y < halfH) {
					module->stepBpm(+step);
				} else {
					module->stepBpm(-step);
				}
				e.consume(this);
				return;
			}

			// Right arrows (Decimal BPM, x > w - 28.0)
			if (x > w - 28.0f) {
				float step = (e.mods & GLFW_MOD_SHIFT) ? 0.10f : 0.01f;
				if (y < halfH) {
					module->stepBpm(+step);
				} else {
					module->stepBpm(-step);
				}
				e.consume(this);
				return;
			}

			// Center area click: open manual BPM dialog
			ui::Menu* menu = createMenu();
			appendBeatriceMenuItems(menu, module);
			e.consume(this);
			return;
		}

		OpaqueWidget::onButton(e);
	}
};

static void appendBeatriceMenuItems(ui::Menu* menu, Beatrice* module) {
	if (!module) return;

	menu->addChild(createMenuLabel("Set BPM"));
	BeatriceBpmTextField* textField = new BeatriceBpmTextField(module);
	menu->addChild(textField);
	if (APP && APP->event) {
		APP->event->setSelectedWidget(textField);
	}

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("Output Pulse Mode"));

	struct OutputModeItem : MenuItem {
		Beatrice* module;
		Beatrice::OutputMode mode;
		void onAction(const ActionEvent& e) override {
			module->outputMode = mode;
		}
	};

	OutputModeItem* trigItem = createMenuItem<OutputModeItem>("Trigger (1 ms pulse)");
	trigItem->module = module;
	trigItem->mode = Beatrice::OUTPUT_MODE_TRIGGER;
	trigItem->rightText = (module->outputMode == Beatrice::OUTPUT_MODE_TRIGGER) ? "✔" : "";
	menu->addChild(trigItem);

	OutputModeItem* gateItem = createMenuItem<OutputModeItem>("Gate (50% duty cycle)");
	gateItem->module = module;
	gateItem->mode = Beatrice::OUTPUT_MODE_GATE;
	gateItem->rightText = (module->outputMode == Beatrice::OUTPUT_MODE_GATE) ? "✔" : "";
	menu->addChild(gateItem);

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("BPM CV Response"));

	struct CvModeItem : MenuItem {
		Beatrice* module;
		Beatrice::BpmCvMode mode;
		void onAction(const ActionEvent& e) override {
			module->bpmCvMode = mode;
		}
	};

	CvModeItem* curveItem = createMenuItem<CvModeItem>("Curved (30-180 BPM prioritized)");
	curveItem->module = module;
	curveItem->mode = Beatrice::BPM_CV_CURVED;
	curveItem->rightText = (module->bpmCvMode == Beatrice::BPM_CV_CURVED) ? "✔" : "";
	menu->addChild(curveItem);

	CvModeItem* linItem = createMenuItem<CvModeItem>("Linear (20 BPM/V)");
	linItem->module = module;
	linItem->mode = Beatrice::BPM_CV_LINEAR;
	linItem->rightText = (module->bpmCvMode == Beatrice::BPM_CV_LINEAR) ? "✔" : "";
	menu->addChild(linItem);

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("Theme"));

	struct ThemeItem : MenuItem {
		Beatrice* module;
		Beatrice::SkinTheme theme;
		void onAction(const ActionEvent& e) override {
			module->skinTheme = theme;
		}
	};

	const char* themeLabels[] = {"Medium (Default)", "Light", "Dark"};
	for (int t = 0; t < 3; ++t) {
		ThemeItem* tItem = createMenuItem<ThemeItem>(themeLabels[t]);
		tItem->module = module;
		tItem->theme = (Beatrice::SkinTheme)t;
		tItem->rightText = (module->skinTheme == t) ? "✔" : "";
		menu->addChild(tItem);
	}
}

// Module Widget with Triple Skin support
struct BeatriceWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Beatrice::SkinTheme lastTheme = Beatrice::THEME_MEDIUM;

	void step() override {
		ModuleWidget::step();
		Beatrice* module = dynamic_cast<Beatrice*>(this->module);
		if (module && panel) {
			if (module->skinTheme != lastTheme) {
				lastTheme = module->skinTheme;
				switch (lastTheme) {
					case Beatrice::THEME_LIGHT:
						panel->setBackground(Svg::load(asset::plugin(pluginInstance, "res/Beatrice_Light.svg")));
						break;
					case Beatrice::THEME_DARK:
						panel->setBackground(Svg::load(asset::plugin(pluginInstance, "res/Beatrice_Dark.svg")));
						break;
					default:
						panel->setBackground(Svg::load(asset::plugin(pluginInstance, "res/Beatrice.svg")));
						break;
				}
			}
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		Beatrice* module = dynamic_cast<Beatrice*>(this->module);
		if (!module) return;
		appendBeatriceMenuItems(menu, module);
	}

	BeatriceWidget(Beatrice* module) {
		setModule(module);
		setPanel(createPanel(
			asset::plugin(pluginInstance, "res/Beatrice.svg"),
			asset::plugin(pluginInstance, "res/Beatrice.svg")
		));
		panel = dynamic_cast<SvgPanel*>(this->panel);

		float col0 = 11.50f;
		float col1 = 20.32f;
		float col2 = 29.14f;

		// LCD Display at Y = 13.50mm, centered horizontally at X = 20.32mm
		BeatriceDisplayWidget* display = new BeatriceDisplayWidget();
		display->module = module;
		display->box.pos = mm2px(Vec(col1 - 17.0f, 13.50f));
		addChild(display);

		// Controls Row at Y = 30.50mm
		float ctrlY = 30.50f;
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(col0, ctrlY)), module, Beatrice::BPM_PARAM));
		addParam(createParamCentered<GarmireButton>(mm2px(Vec(col1, ctrlY)), module, Beatrice::START_PARAM));
		addParam(createParamCentered<GarmireButton>(mm2px(Vec(col2, ctrlY)), module, Beatrice::RESET_PARAM));

		// Jack Rows (Pitch: 11.50mm, bottom anchor: 113.00mm)
		float pitch = 11.50f;
		float bottomY = 113.00f;
		float jackRows[7];
		for (int i = 0; i < 7; ++i) {
			jackRows[i] = bottomY - (6 - i) * pitch;
		}

		// Row 1 Jacks: BPM, START, RESET at Y = 44.00mm
		float r1Y = jackRows[0];
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col0, r1Y)), module, Beatrice::BPM_CV_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col1, r1Y)), module, Beatrice::START_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col2, r1Y)), module, Beatrice::RESET_INPUT));

		// Start and Reset LEDs at (x + 4.50mm, y - 4.00mm)
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(col1 + 4.50f, r1Y - 4.00f)), module, Beatrice::START_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(col2 + 4.50f, r1Y - 4.00f)), module, Beatrice::RESET_LIGHT));

		// Rows 2-7: 18 Division Jacks & LEDs
		for (int r = 0; r < 6; ++r) {
			float jy = jackRows[r + 1];
			int baseIdx = r * 3;

			// Col 0: Straight
			addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col0, jy)), module, Beatrice::DIV_1_OUTPUT + baseIdx + 0));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(col0 + 4.50f, jy - 4.00f)), module, Beatrice::DIV_1_LIGHT + baseIdx + 0));

			// Col 1: Dotted
			addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col1, jy)), module, Beatrice::DIV_1_OUTPUT + baseIdx + 1));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(col1 + 4.50f, jy - 4.00f)), module, Beatrice::DIV_1_LIGHT + baseIdx + 1));

			// Col 2: Triplet
			addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col2, jy)), module, Beatrice::DIV_1_OUTPUT + baseIdx + 2));
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(col2 + 4.50f, jy - 4.00f)), module, Beatrice::DIV_1_LIGHT + baseIdx + 2));
		}
	}
};

Model* modelBeatrice = createModel<Beatrice, BeatriceWidget>("Beatrice");
