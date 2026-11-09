#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <atomic>
#include <array>
#include <string>

// ─────────────────────────────────────────────────────────────────────
//  Hecate — 3-Band Envelope Follower & Audio Crossover CV Generator (16 HP, v1.0.0)
//  Utility class module providing real-time 3-band Linkwitz-Riley / SVF
//  audio crossover splitting, independent full-wave rectified envelope
//  followers with attack/decay smoothing, bipolar offset attenuverters,
//  and an authentic multi-tint visual display with dynamic crossover
//  boundary dividers (Violet / Teal / Orangey-Yellow).
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

// 2nd-Order State Variable Filter for clean crossover splitting
struct CrossoverSVF {
	float s1 = 0.0f;
	float s2 = 0.0f;

	void reset() {
		s1 = 0.0f;
		s2 = 0.0f;
	}

	void process(float in, float fc, float sampleRate, float& lp, float& hp, float& bp) {
		float f = clampf(fc, 10.0f, sampleRate * 0.45f);
		float g = std::tan((float)M_PI * f / sampleRate);
		float k = 1.41421356f; // Q = 0.7071 Butterworth
		float a1 = 1.0f / (1.0f + g * (g + k));
		float a2 = g * a1;
		float a3 = g * a2;

		float v0 = in;
		float v1 = a1 * s1 + a2 * (v0 - s2);
		float v2 = s2 + a2 * s1 + a3 * (v0 - s2);
		s1 = 2.0f * v1 - s1;
		s2 = 2.0f * v2 - s2;

		lp = v2;
		bp = v1;
		hp = v0 - k * v1 - v2;
	}
};

} // namespace

struct Hecate : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT = 1,
		THEME_DARK = 2
	};

	enum ParamId {
		TIME_PARAM,
		LOW_MID_PARAM,
		MID_HIGH_PARAM,

		LOW_AMP_PARAM,
		MID_AMP_PARAM,
		HIGH_AMP_PARAM,

		IN_TRIM_PARAM,
		LOW_SMOOTH_PARAM,
		MID_SMOOTH_PARAM,
		HIGH_SMOOTH_PARAM,

		LOW_MID_CV_PARAM,
		LOW_AMP_CV_PARAM,
		MID_AMP_CV_PARAM,
		HIGH_AMP_CV_PARAM,

		MID_HIGH_CV_PARAM,
		LOW_SMOOTH_CV_PARAM,
		MID_SMOOTH_CV_PARAM,
		HIGH_SMOOTH_CV_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		AUDIO_INPUT,
		LOW_AMP_CV_INPUT,
		MID_AMP_CV_INPUT,
		HIGH_AMP_CV_INPUT,

		LOW_MID_CV_INPUT,
		LOW_SMOOTH_CV_INPUT,
		MID_SMOOTH_CV_INPUT,
		HIGH_SMOOTH_CV_INPUT,

		MID_HIGH_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		LOW_OUTPUT,
		MID_OUTPUT,
		HIGH_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	SkinTheme skinTheme = THEME_MEDIUM;

	// DSP Crossover filters (2 stages)
	CrossoverSVF filterLowMid;
	CrossoverSVF filterMidHigh;

	// Smoothed envelope followers for the 3 bands
	float envFollower[3] = {0.0f, 0.0f, 0.0f};

	// Display ring buffer for lock-free, real-time safe visualizer rendering
	static constexpr int BUFFER_SIZE = 2048;
	struct DisplayPoint {
		float inAudio;
		float bandAudio[3];
		float bandEnv[3];
		float bandAmp[3];
	};

	std::array<DisplayPoint, BUFFER_SIZE> displayBuffer;
	std::atomic<int> bufferWriteHead{0};
	int decimationCounter = 0;

	// Dynamic live crossover points for the display widget
	std::atomic<float> displayFreq1{300.0f};
	std::atomic<float> displayFreq2{3000.0f};

	Hecate() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Knobs
		configParam(TIME_PARAM, 0.0f, 1.0f, 0.5f, "Display time");
		configParam(LOW_MID_PARAM, 40.0f, 2000.0f, 300.0f, "Low/mid point", " Hz");
		configParam(MID_HIGH_PARAM, 300.0f, 12000.0f, 3000.0f, "Mid/high point", " Hz");

		configParam(LOW_AMP_PARAM, -2.0f, 2.0f, 1.0f, "Low amplitude");
		configParam(MID_AMP_PARAM, -2.0f, 2.0f, 1.0f, "Mid amplitude");
		configParam(HIGH_AMP_PARAM, -2.0f, 2.0f, 1.0f, "High amplitude");

		configParam(IN_TRIM_PARAM, 0.0f, 2.0f, 1.0f, "Audio level trim");
		configParam(LOW_SMOOTH_PARAM, 0.0f, 1.0f, 0.3f, "Low smoothing");
		configParam(MID_SMOOTH_PARAM, 0.0f, 1.0f, 0.3f, "Mid smoothing");
		configParam(HIGH_SMOOTH_PARAM, 0.0f, 1.0f, 0.3f, "High smoothing");

		// Attenuverter trimpots strictly following '<Parameter> CV depth'
		configParam(LOW_MID_CV_PARAM, -1.0f, 1.0f, 0.0f, "Low/mid point CV depth");
		configParam(LOW_AMP_CV_PARAM, -1.0f, 1.0f, 0.0f, "Low amplitude CV depth");
		configParam(MID_AMP_CV_PARAM, -1.0f, 1.0f, 0.0f, "Mid amplitude CV depth");
		configParam(HIGH_AMP_CV_PARAM, -1.0f, 1.0f, 0.0f, "High amplitude CV depth");

		configParam(MID_HIGH_CV_PARAM, -1.0f, 1.0f, 0.0f, "Mid/high point CV depth");
		configParam(LOW_SMOOTH_CV_PARAM, -1.0f, 1.0f, 0.0f, "Low smoothing CV depth");
		configParam(MID_SMOOTH_CV_PARAM, -1.0f, 1.0f, 0.0f, "Mid smoothing CV depth");
		configParam(HIGH_SMOOTH_CV_PARAM, -1.0f, 1.0f, 0.0f, "High smoothing CV depth");

		// Ports strictly avoiding redundant 'input' / 'output' / 'in' / 'out'
		configInput(AUDIO_INPUT, "Audio");
		configInput(LOW_AMP_CV_INPUT, "Low amplitude CV");
		configInput(MID_AMP_CV_INPUT, "Mid amplitude CV");
		configInput(HIGH_AMP_CV_INPUT, "High amplitude CV");

		configInput(LOW_MID_CV_INPUT, "Low/mid point CV");
		configInput(LOW_SMOOTH_CV_INPUT, "Low smoothing CV");
		configInput(MID_SMOOTH_CV_INPUT, "Mid smoothing CV");
		configInput(HIGH_SMOOTH_CV_INPUT, "High smoothing CV");

		configInput(MID_HIGH_CV_INPUT, "Mid/high point CV");

		configOutput(LOW_OUTPUT, "Low envelope");
		configOutput(MID_OUTPUT, "Mid envelope");
		configOutput(HIGH_OUTPUT, "High envelope");

		displayBuffer.fill({0.f, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}});
	}

	void onReset() override {
		filterLowMid.reset();
		filterMidHigh.reset();
		for (int i = 0; i < 3; i++) {
			envFollower[i] = 0.0f;
		}
	}

	void process(const ProcessArgs& args) override {
		float inVoltage = inputs[AUDIO_INPUT].getVoltage();
		float inTrim = params[IN_TRIM_PARAM].getValue();
		float audio = inVoltage * inTrim;

		// Crossover frequency calculations with 1V/Oct CV modulation
		float baseFreq1 = params[LOW_MID_PARAM].getValue();
		float cv1 = inputs[LOW_MID_CV_INPUT].getVoltage() * params[LOW_MID_CV_PARAM].getValue();
		float f1 = baseFreq1 * std::pow(2.0f, cv1);
		f1 = clampf(f1, 20.0f, 4000.0f);

		float baseFreq2 = params[MID_HIGH_PARAM].getValue();
		float cv2 = inputs[MID_HIGH_CV_INPUT].getVoltage() * params[MID_HIGH_CV_PARAM].getValue();
		float f2 = baseFreq2 * std::pow(2.0f, cv2);
		f2 = clampf(f2, f1 + 20.0f, 16000.0f);

		displayFreq1.store(f1, std::memory_order_relaxed);
		displayFreq2.store(f2, std::memory_order_relaxed);

		// 3-Band Crossover Filtering
		// Stage 1: split audio into Low (< f1) and High-Pass (> f1)
		float lowAudio = 0.0f, hp1 = 0.0f, bp1 = 0.0f;
		filterLowMid.process(audio, f1, args.sampleRate, lowAudio, hp1, bp1);

		// Stage 2: split hp1 into Mid (< f2) and High (> f2)
		float midAudio = 0.0f, highAudio = 0.0f, bp2 = 0.0f;
		filterMidHigh.process(hp1, f2, args.sampleRate, midAudio, highAudio, bp2);

		float bandAudio[3] = {lowAudio, midAudio, highAudio};

		// Amplitude attenuverters (voltage offset attenuverter)
		float ampParams[3] = {
			params[LOW_AMP_PARAM].getValue(),
			params[MID_AMP_PARAM].getValue(),
			params[HIGH_AMP_PARAM].getValue()
		};
		float ampCv[3] = {
			inputs[LOW_AMP_CV_INPUT].getVoltage() * params[LOW_AMP_CV_PARAM].getValue(),
			inputs[MID_AMP_CV_INPUT].getVoltage() * params[MID_AMP_CV_PARAM].getValue(),
			inputs[HIGH_AMP_CV_INPUT].getVoltage() * params[HIGH_AMP_CV_PARAM].getValue()
		};
		float effAmp[3];
		for (int i = 0; i < 3; i++) {
			effAmp[i] = clampf(ampParams[i] + ampCv[i] * 0.2f, -2.0f, 2.0f);
		}

		// Smoothing parameters & envelope follower calculations
		float smoothParams[3] = {
			params[LOW_SMOOTH_PARAM].getValue(),
			params[MID_SMOOTH_PARAM].getValue(),
			params[HIGH_SMOOTH_PARAM].getValue()
		};
		float smoothCv[3] = {
			inputs[LOW_SMOOTH_CV_INPUT].getVoltage() * params[LOW_SMOOTH_CV_PARAM].getValue(),
			inputs[MID_SMOOTH_CV_INPUT].getVoltage() * params[MID_SMOOTH_CV_PARAM].getValue(),
			inputs[HIGH_SMOOTH_CV_INPUT].getVoltage() * params[HIGH_SMOOTH_CV_PARAM].getValue()
		};

		float outVoltage[3];
		for (int i = 0; i < 3; i++) {
			float sm = clampf(smoothParams[i] + smoothCv[i] * 0.1f, 0.0f, 1.0f);
			// Exponential tau from 1 ms to 1000 ms
			float tau = 0.001f * std::pow(1000.0f, sm);
			float alpha = 1.0f - std::exp(-1.0f / (tau * args.sampleRate));

			float rect = std::abs(bandAudio[i]);
			envFollower[i] += alpha * (rect - envFollower[i]);

			// Output CV scaled by amplitude attenuverter (modular +/- 10V)
			outVoltage[i] = clampf(envFollower[i] * effAmp[i], -10.0f, 10.0f);
		}

		outputs[LOW_OUTPUT].setVoltage(outVoltage[0]);
		outputs[MID_OUTPUT].setVoltage(outVoltage[1]);
		outputs[HIGH_OUTPUT].setVoltage(outVoltage[2]);

		// Circular Display Buffer Recording (Decimated according to TIME_PARAM)
		float timeParam = params[TIME_PARAM].getValue();
		// Zoom scale: from decimate = 1 (fastest timescale) to decimate = 64 (slowest timescale)
		int targetDecimate = (int)(1.0f + 63.0f * timeParam);
		if (++decimationCounter >= targetDecimate) {
			decimationCounter = 0;
			int head = bufferWriteHead.load(std::memory_order_relaxed);
			DisplayPoint pt;
			pt.inAudio = audio;
			for (int i = 0; i < 3; i++) {
				pt.bandAudio[i] = bandAudio[i];
				pt.bandEnv[i] = envFollower[i];
				pt.bandAmp[i] = effAmp[i];
			}
			displayBuffer[head] = pt;
			bufferWriteHead.store((head + 1) % BUFFER_SIZE, std::memory_order_release);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);
	}
};

// ── Custom Hardware Widgets ──
struct HecatePort : app::SvgPort {
	HecatePort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};

struct HecateKnob : RoundKnob {
	HecateKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct HecateTrimpot : RoundKnob {
	HecateTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

// ── Hecate 3-Band Multi-Tint Waveform & Tracking Envelope Display ──
struct HecateDisplayWidget : widget::TransparentWidget {
	Hecate* module = nullptr;

	HecateDisplayWidget(Hecate* m) : module(m) {
		// Box width 58.500 mm, height 26.000 mm
		box.size = mm2px(Vec(58.500f, 26.000f));
	}

	void draw(const DrawArgs& args) override {
		float w = box.size.x;
		float h = box.size.y;
		float yMid = h / 2.0f;

		nvgSave(args.vg);
		nvgScissor(args.vg, 0.f, 0.f, w, h);

		// 1. Dark Screen Background
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, w, h, mm2px(1.5f));
		nvgFillColor(args.vg, nvgRGBA(0x12, 0x12, 0x16, 0xff));
		nvgFill(args.vg);

		// 2. Dashed Horizontal Center Line (Baseline)
		float dashLength = 4.0f;
		float dashGap = 3.0f;
		nvgStrokeColor(args.vg, nvgRGBA(0x60, 0x64, 0x70, 0x90));
		nvgStrokeWidth(args.vg, 1.0f);
		for (float x = 2.0f; x < w - 2.0f; x += (dashLength + dashGap)) {
			float x2 = std::min(x + dashLength, w - 2.0f);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, x, yMid);
			nvgLineTo(args.vg, x2, yMid);
			nvgStroke(args.vg);
		}

		// Compute dynamic crossover boundary positions
		float f1 = 300.0f;
		float f2 = 3000.0f;
		if (module) {
			f1 = module->displayFreq1.load(std::memory_order_relaxed);
			f2 = module->displayFreq2.load(std::memory_order_relaxed);
		}

		// Normalized logarithmic mapping for the two vertical divider lines
		float p1 = clampf((std::log(f1 / 40.0f) / std::log(2000.0f / 40.0f)), 0.05f, 0.95f);
		float p2 = clampf((std::log(f2 / 300.0f) / std::log(12000.0f / 300.0f)), 0.05f, 0.95f);

		float xDiv1 = (0.12f + 0.36f * p1) * w;
		float xDiv2 = (0.52f + 0.36f * p2) * w;
		if (xDiv1 > xDiv2 - 8.0f) xDiv1 = xDiv2 - 8.0f;

		// 3. Retrieve Ring Buffer Snapshot for Display Rendering
		int writeHead = module ? module->bufferWriteHead.load(std::memory_order_acquire) : 0;
		int numPts = (int)w;
		if (numPts > Hecate::BUFFER_SIZE) numPts = Hecate::BUFFER_SIZE;
		if (numPts < 20) numPts = 20;

		// Colors for the 3 Bands:
		// Low Band: Violet
		NVGcolor colLowFill = nvgRGBA(165, 80, 240, 75);
		NVGcolor colLowEnv = nvgRGBA(205, 125, 255, 245);
		NVGcolor colLowGlow = nvgRGBA(180, 95, 250, 60);

		// Mid Band: Teal
		NVGcolor colMidFill = nvgRGBA(26, 195, 165, 75);
		NVGcolor colMidEnv = nvgRGBA(70, 245, 215, 245);
		NVGcolor colMidGlow = nvgRGBA(40, 215, 185, 60);

		// High Band: Orangey-Yellow
		NVGcolor colHighFill = nvgRGBA(255, 170, 25, 75);
		NVGcolor colHighEnv = nvgRGBA(255, 210, 65, 245);
		NVGcolor colHighGlow = nvgRGBA(255, 185, 40, 60);

		// Render the 3 Bands
		struct BandInfo {
			float xStart;
			float xEnd;
			int bandIdx;
			NVGcolor fillCol;
			NVGcolor envCol;
			NVGcolor glowCol;
		};

		BandInfo bands[3] = {
			{0.0f, xDiv1, 0, colLowFill, colLowEnv, colLowGlow},
			{xDiv1, xDiv2, 1, colMidFill, colMidEnv, colMidGlow},
			{xDiv2, w, 2, colHighFill, colHighEnv, colHighGlow}
		};

		float maxAudioAmp = 10.0f;
		float maxScaleY = (yMid - 3.0f);

		for (int b = 0; b < 3; b++) {
			float xs = bands[b].xStart;
			float xe = bands[b].xEnd;
			int bIdx = bands[b].bandIdx;

			if (xe <= xs + 1.0f) continue;

			// A. Translucent Filled Band-Passed Audio Waveform
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, xs, yMid);

			for (float curX = xs; curX <= xe; curX += 1.0f) {
				float frac = curX / w;
				int bufIdx = (writeHead - numPts + (int)(frac * numPts) + Hecate::BUFFER_SIZE) % Hecate::BUFFER_SIZE;
				float audioVal = 0.0f;
				float ampVal = 1.0f;
				if (module) {
					audioVal = module->displayBuffer[bufIdx].bandAudio[bIdx];
					ampVal = module->displayBuffer[bufIdx].bandAmp[bIdx];
				} else {
					// Demonstration sine waves if in browser/preview
					float t = curX * 0.15f * (bIdx + 1);
					audioVal = std::sin(t) * 4.0f;
				}

				// If inverted (amp < 0), display below center line
				float yOffset = (audioVal / maxAudioAmp) * maxScaleY * (std::abs(ampVal) * 0.5f);
				if (ampVal < 0.0f) {
					yOffset = -std::abs(yOffset);
				}
				yOffset = clampf(yOffset, -maxScaleY, maxScaleY);

				nvgLineTo(args.vg, curX, yMid - yOffset);
			}

			nvgLineTo(args.vg, xe, yMid);
			nvgClosePath(args.vg);
			nvgFillColor(args.vg, bands[b].fillCol);
			nvgFill(args.vg);

			// B. Solid Tracking Envelope Follower Curve (with soft glow aura)
			// Draw Glow
			nvgBeginPath(args.vg);
			bool firstPt = true;
			for (float curX = xs; curX <= xe; curX += 1.5f) {
				float frac = curX / w;
				int bufIdx = (writeHead - numPts + (int)(frac * numPts) + Hecate::BUFFER_SIZE) % Hecate::BUFFER_SIZE;
				float envVal = 0.0f;
				float ampVal = 1.0f;
				if (module) {
					envVal = module->displayBuffer[bufIdx].bandEnv[bIdx];
					ampVal = module->displayBuffer[bufIdx].bandAmp[bIdx];
				} else {
					envVal = 3.5f;
				}

				float envYOffset = (envVal / maxAudioAmp) * maxScaleY * std::abs(ampVal);
				if (ampVal < 0.0f) {
					envYOffset = -std::abs(envYOffset);
				}
				envYOffset = clampf(envYOffset, -maxScaleY, maxScaleY);
				float ptY = yMid - envYOffset;

				if (firstPt) {
					nvgMoveTo(args.vg, curX, ptY);
					firstPt = false;
				} else {
					nvgLineTo(args.vg, curX, ptY);
				}
			}
			nvgStrokeColor(args.vg, bands[b].glowCol);
			nvgStrokeWidth(args.vg, 3.5f);
			nvgStroke(args.vg);

			// Draw Solid Tracking Curve
			nvgStrokeColor(args.vg, bands[b].envCol);
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStroke(args.vg);
		}

		// 4. Two Crisp Vertical Divider Lines separating the 3 bands
		// Line 1: Low / Mid divider
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, xDiv1, 1.0f);
		nvgLineTo(args.vg, xDiv1, h - 1.0f);
		nvgStrokeColor(args.vg, nvgRGBA(0xec, 0xf0, 0xf1, 0xd0));
		nvgStrokeWidth(args.vg, 1.1f);
		nvgStroke(args.vg);

		// Line 2: Mid / High divider
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, xDiv2, 1.0f);
		nvgLineTo(args.vg, xDiv2, h - 1.0f);
		nvgStrokeColor(args.vg, nvgRGBA(0xec, 0xf0, 0xf1, 0xd0));
		nvgStrokeWidth(args.vg, 1.1f);
		nvgStroke(args.vg);

		// 5. Bezel Inner Subtle Shadow & Border
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, mm2px(1.5f));
		nvgStrokeColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0x80));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);

		nvgRestore(args.vg);
	}
};

struct HecateWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Hecate::SkinTheme currentTheme = Hecate::THEME_MEDIUM;

	void setTheme(Hecate::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Hecate::THEME_LIGHT:  path = asset::plugin(pluginInstance, "res/Hecate_Light.svg"); break;
				case Hecate::THEME_DARK:   path = asset::plugin(pluginInstance, "res/Hecate_Dark.svg");  break;
				case Hecate::THEME_MEDIUM:
				default:                   path = asset::plugin(pluginInstance, "res/Hecate.svg");        break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Hecate*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(Menu* menu) override {
		auto* mod = dynamic_cast<Hecate*>(module);
		if (!mod) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));
		const char* skinLabels[] = {"Medium (Default)", "Light", "Dark"};
		for (int i = 0; i < 3; i++) {
			Hecate::SkinTheme theme = (Hecate::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}

	HecateWidget(Hecate* module) {
		setModule(module);

		panel = new SvgPanel();
		panel->box.size = box.size;
		setTheme(Hecate::THEME_MEDIUM);
		setPanel(panel);

		// Exact 4 Column Centers (Margins 7.000 mm, Pitch 22.427 mm)
		const float col_x[4] = {7.000f, 29.427f, 51.853f, 74.280f};

		// ── 3-Band Display Widget (X = 18.000 mm, Y = 13.500 mm) ──
		auto* display = new HecateDisplayWidget(module);
		display->box.pos = mm2px(Vec(18.000f, 13.500f));
		addChild(display);

		// ── Column 0 Knobs beside display ──
		// TIME Knob at Y = 18.000 mm
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[0], 18.000f)), module, Hecate::TIME_PARAM));
		// LOW/MID Crossover Knob at Y = 34.000 mm
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[0], 34.000f)), module, Hecate::LOW_MID_PARAM));

		// ── Primary Knob Row 1 at Y = 50.000 mm ──
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[0], 50.000f)), module, Hecate::MID_HIGH_PARAM));
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[1], 50.000f)), module, Hecate::LOW_AMP_PARAM));
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[2], 50.000f)), module, Hecate::MID_AMP_PARAM));
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[3], 50.000f)), module, Hecate::HIGH_AMP_PARAM));

		// ── Primary Knob Row 2 at Y = 65.000 mm ──
		// Col 0: Audio Input Trim Trimpot
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[0], 65.000f)), module, Hecate::IN_TRIM_PARAM));
		// Cols 1, 2, 3: SMOOTH Knobs
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[1], 65.000f)), module, Hecate::LOW_SMOOTH_PARAM));
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[2], 65.000f)), module, Hecate::MID_SMOOTH_PARAM));
		addParam(createParamCentered<HecateKnob>(mm2px(Vec(col_x[3], 65.000f)), module, Hecate::HIGH_SMOOTH_PARAM));

		// ── CV Attenuverter Trimpots Row 1 at Y = 76.000 mm ──
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[0], 76.000f)), module, Hecate::LOW_MID_CV_PARAM));
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[1], 76.000f)), module, Hecate::LOW_AMP_CV_PARAM));
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[2], 76.000f)), module, Hecate::MID_AMP_CV_PARAM));
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[3], 76.000f)), module, Hecate::HIGH_AMP_CV_PARAM));

		// ── CV Attenuverter Trimpots Row 2 at Y = 86.000 mm ──
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[0], 86.000f)), module, Hecate::MID_HIGH_CV_PARAM));
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[1], 86.000f)), module, Hecate::LOW_SMOOTH_CV_PARAM));
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[2], 86.000f)), module, Hecate::MID_SMOOTH_CV_PARAM));
		addParam(createParamCentered<HecateTrimpot>(mm2px(Vec(col_x[3], 86.000f)), module, Hecate::HIGH_SMOOTH_CV_PARAM));

		// ── Jack Row 1 at Y = 96.000 mm ──
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[0], 96.000f)), module, Hecate::AUDIO_INPUT));
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[1], 96.000f)), module, Hecate::LOW_AMP_CV_INPUT));
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[2], 96.000f)), module, Hecate::MID_AMP_CV_INPUT));
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[3], 96.000f)), module, Hecate::HIGH_AMP_CV_INPUT));

		// ── Jack Row 2 at Y = 106.500 mm ──
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[0], 106.500f)), module, Hecate::LOW_MID_CV_INPUT));
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[1], 106.500f)), module, Hecate::LOW_SMOOTH_CV_INPUT));
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[2], 106.500f)), module, Hecate::MID_SMOOTH_CV_INPUT));
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[3], 106.500f)), module, Hecate::HIGH_SMOOTH_CV_INPUT));

		// ── Jack Row 3 at Y = 117.000 mm (Bottom Outputs & M/H CV) ──
		addInput(createInputCentered<HecatePort>(mm2px(Vec(col_x[0], 117.000f)), module, Hecate::MID_HIGH_CV_INPUT));
		addOutput(createOutputCentered<HecatePort>(mm2px(Vec(col_x[1], 117.000f)), module, Hecate::LOW_OUTPUT));
		addOutput(createOutputCentered<HecatePort>(mm2px(Vec(col_x[2], 117.000f)), module, Hecate::MID_OUTPUT));
		addOutput(createOutputCentered<HecatePort>(mm2px(Vec(col_x[3], 117.000f)), module, Hecate::HIGH_OUTPUT));
	}
};

Model* modelHecate = createModel<Hecate, HecateWidget>("Hecate");
