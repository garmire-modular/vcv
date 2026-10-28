#include "plugin.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

#if defined(ARCH_WIN) || defined(ARCH_MAC) || defined(ARCH_LIN)
#define GLEW_STATIC
#define GLEW_NO_GLU
#include <GL/glew.h>
#endif

// ─────────────────────────────────────────────────────────────────────
//  SERQET — Dual-Input Dual-Preview CRT Oscilloscope & Video Crossfader
//  Columns:
//   Col 1 (7.00 mm):  Ch 1 X, Y, R, G, B inputs
//   Col 2 (20.32 mm): Ch 2 X, Y, R, G, B inputs
//   Col 3 (50.80 mm): Dual CRT Scopes (Main Out top, Cue bottom with Ch1/Ch2 buttons)
//   Col 4 (81.28 mm): Vertical crossfader (1 -> 2) & CV jack
//   Col 5 (94.60 mm): Mixed X, Y, R, G, B outputs
// ─────────────────────────────────────────────────────────────────────

namespace SerqetConstants {
	static constexpr size_t BUFFER_SIZE = 16384;
	static constexpr float DEFAULT_FOCUS = 0.008f;
	static constexpr float DEFAULT_INTENSITY = 1.0f;
	static constexpr float DEFAULT_AFTERGLOW = 1.0f;
	static constexpr float DEFAULT_EXPOSURE = 1.0f;
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
		CUE_CH1_LIGHT,
		CUE_CH2_LIGHT,
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

	// Dual ring buffers for OpenGL displays
	SerqetRingBuffer mainBuffer;
	SerqetRingBuffer cueBuffer;

	Serqet() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters
		configParam(CROSSFADE_PARAM, 0.0f, 1.0f, 0.0f, "Crossfade", "%", 0.0f, 100.0f);
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
		lights[CUE_CH1_LIGHT].setBrightness(cueSource == 0 ? 1.0f : 0.0f);
		lights[CUE_CH2_LIGHT].setBrightness(cueSource == 1 ? 1.0f : 0.0f);

		// 2. Crossfade alpha
		float fader = params[CROSSFADE_PARAM].getValue();
		if (inputs[XFADE_CV_INPUT].isConnected()) {
			fader += inputs[XFADE_CV_INPUT].getVoltage() * 0.1f; // 0..10V maps to 0..1
		}
		float alpha = clamp(fader, 0.0f, 1.0f);
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

struct SerqetCrtWidget : widget::OpenGlWidget {
	Serqet* module = nullptr;
	bool isMainOut = true; // true = Main Out, false = Cue

	// OpenGL Resources
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint beamProgram = 0;

	// Framebuffer for CRT display
	GLuint lineFbo = 0;
	GLuint lineTex = 0;
	int fboW = 256;
	int fboH = 256;

	SerqetCrtWidget(Serqet* m, bool mainOut) : module(m), isMainOut(mainOut) {
		box.size = mm2px(Vec(44.0f, 44.0f));
	}

	~SerqetCrtWidget() {
		cleanupGl();
	}

	void cleanupGl() {
#if defined(ARCH_WIN) || defined(ARCH_MAC) || defined(ARCH_LIN)
		if (lineFbo) { glDeleteFramebuffers(1, &lineFbo); lineFbo = 0; }
		if (lineTex) { glDeleteTextures(1, &lineTex); lineTex = 0; }
		if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
		if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
		if (beamProgram) { glDeleteProgram(beamProgram); beamProgram = 0; }
#endif
	}

	void initGl() {
#if defined(ARCH_WIN) || defined(ARCH_MAC) || defined(ARCH_LIN)
		if (lineTex != 0) return;

		// 1. Setup Texture & FBO
		glGenTextures(1, &lineTex);
		glBindTexture(GL_TEXTURE_2D, lineTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fboW, fboH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenFramebuffers(1, &lineFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, lineFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lineTex, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// 2. Vertex & Index Buffers
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
#endif
	}

	void drawFramebuffer() override {
#if defined(ARCH_WIN) || defined(ARCH_MAC) || defined(ARCH_LIN)
		initGl();

		GLint prevFbo = 0;
		GLint prevVp[4];
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
		glGetIntegerv(GL_VIEWPORT, prevVp);

		// Render trajectory into FBO
		glBindFramebuffer(GL_FRAMEBUFFER, lineFbo);
		glViewport(0, 0, fboW, fboH);
		glClearColor(0.04f, 0.05f, 0.04f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);

		// Retrieve latest 512 samples from module ring buffer
		if (module) {
			const SerqetRingBuffer& rb = isMainOut ? module->mainBuffer : module->cueBuffer;
			uint32_t head = rb.getHead();

			// Draw analytical beam lines
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();
			glOrtho(-1.1, 1.1, -1.1, 1.1, -1.0, 1.0);

			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadIdentity();

			// CRT Phosphor glow line passes (Wide soft glow + sharp core)
			for (int pass = 0; pass < 2; pass++) {
				if (pass == 0) {
					glLineWidth(3.0f);
				} else {
					glLineWidth(1.0f);
				}

				glBegin(GL_LINE_STRIP);
				for (size_t i = 0; i < 512; i += 2) {
					SerqetSample s = rb.readOffset(head, 512 - i);
					float r = clamp(s.r, 0.0f, 1.0f);
					float g = clamp(s.g, 0.0f, 1.0f);
					float b = clamp(s.b, 0.0f, 1.0f);
					if (pass == 0) {
						// Soft outer phosphor halo
						glColor4f(r * 0.4f, g * 0.4f, b * 0.4f, 0.25f);
					} else {
						// Core white-hot beam
						glColor4f(std::min(1.0f, r + 0.3f), std::min(1.0f, g + 0.3f), std::min(1.0f, b + 0.3f), 0.85f);
					}
					glVertex2f(s.x, s.y);
				}
				glEnd();
			}

			glPopMatrix();
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
		}

		glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
		glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
#endif
	}

	void draw(const DrawArgs& args) override {
		// Draw Framebuffer texture via NanoVG
		widget::OpenGlWidget::draw(args);

		// Draw CRT Vector Graticule Grid
		nvgBeginPath(args.vg);
		float w = box.size.x;
		float h = box.size.y;
		float cx = w * 0.5f;
		float cy = h * 0.5f;

		// Subtle Graticule crosshair
		nvgStrokeColor(args.vg, nvgRGBA(0x00, 0xff, 0x66, 0x22));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgMoveTo(args.vg, 0.0f, cy);
		nvgLineTo(args.vg, w, cy);
		nvgMoveTo(args.vg, cx, 0.0f);
		nvgLineTo(args.vg, cx, h);
		nvgStroke(args.vg);

		// CRT Inner Reticle Sub-divisions
		nvgBeginPath(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(0x00, 0xff, 0x66, 0x15));
		nvgStrokeWidth(args.vg, 0.5f);
		for (int i = 1; i <= 4; i++) {
			float dx = (w * 0.5f) * (i / 4.0f);
			float dy = (h * 0.5f) * (i / 4.0f);
			nvgMoveTo(args.vg, cx - dx, cy - 3.0f); nvgLineTo(args.vg, cx - dx, cy + 3.0f);
			nvgMoveTo(args.vg, cx + dx, cy - 3.0f); nvgLineTo(args.vg, cx + dx, cy + 3.0f);
			nvgMoveTo(args.vg, cx - 3.0f, cy - dy); nvgLineTo(args.vg, cx + 3.0f, cy - dy);
			nvgMoveTo(args.vg, cx - 3.0f, cy + dy); nvgLineTo(args.vg, cx + 3.0f, cy + dy);
		}
		nvgStroke(args.vg);
	}
};

// ─────────────────────────────────────────────────────────────────────
//  SerqetWidget — Panel & UI Layout (20 HP)
// ─────────────────────────────────────────────────────────────────────

struct SerqetPort : app::SvgPort {
	SerqetPort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
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
			math::Vec(box.size.x / 2.0f, halfHandle),
			math::Vec(box.size.x / 2.0f, box.size.y - halfHandle)
		);
	}
};

struct SerqetCueLightRing : ModuleLightWidget {
	SerqetCueLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));
		this->addBaseColor(nvgRGBA(0x00, 0xee, 0x88, 0xff)); // Emerald Green
	}

	void drawBackground(const DrawArgs& args) override {
		Vec c = box.size.div(2.0f);
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
		Vec c = box.size.div(2.0f);
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
		Vec c = box.size.div(2.0f);
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

		// Row Coordinates for Jacks (Bottom-Up: 65, 78, 91, 104, 117 mm)
		const float jack_y[5] = { 65.00f, 78.00f, 91.00f, 104.00f, 117.00f };

		// ── Column 1: Ch 1 Inputs (X, Y, R, G, B) ──
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col1_x, jack_y[0])), module, Serqet::CH1_X_INPUT));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col1_x, jack_y[1])), module, Serqet::CH1_Y_INPUT));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col1_x, jack_y[2])), module, Serqet::CH1_R_INPUT));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col1_x, jack_y[3])), module, Serqet::CH1_G_INPUT));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col1_x, jack_y[4])), module, Serqet::CH1_B_INPUT));

		// ── Column 2: Ch 2 Inputs (X, Y, R, G, B) ──
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col2_x, jack_y[0])), module, Serqet::CH2_X_INPUT));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col2_x, jack_y[1])), module, Serqet::CH2_Y_INPUT));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col2_x, jack_y[2])), module, Serqet::CH2_R_INPUT));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col2_x, jack_y[3])), module, Serqet::CH2_G_INPUT));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col2_x, jack_y[4])), module, Serqet::CH2_B_INPUT));

		// ── Column 3: Dual Oscilloscopes ──
		// Scope 1 (Main Out): Center (50.80, 39.00 mm), Size 44 x 44 mm
		auto* scope1 = new SerqetCrtWidget(module, true);
		scope1->box.pos = mm2px(Vec(col3_x - 22.0f, 39.00f - 22.0f));
		addChild(scope1);

		// Scope 2 (Cue Monitor): Center (50.80, 89.00 mm), Size 44 x 44 mm
		auto* scope2 = new SerqetCrtWidget(module, false);
		scope2->box.pos = mm2px(Vec(col3_x - 22.0f, 89.00f - 22.0f));
		addChild(scope2);

		// Cue Buttons with concentric light rings (Y = 117.00 mm)
		const float btn1_x = col3_x - 7.50f;
		const float btn2_x = col3_x + 7.50f;

		addChild(createLightCentered<SerqetCueLightRing>(mm2px(Vec(btn1_x, 117.00f)), module, Serqet::CUE_CH1_LIGHT));
		addParam(createParamCentered<SerqetButton>(mm2px(Vec(btn1_x, 117.00f)), module, Serqet::CUE_CH1_PARAM));

		addChild(createLightCentered<SerqetCueLightRing>(mm2px(Vec(btn2_x, 117.00f)), module, Serqet::CUE_CH2_LIGHT));
		addParam(createParamCentered<SerqetButton>(mm2px(Vec(btn2_x, 117.00f)), module, Serqet::CUE_CH2_PARAM));

		// ── Column 4: Crossfader Slider & CV Jack ──
		// Slider Center at (81.28, 57.00 mm)
		addParam(createParamCentered<SerqetFader>(mm2px(Vec(col4_x, 57.00f)), module, Serqet::CROSSFADE_PARAM));
		addInput(createInputCentered<SerqetPort>(mm2px(Vec(col4_x, 117.00f)), module, Serqet::XFADE_CV_INPUT));

		// ── Column 5: Outputs (X, Y, R, G, B) ──
		addOutput(createOutputCentered<SerqetPort>(mm2px(Vec(col5_x, jack_y[0])), module, Serqet::X_OUTPUT));
		addOutput(createOutputCentered<SerqetPort>(mm2px(Vec(col5_x, jack_y[1])), module, Serqet::Y_OUTPUT));
		addOutput(createOutputCentered<SerqetPort>(mm2px(Vec(col5_x, jack_y[2])), module, Serqet::R_OUTPUT));
		addOutput(createOutputCentered<SerqetPort>(mm2px(Vec(col5_x, jack_y[3])), module, Serqet::G_OUTPUT));
		addOutput(createOutputCentered<SerqetPort>(mm2px(Vec(col5_x, jack_y[4])), module, Serqet::B_OUTPUT));
	}
};

Model* modelSerqet = createModel<Serqet, SerqetWidget>("Serqet");
