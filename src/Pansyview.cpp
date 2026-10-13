#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
inline float clampf(float v, float lo, float hi) {
	return std::max(lo, std::min(v, hi));
}

// Convert Hue (0..360), Saturation (0..1), Value (0..1) to NVGcolor with alpha
inline NVGcolor hsvToNvgColor(float h, float s, float v, float a) {
	float c = v * s;
	float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
	float m = v - c;
	float r = 0.f, g = 0.f, b = 0.f;
	if (h >= 0.f && h < 60.f) {
		r = c; g = x; b = 0.f;
	} else if (h >= 60.f && h < 120.f) {
		r = x; g = c; b = 0.f;
	} else if (h >= 120.f && h < 180.f) {
		r = 0.f; g = c; b = x;
	} else if (h >= 180.f && h < 240.f) {
		r = 0.f; g = x; b = c;
	} else if (h >= 240.f && h < 300.f) {
		r = x; g = 0.f; b = c;
	} else {
		r = c; g = 0.f; b = x;
	}
	return nvgRGBAf(r + m, g + m, b + m, a);
}
} // namespace

struct Pansyview : Module {
	enum ParamIds {
		LAYOUT_PARAM,

		// Visual customization parameters
		PUCK_OPACITY_PARAM,    // LOCATION
		PUCK_HUE_PARAM,        // LOC. HUE
		TRAIL_LENGTH_PARAM,    // TRAIL
		TRAIL_HUE_PARAM,       // TRAIL HUE
		GLOW_INTENSITY_PARAM,  // GLOW
		GLOW_HUE_PARAM,        // GLOW HUE
		SPEAKER_OPACITY_PARAM, // SPEAKERS
		FIELD_OPACITY_PARAM,   // FIELD

		PARAMS_LEN
	};

	enum InputIds {
		X_INPUT,
		Y_INPUT,
		ROTATION_INPUT,
		FOCUS_INPUT,
		CENTER_INPUT,
		SMOOTH_INPUT,
		INPUTS_LEN
	};

	enum OutputIds {
		OUTPUTS_LEN
	};

	enum LightIds {
		LIGHTS_LEN
	};

	enum SpeakerLayoutId {
		LAYOUT_2CH = 0,
		LAYOUT_4CH_ROOM = 1,
		LAYOUT_4CH_CIRCLE = 2,
		LAYOUT_4CH_CENTER = 3,
		LAYOUT_6CH_ROOM = 4,
		LAYOUT_6CH_CIRCLE = 5,
		LAYOUT_6CH_CENTER = 6,
		LAYOUT_8CH_ROOM = 7,
		LAYOUT_8CH_CIRCLE = 8,
		LAYOUT_8CH_CENTER = 9,
		LAYOUTS_COUNT = 10
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT = 1,
		THEME_DARK = 2
	};

	struct SpeakerDef {
		std::string label;
		float x = 0.0f;
		float y = 0.0f;
		float gain = 0.0f;
		float dB = -60.0f;

		SpeakerDef() = default;
		SpeakerDef(const std::string& l, float xVal, float yVal, float g = 0.0f, float d = -60.0f)
			: label(l), x(xVal), y(yVal), gain(g), dB(d) {}
	};

	SkinTheme skinTheme = THEME_MEDIUM;

	// Thread-safe buffered visual state for the UI widget
	struct VisualState {
		float rotX = 0.0f;
		float rotY = 0.0f;
		float focus = 50.0f;
		float center = 50.0f;
		int layoutIndex = 1; // 4-CH Room default
		int numSpeakers = 4;
		SpeakerDef speakers[8];
	};

	VisualState visualState;

	// Audio-rate single-pole IIR smoothing filter state (from Gen~: x_rot * (1 - smoo) + x_sm * smoo)
	float smoothedRotX = 0.0f;
	float smoothedRotY = 0.0f;
	bool smoothedInit = false;

	Pansyview() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Layout selector knob: 10 discrete positions [0 to 9]
		configSwitch(LAYOUT_PARAM, 0.0f, 9.0f, 1.0f, "Arrangement", {
			"Stereo (2-CH)",
			"4-CH Room",
			"4-CH Circle",
			"4-CH Center",
			"6-CH Room",
			"6-CH Circle",
			"6-CH Center",
			"8-CH Room",
			"8-CH Circle",
			"8-CH Center"
		});

		// Visual customization parameters
		configParam(PUCK_OPACITY_PARAM, 0.0f, 1.0f, 1.0f, "Location opacity", "%", 0.0f, 100.0f);
		configParam(PUCK_HUE_PARAM, 0.0f, 360.0f, 210.0f, "Location hue", "°"); // Default cyan/blue ~210°
		configParam(TRAIL_LENGTH_PARAM, 0.0f, 1.0f, 0.5f, "Trail length", "%", 0.0f, 100.0f);
		configParam(TRAIL_HUE_PARAM, 0.0f, 360.0f, 210.0f, "Trail hue", "°");
		configParam(GLOW_INTENSITY_PARAM, 0.0f, 1.0f, 1.0f, "Speaker glow", "%", 0.0f, 100.0f);
		configParam(GLOW_HUE_PARAM, 0.0f, 360.0f, 135.0f, "Glow hue", "°"); // Default green ~135°
		configParam(SPEAKER_OPACITY_PARAM, 0.0f, 1.0f, 1.0f, "Speakers opacity", "%", 0.0f, 100.0f);
		configParam(FIELD_OPACITY_PARAM, 0.0f, 1.0f, 0.8f, "Field opacity", "%", 0.0f, 100.0f);

		// CV Inputs (strict sentence case, no "input" suffix per AGENTS.md rules)
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(ROTATION_INPUT, "Rotation");
		configInput(FOCUS_INPUT, "Focus");
		configInput(CENTER_INPUT, "Center");
		configInput(SMOOTH_INPUT, "Smooth");
	}

	void getSpeakerPositions(int layout, std::vector<SpeakerDef>& out) {
		out.clear();
		switch (layout) {
			case LAYOUT_2CH:
				out.push_back({"Left", -1.000f, 0.000f, 0.f, -60.f});
				out.push_back({"Right", +1.000f, 0.000f, 0.f, -60.f});
				break;
			case LAYOUT_4CH_ROOM:
				out.push_back({"FL", -0.750f, +0.750f, 0.f, -60.f});
				out.push_back({"FR", +0.750f, +0.750f, 0.f, -60.f});
				out.push_back({"RL", -0.750f, -0.750f, 0.f, -60.f});
				out.push_back({"RR", +0.750f, -0.750f, 0.f, -60.f});
				break;
			case LAYOUT_4CH_CIRCLE:
				out.push_back({"FL", -0.750f, +0.750f, 0.f, -60.f});
				out.push_back({"FR", +0.750f, +0.750f, 0.f, -60.f});
				out.push_back({"RR", +0.750f, -0.750f, 0.f, -60.f});
				out.push_back({"RL", -0.750f, -0.750f, 0.f, -60.f});
				break;
			case LAYOUT_4CH_CENTER:
				out.push_back({"Front", 0.000f, +1.000f, 0.f, -60.f});
				out.push_back({"Right", +1.000f, 0.000f, 0.f, -60.f});
				out.push_back({"Back", 0.000f, -1.000f, 0.f, -60.f});
				out.push_back({"Left", -1.000f, 0.000f, 0.f, -60.f});
				break;
			case LAYOUT_6CH_ROOM:
				out.push_back({"FL", -0.750f, +0.820f, 0.f, -60.f});
				out.push_back({"FR", +0.750f, +0.820f, 0.f, -60.f});
				out.push_back({"SL", -0.880f, 0.000f, 0.f, -60.f});
				out.push_back({"SR", +0.880f, 0.000f, 0.f, -60.f});
				out.push_back({"RL", -0.750f, -0.820f, 0.f, -60.f});
				out.push_back({"RR", +0.750f, -0.820f, 0.f, -60.f});
				break;
			case LAYOUT_6CH_CIRCLE:
				out.push_back({"FL", -0.750f, +0.820f, 0.f, -60.f});
				out.push_back({"FR", +0.750f, +0.820f, 0.f, -60.f});
				out.push_back({"SR", +0.880f, 0.000f, 0.f, -60.f});
				out.push_back({"RR", +0.750f, -0.820f, 0.f, -60.f});
				out.push_back({"RL", -0.750f, -0.820f, 0.f, -60.f});
				out.push_back({"SL", -0.880f, 0.000f, 0.f, -60.f});
				break;
			case LAYOUT_6CH_CENTER:
				// Angles: 90°, 30°, 330°, 270°, 210°, 150°
				out.push_back({"Front", 0.000f, +1.000f, 0.f, -60.f});
				out.push_back({"FR", +0.866f, +0.500f, 0.f, -60.f});
				out.push_back({"RR", +0.866f, -0.500f, 0.f, -60.f});
				out.push_back({"Back", 0.000f, -1.000f, 0.f, -60.f});
				out.push_back({"RL", -0.866f, -0.500f, 0.f, -60.f});
				out.push_back({"FL", -0.866f, +0.500f, 0.f, -60.f});
				break;
			case LAYOUT_8CH_ROOM:
				out.push_back({"FL", -0.450f, +0.880f, 0.f, -60.f});
				out.push_back({"FR", +0.450f, +0.880f, 0.f, -60.f});
				out.push_back({"Side-UL", -0.880f, +0.380f, 0.f, -60.f});
				out.push_back({"Side-UR", +0.880f, +0.380f, 0.f, -60.f});
				out.push_back({"Side-DL", -0.880f, -0.380f, 0.f, -60.f});
				out.push_back({"Side-DR", +0.880f, -0.380f, 0.f, -60.f});
				out.push_back({"RL", -0.450f, -0.880f, 0.f, -60.f});
				out.push_back({"RR", +0.450f, -0.880f, 0.f, -60.f});
				break;
			case LAYOUT_8CH_CIRCLE:
				out.push_back({"FL", -0.450f, +0.880f, 0.f, -60.f});
				out.push_back({"FR", +0.450f, +0.880f, 0.f, -60.f});
				out.push_back({"Side-UR", +0.880f, +0.380f, 0.f, -60.f});
				out.push_back({"Side-DR", +0.880f, -0.380f, 0.f, -60.f});
				out.push_back({"RR", +0.450f, -0.880f, 0.f, -60.f});
				out.push_back({"RL", -0.450f, -0.880f, 0.f, -60.f});
				out.push_back({"Side-DL", -0.880f, -0.380f, 0.f, -60.f});
				out.push_back({"Side-UL", -0.880f, +0.380f, 0.f, -60.f});
				break;
			case LAYOUT_8CH_CENTER:
				// Angles: 90°, 45°, 0°, 315°, 270°, 225°, 180°, 135°
				out.push_back({"Front", 0.000f, +1.000f, 0.f, -60.f});
				out.push_back({"FR", +0.707f, +0.707f, 0.f, -60.f});
				out.push_back({"Right", +1.000f, 0.000f, 0.f, -60.f});
				out.push_back({"RR", +0.707f, -0.707f, 0.f, -60.f});
				out.push_back({"Back", 0.000f, -1.000f, 0.f, -60.f});
				out.push_back({"RL", -0.707f, -0.707f, 0.f, -60.f});
				out.push_back({"Left", -1.000f, 0.000f, 0.f, -60.f});
				out.push_back({"FL", -0.707f, +0.707f, 0.f, -60.f});
				break;
		}
	}

	void process(const ProcessArgs& args) override {
		// Read inputs with defaults when unpatched:
		// X = 0V, Y = 0V, Rotation = 0V, Focus = 5V (50), Center = 5V (50), Smooth = 2V (20 default)
		float xVolt = inputs[X_INPUT].isConnected() ? inputs[X_INPUT].getVoltage() : 0.0f;
		float yVolt = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getVoltage() : 0.0f;
		float rotVolt = inputs[ROTATION_INPUT].isConnected() ? inputs[ROTATION_INPUT].getVoltage() : 0.0f;
		float focVolt = inputs[FOCUS_INPUT].isConnected() ? inputs[FOCUS_INPUT].getVoltage() : 5.0f;
		float ctrVolt = inputs[CENTER_INPUT].isConnected() ? inputs[CENTER_INPUT].getVoltage() : 5.0f;
		float smhVolt = inputs[SMOOTH_INPUT].isConnected() ? inputs[SMOOTH_INPUT].getVoltage() : 2.0f;

		// Normalization per spec:
		float normX = clampf(xVolt / 5.0f, -1.0f, 1.0f);
		float normY = clampf(yVolt / 5.0f, -1.0f, 1.0f);
		float rotationDeg = clampf(rotVolt / 5.0f * 180.0f, -180.0f, 180.0f);
		float focus = clampf(focVolt / 10.0f * 100.0f, 0.0f, 100.0f);
		float center = clampf(ctrVolt / 10.0f * 100.0f, 0.0f, 100.0f);
		float smoothDial = clampf(smhVolt / 10.0f * 100.0f, 0.0f, 100.0f);

		// Apply rotation to incoming X/Y before gain calc and display:
		float radR = rotationDeg * (float)M_PI / 180.0f;
		float cosR = std::cos(radR);
		float sinR = std::sin(radR);
		float rotX = normX * cosR - normY * sinR;
		float rotY = normX * sinR + normY * cosR;

		// Audio-rate single-pole IIR lowpass filter with exact Max for Live parameter mapping:
		// In Live's Surround Panner, Smooth dial [0..100] is mapped through:
		// [scale 0. 1. 1000. 0.5 0.05 @classic 0] -> smoothing in Gen~.
		// Higher Smooth dial -> lower smoothing value in Gen~ -> longer tau (sluggish movement).
		// Dial 0%   -> smoothing = 1000.0 (tau ≈ 1.0 ms, instantaneous response)
		// Dial 20%  -> smoothing = 77.8 (tau ≈ 13 ms, default setting)
		// Dial 50%  -> smoothing = 34.5 (tau ≈ 29 ms)
		// Dial 100% -> smoothing = 0.5 (tau ≈ 2000 ms = 2.0 s, very heavy/slow movement)
		float u_sm = smoothDial / 100.0f;
		float smoothingVal;
		if (u_sm <= 0.0f) {
			smoothingVal = 1000.0f;
		} else {
			smoothingVal = 1000.0f + (0.5f - 1000.0f) * std::pow(u_sm, 0.05f);
		}

		if (!smoothedInit) {
			smoothedRotX = rotX;
			smoothedRotY = rotY;
			smoothedInit = true;
		} else {
			if (smoothDial <= 0.001f) {
				// Instant bypass when smooth dial is at 0
				smoothedRotX = rotX;
				smoothedRotY = rotY;
			} else {
				float smoo = clampf(smoothingVal * args.sampleTime, 0.0f, 1.0f);
				smoothedRotX = smoothedRotX * (1.0f - smoo) + rotX * smoo;
				smoothedRotY = smoothedRotY * (1.0f - smoo) + rotY * smoo;
			}
		}

		int layout = (int)clampf(std::round(params[LAYOUT_PARAM].getValue()), 0.0f, 9.0f);

		// Preset geometry scaling from Ableton Surround Panner presets:
		// 2-CH: 4.0 | 4-Room, 4-Circle, 4-Center: 5.0 | 6-Room, 6-Circle, 6-Center: 7.0 | 8-Room, 8-Circle, 8-Center: 8.0
		const float layoutScales[10] = { 4.0f, 5.0f, 5.0f, 5.0f, 7.0f, 7.0f, 7.0f, 8.0f, 8.0f, 8.0f };
		float scale = layoutScales[layout];

		// Focus & Center source-verified Gen~ formulas:
		// focus is mapped from [0..100] via [scale 0. 1. 1. 4.] -> focus_gen in [1.0 .. 4.0]
		// center is mapped from [0..100] via [scale 0. 1. 0. 1.] -> center_gen in [0.0 .. 1.0]
		// centerx = sqrt(center * sqrt(focus) * clip(1 - (x_rot² + y_rot²), 0, 1));
		// focusx  = (0.525 * (1 - centerx)) + centerx * 0.25;
		// size    = focusx * scale;
		// level   = dbtoa(-pow(distance * size, focus));
		float focus_gen = 1.0f + (focus / 100.0f) * 3.0f; // [1.0 .. 4.0]
		float center_gen = center / 100.0f;               // [0.0 .. 1.0]
		float distFromCenterSq = smoothedRotX * smoothedRotX + smoothedRotY * smoothedRotY;
		float proximity = clampf(1.0f - distFromCenterSq, 0.0f, 1.0f);
		float centerx = std::sqrt(center_gen * std::sqrt(focus_gen) * proximity);
		float focusx = 0.525f * (1.0f - centerx) + centerx * 0.25f;

		float size = focusx * scale;

		std::vector<SpeakerDef> spkList;
		getSpeakerPositions(layout, spkList);
		int nSpeakers = (int)spkList.size();

		float sumSq = 0.0f;
		float weights[8];
		for (int i = 0; i < nSpeakers; i++) {
			float sx = spkList[i].x;
			float sy = spkList[i].y;
			float dist = std::sqrt((smoothedRotX - sx) * (smoothedRotX - sx) + (smoothedRotY - sy) * (smoothedRotY - sy));
			float term = dist * size;
			// Gen~ exact formula: level = dbtoa(-pow(distance * size, focus));
			float dbAtten = -std::pow(std::max(0.0f, term), focus_gen);
			float gain = std::pow(10.0f, dbAtten / 20.0f); // dbtoa

			weights[i] = gain;
			sumSq += gain * gain;
		}

		float normFactor = (sumSq > 1e-6f) ? std::sqrt(sumSq) : 1.0f;

		// Buffer state for UI thread
		visualState.rotX = smoothedRotX;
		visualState.rotY = smoothedRotY;
		visualState.focus = focus;
		visualState.center = center;
		visualState.layoutIndex = layout;
		visualState.numSpeakers = nSpeakers;

		for (int i = 0; i < nSpeakers; i++) {
			float gain = weights[i] / normFactor;
			float db = (gain > 0.001f) ? (20.0f * std::log10(gain)) : -60.0f;
			if (db < -60.0f) db = -60.0f;
			visualState.speakers[i].label = spkList[i].label;
			visualState.speakers[i].x = spkList[i].x;
			visualState.speakers[i].y = spkList[i].y;
			visualState.speakers[i].gain = gain;
			visualState.speakers[i].dB = db;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) {
			skinTheme = (SkinTheme)json_integer_value(stJ);
		}
	}
};

// ── Custom Hardware Widgets (All Attenuverter Sized) ──
struct PansyviewLayoutKnob : RoundKnob {
	PansyviewLayoutKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
		snap = true;
	}
};

struct PansyviewTrimpot : RoundKnob {
	PansyviewTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct PansyviewPort : app::SvgPort {
	PansyviewPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

// ── Square Field Canvas Display Widget ──
struct PansyviewDisplayWidget : TransparentWidget {
	Pansyview* module = nullptr;
	std::vector<std::pair<float, float>> trail;
	const size_t maxTrail = 2000; // Large buffer for persistent non-linear trail

	// 5x5 Reference Fraction Table for Ableton Live Dispersion Disc Radius (as fraction of field radius)
	const float G[5][5] = {
		{1.000f, 1.000f, 1.000f, 1.000f, 1.000f},
		{0.618f, 0.717f, 0.825f, 0.943f, 1.000f},
		{0.451f, 0.534f, 0.630f, 0.743f, 0.884f},
		{0.361f, 0.436f, 0.525f, 0.636f, 0.784f},
		{0.302f, 0.372f, 0.457f, 0.567f, 0.730f}
	};

	float calcDispersionRadius(float focus, float center, float rotX, float rotY, int layoutIndex, float radiusPx) {
		// Preset geometry scaling from Ableton Surround Panner presets:
		// 2-CH: 4.0 | 4-Room, 4-Circle, 4-Center: 5.0 | 6-Room, 6-Circle, 6-Center: 7.0 | 8-Room, 8-Circle, 8-Center: 8.0
		const float layoutScales[10] = { 4.0f, 5.0f, 5.0f, 5.0f, 7.0f, 7.0f, 7.0f, 8.0f, 8.0f, 8.0f };
		int lIdx = (int)clampf((float)layoutIndex, 0.0f, 9.0f);
		float scale = layoutScales[lIdx];

		// Gen~ source codebox exact formulas:
		// focus_gen: dial [0..100] mapped via [scale 0. 1. 1. 4.] -> [1.0 .. 4.0]
		// center_gen: dial [0..100] mapped via [scale 0. 1. 0. 1.] -> [0.0 .. 1.0]
		float focus_gen = 1.0f + (clampf(focus, 0.0f, 100.0f) / 100.0f) * 3.0f;
		float center_gen = clampf(center, 0.0f, 100.0f) / 100.0f;

		// Position-dependent center boost:
		float distFromCenterSq = clampf(rotX * rotX + rotY * rotY, 0.0f, 1.0f);
		float proximity = clampf(1.0f - distFromCenterSq, 0.0f, 1.0f);
		float centerx = std::sqrt(center_gen * std::sqrt(focus_gen) * proximity);
		float focusx = 0.525f * (1.0f - centerx) + centerx * 0.25f;
		float size = focusx * scale;

		// Gen~ out11 formula: out11 = 400 / (size * focus);
		// In Ableton Live (xypad.js), focus ring diameter = out11, drawn on a canvas where field circle radius = 61.0 px.
		// Therefore, focus ring radius = out11 / 2.0 = 200.0 / (size * focus_gen).
		// Relative fraction of field radius = (200.0 / (size * focus_gen)) / 61.0.
		float frac = (200.0f / (size * focus_gen)) / 61.0f;

		float finalRadius = frac * radiusPx;
		return clampf(finalRadius, 0.0f, radiusPx);
	}

	void draw(const DrawArgs& args) override {
		Pansyview::VisualState state;
		float puckOpacity = 1.0f;
		float puckHue = 210.0f;
		float trailParam = 0.5f;
		float trailHue = 210.0f;
		float glowInt = 1.0f;
		float glowHue = 135.0f;
		float spkOpacity = 1.0f;
		float fieldOpacityParam = 0.8f;

		if (module) {
			state = module->visualState;
			puckOpacity = module->params[Pansyview::PUCK_OPACITY_PARAM].getValue();
			puckHue = module->params[Pansyview::PUCK_HUE_PARAM].getValue();
			trailParam = module->params[Pansyview::TRAIL_LENGTH_PARAM].getValue();
			trailHue = module->params[Pansyview::TRAIL_HUE_PARAM].getValue();
			glowInt = module->params[Pansyview::GLOW_INTENSITY_PARAM].getValue();
			glowHue = module->params[Pansyview::GLOW_HUE_PARAM].getValue();
			spkOpacity = module->params[Pansyview::SPEAKER_OPACITY_PARAM].getValue();
			fieldOpacityParam = module->params[Pansyview::FIELD_OPACITY_PARAM].getValue();
		}

		float w = box.size.x;
		float h = box.size.y;
		float radiusPx = (std::min(w, h) / 2.0f) * 0.88f;
		float centerX_px = w / 2.0f;
		float centerY_px = h / 2.0f;

		// 1. Dark background canvas (Strict crisp square corners, no rounded rect)
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, w, h);
		nvgFillColor(args.vg, nvgRGBA(0x12, 0x12, 0x14, 0xff));
		nvgFill(args.vg);

		// 2. Field rings and radial axes (Field Opacity: 0% = invisible, 100% = 25% brighter than default)
		float fieldAlphaScale = fieldOpacityParam * 1.25f;

		if (fieldAlphaScale > 0.001f) {
			// Rings at r = 0.25, 0.50, 0.75
			const float ringFracs[] = {0.25f, 0.50f, 0.75f};
			for (float f : ringFracs) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, centerX_px, centerY_px, radiusPx * f);
				nvgStrokeColor(args.vg, nvgRGBAf(0.21f, 0.21f, 0.24f, clampf(0.53f * fieldAlphaScale, 0.f, 1.f)));
				nvgStrokeWidth(args.vg, 1.0f);
				nvgStroke(args.vg);
			}

			// Outer boundary circle at r = 1.0
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, centerX_px, centerY_px, radiusPx);
			nvgStrokeColor(args.vg, nvgRGBAf(0.38f, 0.38f, 0.44f, clampf(0.80f * fieldAlphaScale, 0.f, 1.f)));
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStroke(args.vg);

			// X and Y Axes
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, centerX_px - radiusPx, centerY_px);
			nvgLineTo(args.vg, centerX_px + radiusPx, centerY_px);
			nvgMoveTo(args.vg, centerX_px, centerY_px - radiusPx);
			nvgLineTo(args.vg, centerX_px, centerY_px + radiusPx);
			nvgStrokeColor(args.vg, nvgRGBAf(0.25f, 0.25f, 0.29f, clampf(0.53f * fieldAlphaScale, 0.f, 1.f)));
			nvgStrokeWidth(args.vg, 1.0f);
			nvgStroke(args.vg);

			// Center crosshair (7px arms)
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, centerX_px - 7.0f, centerY_px);
			nvgLineTo(args.vg, centerX_px + 7.0f, centerY_px);
			nvgMoveTo(args.vg, centerX_px, centerY_px - 7.0f);
			nvgLineTo(args.vg, centerX_px, centerY_px + 7.0f);
			nvgStrokeColor(args.vg, nvgRGBAf(0.53f, 0.53f, 0.60f, clampf(0.66f * fieldAlphaScale, 0.f, 1.f)));
			nvgStrokeWidth(args.vg, 1.2f);
			nvgStroke(args.vg);

			// Cardinal labels
			nvgFontSize(args.vg, 10.0f);
			nvgFontFaceId(args.vg, 0); // standard font
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGBAf(0.50f, 0.50f, 0.56f, clampf(0.93f * fieldAlphaScale, 0.f, 1.f)));

			nvgText(args.vg, centerX_px, centerY_px - radiusPx - 9.0f, "FRONT", nullptr);
			nvgText(args.vg, centerX_px, centerY_px + radiusPx + 9.0f, "REAR", nullptr);
			nvgText(args.vg, centerX_px - radiusPx - 16.0f, centerY_px, "LEFT", nullptr);
			nvgText(args.vg, centerX_px + radiusPx + 18.0f, centerY_px, "RIGHT", nullptr);
		}

		// 3. Puck Position & Trail
		float screenPuckX = centerX_px + state.rotX * radiusPx;
		float screenPuckY = centerY_px - state.rotY * radiusPx;

		// Non-linear trail mapping:
		// 0% = no trail (reset trail buffer)
		// 75% = 200 points (matches previous 100% capacity)
		// 100% = infinite persistence (up to 2000 points, zero fading)
		if (trailParam <= 0.005f) {
			trail.clear();
		} else {
			trail.push_back({screenPuckX, screenPuckY});

			size_t targetCap;
			if (trailParam < 0.75f) {
				float u = trailParam / 0.75f;
				targetCap = std::max((size_t)2, (size_t)(u * 200.0f));
			} else {
				float u = (trailParam - 0.75f) / 0.25f;
				targetCap = (size_t)(200.0f + u * (2000.0f - 200.0f));
			}

			while (trail.size() > targetCap) {
				trail.erase(trail.begin());
			}

			// Draw Trail polyline
			if (trail.size() > 1 && puckOpacity > 0.005f) {
				bool isInfinite = (trailParam >= 0.995f);
				for (size_t t = 1; t < trail.size(); t++) {
					float frac = (float)t / (float)trail.size();
					// At 100% (isInfinite), no fade across the trail (uniform full opacity)
					float tAlpha = isInfinite ? (0.75f * puckOpacity) : (frac * 0.75f * puckOpacity);
					NVGcolor trailCol = hsvToNvgColor(trailHue, 0.75f, 1.0f, tAlpha);
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, trail[t - 1].first, trail[t - 1].second);
					nvgLineTo(args.vg, trail[t].first, trail[t].second);
					nvgStrokeColor(args.vg, trailCol);
					nvgStrokeWidth(args.vg, 1.0f + frac * 2.5f);
					nvgStroke(args.vg);
				}
			}
		}

		// 4. Dispersion Disc around Puck
		if (puckOpacity > 0.005f) {
			float dispR = calcDispersionRadius(state.focus, state.center, state.rotX, state.rotY, state.layoutIndex, radiusPx);

			// Draw shaded dispersion disc
			NVGcolor discInner = hsvToNvgColor(puckHue, 0.80f, 1.0f, 0.40f * puckOpacity);
			NVGcolor discOuter = hsvToNvgColor(puckHue, 0.90f, 0.8f, 0.03f * puckOpacity);
			NVGpaint discPaint = nvgRadialGradient(args.vg, screenPuckX, screenPuckY, 0.0f, dispR, discInner, discOuter);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, screenPuckX, screenPuckY, dispR);
			nvgFillPaint(args.vg, discPaint);
			nvgFill(args.vg);

			nvgBeginPath(args.vg);
			nvgCircle(args.vg, screenPuckX, screenPuckY, dispR);
			nvgStrokeColor(args.vg, hsvToNvgColor(puckHue, 0.75f, 1.0f, 0.53f * puckOpacity));
			nvgStrokeWidth(args.vg, 1.2f);
			nvgStroke(args.vg);

			// Strict Outer Boundary Mask:
			// Mask off everything outside the field circle (r = radiusPx) so that the disc and its stroke NEVER leak beyond the perimeter.
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0.f, 0.f, w, h);
			nvgCircle(args.vg, centerX_px, centerY_px, radiusPx);
			nvgPathWinding(args.vg, NVG_HOLE);
			nvgFillColor(args.vg, nvgRGBA(0x12, 0x12, 0x14, 0xff));
			nvgFill(args.vg);

			// Crisp re-stroke of the outer boundary circle at r = radiusPx
			if (fieldAlphaScale > 0.001f) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, centerX_px, centerY_px, radiusPx);
				nvgStrokeColor(args.vg, nvgRGBAf(0.38f, 0.38f, 0.44f, clampf(0.80f * fieldAlphaScale, 0.f, 1.f)));
				nvgStrokeWidth(args.vg, 1.5f);
				nvgStroke(args.vg);
			}

			// Core puck ring (filled circle r=7, surrounding stroke r=8.5)
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, screenPuckX, screenPuckY, 7.0f);
			nvgFillColor(args.vg, hsvToNvgColor(puckHue, 0.15f, 1.0f, puckOpacity));
			nvgFill(args.vg);

			nvgBeginPath(args.vg);
			nvgCircle(args.vg, screenPuckX, screenPuckY, 8.5f);
			nvgStrokeColor(args.vg, hsvToNvgColor(puckHue, 0.90f, 0.8f, puckOpacity));
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStroke(args.vg);

			// Puck center crosshair (4px arms)
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, screenPuckX - 4.0f, screenPuckY);
			nvgLineTo(args.vg, screenPuckX + 4.0f, screenPuckY);
			nvgMoveTo(args.vg, screenPuckX, screenPuckY - 4.0f);
			nvgLineTo(args.vg, screenPuckX, screenPuckY + 4.0f);
			nvgStrokeColor(args.vg, nvgRGBAf(0.08f, 0.12f, 0.20f, puckOpacity));
			nvgStrokeWidth(args.vg, 1.0f);
			nvgStroke(args.vg);
		}

		// 5. Speaker Icons (Speaker dB visualization removed entirely)
		for (int i = 0; i < state.numSpeakers; i++) {
			float spkScreenX = centerX_px + state.speakers[i].x * radiusPx;
			float spkScreenY = centerY_px - state.speakers[i].y * radiusPx;
			float gain = state.speakers[i].gain;

			// Gain halo glow: scales in size and opacity with glowInt
			if (glowInt > 0.005f) {
				float haloR = 16.0f + (gain * 36.0f * glowInt);
				float haloAlpha = clampf(gain * 0.85f * glowInt, 0.0f, 0.85f);
				NVGcolor glowInner = hsvToNvgColor(glowHue, 0.75f, 0.95f, haloAlpha);
				NVGcolor glowOuter = hsvToNvgColor(glowHue, 0.85f, 0.80f, 0.0f);
				NVGpaint haloPaint = nvgRadialGradient(args.vg, spkScreenX, spkScreenY, 8.0f, haloR, glowInner, glowOuter);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, spkScreenX, spkScreenY, haloR);
				nvgFillPaint(args.vg, haloPaint);
				nvgFill(args.vg);
			}

			// Speaker body (opacity controlled by spkOpacity)
			if (spkOpacity > 0.005f) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, spkScreenX, spkScreenY, 13.0f);
				nvgFillColor(args.vg, nvgRGBAf(0.14f, 0.14f, 0.16f, spkOpacity));
				nvgFill(args.vg);
				nvgStrokeColor(args.vg, nvgRGBAf(0.29f, 0.29f, 0.33f, spkOpacity));
				nvgStrokeWidth(args.vg, 1.5f);
				nvgStroke(args.vg);

				// Cone accent: inner circle r = 7 px, brighter when gain > 0.1
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, spkScreenX, spkScreenY, 7.0f);
				if (gain > 0.10f) {
					float cBri = clampf(gain, 0.2f, 1.0f);
					nvgFillColor(args.vg, hsvToNvgColor(glowHue, 0.70f, cBri, 0.95f * spkOpacity));
				} else {
					nvgFillColor(args.vg, nvgRGBAf(0.22f, 0.22f, 0.25f, spkOpacity));
				}
				nvgFill(args.vg);

				// Speaker badge number (1, 2, 3...)
				char numBuf[8];
				std::snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
				nvgFontSize(args.vg, 10.0f);
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgFillColor(args.vg, (gain > 0.35f) ? nvgRGBAf(0.06f, 0.12f, 0.06f, spkOpacity) : nvgRGBAf(1.f, 1.f, 1.f, 0.93f * spkOpacity));
				nvgText(args.vg, spkScreenX, spkScreenY, numBuf, nullptr);
			}
		}
	}
};

struct PansyviewWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Pansyview::SkinTheme currentTheme = Pansyview::THEME_MEDIUM;
	PansyviewDisplayWidget* displayWidget = nullptr;

	void setTheme(Pansyview::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Pansyview::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Pansyview_Light.svg");
					break;
				case Pansyview::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Pansyview_Dark.svg");
					break;
				case Pansyview::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Pansyview.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		Pansyview* module = dynamic_cast<Pansyview*>(this->module);
		if (module && module->skinTheme != currentTheme) {
			setTheme(module->skinTheme);
		}
	}

	PansyviewWidget(Pansyview* module) {
		setModule(module);
		panel = new SvgPanel();
		Pansyview::SkinTheme initTheme = module ? module->skinTheme : Pansyview::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Pansyview::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Pansyview_Light.svg");
				break;
			case Pansyview::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Pansyview_Dark.svg");
				break;
			case Pansyview::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Pansyview.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Display canvas widget: 94mm x 94mm frame at (10.00, 15.00) mm
		displayWidget = new PansyviewDisplayWidget();
		displayWidget->module = module;
		displayWidget->box.pos = mm2px(Vec(10.00f, 15.00f));
		displayWidget->box.size = mm2px(Vec(94.00f, 94.00f));
		addChild(displayWidget);

		// Single Vertical Column for All 9 Knobs (Attenuverter sized):
		// Centered at X = 119.00 mm
		// Y positions: [19.00, 31.25, 43.50, 55.75, 68.00, 80.25, 92.50, 104.75, 117.00] mm
		const float ctrl_x = 119.00f;
		const float knob_y[9] = { 19.00f, 31.25f, 43.50f, 55.75f, 68.00f, 80.25f, 92.50f, 104.75f, 117.00f };

		// Knob 1: Arrangement (Stepped 0..9)
		addParam(createParamCentered<PansyviewLayoutKnob>(mm2px(Vec(ctrl_x, knob_y[0])), module, Pansyview::LAYOUT_PARAM));

		// Knob 2: Location
		addParam(createParamCentered<PansyviewTrimpot>(mm2px(Vec(ctrl_x, knob_y[1])), module, Pansyview::PUCK_OPACITY_PARAM));

		// Knob 3: Loc. Hue
		addParam(createParamCentered<PansyviewTrimpot>(mm2px(Vec(ctrl_x, knob_y[2])), module, Pansyview::PUCK_HUE_PARAM));

		// Knob 4: Trail
		addParam(createParamCentered<PansyviewTrimpot>(mm2px(Vec(ctrl_x, knob_y[3])), module, Pansyview::TRAIL_LENGTH_PARAM));

		// Knob 5: Trail Hue
		addParam(createParamCentered<PansyviewTrimpot>(mm2px(Vec(ctrl_x, knob_y[4])), module, Pansyview::TRAIL_HUE_PARAM));

		// Knob 6: Glow
		addParam(createParamCentered<PansyviewTrimpot>(mm2px(Vec(ctrl_x, knob_y[5])), module, Pansyview::GLOW_INTENSITY_PARAM));

		// Knob 7: Glow Hue
		addParam(createParamCentered<PansyviewTrimpot>(mm2px(Vec(ctrl_x, knob_y[6])), module, Pansyview::GLOW_HUE_PARAM));

		// Knob 8: Speakers
		addParam(createParamCentered<PansyviewTrimpot>(mm2px(Vec(ctrl_x, knob_y[7])), module, Pansyview::SPEAKER_OPACITY_PARAM));

		// Knob 9: Field
		addParam(createParamCentered<PansyviewTrimpot>(mm2px(Vec(ctrl_x, knob_y[8])), module, Pansyview::FIELD_OPACITY_PARAM));

		// Input Jacks Row: 6 inputs below the field visualizer at Y = 118.00 mm
		const float jack_x[6] = { 15.00f, 30.00f, 45.00f, 60.00f, 75.00f, 90.00f };
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(jack_x[0], 118.00f)), module, Pansyview::X_INPUT));
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(jack_x[1], 118.00f)), module, Pansyview::Y_INPUT));
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(jack_x[2], 118.00f)), module, Pansyview::ROTATION_INPUT));
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(jack_x[3], 118.00f)), module, Pansyview::FOCUS_INPUT));
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(jack_x[4], 118.00f)), module, Pansyview::CENTER_INPUT));
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(jack_x[5], 118.00f)), module, Pansyview::SMOOTH_INPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Pansyview* module = dynamic_cast<Pansyview*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));

		const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};

		for (int i = 0; i < 3; i++) {
			Pansyview::SkinTheme theme = (Pansyview::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return module->skinTheme == theme; },
				[=]() {
					module->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}
};

Model* modelPansyview = createModel<Pansyview, PansyviewWidget>("Pansyview");
