#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Octavia — Octature Polyphase Oscillator (8 HP)
//  Generate class module featuring standardized Freq, Range, Fine controls
//  and 24 simultaneous polyphase oscillator outputs across 3 waveforms:
//  Columns: SINE, RAMP, SAW in 8 phase increments (0°, 45°, 90°, 135°,
//  180°, 225°, 270°, 315°).
//  Selectable output voltage ranges: 0..1V, ±1V, 0..5V, ±5V, 0..10V, ±10V.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}
} // namespace

struct Octavia : Module {
	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum VoltageRange {
		RANGE_0_1V = 0,
		RANGE_BIPOLAR_1V,
		RANGE_0_5V,
		RANGE_BIPOLAR_5V, // Default
		RANGE_0_10V,
		RANGE_BIPOLAR_10V,
		VOLTAGE_RANGE_LEN
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};

	enum ParamId {
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		INPUTS_LEN = 0
	};

	enum OutputId {
		// 8 rows x 3 columns = 24 outputs
		// Row r (0..7) has phase = r * 45°
		// Col 0: SINE
		// Col 1: RAMP
		// Col 2: SAW
		SINE_0_OUTPUT,
		RAMP_0_OUTPUT,
		SAW_0_OUTPUT,

		SINE_45_OUTPUT,
		RAMP_45_OUTPUT,
		SAW_45_OUTPUT,

		SINE_90_OUTPUT,
		RAMP_90_OUTPUT,
		SAW_90_OUTPUT,

		SINE_135_OUTPUT,
		RAMP_135_OUTPUT,
		SAW_135_OUTPUT,

		SINE_180_OUTPUT,
		RAMP_180_OUTPUT,
		SAW_180_OUTPUT,

		SINE_225_OUTPUT,
		RAMP_225_OUTPUT,
		SAW_225_OUTPUT,

		SINE_270_OUTPUT,
		RAMP_270_OUTPUT,
		SAW_270_OUTPUT,

		SINE_315_OUTPUT,
		RAMP_315_OUTPUT,
		SAW_315_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_ORANGE,
		RANGE_LIGHT_PURPLE,
		LIGHTS_LEN
	};

	RangeMode rangeMode = RANGE_LFO;
	VoltageRange voltageRange = RANGE_BIPOLAR_5V;
	SkinTheme skinTheme = THEME_MEDIUM;

	dsp::BooleanTrigger rangeTrigger;
	float phase = 0.0f; // Master phase [0, 1)

	float calculateBaseFrequency(float coarse, float fine) {
		if (rangeMode == RANGE_VERY_SLOW) {
			// Period T in seconds: 600.0s to 0.1s
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float period = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			return (period > 1e-6f) ? (1.0f / period) : 0.001f;
		} else if (rangeMode == RANGE_LFO) {
			// Frequency in Hz: 0.01 to 200.00 Hz
			float fCoarse = 0.01f + 199.99f * coarse * coarse;
			return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
		} else {
			// Frequency in Hz: 150.00 to 2000.00 Hz
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
		}
	}

	struct FreqParamQuantity : ParamQuantity {
		Octavia* getOctavia() {
			return dynamic_cast<Octavia*>(module);
		}

		float getDisplayValue() override {
			Octavia* mod = getOctavia();
			if (!mod) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (mod->paramQuantities.size() > Octavia::FINE_PARAM) ? mod->params[Octavia::FINE_PARAM].getValue() : 0.f;

			if (mod->rangeMode == Octavia::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (mod->rangeMode == Octavia::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Octavia* mod = getOctavia();
			if (!mod) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (mod->rangeMode == Octavia::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}
	};

	Octavia() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency", " Hz");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		static const char* phaseNames[8] = {
			"0°", "45°", "90°", "135°", "180°", "225°", "270°", "315°"
		};

		for (int r = 0; r < 8; ++r) {
			int baseIdx = r * 3;
			std::string pStr = phaseNames[r];
			configOutput(SINE_0_OUTPUT + baseIdx + 0, "Sine " + pStr);
			configOutput(SINE_0_OUTPUT + baseIdx + 1, "Ramp " + pStr);
			configOutput(SINE_0_OUTPUT + baseIdx + 2, "Saw " + pStr);
		}
	}

	float scaleVoltage(float normBipolar) {
		// normBipolar is in [-1.0, 1.0]
		switch (voltageRange) {
			case RANGE_0_1V:
				return (normBipolar * 0.5f + 0.5f) * 1.0f;
			case RANGE_BIPOLAR_1V:
				return normBipolar * 1.0f;
			case RANGE_0_5V:
				return (normBipolar * 0.5f + 0.5f) * 5.0f;
			case RANGE_BIPOLAR_5V:
			default:
				return normBipolar * 5.0f;
			case RANGE_0_10V:
				return (normBipolar * 0.5f + 0.5f) * 10.0f;
			case RANGE_BIPOLAR_10V:
				return normBipolar * 10.0f;
		}
	}

	void process(const ProcessArgs& args) override {
		// 3-state Range Button trigger
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		// Update 3-State LED Ring Indicator
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam = params[FINE_PARAM].getValue();
		float f0 = calculateBaseFrequency(coarseParam, fineParam);

		// Advance master phase
		phase += f0 * args.sampleTime;
		if (phase >= 1.0f) {
			phase -= std::floor(phase);
		}

		// Calculate 8 polyphase offsets and 3 waveforms
		const float twoPi = 2.0f * (float)M_PI;

		for (int r = 0; r < 8; ++r) {
			float phaseOffset = (float)r / 8.0f; // 0, 1/8, 2/8...
			float p = phase + phaseOffset;
			p -= std::floor(p); // [0, 1)

			// 1. Sine: sin(2 * pi * p)
			float sineVal = std::sin(twoPi * p);

			// 2. Ramp (rising sawtooth): from -1 at p=0 to +1 at p=1
			float rampVal = 2.0f * p - 1.0f;

			// 3. Saw (falling sawtooth): from +1 at p=0 to -1 at p=1
			float sawVal = 1.0f - 2.0f * p;

			int baseIdx = r * 3;
			outputs[SINE_0_OUTPUT + baseIdx + 0].setVoltage(scaleVoltage(sineVal));
			outputs[SINE_0_OUTPUT + baseIdx + 1].setVoltage(scaleVoltage(rampVal));
			outputs[SINE_0_OUTPUT + baseIdx + 2].setVoltage(scaleVoltage(sawVal));
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) rangeMode = (RangeMode)json_integer_value(rmJ);

		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) voltageRange = (VoltageRange)json_integer_value(vrJ);

		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);
	}
};

// Custom hardware widgets per AGENTS.md Section 7.4
struct OctaviaPort : app::SvgPort {
	OctaviaPort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};

struct OctaviaKnob : RoundKnob {
	OctaviaKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct OctaviaTrimpot : RoundKnob {
	OctaviaTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct OctaviaButton : app::SvgSwitch {
	OctaviaButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// Range button illuminated indicator ring
struct OctaviaRangeLightRing : ModuleLightWidget {
	OctaviaRangeLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));
		addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff)); // Very Slow (Warm Gold)
		addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // LFO (Teal)
		addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff)); // VCO (Magenta)
	}

	void drawBackground(const DrawArgs& args) override {
		Vec c = box.size.div(2.f);
		float r = mm2px(4.12f);
		float w = mm2px(1.40f);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, nvgRGBA(0x22, 0x22, 0x22, 0x99));
		nvgStrokeWidth(args.vg, w);
		nvgStroke(args.vg);
	}

	void drawLight(const DrawArgs& args) override {
		if (color.a <= 0.0f) return;
		Vec c = box.size.div(2.f);
		float r = mm2px(4.12f);
		float w = mm2px(1.40f);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, color);
		nvgStrokeWidth(args.vg, w);
		nvgStroke(args.vg);
	}

	void drawHalo(const DrawArgs& args) override {
		if (color.a <= 0.0f) return;
		Vec c = box.size.div(2.f);
		float r = mm2px(4.12f);
		float haloW = mm2px(1.00f);
		NVGcolor haloColor = color;
		haloColor.a *= 0.175f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, haloColor);
		nvgStrokeWidth(args.vg, mm2px(1.40f) + haloW);
		nvgStroke(args.vg);
	}
};

struct OctaviaWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Octavia::SkinTheme currentTheme = Octavia::THEME_MEDIUM;

	void setTheme(Octavia::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Octavia::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Octavia_Light.svg");
					break;
				case Octavia::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Octavia_Dark.svg");
					break;
				case Octavia::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Octavia.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		Octavia* module = dynamic_cast<Octavia*>(this->module);
		if (module && module->skinTheme != currentTheme) {
			setTheme(module->skinTheme);
		}
	}

	void appendContextMenu(ui::Menu* menu) override {
		Octavia* module = dynamic_cast<Octavia*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Voltage Range"));

		static const char* rangeLabels[] = {
			"0V to 1V",
			"±1V",
			"0V to 5V",
			"±5V (Default)",
			"0V to 10V",
			"±10V"
		};

		for (int i = 0; i < Octavia::VOLTAGE_RANGE_LEN; ++i) {
			Octavia::VoltageRange vr = (Octavia::VoltageRange)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->voltageRange == vr; },
				[=]() { module->voltageRange = vr; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));

		const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; ++i) {
			Octavia::SkinTheme theme = (Octavia::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return module->skinTheme == theme; },
				[=]() {
					module->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	OctaviaWidget(Octavia* module) {
		setModule(module);
		panel = new SvgPanel();
		Octavia::SkinTheme initTheme = module ? module->skinTheme : Octavia::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Octavia::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Octavia_Light.svg");
				break;
			case Octavia::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Octavia_Dark.svg");
				break;
			case Octavia::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Octavia.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Columns: col0 = 7.00mm, col1 = 20.32mm, col2 = 33.64mm
		float col0 = 7.00f;
		float col1 = 20.32f;
		float col2 = 33.64f;

		// Row 1 Controls at Y = 17.50mm
		float ctrlY = 17.50f;
		addParam(createParamCentered<OctaviaKnob>(mm2px(Vec(col0, ctrlY)), module, Octavia::FREQ_PARAM));
		addChild(createLightCentered<OctaviaRangeLightRing>(mm2px(Vec(col1, ctrlY)), module, Octavia::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<OctaviaButton>(mm2px(Vec(col1, ctrlY)), module, Octavia::RANGE_PARAM));
		addParam(createParamCentered<OctaviaTrimpot>(mm2px(Vec(col2, ctrlY)), module, Octavia::FINE_PARAM));

		// 8 Jack Rows (Y = 26.00mm down to 117.00mm, pitch 13.00mm)
		float pitch = 13.00f;
		float bottomY = 117.00f;
		float jackRows[8];
		for (int i = 0; i < 8; ++i) {
			jackRows[i] = bottomY - (7 - i) * pitch;
		}

		for (int r = 0; r < 8; ++r) {
			float jy = jackRows[r];
			int baseIdx = r * 3;
			addOutput(createOutputCentered<OctaviaPort>(mm2px(Vec(col0, jy)), module, Octavia::SINE_0_OUTPUT + baseIdx + 0));
			addOutput(createOutputCentered<OctaviaPort>(mm2px(Vec(col1, jy)), module, Octavia::SINE_0_OUTPUT + baseIdx + 1));
			addOutput(createOutputCentered<OctaviaPort>(mm2px(Vec(col2, jy)), module, Octavia::SINE_0_OUTPUT + baseIdx + 2));
		}
	}
};

Model* modelOctavia = createModel<Octavia, OctaviaWidget>("Octavia");
