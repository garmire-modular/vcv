#include "plugin.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <GL/glew.h>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <GLFW/glfw3.h>

// ─────────────────────────────────────────────────────────────────────
//  SERQET — Dual-Preview CRT Oscilloscope & Video Crossfader (28 HP)
//  Layout:
//   Top: Side-by-side Dual CRT Scopes (Cue Monitor left, Main Out right)
//   Mid: Cue buttons, status labels, and full-width Horizontal Xfade Slider
//   Bottom: 11 columns with Ch 1 / Ch 2 inputs, Xfade CV, Attenuverters, Outputs
// ─────────────────────────────────────────────────────────────────────

namespace SerqetConstants {
	static constexpr size_t BUFFER_SIZE = 16384;
}

struct SerqetSample {
	float x = 0.0f;
	float y = 0.0f;
	float z = 1.0f;
	float r = 0.0f;
	float g = 1.0f;
	float b = 0.0f;
};

// Circular ring buffer for audio/video trajectory streams
struct SerqetRingBuffer {
	SerqetSample buffer[SerqetConstants::BUFFER_SIZE];
	uint32_t writeHead = 0;

	inline void write(float x, float y, float z, float r, float g, float b) {
		uint32_t idx = writeHead & (SerqetConstants::BUFFER_SIZE - 1);
		buffer[idx].x = x;
		buffer[idx].y = y;
		buffer[idx].z = z;
		buffer[idx].r = r;
		buffer[idx].g = g;
		buffer[idx].b = b;
		writeHead++;
	}

	inline uint32_t getHead() const {
		return writeHead;
	}

	inline SerqetSample readOffset(uint32_t head, size_t offsetBack) const {
		uint32_t idx = (head - 1 - offsetBack) & (SerqetConstants::BUFFER_SIZE - 1);
		return buffer[idx];
	}
};

struct Serqet : Module {
	enum ParamIds {
		// CRT Emulation Controls (Attenuverter Knobs)
		GRATICULE_CUE_PARAM,
		GRATICULE_MAIN_PARAM,
		FOCUS_PARAM,
		BEAM_PARAM,
		DECAY_PARAM,
		BLOOM_PARAM,

		// Crossfade & Cue
		CROSSFADE_PARAM,
		CUE_CH1_PARAM,
		CUE_CH2_PARAM,
		BUMPER_CH1_PARAM,
		BUMPER_CH2_PARAM,
		PARAMS_LEN
	};

	enum InputIds {
		// Row 1 (Y = 104 mm): Ch 1 (X, Y, R, G, B)
		CH1_X_INPUT,
		CH1_Y_INPUT,
		CH1_R_INPUT,
		CH1_G_INPUT,
		CH1_B_INPUT,

		// Row 2 (Y = 117 mm): Ch 2 (X, Y, R, G, B) & Xfade CV
		CH2_X_INPUT,
		CH2_Y_INPUT,
		CH2_R_INPUT,
		CH2_G_INPUT,
		CH2_B_INPUT,
		XFADE_CV_INPUT,
		INPUTS_LEN
	};

	enum OutputIds {
		// Row 2 (Y = 117 mm): Master Outputs
		X_OUTPUT,
		Y_OUTPUT,
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightIds {
		LIGHTS_LEN
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT = 1,
		THEME_DARK = 2
	};

	enum XfadeCurve {
		XFADE_LINEAR = 0,
		XFADE_SMOOTHSTEP = 1,
		XFADE_CONSTANT_POWER = 2,
		XFADE_CURVES_COUNT = 3
	};

	enum GraticuleColor {
		GRAT_COLOR_GREEN = 0,
		GRAT_COLOR_AMBER = 1,
		GRAT_COLOR_NAVY = 2,
		GRAT_COLOR_RED = 3,
		GRAT_COLOR_WHITE = 4,
		GRAT_COLORS_COUNT = 5
	};

	SkinTheme skinTheme = THEME_MEDIUM;
	XfadeCurve xfadeCurve = XFADE_LINEAR;
	GraticuleColor cueGraticuleColor = GRAT_COLOR_AMBER;
	GraticuleColor mainGraticuleColor = GRAT_COLOR_GREEN;

	// Cue selection: 0 = Ch 1, 1 = Ch 2
	int cueSource = 0;
	dsp::BooleanTrigger ch1BtnTrigger;
	dsp::BooleanTrigger ch2BtnTrigger;
	dsp::BooleanTrigger bumperCh1Trigger;
	dsp::BooleanTrigger bumperCh2Trigger;
	std::atomic<float> effectiveAlpha{0.0f};

	// Dual ring buffers for CRT displays
	SerqetRingBuffer mainBuffer;
	SerqetRingBuffer cueBuffer;

	Serqet() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// CRT Emulation Attenuverter Knobs
		configParam(GRATICULE_CUE_PARAM, 0.0f, 1.0f, 0.25f, "Cue graticule opacity", "%", 0.0f, 100.0f);
		configParam(GRATICULE_MAIN_PARAM, 0.0f, 1.0f, 0.0f, "Main graticule opacity", "%", 0.0f, 100.0f);
		// Focus: min = current 40%, max = current 200%, default = current 50%
		configParam(FOCUS_PARAM, 0.0f, 1.0f, 0.50f, "Focus", "%", 0.0f, 100.0f);
		// Beam Intensity: default 50%
		configParam(BEAM_PARAM, 0.0f, 1.0f, 0.50f, "Beam intensity", "%", 0.0f, 100.0f);
		// Phosphor Decay: non-linear tanh afterglow, max 75%, default 5%
		configParam(DECAY_PARAM, 0.0f, 1.0f, 0.05f, "Phosphor decay", "%", 0.0f, 100.0f);
		// Optical Bloom: dual-scale halation and glass scatter, default 25%
		configParam(BLOOM_PARAM, 0.0f, 1.0f, 0.25f, "Bloom", "%", 0.0f, 100.0f);

		// Horizontal Crossfade Slider, Cue Buttons & Bumper Buttons
		configParam(CROSSFADE_PARAM, 0.0f, 1.0f, 0.0f, "Crossfade", "%", 0.0f, 100.0f);
		configButton(CUE_CH1_PARAM, "Cue channel 1");
		configButton(CUE_CH2_PARAM, "Cue channel 2");
		configButton(BUMPER_CH1_PARAM, "Channel 1 crossfader bumper");
		configButton(BUMPER_CH2_PARAM, "Channel 2 crossfader bumper");

		// Inputs
		configInput(CH1_X_INPUT, "Ch 1 X");
		configInput(CH1_Y_INPUT, "Ch 1 Y");
		configInput(CH1_R_INPUT, "Ch 1 red");
		configInput(CH1_G_INPUT, "Ch 1 green");
		configInput(CH1_B_INPUT, "Ch 1 blue");

		configInput(CH2_X_INPUT, "Ch 2 X");
		configInput(CH2_Y_INPUT, "Ch 2 Y");
		configInput(CH2_R_INPUT, "Ch 2 red");
		configInput(CH2_G_INPUT, "Ch 2 green");
		configInput(CH2_B_INPUT, "Ch 2 blue");

		configInput(XFADE_CV_INPUT, "Crossfade CV");

		// Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(R_OUTPUT, "Red");
		configOutput(G_OUTPUT, "Green");
		configOutput(B_OUTPUT, "Blue");
	}

	void onReset() override {
		cueSource = 0;
		xfadeCurve = XFADE_LINEAR;
		skinTheme = THEME_MEDIUM;
		cueGraticuleColor = GRAT_COLOR_AMBER;
		mainGraticuleColor = GRAT_COLOR_GREEN;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "xfadeCurve", json_integer((int)xfadeCurve));
		json_object_set_new(rootJ, "cueSource", json_integer(cueSource));
		json_object_set_new(rootJ, "cueGraticuleColor", json_integer((int)cueGraticuleColor));
		json_object_set_new(rootJ, "mainGraticuleColor", json_integer((int)mainGraticuleColor));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* themeJ = json_object_get(rootJ, "skinTheme");
		if (!themeJ) themeJ = json_object_get(rootJ, "theme");
		if (themeJ) skinTheme = (SkinTheme)json_integer_value(themeJ);

		json_t* curveJ = json_object_get(rootJ, "xfadeCurve");
		if (curveJ) xfadeCurve = (XfadeCurve)json_integer_value(curveJ);

		json_t* cueJ = json_object_get(rootJ, "cueSource");
		if (cueJ) cueSource = json_integer_value(cueJ);

		json_t* cueColJ = json_object_get(rootJ, "cueGraticuleColor");
		if (cueColJ) cueGraticuleColor = (GraticuleColor)json_integer_value(cueColJ);

		json_t* mainColJ = json_object_get(rootJ, "mainGraticuleColor");
		if (mainColJ) mainGraticuleColor = (GraticuleColor)json_integer_value(mainColJ);
	}

	inline float computeCrossfadeRatio(float rawAlpha) const {
		float a = clamp(rawAlpha, 0.0f, 1.0f);
		switch (xfadeCurve) {
			case XFADE_SMOOTHSTEP:
				return a * a * (3.0f - 2.0f * a);
			case XFADE_CONSTANT_POWER:
				return std::sin(a * 1.57079632679f);
			case XFADE_LINEAR:
			default:
				return a;
		}
	}

	void process(const ProcessArgs& args) override {
		// 1. Process Cue Mode Buttons
		if (ch1BtnTrigger.process(params[CUE_CH1_PARAM].getValue() > 0.5f)) {
			cueSource = 0;
		}
		if (ch2BtnTrigger.process(params[CUE_CH2_PARAM].getValue() > 0.5f)) {
			cueSource = 1;
		}

		// 1b. Process Bumper Buttons to immediately snap crossfader
		if (bumperCh1Trigger.process(params[BUMPER_CH1_PARAM].getValue() > 0.5f)) {
			params[CROSSFADE_PARAM].setValue(0.0f);
		}
		if (bumperCh2Trigger.process(params[BUMPER_CH2_PARAM].getValue() > 0.5f)) {
			params[CROSSFADE_PARAM].setValue(1.0f);
		}

		// 2. Crossfade alpha
		// Fader value 0.0 = Left (Ch 1), value 1.0 = Right (Ch 2)
		float fader = params[CROSSFADE_PARAM].getValue();
		if (inputs[XFADE_CV_INPUT].isConnected()) {
			fader += inputs[XFADE_CV_INPUT].getVoltage() * 0.1f; // Positive CV pushes towards Ch 2 (right)
		}
		float alpha = clamp(fader, 0.0f, 1.0f); // 0.0 = Ch 1, 1.0 = Ch 2
		effectiveAlpha.store(alpha, std::memory_order_relaxed);
		float mixGain2 = computeCrossfadeRatio(alpha);
		float mixGain1 = (xfadeCurve == XFADE_CONSTANT_POWER) ? std::cos(alpha * 1.57079632679f) : (1.0f - mixGain2);

		// Polyphony handling
		int channels = std::max({
			inputs[CH1_X_INPUT].getChannels(), inputs[CH1_Y_INPUT].getChannels(),
			inputs[CH2_X_INPUT].getChannels(), inputs[CH2_Y_INPUT].getChannels(), 1
		});

		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
		outputs[R_OUTPUT].setChannels(channels);
		outputs[G_OUTPUT].setChannels(channels);
		outputs[B_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			float x1 = inputs[CH1_X_INPUT].getPolyVoltage(c);
			float y1 = inputs[CH1_Y_INPUT].getPolyVoltage(c);
			float r1 = inputs[CH1_R_INPUT].isConnected() ? inputs[CH1_R_INPUT].getPolyVoltage(c) : 0.0f;
			float g1 = inputs[CH1_G_INPUT].isConnected() ? inputs[CH1_G_INPUT].getPolyVoltage(c) : 10.0f;
			float b1 = inputs[CH1_B_INPUT].isConnected() ? inputs[CH1_B_INPUT].getPolyVoltage(c) : 0.0f;

			float x2 = inputs[CH2_X_INPUT].getPolyVoltage(c);
			float y2 = inputs[CH2_Y_INPUT].getPolyVoltage(c);
			float r2 = inputs[CH2_R_INPUT].isConnected() ? inputs[CH2_R_INPUT].getPolyVoltage(c) : 10.0f;
			float g2 = inputs[CH2_G_INPUT].isConnected() ? inputs[CH2_G_INPUT].getPolyVoltage(c) : 0.5f;
			float b2 = inputs[CH2_B_INPUT].isConnected() ? inputs[CH2_B_INPUT].getPolyVoltage(c) : 0.0f;

			// Crossfaded Main Output
			float mx = x1 * mixGain1 + x2 * mixGain2;
			float my = y1 * mixGain1 + y2 * mixGain2;
			float mr = r1 * mixGain1 + r2 * mixGain2;
			float mg = g1 * mixGain1 + g2 * mixGain2;
			float mb = b1 * mixGain1 + b2 * mixGain2;

			outputs[X_OUTPUT].setVoltage(mx, c);
			outputs[Y_OUTPUT].setVoltage(my, c);
			outputs[R_OUTPUT].setVoltage(mr, c);
			outputs[G_OUTPUT].setVoltage(mg, c);
			outputs[B_OUTPUT].setVoltage(mb, c);

			// Write poly channel 0 to visual oscilloscopes
			if (c == 0) {
				// Normalize +/-5V to +/-1.0 coordinate space for oscilloscope display
				mainBuffer.write(mx * 0.2f, my * 0.2f, 1.0f, mr * 0.1f, mg * 0.1f, mb * 0.1f);

				if (cueSource == 0) {
					cueBuffer.write(x1 * 0.2f, y1 * 0.2f, 1.0f, r1 * 0.1f, g1 * 0.1f, b1 * 0.1f);
				} else {
					cueBuffer.write(x2 * 0.2f, y2 * 0.2f, 1.0f, r2 * 0.1f, g2 * 0.1f, b2 * 0.1f);
				}
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────
//  SerqetCrtWidget — Authentic Real-Time CRT Oscilloscope Display
// ─────────────────────────────────────────────────────────────────────

// Fast tone mapping and tanh lookup tables
struct SerqetLut {
	static constexpr int TONE_LUT_SIZE = 2048;
	static constexpr int TANH_LUT_SIZE = 1024;
	float toneLut[TONE_LUT_SIZE];
	float tanhLut[TANH_LUT_SIZE];

	SerqetLut() {
		for (int i = 0; i < TONE_LUT_SIZE; i++) {
			float x = (float)i * (16.0f / (float)(TONE_LUT_SIZE - 1));
			toneLut[i] = 1.0f - std::exp(-0.18f * x);
		}
		for (int i = 0; i < TANH_LUT_SIZE; i++) {
			float x = (float)i * (10.0f / (float)(TANH_LUT_SIZE - 1));
			tanhLut[i] = std::tanh(x);
		}
	}

	inline float fastTone(float x) const {
		if (x <= 0.0f) return 0.0f;
		float idxF = x * ((float)(TONE_LUT_SIZE - 1) / 16.0f);
		int idx = (int)idxF;
		if (idx >= TONE_LUT_SIZE - 1) return 1.0f - std::exp(-0.18f * x);
		float t = idxF - (float)idx;
		return (1.0f - t) * toneLut[idx] + t * toneLut[idx + 1];
	}

	inline float fastTanh(float x) const {
		if (x <= 0.0f) return 0.0f;
		if (x >= 10.0f) return 1.0f;
		float idxF = x * ((float)(TANH_LUT_SIZE - 1) / 10.0f);
		int idx = (int)idxF;
		float t = idxF - (float)idx;
		return (1.0f - t) * tanhLut[idx] + t * tanhLut[idx + 1];
	}
};

static const SerqetLut g_serqetLut;

struct SerqetCrtWidget : widget::TransparentWidget {
	Serqet* module = nullptr;
	bool isMainOut = true; // true = Main Out, false = Cue

	static constexpr int CRT_RES = 512;
	static constexpr int BLUR_RES_1 = 256;
	static constexpr int BLUR_RES_2 = 128;

	// Multi-frame phosphor persistence accumulation buffer
	std::vector<float> accumR;
	std::vector<float> accumG;
	std::vector<float> accumB;

	// Dual-kernel bloom buffers
	std::vector<float> tightR, tightG, tightB, tightTmp;
	std::vector<float> scatterR, scatterG, scatterB, scatterTmp;

	// 32-bit RGBA output image buffer
	std::vector<uint32_t> outputRgba;

	// Precomputed vignette factor per coordinate
	std::vector<float> vignetteTable;

	GLuint glTexId = 0;
	int nvgImageId = -1;
	NVGcontext* boundVg = nullptr;

	std::chrono::steady_clock::time_point lastRenderTime;
	bool hasRenderedFirstFrame = false;

	SerqetCrtWidget(Serqet* m, bool mainOut) : module(m), isMainOut(mainOut) {
		box.size = mm2px(Vec(66.0f, 66.0f));

		accumR.assign(CRT_RES * CRT_RES, 0.0f);
		accumG.assign(CRT_RES * CRT_RES, 0.0f);
		accumB.assign(CRT_RES * CRT_RES, 0.0f);

		tightR.assign(BLUR_RES_1 * BLUR_RES_1, 0.0f);
		tightG.assign(BLUR_RES_1 * BLUR_RES_1, 0.0f);
		tightB.assign(BLUR_RES_1 * BLUR_RES_1, 0.0f);
		tightTmp.assign(BLUR_RES_1 * BLUR_RES_1, 0.0f);

		scatterR.assign(BLUR_RES_2 * BLUR_RES_2, 0.0f);
		scatterG.assign(BLUR_RES_2 * BLUR_RES_2, 0.0f);
		scatterB.assign(BLUR_RES_2 * BLUR_RES_2, 0.0f);
		scatterTmp.assign(BLUR_RES_2 * BLUR_RES_2, 0.0f);

		outputRgba.assign(CRT_RES * CRT_RES, 0xff0a1208); // Dark obsidian green glass

		vignetteTable.resize(CRT_RES * CRT_RES);
		for (int y = 0; y < CRT_RES; y++) {
			float dy_v = ((float)y - 256.0f) / 256.0f;
			for (int x = 0; x < CRT_RES; x++) {
				float dx_v = ((float)x - 256.0f) / 256.0f;
				float r2 = dx_v * dx_v + dy_v * dy_v;
				vignetteTable[y * CRT_RES + x] = std::max(0.0f, std::min(1.0f, 1.0f - 0.22f * r2));
			}
		}

		// Interleave initial timestamps so Cue Monitor and Main Out rasterize on alternating frames
		lastRenderTime = std::chrono::steady_clock::now() - (mainOut ? std::chrono::milliseconds(16) : std::chrono::milliseconds(0));
	}

	~SerqetCrtWidget() {
		if (glTexId != 0) {
			if (glfwGetCurrentContext()) {
				glDeleteTextures(1, &glTexId);
			}
			glTexId = 0;
		}
		nvgImageId = -1;
		boundVg = nullptr;
	}

	static inline float sampleBilinear(const float* buf, int w, int h, float fx, float fy) {
		fx = clamp(fx, 0.0f, (float)(w - 1));
		fy = clamp(fy, 0.0f, (float)(h - 1));
		int x0 = (int)fx;
		int y0 = (int)fy;
		int x1 = std::min(w - 1, x0 + 1);
		int y1 = std::min(h - 1, y0 + 1);
		float tx = fx - (float)x0;
		float ty = fy - (float)y0;
		float top = (1.0f - tx) * buf[y0 * w + x0] + tx * buf[y0 * w + x1];
		float bot = (1.0f - tx) * buf[y1 * w + x0] + tx * buf[y1 * w + x1];
		return (1.0f - ty) * top + ty * bot;
	}

	static void boxBlurH(const float* src, float* dst, int w, int h, int r) {
		float scale = 1.0f / (float)(2 * r + 1);
		for (int y = 0; y < h; y++) {
			int row = y * w;
			float val = src[row] * (float)r;
			for (int i = 0; i <= r; i++) {
				val += src[row + std::min(w - 1, i)];
			}
			for (int x = 0; x < w; x++) {
				dst[row + x] = val * scale;
				val += src[row + std::min(w - 1, x + r + 1)] - src[row + std::max(0, x - r)];
			}
		}
	}

	static void boxBlurV(const float* src, float* dst, int w, int h, int r) {
		float scale = 1.0f / (float)(2 * r + 1);
		for (int x = 0; x < w; x++) {
			float val = src[x] * (float)r;
			for (int i = 0; i <= r; i++) {
				val += src[std::min(h - 1, i) * w + x];
			}
			for (int y = 0; y < h; y++) {
				dst[y * w + x] = val * scale;
				val += src[std::min(h - 1, y + r + 1) * w + x] - src[std::max(0, y - r) * w + x];
			}
		}
	}

	static void gaussianBlur(float* buf, float* tmp, int w, int h, int r) {
		// 3 passes of 1D sliding-window box blur mathematically converges to exact Gaussian (Central Limit Theorem)
		for (int p = 0; p < 3; p++) {
			boxBlurH(buf, tmp, w, h, r);
			boxBlurV(tmp, buf, w, h, r);
		}
	}

	static void downsample2x(float* dst, const float* src, int w, int h) {
		int dstW = w / 2;
		int dstH = h / 2;
		for (int y = 0; y < dstH; y++) {
			int sy0 = (y * 2) * w;
			int sy1 = (y * 2 + 1) * w;
			int dy = y * dstW;
			for (int x = 0; x < dstW; x++) {
				int sx0 = x * 2;
				dst[dy + x] = 0.25f * (src[sy0 + sx0] + src[sy0 + sx0 + 1] +
				                       src[sy1 + sx0] + src[sy1 + sx0 + 1]);
			}
		}
	}

	static inline float erfApprox(float x) {
		float s = (x >= 0.0f) ? 1.0f : -1.0f;
		float a = std::abs(x);
		float t = 1.0f + (0.278393f + (0.230389f + 0.078108f * (a * a)) * a) * a;
		t *= t;
		return s * (1.0f - 1.0f / (t * t));
	}

	void renderCrtSimulation(float focusNorm, float beamNorm, float decayNorm, float bloomNorm) {
		// 1. Phosphor Afterglow Decay (tanh persistence law from sosci afterglow_decay.frag)
		float effectiveDecay = decayNorm * 0.75f;
		// Smooth persistence: at decay 5%, fadeAmount ~ 0.45 per frame for smooth frame blending
		float fadeAmount = 0.50f * std::pow(1.0f - effectiveDecay, 2.2f) + 0.015f;
		float adjustedAfterglow = (0.25f + effectiveDecay * 3.0f) * 1.5f;
		float minFade = 0.08f * (1.0f - clamp(adjustedAfterglow / 10.0f, 0.0f, 1.0f));
		float invAfterglow = 1.0f / adjustedAfterglow;

		for (int i = 0; i < CRT_RES * CRT_RES; i++) {
			float r = accumR[i];
			float g = accumG[i];
			float b = accumB[i];
			float lum = std::max({r, g, b});
			if (lum <= 1e-4f) {
				accumR[i] = 0.0f;
				accumG[i] = 0.0f;
				accumB[i] = 0.0f;
				continue;
			}
			float x = std::min(lum * invAfterglow, 10.0f);
			float fade = fadeAmount * ((1.0f - minFade) * g_serqetLut.fastTanh(x) + minFade);
			fade = clamp(fade, 0.0f, fadeAmount);
			float keep = std::max(0.0f, 1.0f - fade);
			accumR[i] = r * keep;
			accumG[i] = g * keep;
			accumB[i] = b * keep;
		}

		// 2. Electron Beam Spot & Intensity Setup (Analytical erf Gaussian Integration)
		// Calibrated Focus Range:
		// knob 0% (min) -> current 40% (sigma = 0.58 px)
		// knob 50% (default) -> current 50% (sigma = 0.625 px)
		// knob 100% (max) -> current 200% (sigma = 1.30 px)
		float currentPercent = (focusNorm <= 0.5f)
			? (0.40f + focusNorm * 0.20f)
			: (0.50f + (focusNorm - 0.50f) * 3.0f);
		float sigma = 0.40f + currentPercent * 0.45f;
		float inv2Sigma2 = 1.0f / (2.0f * sigma * sigma);
		float sqrt2Sigma = 1.41421356f * sigma;

		// Calibrated Beam Intensity: 25% (at 0% knob) to 150% (at 100% knob)
		// At beamNorm = 0 (25% default), baseIntensity = 0.25 * 65.0 = 16.25 (delicate, pristine color preservation)
		float beamIntensity = 0.25f + beamNorm * 1.25f;
		float baseIntensity = beamIntensity * 65.0f;

		// 3. Trajectory Fetch & Analytical Rasterization
		if (module) {
			const SerqetRingBuffer& rb = isMainOut ? module->mainBuffer : module->cueBuffer;
			uint32_t head = rb.getHead();

			bool ch1XConn = module->inputs[Serqet::CH1_X_INPUT].isConnected();
			bool ch1YConn = module->inputs[Serqet::CH1_Y_INPUT].isConnected();
			bool ch2XConn = module->inputs[Serqet::CH2_X_INPUT].isConnected();
			bool ch2YConn = module->inputs[Serqet::CH2_Y_INPUT].isConnected();

			bool isXConnected = isMainOut ? (ch1XConn || ch2XConn) : (module->cueSource == 0 ? ch1XConn : ch2XConn);
			bool isYConnected = isMainOut ? (ch1YConn || ch2YConn) : (module->cueSource == 0 ? ch1YConn : ch2YConn);

			float sampleRate = (APP && APP->engine) ? APP->engine->getSampleRate() : 44100.0f;
			if (sampleRate <= 0.0f) sampleRate = 44100.0f;
			// Reverse-engineered sosci window duration: 0.042666666666666665s (~42.67ms)
			size_t windowSamples = (size_t)std::round(sampleRate * 0.042666666666666665f);
			windowSamples = std::max((size_t)128, std::min(windowSamples, (size_t)SerqetConstants::BUFFER_SIZE));

			// Trigger stabilization in 1D time-domain mode (only Y connected)
			size_t triggerOffset = 0;
			if (!isXConnected && isYConnected) {
				size_t searchLimit = windowSamples / 2;
				for (size_t k = windowSamples - 1; k > searchLimit; k--) {
					float yPrev = rb.readOffset(head, k).y;
					float yCurr = rb.readOffset(head, k - 1).y;
					if (yPrev <= 0.0f && yCurr > 0.0f) {
						triggerOffset = windowSamples - 1 - k;
						break;
					}
				}
			}

			size_t effectiveWindow = windowSamples - triggerOffset;
			if (effectiveWindow < 128) effectiveWindow = 128;

			static constexpr size_t NUM_POINTS = 512;
			Vec pts[NUM_POINTS];
			float sampleR[NUM_POINTS];
			float sampleG[NUM_POINTS];
			float sampleB[NUM_POINTS];

			const float cx = CRT_RES * 0.5f;
			const float cy = CRT_RES * 0.5f;
			const float scaleFactor = cx * 0.88f;

			for (size_t i = 0; i < NUM_POINTS; i++) {
				size_t offsetBack = triggerOffset + effectiveWindow - 1 - (size_t)((i * (effectiveWindow - 1)) / (NUM_POINTS - 1));
				SerqetSample s = rb.readOffset(head, offsetBack);
				float px = cx;
				float py = cy;

				if (isXConnected && isYConnected) {
					// True XY Lissajous Vector Mode
					px = cx + clamp(s.x, -1.15f, 1.15f) * scaleFactor;
					py = cy - clamp(s.y, -1.15f, 1.15f) * scaleFactor;
				} else if (!isXConnected && isYConnected) {
					// Auto Time-Domain Sweep along X
					float sweepNorm = -1.0f + 2.0f * ((float)i / (NUM_POINTS - 1));
					px = cx + sweepNorm * scaleFactor;
					py = cy - clamp(s.y, -1.15f, 1.15f) * scaleFactor;
				} else if (isXConnected && !isYConnected) {
					px = cx + clamp(s.x, -1.15f, 1.15f) * scaleFactor;
					py = cy;
				} else {
					// Idle: electron beam rests at center
					px = cx;
					py = cy;
				}

				pts[i] = Vec(px, py);

				float r = clamp(s.r, 0.0f, 1.0f);
				float g = clamp(s.g, 0.0f, 1.0f);
				float b = clamp(s.b, 0.0f, 1.0f);

				// Fallback to CRT phosphor green or warm amber if inputs inactive
				if (r + g + b < 0.05f) {
					if (isMainOut) {
						g = 1.0f;
					} else {
						if (module->cueSource == 0) { g = 1.0f; }
						else { r = 1.0f; g = 0.55f; }
					}
				}

				sampleR[i] = r;
				sampleG[i] = g;
				sampleB[i] = b;
			}

			// Analytical Gaussian Beam Segment Integration (Exact sosci formulation)
			for (size_t i = 0; i < NUM_POINTS - 1; i++) {
				Vec p0 = pts[i];
				Vec p1 = pts[i + 1];
				float dx = p1.x - p0.x;
				float dy = p1.y - p0.y;
				float L = std::sqrt(dx * dx + dy * dy);

				// Temporal fade along the sweep
				float intensityScale = (float)i / (float)(NUM_POINTS - 1);
				float intensityFade = 0.70f + 0.30f * intensityScale;

				// Energy conservation per unit segment length (sosci safeLen)
				float safeLen = std::max(L, 2.0f * sigma);
				float segEnergy = baseIntensity * intensityFade;

				float r = sampleR[i];
				float g = sampleG[i];
				float b = sampleB[i];

				if (L < 1e-4f) {
					// Point spot (stationary beam)
					float invSqrt2PiSigma = 1.0f / (2.50662827f * sigma);
					int spotR = std::max(1, (int)std::ceil(3.0f * sigma));
					int minX = std::max(0, (int)std::floor(p0.x - spotR));
					int maxX = std::min(CRT_RES - 1, (int)std::ceil(p0.x + spotR));
					int minY = std::max(0, (int)std::floor(p0.y - spotR));
					int maxY = std::min(CRT_RES - 1, (int)std::ceil(p0.y + spotR));
					for (int iy = minY; iy <= maxY; iy++) {
						float distY = (float)iy - p0.y;
						float dy2 = distY * distY;
						int row = iy * CRT_RES;
						for (int ix = minX; ix <= maxX; ix++) {
							float distX = (float)ix - p0.x;
							float d2 = dy2 + distX * distX;
							float w = std::exp(-d2 * inv2Sigma2) * invSqrt2PiSigma;
							float e = segEnergy * w;
							accumR[row + ix] += e * r;
							accumG[row + ix] += e * g;
							accumB[row + ix] += e * b;
						}
					}
				} else {
					// Exact analytical erf Gaussian segment integral from sosci
					float tx = dx / L;
					float ty = dy / L;
					float nx = -ty;
					float ny = tx;

					float pad = 3.0f * sigma;
					int minX = std::max(0, (int)std::floor(std::min(p0.x, p1.x) - pad));
					int maxX = std::min(CRT_RES - 1, (int)std::ceil(std::max(p0.x, p1.x) + pad));
					int minY = std::max(0, (int)std::floor(std::min(p0.y, p1.y) - pad));
					int maxY = std::min(CRT_RES - 1, (int)std::ceil(std::max(p0.y, p1.y) + pad));

					for (int iy = minY; iy <= maxY; iy++) {
						float py = (float)iy - p0.y;
						int row = iy * CRT_RES;
						for (int ix = minX; ix <= maxX; ix++) {
							float px = (float)ix - p0.x;
							float u = px * tx + py * ty;
							float v = px * nx + py * ny;

							if (std::abs(v) > pad || u < -pad || u > L + pad) continue;

							float erfTerm = erfApprox(u / sqrt2Sigma) - erfApprox((u - L) / sqrt2Sigma);
							float wPerp = std::exp(-v * v * inv2Sigma2);
							float brightness = 0.5f * erfTerm * wPerp / safeLen;
							float e = segEnergy * brightness;

							accumR[row + ix] += e * r;
							accumG[row + ix] += e * g;
							accumB[row + ix] += e * b;
						}
					}
				}
			}
		}

		// 4. Dual-Kernel Separable Bloom (Tight excited halo + wide glass scatter)
		// Tight bloom at 256x256 (2x downsample from 512x512)
		downsample2x(tightR.data(), accumR.data(), CRT_RES, CRT_RES);
		downsample2x(tightG.data(), accumG.data(), CRT_RES, CRT_RES);
		downsample2x(tightB.data(), accumB.data(), CRT_RES, CRT_RES);

		gaussianBlur(tightR.data(), tightTmp.data(), BLUR_RES_1, BLUR_RES_1, 2);
		gaussianBlur(tightG.data(), tightTmp.data(), BLUR_RES_1, BLUR_RES_1, 2);
		gaussianBlur(tightB.data(), tightTmp.data(), BLUR_RES_1, BLUR_RES_1, 2);

		// Wide scatter at 128x128 (2x downsample from 256x256)
		downsample2x(scatterR.data(), tightR.data(), BLUR_RES_1, BLUR_RES_1);
		downsample2x(scatterG.data(), tightG.data(), BLUR_RES_1, BLUR_RES_1);
		downsample2x(scatterB.data(), tightB.data(), BLUR_RES_1, BLUR_RES_1);

		gaussianBlur(scatterR.data(), scatterTmp.data(), BLUR_RES_2, BLUR_RES_2, 5);
		gaussianBlur(scatterG.data(), scatterTmp.data(), BLUR_RES_2, BLUR_RES_2, 5);
		gaussianBlur(scatterB.data(), scatterTmp.data(), BLUR_RES_2, BLUR_RES_2, 5);

		// 5. Tone Mapping, Exponential Saturation & Overexposure Whitening (sosci composite)
		float glow = 1.75f * std::pow(bloomNorm, 1.5f);
		float scaleTight = (float)(BLUR_RES_1 - 1) / (float)(CRT_RES - 1);
		float scaleScatter = (float)(BLUR_RES_2 - 1) / (float)(CRT_RES - 1);

		float uOverexposure = 0.5f * beamNorm;

		for (int y = 0; y < CRT_RES; y++) {
			float fyTight = (float)y * scaleTight;
			float fyScatter = (float)y * scaleScatter;
			int row = y * CRT_RES;

			for (int x = 0; x < CRT_RES; x++) {
				float fxTight = (float)x * scaleTight;
				float fxScatter = (float)x * scaleScatter;
				int idx = row + x;

				float rLine = accumR[idx];
				float gLine = accumG[idx];
				float bLine = accumB[idx];

				float rTight = sampleBilinear(tightR.data(), BLUR_RES_1, BLUR_RES_1, fxTight, fyTight);
				float gTight = sampleBilinear(tightG.data(), BLUR_RES_1, BLUR_RES_1, fxTight, fyTight);
				float bTight = sampleBilinear(tightB.data(), BLUR_RES_1, BLUR_RES_1, fxTight, fyTight);

				float rScatter = sampleBilinear(scatterR.data(), BLUR_RES_2, BLUR_RES_2, fxScatter, fyScatter);
				float gScatter = sampleBilinear(scatterG.data(), BLUR_RES_2, BLUR_RES_2, fxScatter, fyScatter);
				float bScatter = sampleBilinear(scatterB.data(), BLUR_RES_2, BLUR_RES_2, fxScatter, fyScatter);

				float rBloom = glow * (0.75f * rTight + 0.60f * rScatter);
				float gBloom = glow * (0.75f * gTight + 0.60f * gScatter);
				float bBloom = glow * (0.75f * bTight + 0.60f * bScatter);

				float rLight = rLine + rBloom;
				float gLight = gLine + gBloom;
				float bLight = bLine + bBloom;

				// Exponential Tone Mapping from sosci: tlight = 1.0 - exp(-uExposure * light) via fast LUT
				float rTone = g_serqetLut.fastTone(rLight);
				float gTone = g_serqetLut.fastTone(gLight);
				float bTone = g_serqetLut.fastTone(bLight);

				float s = std::max({rTone, gTone, bTone});
				float s2 = s * s;
				float whiteMix = clamp(s2 * s2 * uOverexposure, 0.0f, 1.0f);

				float outR = rTone * (1.0f - whiteMix) + s * whiteMix;
				float outG = gTone * (1.0f - whiteMix) + s * whiteMix;
				float outB = bTone * (1.0f - whiteMix) + s * whiteMix;

				// CRT Dark Phosphorescent Obsidian Glass & Vignette (from precomputed table)
				float vignette = vignetteTable[idx];

				float crtBgR = 0.005f * vignette;
				float crtBgG = 0.009f * vignette;
				float crtBgB = 0.006f * vignette;

				uint8_t pixR = (uint8_t)clamp((outR + crtBgR) * 255.0f, 0.0f, 255.0f);
				uint8_t pixG = (uint8_t)clamp((outG + crtBgG) * 255.0f, 0.0f, 255.0f);
				uint8_t pixB = (uint8_t)clamp((outB + crtBgB) * 255.0f, 0.0f, 255.0f);

				outputRgba[idx] = ((uint32_t)pixR) | (((uint32_t)pixG) << 8) | (((uint32_t)pixB) << 16) | 0xff000000;
			}
		}
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float w = box.size.x;
		float h = box.size.y;
		float cx = w * 0.5f;
		float cy = h * 0.5f;

		// Read module parameter states
		float focusVal = module ? module->params[Serqet::FOCUS_PARAM].getValue() : 0.10f;
		float beamVal = module ? module->params[Serqet::BEAM_PARAM].getValue() : 0.00f;
		float decayVal = module ? module->params[Serqet::DECAY_PARAM].getValue() : 0.05f;
		float bloomVal = module ? module->params[Serqet::BLOOM_PARAM].getValue() : 0.30f;

		// Dual Graticule opacities
		float gratAlpha = 0.0f;
		if (module) {
			gratAlpha = isMainOut ? module->params[Serqet::GRATICULE_MAIN_PARAM].getValue()
			                      : module->params[Serqet::GRATICULE_CUE_PARAM].getValue();
		} else {
			gratAlpha = isMainOut ? 0.0f : 0.25f;
		}

		// 1. Scissor clip to the 66x66 mm CRT scope screen area with 1.5mm corner radius
		nvgIntersectScissor(args.vg, 0.0f, 0.0f, w, h);

		// 2. Interleaved 30 Hz updates per scope (~32ms) to ensure rock-solid 60 FPS UI performance without CPU spikes
		auto now = std::chrono::steady_clock::now();
		float elapsedMs = std::chrono::duration<float, std::milli>(now - lastRenderTime).count();
		bool shouldUpdate = !hasRenderedFirstFrame || (elapsedMs >= 32.0f);

		if (shouldUpdate) {
			lastRenderTime = now;
			hasRenderedFirstFrame = true;
			renderCrtSimulation(focusVal, beamVal, decayVal, bloomVal);

			// 3. Upload pixels directly to dedicated OpenGL texture
			if (glTexId == 0) {
				glGenTextures(1, &glTexId);
				glBindTexture(GL_TEXTURE_2D, glTexId);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CRT_RES, CRT_RES, 0, GL_RGBA, GL_UNSIGNED_BYTE, outputRgba.data());
				glBindTexture(GL_TEXTURE_2D, 0);
			} else {
				glBindTexture(GL_TEXTURE_2D, glTexId);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CRT_RES, CRT_RES, GL_RGBA, GL_UNSIGNED_BYTE, outputRgba.data());
				glBindTexture(GL_TEXTURE_2D, 0);
			}
		}

		// 4. Wrap dedicated OpenGL texture into NanoVG image handle (handles context changes across Rack views)
		if (glTexId != 0) {
			if (nvgImageId < 0 || boundVg != args.vg) {
				boundVg = args.vg;
				nvgImageId = nvglCreateImageFromHandleGL2(args.vg, glTexId, CRT_RES, CRT_RES, NVG_IMAGE_NODELETE);
			}
		}

		if (nvgImageId >= 0) {
			NVGpaint imgPaint = nvgImagePattern(args.vg, 0.0f, 0.0f, w, h, 0.0f, nvgImageId, 1.0f);
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, mm2px(1.5f));
			nvgFillPaint(args.vg, imgPaint);
			nvgFill(args.vg);
		}

		// 4. Draw Vector Graticule Grid (if opacity > 0)
		if (gratAlpha > 0.005f) {
			Serqet::GraticuleColor gCol = isMainOut ? (module ? module->mainGraticuleColor : Serqet::GRAT_COLOR_GREEN)
			                                       : (module ? module->cueGraticuleColor : Serqet::GRAT_COLOR_AMBER);
			float rBase = 0.00f, gBase = 0.93f, bBase = 0.40f;
			switch (gCol) {
				case Serqet::GRAT_COLOR_AMBER:
					rBase = 1.00f; gBase = 0.72f; bBase = 0.15f;
					break;
				case Serqet::GRAT_COLOR_NAVY:
					rBase = 0.22f; gBase = 0.60f; bBase = 1.00f;
					break;
				case Serqet::GRAT_COLOR_RED:
					rBase = 1.00f; gBase = 0.22f; bBase = 0.22f;
					break;
				case Serqet::GRAT_COLOR_WHITE:
					rBase = 0.95f; gBase = 0.95f; bBase = 0.95f;
					break;
				case Serqet::GRAT_COLOR_GREEN:
				default:
					rBase = 0.00f; gBase = 0.93f; bBase = 0.40f;
					break;
			}

			NVGcolor gridCol = nvgRGBAf(rBase, gBase, bBase, 0.22f * gratAlpha);
			NVGcolor crosshairCol = nvgRGBAf(rBase, gBase, bBase, 0.48f * gratAlpha);
			NVGcolor tickCol = nvgRGBAf(rBase, gBase, bBase, 0.66f * gratAlpha);

			nvgBeginPath(args.vg);
			nvgStrokeColor(args.vg, gridCol);
			nvgStrokeWidth(args.vg, 0.6f);

			for (int i = 1; i < 4; i++) {
				float offset = cx * (i / 4.0f);
				nvgMoveTo(args.vg, cx - offset, 0.0f);
				nvgLineTo(args.vg, cx - offset, h);
				nvgMoveTo(args.vg, cx + offset, 0.0f);
				nvgLineTo(args.vg, cx + offset, h);

				nvgMoveTo(args.vg, 0.0f, cy - offset);
				nvgLineTo(args.vg, w, cy - offset);
				nvgMoveTo(args.vg, 0.0f, cy + offset);
				nvgLineTo(args.vg, w, cy + offset);
			}
			nvgStroke(args.vg);

			// Major axes crosshair
			nvgBeginPath(args.vg);
			nvgStrokeColor(args.vg, crosshairCol);
			nvgStrokeWidth(args.vg, 0.8f);
			nvgMoveTo(args.vg, 0.0f, cy);
			nvgLineTo(args.vg, w, cy);
			nvgMoveTo(args.vg, cx, 0.0f);
			nvgLineTo(args.vg, cx, h);
			nvgStroke(args.vg);

			// Reticle sub-division ticks on center crosshair
			nvgBeginPath(args.vg);
			nvgStrokeColor(args.vg, tickCol);
			nvgStrokeWidth(args.vg, 0.8f);
			for (int i = 1; i <= 8; i++) {
				float d = cx * (i / 8.0f);
				nvgMoveTo(args.vg, cx - d, cy - 2.5f); nvgLineTo(args.vg, cx - d, cy + 2.5f);
				nvgMoveTo(args.vg, cx + d, cy - 2.5f); nvgLineTo(args.vg, cx + d, cy + 2.5f);
				nvgMoveTo(args.vg, cx - 2.5f, cy - d); nvgLineTo(args.vg, cx + 2.5f, cy - d);
				nvgMoveTo(args.vg, cx - 2.5f, cy + d); nvgLineTo(args.vg, cx + 2.5f, cy + d);
			}
			nvgStroke(args.vg);
		}

		// 5. CRT Glass Vignette Inner Shadow & Bezel Rim
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, mm2px(1.5f));
		nvgStrokeColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0x88));
		nvgStrokeWidth(args.vg, 1.5f);
		nvgStroke(args.vg);

		nvgRestore(args.vg);
	}
};

// ─────────────────────────────────────────────────────────────────────
//  Hardware Widgets
// ─────────────────────────────────────────────────────────────────────

struct GarmirePort : app::SvgPort {
	GarmirePort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};

struct GarmireTrimpot : RoundKnob {
	GarmireTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct SerqetButton : app::SvgSwitch {
	SerqetButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_round_4mm_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_round_4mm_1.svg")));
	}
};

struct SerqetBumperButton : app::SvgSwitch {
	SerqetBumperButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/btn_bumper_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/btn_bumper_1.svg")));
	}
};

struct SerqetFader : app::SvgSlider {
	SerqetFader() {
		horizontal = true;
		box.size = mm2px(math::Vec(108.24f, 10.00f));
		fb->box.size = box.size;
		setHandleSvg(Svg::load(asset::plugin(pluginInstance, "res/crossfader.svg")));
		float halfHandleX = handle->box.size.x / 2.0f;
		float cy = box.size.y / 2.0f;
		setHandlePosCentered(
			math::Vec(halfHandleX, cy),                  // minValue (0.0, Ch 1) at LEFT
			math::Vec(box.size.x - halfHandleX, cy)     // maxValue (1.0, Ch 2) at RIGHT
		);
	}

	void step() override {
		SvgSlider::step();
		auto* mod = dynamic_cast<Serqet*>(module);
		if (mod && handle) {
			float norm = mod->effectiveAlpha.load(std::memory_order_relaxed);
			handle->box.pos = minHandlePos.plus(maxHandlePos.minus(minHandlePos).mult(norm));
		}
	}
};

// ─────────────────────────────────────────────────────────────────────
//  SerqetWidget — Panel & UI Layout (28 HP)
// ─────────────────────────────────────────────────────────────────────

struct SerqetWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Serqet::SkinTheme currentTheme = Serqet::THEME_MEDIUM;

	void setTheme(Serqet::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Serqet::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Serqet_Light.svg");
					break;
				case Serqet::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Serqet_Dark.svg");
					break;
				case Serqet::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Serqet.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Serqet*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	void appendContextMenu(Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		auto* mod = dynamic_cast<Serqet*>(module);
		if (!mod) return;

		menu->addChild(new MenuSeparator());

		// Crossfade Curve Selection Menu
		menu->addChild(createMenuLabel("Crossfade Response"));
		const char* curveLabels[] = { "Linear", "Smoothstep", "Constant Power (Cosine)" };
		for (int i = 0; i < Serqet::XFADE_CURVES_COUNT; i++) {
			Serqet::XfadeCurve selected = (Serqet::XfadeCurve)i;
			menu->addChild(createCheckMenuItem(curveLabels[i], "",
				[=]() { return mod->xfadeCurve == selected; },
				[=]() { mod->xfadeCurve = selected; }
			));
		}

		// Cue Monitor Graticule Color
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Cue Monitor Graticule"));
		const char* gratColorNames[] = { "Green", "Amber", "Navy", "Red", "White" };
		for (int i = 0; i < Serqet::GRAT_COLORS_COUNT; i++) {
			Serqet::GraticuleColor col = (Serqet::GraticuleColor)i;
			menu->addChild(createCheckMenuItem(gratColorNames[i], "",
				[=]() { return mod->cueGraticuleColor == col; },
				[=]() { mod->cueGraticuleColor = col; }
			));
		}

		// Main Out Graticule Color
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Main Out Graticule"));
		for (int i = 0; i < Serqet::GRAT_COLORS_COUNT; i++) {
			Serqet::GraticuleColor col = (Serqet::GraticuleColor)i;
			menu->addChild(createCheckMenuItem(gratColorNames[i], "",
				[=]() { return mod->mainGraticuleColor == col; },
				[=]() { mod->mainGraticuleColor = col; }
			));
		}

		// Theme Selection Menu
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));
		const char* skinLabels[] = { "Medium (Default)", "Light", "Dark" };
		for (int t = 0; t < 3; t++) {
			Serqet::SkinTheme selectedTheme = (Serqet::SkinTheme)t;
			menu->addChild(createCheckMenuItem(skinLabels[t], "",
				[=]() { return mod->skinTheme == selectedTheme; },
				[=]() {
					mod->skinTheme = selectedTheme;
					setTheme(selectedTheme);
				}
			));
		}
	}

	SerqetWidget(Serqet* module) {
		setModule(module);
		panel = new SvgPanel();
		Serqet::SkinTheme initTheme = module ? module->skinTheme : Serqet::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string path;
		switch (initTheme) {
			case Serqet::THEME_LIGHT:
				path = asset::plugin(pluginInstance, "res/Serqet_Light.svg");
				break;
			case Serqet::THEME_DARK:
				path = asset::plugin(pluginInstance, "res/Serqet_Dark.svg");
				break;
			case Serqet::THEME_MEDIUM:
			default:
				path = asset::plugin(pluginInstance, "res/Serqet.svg");
				break;
		}
		panel->setBackground(Svg::load(path));
		box.size = panel->box.size;
		setPanel(panel);

		// ── Column Coordinates (mm) ──
		const float col_x[11] = {
			7.000f, 19.824f, 32.648f, 45.472f, 58.296f,
			71.120f,
			83.944f, 96.768f, 109.592f, 122.416f, 135.240f
		};

		// ── Dual CRT Scopes (66 x 66 mm, Y center = 43.50 mm) ──
		// Scope 1: CUE MONITOR (Center X = 37.00 mm, Center Y = 43.50 mm)
		auto* scopeCue = new SerqetCrtWidget(module, false);
		scopeCue->box.pos = mm2px(Vec(37.00f - 33.00f, 43.50f - 33.00f));
		addChild(scopeCue);

		// Scope 2: MAIN OUT (Center X = 105.24 mm, Center Y = 43.50 mm)
		auto* scopeMain = new SerqetCrtWidget(module, true);
		scopeMain->box.pos = mm2px(Vec(105.24f - 33.00f, 43.50f - 33.00f));
		addChild(scopeMain);

		// ── Cue Buttons (Center Y = 80.00 mm) ──
		// CH 1 button at Center X = 7.00 mm, CH 2 button at Center X = 67.00 mm
		addParam(createParamCentered<SerqetButton>(mm2px(Vec(7.00f, 80.00f)), module, Serqet::CUE_CH1_PARAM));
		addParam(createParamCentered<SerqetButton>(mm2px(Vec(67.00f, 80.00f)), module, Serqet::CUE_CH2_PARAM));

		// ── Horizontal Crossfader Slider (Center X = 71.12 mm, Center Y = 88.50 mm, Travel = 108.24 mm) ──
		addParam(createParamCentered<SerqetFader>(mm2px(Vec(71.12f, 88.50f)), module, Serqet::CROSSFADE_PARAM));

		// ── Crossfader Bumper Buttons (8x8mm, Center Y = 88.50 mm) ──
		// Ch 1 bumper at Center X = 9.00 mm, Ch 2 bumper at Center X = 133.24 mm
		addParam(createParamCentered<SerqetBumperButton>(mm2px(Vec(9.00f, 88.50f)), module, Serqet::BUMPER_CH1_PARAM));
		addParam(createParamCentered<SerqetBumperButton>(mm2px(Vec(133.24f, 88.50f)), module, Serqet::BUMPER_CH2_PARAM));

		// ── Row 1 (Center Y = 104.00 mm) ──
		// CH 1 Inputs (X, Y, R, G, B)
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[0], 104.00f)), module, Serqet::CH1_X_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[1], 104.00f)), module, Serqet::CH1_Y_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[2], 104.00f)), module, Serqet::CH1_R_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[3], 104.00f)), module, Serqet::CH1_G_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[4], 104.00f)), module, Serqet::CH1_B_INPUT));

		// Attenuverter Knobs
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col_x[5], 104.00f)), module, Serqet::GRATICULE_CUE_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col_x[6], 104.00f)), module, Serqet::GRATICULE_MAIN_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col_x[7], 104.00f)), module, Serqet::FOCUS_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col_x[8], 104.00f)), module, Serqet::BEAM_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col_x[9], 104.00f)), module, Serqet::DECAY_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col_x[10], 104.00f)), module, Serqet::BLOOM_PARAM));

		// ── Row 2 (Center Y = 117.00 mm) ──
		// CH 2 Inputs (X, Y, R, G, B)
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[0], 117.00f)), module, Serqet::CH2_X_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[1], 117.00f)), module, Serqet::CH2_Y_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[2], 117.00f)), module, Serqet::CH2_R_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[3], 117.00f)), module, Serqet::CH2_G_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[4], 117.00f)), module, Serqet::CH2_B_INPUT));

		// XFADE CV Input (Center X = 71.12 mm)
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col_x[5], 117.00f)), module, Serqet::XFADE_CV_INPUT));

		// Master Outputs (X, Y, R, G, B)
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col_x[6], 117.00f)), module, Serqet::X_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col_x[7], 117.00f)), module, Serqet::Y_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col_x[8], 117.00f)), module, Serqet::R_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col_x[9], 117.00f)), module, Serqet::G_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col_x[10], 117.00f)), module, Serqet::B_OUTPUT));
	}
};

Model* modelSerqet = createModel<Serqet, SerqetWidget>("Serqet");
