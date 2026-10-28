#include "plugin.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <nanovg.h>

// ─────────────────────────────────────────────────────────────────────
//  SERQET — Dual-Input Dual-Preview CRT Oscilloscope & Video Crossfader
//  Columns:
//   Col 1 (7.00 mm):  Focus / Decay / Main Graticule & Ch 1 X, Y, R, G, B inputs
//   Col 2 (20.32 mm): Beam / Bloom / Cue Graticule & Ch 2 X, Y, R, G, B inputs
//   Col 3 (50.80 mm): Dual CRT Scopes (Main Out top, Cue bottom with Ch1/Ch2 buttons)
//   Col 4 (81.28 mm): Vertical crossfader (1 -> 2) & CV jack
//   Col 5 (94.60 mm): Mixed X, Y, R, G, B outputs
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
		// CRT Emulation Controls
		FOCUS_PARAM,
		BEAM_PARAM,
		DECAY_PARAM,
		BLOOM_PARAM,
		GRATICULE_MAIN_PARAM,
		GRATICULE_CUE_PARAM,

		// Crossfade & Cue
		CROSSFADE_PARAM,
		CUE_CH1_PARAM,
		CUE_CH2_PARAM,
		PARAMS_LEN
	};

	enum InputIds {
		// Col 1: Ch 1
		CH1_X_INPUT,
		CH1_Y_INPUT,
		CH1_R_INPUT,
		CH1_G_INPUT,
		CH1_B_INPUT,

		// Col 2: Ch 2
		CH2_X_INPUT,
		CH2_Y_INPUT,
		CH2_R_INPUT,
		CH2_G_INPUT,
		CH2_B_INPUT,

		// Col 4: Fader CV
		XFADE_CV_INPUT,
		INPUTS_LEN
	};

	enum OutputIds {
		// Col 5: Outputs
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

	SkinTheme skinTheme = THEME_MEDIUM;
	XfadeCurve xfadeCurve = XFADE_LINEAR;

	// Cue selection: 0 = Ch 1, 1 = Ch 2
	int cueSource = 0;
	dsp::BooleanTrigger ch1BtnTrigger;
	dsp::BooleanTrigger ch2BtnTrigger;

	// Dual ring buffers for CRT displays
	SerqetRingBuffer mainBuffer;
	SerqetRingBuffer cueBuffer;

	Serqet() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// CRT Emulation Knobs
		// Focus: 0.25mm (0%) to 5.00mm (100%), default 20% (~1.20mm)
		configParam(FOCUS_PARAM, 0.0f, 1.0f, 0.20f, "Focus", "%", 0.0f, 100.0f);
		// Beam Intensity: 25% (0%) to 150% (100%), default 60% (100% nominal)
		configParam(BEAM_PARAM, 0.0f, 1.0f, 0.60f, "Beam intensity", "%", 0.0f, 100.0f);
		// Phosphor Decay: non-linear tanh afterglow, default 50%
		configParam(DECAY_PARAM, 0.0f, 1.0f, 0.50f, "Phosphor decay", "%", 0.0f, 100.0f);
		// Optical Bloom: dual-scale halation and glass scatter, default 50%
		configParam(BLOOM_PARAM, 0.0f, 1.0f, 0.50f, "Bloom", "%", 0.0f, 100.0f);

		// Dual Graticule Trimpots: Main Out (default 0%) and Cue Monitor (default 25%)
		configParam(GRATICULE_MAIN_PARAM, 0.0f, 1.0f, 0.0f, "Main graticule opacity", "%", 0.0f, 100.0f);
		configParam(GRATICULE_CUE_PARAM, 0.0f, 1.0f, 0.25f, "Cue graticule opacity", "%", 0.0f, 100.0f);

		// Crossfade & Cue
		configParam(CROSSFADE_PARAM, 0.0f, 1.0f, 1.0f, "Crossfade", "%", 0.0f, 100.0f);
		configButton(CUE_CH1_PARAM, "Cue channel 1");
		configButton(CUE_CH2_PARAM, "Cue channel 2");

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
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "xfadeCurve", json_integer((int)xfadeCurve));
		json_object_set_new(rootJ, "cueSource", json_integer(cueSource));
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

		// 2. Crossfade alpha
		// Fader value 1.0 = top (Ch 1), value 0.0 = bottom (Ch 2)
		float fader = params[CROSSFADE_PARAM].getValue();
		if (inputs[XFADE_CV_INPUT].isConnected()) {
			fader -= inputs[XFADE_CV_INPUT].getVoltage() * 0.1f; // Positive CV pushes towards Ch 2
		}
		float fadePos = clamp(fader, 0.0f, 1.0f);
		float alpha = 1.0f - fadePos; // 0.0 = Ch 1, 1.0 = Ch 2
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

struct SerqetCrtWidget : widget::TransparentWidget {
	Serqet* module = nullptr;
	bool isMainOut = true; // true = Main Out, false = Cue

	static constexpr int CRT_RES = 256;
	static constexpr int BLUR_RES_1 = 128;
	static constexpr int BLUR_RES_2 = 32;

	// Multi-frame phosphor persistence accumulation buffer
	std::vector<float> accumR;
	std::vector<float> accumG;
	std::vector<float> accumB;

	// Dual-kernel bloom buffers
	std::vector<float> tightR, tightG, tightB, tightTmp;
	std::vector<float> scatterR, scatterG, scatterB, scatterTmp;

	// 32-bit RGBA output image buffer
	std::vector<uint32_t> outputRgba;

	int nvgImageId = -1;
	NVGcontext* cachedVg = nullptr;

	SerqetCrtWidget(Serqet* m, bool mainOut) : module(m), isMainOut(mainOut) {
		box.size = mm2px(Vec(44.0f, 44.0f));

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
	}

	~SerqetCrtWidget() {
		if (nvgImageId >= 0 && cachedVg) {
			nvgDeleteImage(cachedVg, nvgImageId);
			nvgImageId = -1;
		}
	}

	static void blur5(float* dst, const float* src, float* tmp, int w, int h) {
		// 5-tap separable Gaussian weights: 0.06, 0.24, 0.40, 0.24, 0.06
		for (int y = 0; y < h; y++) {
			int row = y * w;
			for (int x = 0; x < w; x++) {
				int x_m2 = std::max(0, x - 2);
				int x_m1 = std::max(0, x - 1);
				int x_p1 = std::min(w - 1, x + 1);
				int x_p2 = std::min(w - 1, x + 2);
				tmp[row + x] = 0.06f * src[row + x_m2] +
				               0.24f * src[row + x_m1] +
				               0.40f * src[row + x] +
				               0.24f * src[row + x_p1] +
				               0.06f * src[row + x_p2];
			}
		}
		for (int y = 0; y < h; y++) {
			int y_m2 = std::max(0, y - 2) * w;
			int y_m1 = std::max(0, y - 1) * w;
			int y_0  = y * w;
			int y_p1 = std::min(h - 1, y + 1) * w;
			int y_p2 = std::min(h - 1, y + 2) * w;
			for (int x = 0; x < w; x++) {
				dst[y_0 + x] = 0.06f * tmp[y_m2 + x] +
				               0.24f * tmp[y_m1 + x] +
				               0.40f * tmp[y_0 + x] +
				               0.24f * tmp[y_p1 + x] +
				               0.06f * tmp[y_p2 + x];
			}
		}
	}

	static void downsample2x(float* dst, const float* src) {
		for (int y = 0; y < BLUR_RES_1; y++) {
			int sy0 = (y * 2) * CRT_RES;
			int sy1 = (y * 2 + 1) * CRT_RES;
			int dy = y * BLUR_RES_1;
			for (int x = 0; x < BLUR_RES_1; x++) {
				int sx = x * 2;
				dst[dy + x] = 0.25f * (src[sy0 + sx] + src[sy0 + sx + 1] +
				                       src[sy1 + sx] + src[sy1 + sx + 1]);
			}
		}
	}

	static void downsample4x(float* dst, const float* src) {
		for (int y = 0; y < BLUR_RES_2; y++) {
			int dy = y * BLUR_RES_2;
			for (int x = 0; x < BLUR_RES_2; x++) {
				float sum = 0.0f;
				for (int dy_sub = 0; dy_sub < 4; dy_sub++) {
					int sy = (y * 4 + dy_sub) * BLUR_RES_1;
					for (int dx_sub = 0; dx_sub < 4; dx_sub++) {
						sum += src[sy + (x * 4 + dx_sub)];
					}
				}
				dst[dy + x] = sum * 0.0625f;
			}
		}
	}

	void renderCrtSimulation(float focusNorm, float beamNorm, float decayNorm, float bloomNorm) {
		// 1. Phosphor Afterglow Decay (tanh persistence law from sosci afterglow_decay.frag)
		// decayNorm 0% = fast fade (wipe in 1-2 frames); 100% = long persistent glow (seconds)
		float fadeAmount = 0.90f * std::pow(1.0f - decayNorm, 1.8f) + 0.02f;
		float adjustedAfterglow = (0.2f + decayNorm * 3.0f) * 1.5f;
		float minFade = 0.1f * (1.0f - clamp(adjustedAfterglow / 10.0f, 0.0f, 1.0f));

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
			float x = std::min(lum / adjustedAfterglow, 10.0f);
			float fade = fadeAmount * ((1.0f - minFade) * std::tanh(x) + minFade);
			fade = clamp(fade, 0.0f, fadeAmount);
			float keep = std::max(0.0f, 1.0f - fade);
			accumR[i] = r * keep;
			accumG[i] = g * keep;
			accumB[i] = b * keep;
		}

		// 2. Electron Beam Spot & Intensity Setup
		// Calibrated Focus: 0.25 mm (0%) to 5.00 mm (100%)
		float focus_mm = 0.25f + focusNorm * (5.00f - 0.25f);
		// Pixels per mm on 44x44 mm box with 256x256 res: 256.0 / 44.0 = 5.818 px/mm
		float sigma = (focus_mm / 44.0f) * (float)CRT_RES * 0.26f;
		sigma = std::max(0.40f, sigma);
		float inv2Sigma2 = 1.0f / (2.0f * sigma * sigma);
		float normFactor = 1.0f / (2.0f * 3.14159265f * sigma * sigma);

		// Calibrated Beam: 25% (0%) to 150% (100%)
		float beamIntensity = 0.25f + beamNorm * (1.50f - 0.25f);
		float baseIntensity = beamIntensity * 2.2f;

		// 3. Trajectory Fetch & Rasterization
		if (module) {
			const SerqetRingBuffer& rb = isMainOut ? module->mainBuffer : module->cueBuffer;
			uint32_t head = rb.getHead();

			bool ch1XConn = module->inputs[Serqet::CH1_X_INPUT].isConnected();
			bool ch1YConn = module->inputs[Serqet::CH1_Y_INPUT].isConnected();
			bool ch2XConn = module->inputs[Serqet::CH2_X_INPUT].isConnected();
			bool ch2YConn = module->inputs[Serqet::CH2_Y_INPUT].isConnected();

			bool isXConnected = isMainOut ? (ch1XConn || ch2XConn) : (module->cueSource == 0 ? ch1XConn : ch2XConn);
			bool isYConnected = isMainOut ? (ch1YConn || ch2YConn) : (module->cueSource == 0 ? ch1YConn : ch2YConn);

			static constexpr size_t NUM_POINTS = 512;
			Vec pts[NUM_POINTS];
			float sampleR[NUM_POINTS];
			float sampleG[NUM_POINTS];
			float sampleB[NUM_POINTS];

			const float cx = CRT_RES * 0.5f;
			const float cy = CRT_RES * 0.5f;
			const float scaleFactor = cx * 0.88f;

			for (size_t i = 0; i < NUM_POINTS; i++) {
				SerqetSample s = rb.readOffset(head, NUM_POINTS - 1 - i);
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

			// Rasterize segments with 1/L velocity-dependent energy deposition
			int spotRadius = std::min(7, std::max(1, (int)std::ceil(2.2f * sigma)));

			for (size_t i = 0; i < NUM_POINTS - 1; i++) {
				Vec p0 = pts[i];
				Vec p1 = pts[i + 1];
				float dx = p1.x - p0.x;
				float dy = p1.y - p0.y;
				float segLen = std::sqrt(dx * dx + dy * dy);

				// Velocity-dependent scaling factor
				float velFactor = 1.0f / std::max(segLen, 1.2f);
				int steps = std::max(1, (int)std::ceil(segLen / 0.75f));
				float stepDt = 1.0f / (float)steps;
				float stepEnergy = baseIntensity * velFactor * (segLen > 0.001f ? (segLen / (float)steps) : 1.0f);

				float r = sampleR[i];
				float g = sampleG[i];
				float b = sampleB[i];

				for (int s = 0; s <= steps; s++) {
					float t = (float)s * stepDt;
					float px = p0.x + t * dx;
					float py = p0.y + t * dy;

					int minX = std::max(0, (int)std::floor(px - spotRadius));
					int maxX = std::min(CRT_RES - 1, (int)std::ceil(px + spotRadius));
					int minY = std::max(0, (int)std::floor(py - spotRadius));
					int maxY = std::min(CRT_RES - 1, (int)std::ceil(py + spotRadius));

					for (int iy = minY; iy <= maxY; iy++) {
						float distY = (float)iy - py;
						float dy2 = distY * distY;
						int row = iy * CRT_RES;
						for (int ix = minX; ix <= maxX; ix++) {
							float distX = (float)ix - px;
							float d2 = dy2 + distX * distX;
							float w = std::exp(-d2 * inv2Sigma2) * normFactor;
							float e = stepEnergy * w;
							accumR[row + ix] += e * r;
							accumG[row + ix] += e * g;
							accumB[row + ix] += e * b;
						}
					}
				}
			}
		}

		// 4. Dual-Kernel Separable Bloom (Tight excited halo + wide glass scatter)
		downsample2x(tightR.data(), accumR.data());
		downsample2x(tightG.data(), accumG.data());
		downsample2x(tightB.data(), accumB.data());

		blur5(tightR.data(), tightR.data(), tightTmp.data(), BLUR_RES_1, BLUR_RES_1);
		blur5(tightG.data(), tightG.data(), tightTmp.data(), BLUR_RES_1, BLUR_RES_1);
		blur5(tightB.data(), tightB.data(), tightTmp.data(), BLUR_RES_1, BLUR_RES_1);

		downsample4x(scatterR.data(), tightR.data());
		downsample4x(scatterG.data(), tightG.data());
		downsample4x(scatterB.data(), tightB.data());

		blur5(scatterR.data(), scatterR.data(), scatterTmp.data(), BLUR_RES_2, BLUR_RES_2);
		blur5(scatterG.data(), scatterG.data(), scatterTmp.data(), BLUR_RES_2, BLUR_RES_2);
		blur5(scatterB.data(), scatterB.data(), scatterTmp.data(), BLUR_RES_2, BLUR_RES_2);

		// 5. Tone Mapping, Exponential Saturation & Overexposure Whitening
		float glow = 1.75f * std::pow(bloomNorm * 1.5f, 1.5f);

		for (int i = 0; i < CRT_RES * CRT_RES; i++) {
			int y = i / CRT_RES;
			int x = i % CRT_RES;

			float rLine = accumR[i];
			float gLine = accumG[i];
			float bLine = accumB[i];

			float rTight = tightR[(y >> 1) * BLUR_RES_1 + (x >> 1)];
			float gTight = tightG[(y >> 1) * BLUR_RES_1 + (x >> 1)];
			float bTight = tightB[(y >> 1) * BLUR_RES_1 + (x >> 1)];

			float rScatter = scatterR[(y >> 3) * BLUR_RES_2 + (x >> 3)];
			float gScatter = scatterG[(y >> 3) * BLUR_RES_2 + (x >> 3)];
			float bScatter = scatterB[(y >> 3) * BLUR_RES_2 + (x >> 3)];

			float rBloom = glow * (0.75f * rTight + 0.60f * rScatter);
			float gBloom = glow * (0.75f * gTight + 0.60f * gScatter);
			float bBloom = glow * (0.75f * bTight + 0.60f * bScatter);

			float rLight = rLine + rBloom;
			float gLight = gLine + gBloom;
			float bLight = bLine + bBloom;

			// Exponential Tone Mapping
			float rTone = 1.0f - std::exp(-1.4f * rLight);
			float gTone = 1.0f - std::exp(-1.4f * gLight);
			float bTone = 1.0f - std::exp(-1.4f * bLight);

			// Overexposure Core Whitening (sosci crt_composite.frag)
			float s = std::max({rTone, gTone, bTone});
			if (s > 1e-5f) {
				float invS = 1.0f / s;
				float baseR = rTone * invS;
				float baseG = gTone * invS;
				float baseB = bTone * invS;
				float s3 = s * s * s;
				float whiteMix = clamp(0.20f + s3 * 1.6f, 0.0f, 1.0f);
				rTone = (baseR * (1.0f - whiteMix) + whiteMix) * s;
				gTone = (baseG * (1.0f - whiteMix) + whiteMix) * s;
				bTone = (baseB * (1.0f - whiteMix) + whiteMix) * s;
			}

			// CRT Dark Phosphorescent Obsidian Glass & Vignette
			float dx = (x - 128.0f) / 128.0f;
			float dy = (y - 128.0f) / 128.0f;
			float r2 = dx * dx + dy * dy;
			float vignette = clamp(1.0f - 0.22f * r2, 0.0f, 1.0f);

			float crtBgR = 0.015f * vignette;
			float crtBgG = 0.035f * vignette;
			float crtBgB = 0.022f * vignette;

			uint8_t outR = (uint8_t)clamp((rTone + crtBgR) * 255.0f, 0.0f, 255.0f);
			uint8_t outG = (uint8_t)clamp((gTone + crtBgG) * 255.0f, 0.0f, 255.0f);
			uint8_t outB = (uint8_t)clamp((bTone + crtBgB) * 255.0f, 0.0f, 255.0f);

			outputRgba[i] = ((uint32_t)outR) | (((uint32_t)outG) << 8) | (((uint32_t)outB) << 16) | 0xff000000;
		}
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float w = box.size.x;
		float h = box.size.y;
		float cx = w * 0.5f;
		float cy = h * 0.5f;

		// Read module parameter states
		float focusVal = module ? module->params[Serqet::FOCUS_PARAM].getValue() : 0.20f;
		float beamVal = module ? module->params[Serqet::BEAM_PARAM].getValue() : 0.60f;
		float decayVal = module ? module->params[Serqet::DECAY_PARAM].getValue() : 0.50f;
		float bloomVal = module ? module->params[Serqet::BLOOM_PARAM].getValue() : 0.50f;

		// Dual Graticule opacities
		float gratAlpha = 0.0f;
		if (module) {
			gratAlpha = isMainOut ? module->params[Serqet::GRATICULE_MAIN_PARAM].getValue()
			                      : module->params[Serqet::GRATICULE_CUE_PARAM].getValue();
		} else {
			gratAlpha = isMainOut ? 0.0f : 0.25f;
		}

		// 1. Scissor clip to the 44x44 mm CRT scope screen area
		nvgScissor(args.vg, 0.0f, 0.0f, w, h);

		// 2. Render CPU-rasterized CRT simulation buffer
		renderCrtSimulation(focusVal, beamVal, decayVal, bloomVal);

		// 3. Upload and draw NanoVG texture
		cachedVg = args.vg;
		if (nvgImageId < 0) {
			nvgImageId = nvgCreateImageRGBA(args.vg, CRT_RES, CRT_RES, NVG_IMAGE_NEAREST, (const unsigned char*)outputRgba.data());
		} else {
			nvgUpdateImage(args.vg, nvgImageId, (const unsigned char*)outputRgba.data());
		}

		if (nvgImageId >= 0) {
			NVGpaint imgPaint = nvgImagePattern(args.vg, 0.0f, 0.0f, w, h, 0.0f, nvgImageId, 1.0f);
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, mm2px(2.5f));
			nvgFillPaint(args.vg, imgPaint);
			nvgFill(args.vg);
		}

		// 4. Draw Vector Graticule Grid (if opacity > 0)
		if (gratAlpha > 0.005f) {
			NVGcolor gridCol;
			NVGcolor crosshairCol;
			NVGcolor tickCol;

			if (!isMainOut && module && module->cueSource == 1) {
				// Warm amber graticule when cueing Channel 2
				gridCol = nvgRGBAf(1.00f, 0.72f, 0.15f, 0.22f * gratAlpha);
				crosshairCol = nvgRGBAf(1.00f, 0.78f, 0.20f, 0.48f * gratAlpha);
				tickCol = nvgRGBAf(1.00f, 0.85f, 0.30f, 0.66f * gratAlpha);
			} else {
				// CRT phosphorescent green graticule
				gridCol = nvgRGBAf(0.00f, 0.93f, 0.40f, 0.22f * gratAlpha);
				crosshairCol = nvgRGBAf(0.00f, 1.00f, 0.47f, 0.48f * gratAlpha);
				tickCol = nvgRGBAf(0.00f, 1.00f, 0.47f, 0.66f * gratAlpha);
			}

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
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, mm2px(2.5f));
		nvgStrokeColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0x88));
		nvgStrokeWidth(args.vg, 1.5f);
		nvgStroke(args.vg);

		nvgResetScissor(args.vg);
		nvgRestore(args.vg);
	}
};

// ─────────────────────────────────────────────────────────────────────
//  Hardware Widgets
// ─────────────────────────────────────────────────────────────────────

struct GarmirePort : app::SvgPort {
	GarmirePort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
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

struct SerqetButton : app::SvgSwitch {
	SerqetButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

struct SerqetFader : app::SvgSlider {
	SerqetFader() {
		box.size = mm2px(math::Vec(7.00f, 70.00f));
		fb->box.size = box.size;
		setHandleSvg(Svg::load(asset::system("res/ComponentLibrary/VCVSliderHandle.svg")));
		float halfHandle = handle->box.size.y / 2.0f;
		setHandlePosCentered(
			math::Vec(box.size.x / 2.0f, box.size.y - halfHandle), // minValue (0.0, Ch 2) at BOTTOM
			math::Vec(box.size.x / 2.0f, halfHandle)               // maxValue (1.0, Ch 1) at TOP
		);
	}
};

// ─────────────────────────────────────────────────────────────────────
//  SerqetWidget — Panel & UI Layout (20 HP)
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

		// Column Coordinates (mm)
		const float col1_x = 7.00f;
		const float col2_x = 20.32f;
		const float col3_x = 50.80f;
		const float col4_x = 81.28f;
		const float col5_x = 94.60f;

		// ── CRT Emulation Knobs (Zone 2 & Zone 3) ──
		// Row 1 (Y = 18.00 mm): FOCUS, BEAM
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(col1_x, 18.00f)), module, Serqet::FOCUS_PARAM));
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(col2_x, 18.00f)), module, Serqet::BEAM_PARAM));

		// Row 2 (Y = 34.00 mm): DECAY, BLOOM
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(col1_x, 34.00f)), module, Serqet::DECAY_PARAM));
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(col2_x, 34.00f)), module, Serqet::BLOOM_PARAM));

		// Row 3 (Y = 47.00 mm): DUAL GRATICULE Trimpots (MAIN at col1_x, CUE at col2_x)
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col1_x, 47.00f)), module, Serqet::GRATICULE_MAIN_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(col2_x, 47.00f)), module, Serqet::GRATICULE_CUE_PARAM));

		// Row Coordinates for Jacks (Bottom-Up: 65, 78, 91, 104, 117 mm)
		const float jack_y[5] = { 65.00f, 78.00f, 91.00f, 104.00f, 117.00f };

		// ── Column 1: Ch 1 Inputs (X, Y, R, G, B) ──
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col1_x, jack_y[0])), module, Serqet::CH1_X_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col1_x, jack_y[1])), module, Serqet::CH1_Y_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col1_x, jack_y[2])), module, Serqet::CH1_R_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col1_x, jack_y[3])), module, Serqet::CH1_G_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col1_x, jack_y[4])), module, Serqet::CH1_B_INPUT));

		// ── Column 2: Ch 2 Inputs (X, Y, R, G, B) ──
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col2_x, jack_y[0])), module, Serqet::CH2_X_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col2_x, jack_y[1])), module, Serqet::CH2_Y_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col2_x, jack_y[2])), module, Serqet::CH2_R_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col2_x, jack_y[3])), module, Serqet::CH2_G_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col2_x, jack_y[4])), module, Serqet::CH2_B_INPUT));

		// ── Column 3: Dual Oscilloscopes ──
		// Scope 1 (Main Out): Center (50.80, 39.00 mm), Size 44 x 44 mm
		auto* scope1 = new SerqetCrtWidget(module, true);
		scope1->box.pos = mm2px(Vec(col3_x - 22.0f, 39.00f - 22.0f));
		addChild(scope1);

		// Scope 2 (Cue Monitor): Center (50.80, 89.00 mm), Size 44 x 44 mm
		auto* scope2 = new SerqetCrtWidget(module, false);
		scope2->box.pos = mm2px(Vec(col3_x - 22.0f, 89.00f - 22.0f));
		addChild(scope2);

		// Cue Buttons: CH 1 ---- ( )  ( ) ---- CH 2 (Y = 117.00 mm)
		// Centers at 44.80 mm and 56.80 mm (no halo rings)
		const float btn1_x = col3_x - 6.00f; // 44.80 mm
		const float btn2_x = col3_x + 6.00f; // 56.80 mm

		addParam(createParamCentered<SerqetButton>(mm2px(Vec(btn1_x, 117.00f)), module, Serqet::CUE_CH1_PARAM));
		addParam(createParamCentered<SerqetButton>(mm2px(Vec(btn2_x, 117.00f)), module, Serqet::CUE_CH2_PARAM));

		// ── Column 4: Crossfader Slider & CV Jack ──
		// Slider Center at (81.28, 57.00 mm)
		addParam(createParamCentered<SerqetFader>(mm2px(Vec(col4_x, 57.00f)), module, Serqet::CROSSFADE_PARAM));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(col4_x, 117.00f)), module, Serqet::XFADE_CV_INPUT));

		// ── Column 5: Outputs (X, Y, R, G, B) ──
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col5_x, jack_y[0])), module, Serqet::X_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col5_x, jack_y[1])), module, Serqet::Y_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col5_x, jack_y[2])), module, Serqet::R_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col5_x, jack_y[3])), module, Serqet::G_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(col5_x, jack_y[4])), module, Serqet::B_OUTPUT));
	}
};

Model* modelSerqet = createModel<Serqet, SerqetWidget>("Serqet");
