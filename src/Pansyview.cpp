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
} // namespace

struct Pansyview : Module {
	enum ParamIds {
		LAYOUT_PARAM,
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

	Pansyview() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Layout selector knob: 10 discrete positions [0 to 9]
		configSwitch(LAYOUT_PARAM, 0.0f, 9.0f, 1.0f, "Speaker layout", {
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
		// X = 0V, Y = 0V, Rotation = 0V, Focus = 5V (50), Center = 5V (50)
		float xVolt = inputs[X_INPUT].isConnected() ? inputs[X_INPUT].getVoltage() : 0.0f;
		float yVolt = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getVoltage() : 0.0f;
		float rotVolt = inputs[ROTATION_INPUT].isConnected() ? inputs[ROTATION_INPUT].getVoltage() : 0.0f;
		float focVolt = inputs[FOCUS_INPUT].isConnected() ? inputs[FOCUS_INPUT].getVoltage() : 5.0f;
		float ctrVolt = inputs[CENTER_INPUT].isConnected() ? inputs[CENTER_INPUT].getVoltage() : 5.0f;

		// Normalization per spec:
		float normX = clampf(xVolt / 5.0f, -1.0f, 1.0f);
		float normY = clampf(yVolt / 5.0f, -1.0f, 1.0f);
		float rotationDeg = clampf(rotVolt / 5.0f * 180.0f, -180.0f, 180.0f);
		float focus = clampf(focVolt / 10.0f * 100.0f, 0.0f, 100.0f);
		float center = clampf(ctrVolt / 10.0f * 100.0f, 0.0f, 100.0f);

		// Apply rotation to incoming X/Y before gain calc and display:
		float radR = rotationDeg * (float)M_PI / 180.0f;
		float cosR = std::cos(radR);
		float sinR = std::sin(radR);
		float rotX = normX * cosR - normY * sinR;
		float rotY = normX * sinR + normY * cosR;

		int layout = (int)clampf(std::round(params[LAYOUT_PARAM].getValue()), 0.0f, 9.0f);

		// Gain calculation (constant-power inverse-distance per spec):
		float focusNorm = focus / 100.0f;
		float powerExp = 0.6f + (focusNorm * focusNorm) * 4.4f; // [0.6, 5.0]
		float diffuseFloor = (1.0f - focusNorm) * 0.35f;        // [0.35, 0.0]
		float centerNorm = center / 100.0f;
		float distFromCenter = std::sqrt(rotX * rotX + rotY * rotY);

		std::vector<SpeakerDef> spkList;
		getSpeakerPositions(layout, spkList);
		int nSpeakers = (int)spkList.size();

		float sumSq = 0.0f;
		float weights[8];
		for (int i = 0; i < nSpeakers; i++) {
			float sx = spkList[i].x;
			float sy = spkList[i].y;
			float dist = std::sqrt((rotX - sx) * (rotX - sx) + (rotY - sy) * (rotY - sy));
			float w = 1.0f / std::pow(dist + 0.18f, powerExp);
			w = (1.0f - diffuseFloor) * w + diffuseFloor;

			if (distFromCenter < 0.35f) {
				float centerFactor = 1.0f - (distFromCenter / 0.35f);
				w *= (0.5f + 0.5f * centerNorm * centerFactor);
			}
			weights[i] = w;
			sumSq += w * w;
		}

		float normFactor = (sumSq > 1e-6f) ? std::sqrt(sumSq) : 1.0f;

		// Buffer state for UI thread
		visualState.rotX = rotX;
		visualState.rotY = rotY;
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

// ── Custom Hardware Widgets ──
struct PansyviewLayoutKnob : RoundKnob {
	PansyviewLayoutKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
		snap = true;
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
	const size_t maxTrail = 50;

	// 5x5 Reference Fraction Table for Ableton Live Dispersion Disc Radius (as fraction of field radius)
	const float G[5][5] = {
		{1.000f, 1.000f, 1.000f, 1.000f, 1.000f},
		{0.618f, 0.717f, 0.825f, 0.943f, 1.000f},
		{0.451f, 0.534f, 0.630f, 0.743f, 0.884f},
		{0.361f, 0.436f, 0.525f, 0.636f, 0.784f},
		{0.302f, 0.372f, 0.457f, 0.567f, 0.730f}
	};

	float calcDispersionRadius(float focus, float center, float radiusPx) {
		// Map parameters to grid coordinates [0, 4]
		float rf = (focus / 100.0f) * 4.0f;
		float rc = (center / 100.0f) * 4.0f;

		// Grid cell indices capped at 3
		int i = (int)clampf(std::floor(rf), 0.0f, 3.0f);
		int j = (int)clampf(std::floor(rc), 0.0f, 3.0f);

		// Fractional position within cell
		float df = rf - (float)i;
		float dc = rc - (float)j;

		// Independent cubic smoothstep S-curves: s(t) = 3t² - 2t³
		float sf = df * df * (3.0f - 2.0f * df);
		float sc = dc * dc * (3.0f - 2.0f * dc);

		// 2D bilinear interpolation using smoothstep weights
		float radius_frac =
			(1.0f - sf) * (1.0f - sc) * G[i][j] +
			(1.0f - sf) * sc          * G[i][j + 1] +
			sf          * (1.0f - sc) * G[i + 1][j] +
			sf          * sc          * G[i + 1][j + 1];

		return radius_frac * radiusPx;
	}

	void draw(const DrawArgs& args) override {
		Pansyview::VisualState state;
		if (module) {
			state = module->visualState;
		}

		float w = box.size.x;
		float h = box.size.y;
		float radiusPx = (std::min(w, h) / 2.0f) * 0.88f;
		float centerX_px = w / 2.0f;
		float centerY_px = h / 2.0f;

		// 1. Dark background canvas
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 6.0f);
		nvgFillColor(args.vg, nvgRGBA(0x12, 0x12, 0x14, 0xff));
		nvgFill(args.vg);

		// 2. Concentric rings and grid
		// Rings at r = 0.25, 0.50, 0.75
		const float ringFracs[] = {0.25f, 0.50f, 0.75f};
		for (float f : ringFracs) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, centerX_px, centerY_px, radiusPx * f);
			nvgStrokeColor(args.vg, nvgRGBA(0x35, 0x35, 0x3c, 0x88));
			nvgStrokeWidth(args.vg, 1.0f);
			nvgStroke(args.vg);
		}

		// Outer boundary circle at r = 1.0
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, centerX_px, centerY_px, radiusPx);
		nvgStrokeColor(args.vg, nvgRGBA(0x60, 0x60, 0x70, 0xcc));
		nvgStrokeWidth(args.vg, 1.5f);
		nvgStroke(args.vg);

		// X and Y Axes (dashed or faint solid line)
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, centerX_px - radiusPx, centerY_px);
		nvgLineTo(args.vg, centerX_px + radiusPx, centerY_px);
		nvgMoveTo(args.vg, centerX_px, centerY_px - radiusPx);
		nvgLineTo(args.vg, centerX_px, centerY_px + radiusPx);
		nvgStrokeColor(args.vg, nvgRGBA(0x40, 0x40, 0x4a, 0x88));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);

		// Center crosshair (7px arms)
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, centerX_px - 7.0f, centerY_px);
		nvgLineTo(args.vg, centerX_px + 7.0f, centerY_px);
		nvgMoveTo(args.vg, centerX_px, centerY_px - 7.0f);
		nvgLineTo(args.vg, centerX_px, centerY_px + 7.0f);
		nvgStrokeColor(args.vg, nvgRGBA(0x88, 0x88, 0x99, 0xaa));
		nvgStrokeWidth(args.vg, 1.2f);
		nvgStroke(args.vg);

		// Cardinal labels
		nvgFontSize(args.vg, 10.0f);
		nvgFontFaceId(args.vg, 0); // standard font
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGBA(0x80, 0x80, 0x90, 0xee));

		nvgText(args.vg, centerX_px, centerY_px - radiusPx - 9.0f, "FRONT", nullptr);
		nvgText(args.vg, centerX_px, centerY_px + radiusPx + 9.0f, "REAR", nullptr);
		nvgText(args.vg, centerX_px - radiusPx - 16.0f, centerY_px, "LEFT", nullptr);
		nvgText(args.vg, centerX_px + radiusPx + 18.0f, centerY_px, "RIGHT", nullptr);

		// 3. Puck Position & Trail
		float screenPuckX = centerX_px + state.rotX * radiusPx;
		float screenPuckY = centerY_px - state.rotY * radiusPx;

		trail.push_back({screenPuckX, screenPuckY});
		if (trail.size() > maxTrail) {
			trail.erase(trail.begin());
		}

		// Draw Trail polyline
		if (trail.size() > 1) {
			for (size_t t = 1; t < trail.size(); t++) {
				float frac = (float)t / (float)trail.size();
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, trail[t - 1].first, trail[t - 1].second);
				nvgLineTo(args.vg, trail[t].first, trail[t].second);
				nvgStrokeColor(args.vg, nvgRGBAf(0.25f, 0.70f, 1.0f, frac * 0.7f));
				nvgStrokeWidth(args.vg, 1.0f + frac * 2.5f);
				nvgStroke(args.vg);
			}
		}

		// 4. Dispersion Disc around Puck
		// Centered on puck (screenPuckX, screenPuckY), strictly masked/clipped to acoustic field boundary
		float dispR = calcDispersionRadius(state.focus, state.center, radiusPx);

		// Draw shaded dispersion disc
		NVGpaint discPaint = nvgRadialGradient(args.vg, screenPuckX, screenPuckY, 0.0f, dispR,
			nvgRGBA(0x30, 0x90, 0xff, 0x66), nvgRGBA(0x10, 0x50, 0xcc, 0x08));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, screenPuckX, screenPuckY, dispR);
		nvgFillPaint(args.vg, discPaint);
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, screenPuckX, screenPuckY, dispR);
		nvgStrokeColor(args.vg, nvgRGBA(0x40, 0xa0, 0xff, 0x88));
		nvgStrokeWidth(args.vg, 1.2f);
		nvgStroke(args.vg);

		// Stencil/Mask outer boundary: clear any disc bleed that extends outside the r = radiusPx field circle
		// by drawing an inverted annular mask in the canvas background color (#121214) with CW inner and CCW outer
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, w, h);
		nvgCircle(args.vg, centerX_px, centerY_px, radiusPx);
		nvgPathWinding(args.vg, NVG_HOLE);
		nvgFillColor(args.vg, nvgRGBA(0x12, 0x12, 0x14, 0xff));
		nvgFill(args.vg);

		// Re-stroke field boundary line so it stays sharp above mask
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, centerX_px, centerY_px, radiusPx);
		nvgStrokeColor(args.vg, nvgRGBA(0x60, 0x60, 0x70, 0xcc));
		nvgStrokeWidth(args.vg, 1.5f);
		nvgStroke(args.vg);

		// Core puck ring (filled circle r=7, surrounding stroke r=8)
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, screenPuckX, screenPuckY, 7.0f);
		nvgFillColor(args.vg, nvgRGBA(0xe0, 0xf0, 0xff, 0xff));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, screenPuckX, screenPuckY, 8.5f);
		nvgStrokeColor(args.vg, nvgRGBA(0x10, 0x60, 0xd0, 0xff));
		nvgStrokeWidth(args.vg, 1.5f);
		nvgStroke(args.vg);

		// Puck center crosshair (4px arms)
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, screenPuckX - 4.0f, screenPuckY);
		nvgLineTo(args.vg, screenPuckX + 4.0f, screenPuckY);
		nvgMoveTo(args.vg, screenPuckX, screenPuckY - 4.0f);
		nvgLineTo(args.vg, screenPuckX, screenPuckY + 4.0f);
		nvgStrokeColor(args.vg, nvgRGBA(0x15, 0x20, 0x35, 0xff));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStroke(args.vg);

		// 5. Speaker Icons
		for (int i = 0; i < state.numSpeakers; i++) {
			float spkScreenX = centerX_px + state.speakers[i].x * radiusPx;
			float spkScreenY = centerY_px - state.speakers[i].y * radiusPx;
			float gain = state.speakers[i].gain;
			float db = state.speakers[i].dB;

			// Gain halo glow: radial gradient glow, radius = 16 + gain * 36 px
			float haloR = 16.0f + gain * 36.0f;
			float haloAlpha = clampf(gain * 0.85f, 0.0f, 0.85f);
			NVGpaint haloPaint = nvgRadialGradient(args.vg, spkScreenX, spkScreenY, 8.0f, haloR,
				nvgRGBAf(0.2f, 0.85f, 0.4f, haloAlpha), nvgRGBAf(0.1f, 0.8f, 0.3f, 0.0f));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, spkScreenX, spkScreenY, haloR);
			nvgFillPaint(args.vg, haloPaint);
			nvgFill(args.vg);

			// Speaker body: filled circle r = 13 px
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, spkScreenX, spkScreenY, 13.0f);
			nvgFillColor(args.vg, nvgRGBA(0x24, 0x24, 0x28, 0xff));
			nvgFill(args.vg);
			nvgStrokeColor(args.vg, nvgRGBA(0x4a, 0x4a, 0x55, 0xff));
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStroke(args.vg);

			// Cone accent: inner circle r = 7 px, brighter when gain > 0.1
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, spkScreenX, spkScreenY, 7.0f);
			if (gain > 0.10f) {
				float cBri = clampf(gain, 0.2f, 1.0f);
				nvgFillColor(args.vg, nvgRGBAf(0.2f * cBri, 0.9f * cBri, 0.4f * cBri, 0.95f));
			} else {
				nvgFillColor(args.vg, nvgRGBA(0x38, 0x38, 0x40, 0xff));
			}
			nvgFill(args.vg);

			// Speaker badge number (1, 2, 3...)
			char numBuf[8];
			std::snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
			nvgFontSize(args.vg, 10.0f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, (gain > 0.35f) ? nvgRGBA(0x10, 0x20, 0x10, 0xff) : nvgRGBA(0xff, 0xff, 0xff, 0xee));
			nvgText(args.vg, spkScreenX, spkScreenY, numBuf, nullptr);

			// dB Badge: inward toward field center (offset ≈ 22 px from speaker center)
			if (gain >= 0.05f) {
				float dxToCenter = centerX_px - spkScreenX;
				float dyToCenter = centerY_px - spkScreenY;
				float len = std::sqrt(dxToCenter * dxToCenter + dyToCenter * dyToCenter);
				float badgeX = spkScreenX;
				float badgeY = spkScreenY;
				if (len > 1e-4f) {
					badgeX += (dxToCenter / len) * 22.0f;
					badgeY += (dyToCenter / len) * 22.0f;
				} else {
					badgeY += 22.0f;
				}

				char dbBuf[16];
				std::snprintf(dbBuf, sizeof(dbBuf), "%.1f dB", db);

				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, badgeX - 19.0f, badgeY - 7.0f, 38.0f, 14.0f, 3.0f);
				nvgFillColor(args.vg, nvgRGBA(0x18, 0x18, 0x1e, 0xee));
				nvgFill(args.vg);
				nvgStrokeColor(args.vg, nvgRGBA(0x38, 0x78, 0x40, 0xaa));
				nvgStrokeWidth(args.vg, 1.0f);
				nvgStroke(args.vg);

				nvgFontSize(args.vg, 8.5f);
				nvgFillColor(args.vg, nvgRGBA(0x50, 0xf0, 0x70, 0xff));
				nvgText(args.vg, badgeX, badgeY + 0.5f, dbBuf, nullptr);
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

		// Display canvas widget: 98mm x 98mm frame at (10.00, 15.00) mm
		displayWidget = new PansyviewDisplayWidget();
		displayWidget->module = module;
		displayWidget->box.pos = mm2px(Vec(10.00f, 15.00f));
		displayWidget->box.size = mm2px(Vec(98.00f, 98.00f));
		addChild(displayWidget);

		// Control Column: X = 119.00 mm
		const float ctrl_x = 119.00f;

		// 10-Position Layout Selector Knob at Y = 23.00 mm
		addParam(createParamCentered<PansyviewLayoutKnob>(mm2px(Vec(ctrl_x, 23.00f)), module, Pansyview::LAYOUT_PARAM));

		// CV Inputs (stacked vertically):
		// Jack 1: X (Center Y = 46.00 mm)
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(ctrl_x, 46.00f)), module, Pansyview::X_INPUT));

		// Jack 2: Y (Center Y = 60.50 mm)
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(ctrl_x, 60.50f)), module, Pansyview::Y_INPUT));

		// Jack 3: Rotation (Center Y = 75.00 mm)
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(ctrl_x, 75.00f)), module, Pansyview::ROTATION_INPUT));

		// Jack 4: Focus (Center Y = 89.50 mm)
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(ctrl_x, 89.50f)), module, Pansyview::FOCUS_INPUT));

		// Jack 5: Center (Center Y = 104.00 mm)
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(ctrl_x, 104.00f)), module, Pansyview::CENTER_INPUT));

		// Jack 6: Smooth (Center Y = 118.00 mm)
		addInput(createInputCentered<PansyviewPort>(mm2px(Vec(ctrl_x, 118.00f)), module, Pansyview::SMOOTH_INPUT));
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
