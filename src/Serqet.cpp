#include "plugin.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
//  SERQET — Dual-Input Dual-Preview CRT Oscilloscope & Video Crossfader
//  Columns:
//   Col 1 (7.00 mm):  Focus / Decay knobs & Ch 1 X, Y, R, G, B inputs
//   Col 2 (20.32 mm): Beam / Bloom knobs & Ch 2 X, Y, R, G, B inputs
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
		GRATICULE_PARAM,

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
		configParam(FOCUS_PARAM, 0.002f, 0.025f, 0.008f, "Focus", " mm", 0.0f, 1000.0f);
		configParam(BEAM_PARAM, 0.1f, 3.0f, 1.0f, "Beam intensity", "%", 0.0f, 100.0f);
		configParam(DECAY_PARAM, 0.05f, 1.0f, 0.35f, "Phosphor decay", "%", 0.0f, 100.0f);
		configParam(BLOOM_PARAM, 0.0f, 2.5f, 1.0f, "Optical bloom", "%", 0.0f, 100.0f);
		configParam(GRATICULE_PARAM, 0.0f, 1.0f, 0.5f, "Graticule opacity", "%", 0.0f, 100.0f);

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
//  SerqetCrtWidget — Standalone Real-Time CRT Oscilloscope Display
// ─────────────────────────────────────────────────────────────────────

struct SerqetCrtWidget : widget::TransparentWidget {
	Serqet* module = nullptr;
	bool isMainOut = true; // true = Main Out, false = Cue

	SerqetCrtWidget(Serqet* m, bool mainOut) : module(m), isMainOut(mainOut) {
		box.size = mm2px(Vec(44.0f, 44.0f));
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float w = box.size.x;
		float h = box.size.y;
		float cx = w * 0.5f;
		float cy = h * 0.5f;
		float scaleFactor = cx * 0.88f;

		// Read module parameter states
		float focusVal = module ? module->params[Serqet::FOCUS_PARAM].getValue() : 0.008f;
		float beamIntensity = module ? module->params[Serqet::BEAM_PARAM].getValue() : 1.0f;
		float decayVal = module ? module->params[Serqet::DECAY_PARAM].getValue() : 0.35f;
		float bloomVal = module ? module->params[Serqet::BLOOM_PARAM].getValue() : 1.0f;
		float gratAlpha = module ? module->params[Serqet::GRATICULE_PARAM].getValue() : 0.5f;

		// 1. Scissor clip to the 44x44 mm CRT scope screen area
		nvgScissor(args.vg, 0.0f, 0.0f, w, h);

		// 2. Draw CRT screen glass background (deep phosphorescent dark obsidian green)
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0f, 0.0f, w, h, mm2px(2.5f));
		nvgFillColor(args.vg, nvgRGBA(0x06, 0x0c, 0x08, 0xff));
		nvgFill(args.vg);

		// 3. Draw CRT Graticule Grid (if opacity > 0)
		if (gratAlpha > 0.005f) {
			nvgBeginPath(args.vg);
			nvgStrokeColor(args.vg, nvgRGBAf(0.00f, 0.93f, 0.40f, 0.22f * gratAlpha));
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
			nvgStrokeColor(args.vg, nvgRGBAf(0.00f, 1.00f, 0.47f, 0.48f * gratAlpha));
			nvgStrokeWidth(args.vg, 0.8f);
			nvgMoveTo(args.vg, 0.0f, cy);
			nvgLineTo(args.vg, w, cy);
			nvgMoveTo(args.vg, cx, 0.0f);
			nvgLineTo(args.vg, cx, h);
			nvgStroke(args.vg);

			// Reticle sub-division ticks on center crosshair
			nvgBeginPath(args.vg);
			nvgStrokeColor(args.vg, nvgRGBAf(0.00f, 1.00f, 0.47f, 0.66f * gratAlpha));
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

		// 4. Retrieve audio/video trajectory from module
		if (module) {
			const SerqetRingBuffer& rb = isMainOut ? module->mainBuffer : module->cueBuffer;
			uint32_t head = rb.getHead();

			// Detect connection state for smart auto-sweep timebase
			bool ch1XConn = module->inputs[Serqet::CH1_X_INPUT].isConnected();
			bool ch1YConn = module->inputs[Serqet::CH1_Y_INPUT].isConnected();
			bool ch2XConn = module->inputs[Serqet::CH2_X_INPUT].isConnected();
			bool ch2YConn = module->inputs[Serqet::CH2_Y_INPUT].isConnected();

			bool isXConnected = isMainOut ? (ch1XConn || ch2XConn) : (module->cueSource == 0 ? ch1XConn : ch2XConn);
			bool isYConnected = isMainOut ? (ch1YConn || ch2YConn) : (module->cueSource == 0 ? ch1YConn : ch2YConn);

			// Dynamic sample count governed by Phosphor Decay knob
			static constexpr size_t NUM_POINTS = 256;
			Vec pts[NUM_POINTS];
			float avgR = 0.0f, avgG = 0.0f, avgB = 0.0f;
			bool isStationary = true;

			// Sample stride adjusted by decay parameter (from 1 up to 4 samples)
			size_t stride = std::max<size_t>(1, (size_t)std::round(1.0f + decayVal * 3.0f));

			for (size_t i = 0; i < NUM_POINTS; i++) {
				SerqetSample s = rb.readOffset(head, (NUM_POINTS - 1 - i) * stride);
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
					// Horizontal deflection only
					px = cx + clamp(s.x, -1.15f, 1.15f) * scaleFactor;
					py = cy;
				} else {
					// Idle / Unpatched Mode: Beam rests at center
					px = cx;
					py = cy;
				}

				pts[i] = Vec(px, py);

				float r = clamp(s.r, 0.0f, 1.0f);
				float g = clamp(s.g, 0.0f, 1.0f);
				float b = clamp(s.b, 0.0f, 1.0f);
				avgR += r;
				avgG += g;
				avgB += b;

				if (std::abs(px - cx) > 1.0f || std::abs(py - cy) > 1.0f) {
					isStationary = false;
				}
			}

			avgR /= NUM_POINTS;
			avgG /= NUM_POINTS;
			avgB /= NUM_POINTS;

			// If color inputs are inactive / blank, default to CRT green (Ch 1) or warm amber (Ch 2)
			if (avgR + avgG + avgB < 0.05f) {
				if (isMainOut) {
					avgG = 1.0f;
				} else {
					if (module->cueSource == 0) { avgG = 1.0f; }
					else { avgR = 1.0f; avgG = 0.5f; }
				}
			}

			// Modulate color by Beam Intensity knob
			avgR = clamp(avgR * beamIntensity, 0.0f, 1.0f);
			avgG = clamp(avgG * beamIntensity, 0.0f, 1.0f);
			avgB = clamp(avgB * beamIntensity, 0.0f, 1.0f);

			// Stroke widths governed by Focus knob (0.002 to 0.025 mm)
			float focusScale = clamp(focusVal / 0.008f, 0.4f, 2.5f);
			float coreW = 0.75f * focusScale;
			float midW = 1.80f * focusScale;
			float haloW = 4.00f * focusScale;

			if (!isStationary) {
				// Pass 1: Soft outer phosphor diffusion halo (scaled by Bloom knob)
				if (bloomVal > 0.01f) {
					nvgBeginPath(args.vg);
					for (size_t i = 0; i < NUM_POINTS; i++) {
						if (i == 0) nvgMoveTo(args.vg, pts[i].x, pts[i].y);
						else nvgLineTo(args.vg, pts[i].x, pts[i].y);
					}
					nvgStrokeColor(args.vg, nvgRGBAf(avgR, avgG, avgB, clamp(0.30f * bloomVal, 0.0f, 0.75f)));
					nvgStrokeWidth(args.vg, haloW);
					nvgStroke(args.vg);
				}

				// Pass 2: Mid-intensity luminous phosphor beam
				nvgBeginPath(args.vg);
				for (size_t i = 0; i < NUM_POINTS; i++) {
					if (i == 0) nvgMoveTo(args.vg, pts[i].x, pts[i].y);
					else nvgLineTo(args.vg, pts[i].x, pts[i].y);
				}
				nvgStrokeColor(args.vg, nvgRGBAf(std::min(1.0f, avgR * 1.2f), std::min(1.0f, avgG * 1.2f), std::min(1.0f, avgB * 1.2f), clamp(0.75f * beamIntensity, 0.1f, 1.0f)));
				nvgStrokeWidth(args.vg, midW);
				nvgStroke(args.vg);

				// Pass 3: White-hot sharp cathode beam core
				nvgBeginPath(args.vg);
				for (size_t i = 0; i < NUM_POINTS; i++) {
					if (i == 0) nvgMoveTo(args.vg, pts[i].x, pts[i].y);
					else nvgLineTo(args.vg, pts[i].x, pts[i].y);
				}
				nvgStrokeColor(args.vg, nvgRGBAf(std::min(1.0f, avgR + 0.5f), std::min(1.0f, avgG + 0.5f), std::min(1.0f, avgB + 0.5f), 0.95f));
				nvgStrokeWidth(args.vg, coreW);
				nvgStroke(args.vg);
			}

			// Cathode Ray Beam Spot (at current head position)
			Vec currentPos = pts[NUM_POINTS - 1];
			// Outer spot halo
			if (bloomVal > 0.01f) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, currentPos.x, currentPos.y, 4.5f * focusScale);
				nvgFillColor(args.vg, nvgRGBAf(avgR, avgG, avgB, clamp(0.40f * bloomVal, 0.0f, 0.85f)));
				nvgFill(args.vg);
			}
			// Inner hot spot core
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, currentPos.x, currentPos.y, 1.5f * focusScale);
			nvgFillColor(args.vg, nvgRGBAf(std::min(1.0f, avgR + 0.6f), std::min(1.0f, avgG + 0.6f), std::min(1.0f, avgB + 0.6f), 0.95f));
			nvgFill(args.vg);
		}

		// 5. CRT Glass Vignette Inner Shadow
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.0f, h - 1.0f, mm2px(2.5f));
		nvgStrokeColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 0x77));
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

		// Row 3 (Y = 47.00 mm): GRATICULE Trimpot
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(13.66f, 47.00f)), module, Serqet::GRATICULE_PARAM));

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
