#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef METAMODULE
#include <osdialog.h>
#else
#include "filesystem/async_filebrowser.hh"
#define NANOSVG_CPLUSPLUS
#define NANOSVG_IMPLEMENTATION
namespace silvia_nsvg {
#include <nanosvg.h>
}
#endif

// ─────────────────────────────────────────────────────────────────────
//  Silvia — SVG-to-XY Vector Trajectory Generator (10 HP, v1.0.1)
//  Generate class module opening any SVG file (defaulting to garmire.svg)
//  and outputting real-time X/Y deflection voltages and Sync pulses.
//  Features:
//    - Row 1: Standard Freq, Time-scale (Range), Fine controls
//    - Row 2: Scale (0-3x chained X/Y), Rotate (±180°), Revolve (fake 3D)
//    - Zone 3: Scale X, Scale Y, Rotate, Revolve CV attenuverters
//    - Zone 4: Scale X/Y/Rotate/Revolve CV inputs, Sync In, X/Y/Sync Out
//    - Context menu: Load SVG, Reset to Default, Invert SVG (B/W),
//      Subpath Traversal (Slewed / Direct Jump), Revolve Mode (3D / Ortho),
//      Oscillator Range, 6 Voltage Ranges, and Triple Skin Theme.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Normalized, Illustrator-fixed black-and-white default Garmire SVG
static const char* DEFAULT_GARMIRE_SVG =
	"<?xml version='1.0' encoding='utf-8'?>\n"
	"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 36.52 59.35\" width=\"36.520px\" height=\"59.350px\">"
	"<defs /><g id=\"Layer_2\"><g id=\"Text\">"
	"<path d=\"M 29.68,59.35 H 2 a 1.93,1.93,0,0,1 -1.5 -.7 A 2,2,0,0,1,0,57.05 L 9.8,1.62 A 2,2,0,0,1,11.24.06 "
	"a 2,2,0,0,1,2,.65 L 24.66,14.57 a 2,2,0,1,1 -3,2.49 L 12.93,6.49,4.3,55.43 H 28 l 4.2 -23.79 H 20.71 "
	"a 2,2,0,0,1,0 -3.92 H 34.56 A 2,2,0,0,1,36.5,30 L 31.61,57.73 a 2,2,0,0,1 -1.93,1.62 Z\" fill=\"#000000\" />"
	"</g></g></svg>\n";

static std::string trimCopy(const std::string& s) {
	size_t a = 0;
	while (a < s.size() && std::isspace((unsigned char)s[a])) a++;
	size_t b = s.size();
	while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
	return s.substr(a, b - a);
}

// Lightweight CSS class inliner for raw Illustrator SVG exports (<style>.cls-1{fill:#ffffff;}</style>)
// Runs strictly on UI/load thread, never in process().
static std::string preprocessSvgStyles(const std::string& input) {
	std::unordered_map<std::string, std::string> classProps;
	size_t pos = 0;
	while (true) {
		size_t styleStart = input.find("<style", pos);
		if (styleStart == std::string::npos) break;
		size_t tagEnd = input.find('>', styleStart);
		if (tagEnd == std::string::npos) break;
		size_t styleClose = input.find("</style>", tagEnd);
		if (styleClose == std::string::npos) break;

		std::string css = input.substr(tagEnd + 1, styleClose - (tagEnd + 1));
		size_t cpos = 0;
		while (cpos < css.size()) {
			size_t bOpen = css.find('{', cpos);
			if (bOpen == std::string::npos) break;
			size_t bClose = css.find('}', bOpen);
			if (bClose == std::string::npos) break;

			std::string selectors = css.substr(cpos, bOpen - cpos);
			std::string decls = css.substr(bOpen + 1, bClose - (bOpen + 1));

			// Build inline XML attribute string for fill/stroke/opacity
			std::string attrStr;
			size_t dp = 0;
			while (dp < decls.size()) {
				size_t semi = decls.find(';', dp);
				std::string item = (semi == std::string::npos) ? decls.substr(dp) : decls.substr(dp, semi - dp);
				dp = (semi == std::string::npos) ? decls.size() : semi + 1;
				size_t colon = item.find(':');
				if (colon == std::string::npos) continue;
				std::string k = trimCopy(item.substr(0, colon));
				std::string v = trimCopy(item.substr(colon + 1));
				for (char& ch : k) ch = (char)std::tolower((unsigned char)ch);
				if (k == "fill" || k == "stroke" || k == "stroke-width" || k == "opacity" || k == "fill-opacity" || k == "stroke-opacity") {
					attrStr += " " + k + "=\"" + v + "\"";
				}
			}

			if (!attrStr.empty()) {
				size_t sp = 0;
				while (sp < selectors.size()) {
					size_t comma = selectors.find(',', sp);
					std::string sel = trimCopy((comma == std::string::npos) ? selectors.substr(sp) : selectors.substr(sp, comma - sp));
					sp = (comma == std::string::npos) ? selectors.size() : comma + 1;
					if (!sel.empty() && sel[0] == '.') {
						std::string clsName = sel.substr(1);
						classProps[clsName] += attrStr;
					}
				}
			}
			cpos = bClose + 1;
		}
		pos = styleClose + 8;
	}

	if (classProps.empty()) return input;

	std::string out;
	out.reserve(input.size() + 256);
	size_t i = 0;
	while (i < input.size()) {
		// Strip <style>...</style> blocks so NanoSVG stays clean
		if (input.compare(i, 6, "<style") == 0) {
			size_t close = input.find("</style>", i);
			if (close != std::string::npos) {
				i = close + 8;
				continue;
			}
		}
		if (input.compare(i, 7, "class=\"") == 0) {
			size_t endQ = input.find('"', i + 7);
			if (endQ != std::string::npos) {
				std::string clsList = input.substr(i + 7, endQ - (i + 7));
				std::string injected;
				size_t cp = 0;
				while (cp < clsList.size()) {
					while (cp < clsList.size() && std::isspace((unsigned char)clsList[cp])) cp++;
					size_t ce = cp;
					while (ce < clsList.size() && !std::isspace((unsigned char)clsList[ce])) ce++;
					if (ce > cp) {
						auto it = classProps.find(clsList.substr(cp, ce - cp));
						if (it != classProps.end()) injected += it->second;
					}
					cp = ce;
				}
				if (!injected.empty()) {
					out += injected;
					i = endQ + 1;
					continue;
				}
			}
		}
		out.push_back(input[i]);
		i++;
	}
	return out;
}

static std::string extractFilenameOnly(const std::string& path) {
	if (path.empty()) return "garmire.svg";
	size_t slash = path.find_last_of("/\\");
	return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

} // namespace

struct Silvia : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,           // 0.01 Hz - 200 Hz
		RANGE_VCO            // 150 Hz - 2000 Hz
	};
	RangeMode rangeMode = RANGE_LFO;

	// Mandatory 6 output voltage standards per docs/classes/generate.md
	enum VoltageRange {
		RANGE_UNIPOLAR_1V = 0,
		RANGE_BIPOLAR_1V,
		RANGE_UNIPOLAR_5V,
		RANGE_BIPOLAR_5V,    // Default ±5V
		RANGE_UNIPOLAR_10V,
		RANGE_BIPOLAR_10V,
		NUM_VOLTAGE_RANGES
	};
	VoltageRange voltageRange = RANGE_BIPOLAR_5V;

	enum TraversalMode {
		TRAVERSAL_SLEWED = 0,      // Option A (Default): smooth slew between subpaths
		TRAVERSAL_DIRECT_JUMP = 1  // Option B: direct jump between subpaths + Sync Out pulse
	};
	TraversalMode traversalMode = TRAVERSAL_SLEWED;

	enum RevolveMode {
		REVOLVE_PERSPECTIVE = 0,   // Option A (Default): 3D yaw with perspective foreshortening
		REVOLVE_ORTHOGRAPHIC = 1   // Option B: linear orthographic yaw rotation
	};
	RevolveMode revolveMode = REVOLVE_PERSPECTIVE;

	enum ParamId {
		// Row 1: Standard Freq, Time-scale (Range), Fine
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,

		// Row 2: Scale (0-3x chained X/Y), Rotate (±180°), Revolve (fake 3D)
		SCALE_PARAM,
		ROTATE_PARAM,
		REVOLVE_PARAM,

		// Zone 3: CV Attenuverters
		SCALE_X_TRIM_PARAM,
		SCALE_Y_TRIM_PARAM,
		ROTATE_TRIM_PARAM,
		REVOLVE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Zone 4 Upper Row: CV Inputs
		SCALE_X_CV_INPUT,
		SCALE_Y_CV_INPUT,
		ROTATE_CV_INPUT,
		REVOLVE_CV_INPUT,

		// Zone 4 Lower Row: Sync In
		SYNC_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Zone 4 Lower Row: X Out, Y Out, Sync Out
		X_OUTPUT,
		Y_OUTPUT,
		SYNC_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_ORANGE,
		RANGE_LIGHT_PURPLE,

		LIGHTS_LEN
	};

	// ── Pre-allocated Lock-Free Triple Buffer for Real-Time Safe SVG Trajectory ──
	static constexpr int MAX_TRAJ_POINTS = 2048;

	struct TrajPoint {
		float x;
		float y;
		bool isJump;
		TrajPoint(float x_ = 0.f, float y_ = 0.f, bool j_ = false) : x(x_), y(y_), isJump(j_) {}
	};

	struct TrajectoryData {
		std::array<TrajPoint, MAX_TRAJ_POINTS> pts;
		int count = 0;
	};

	// Triple buffer slots: UI owns uiBufIdx, Audio owns audioBufIdx, midBufIdx is atomic exchange
	TrajectoryData trajBuffers[3];
	int uiBufIdx = 0;
	int audioBufIdx = 1;
	std::atomic<int> midBufIdx{2};

	// UI-thread preview copy and SVG metadata (never touched by audio thread)
	std::mutex uiMutex;
	TrajectoryData uiPreviewCopy;
	std::string svgPath;
	std::string loadedRawSvg = DEFAULT_GARMIRE_SVG;
	bool invertSvg = false;

	// Live preview phase indicator (written by audio voice 0, read by UI draw)
	std::atomic<float> displayPhase{0.f};

	dsp::BooleanTrigger rangeTrigger;

	struct VoiceState {
		float basePhase = 0.f;
		int lastIdx = 0;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		dsp::SchmittTrigger syncTrigger;
		dsp::PulseGenerator syncPulse;
	};
	VoiceState voices[16];

	// ── Standard Garmire Generate FreqParamQuantity ──
	struct FreqParamQuantity : ParamQuantity {
		Silvia* getSilvia() {
			return dynamic_cast<Silvia*>(module);
		}

		float getDisplayValue() override {
			Silvia* mod = getSilvia();
			if (!mod) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (mod->paramQuantities.size() > Silvia::FINE_PARAM) ? mod->params[Silvia::FINE_PARAM].getValue() : 0.f;

			if (mod->rangeMode == Silvia::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (mod->rangeMode == Silvia::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Silvia* mod = getSilvia();
			if (!mod) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (mod->rangeMode == Silvia::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				if (val < 10.0f) {
					std::snprintf(buf, sizeof(buf), "%.3f", val);
				} else if (val < 100.0f) {
					std::snprintf(buf, sizeof(buf), "%.2f", val);
				} else {
					std::snprintf(buf, sizeof(buf), "%.1f", val);
				}
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Silvia* mod = getSilvia();
			if (!mod) return;

			float fine = (mod->paramQuantities.size() > Silvia::FINE_PARAM) ? mod->params[Silvia::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (mod->rangeMode == Silvia::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (mod->rangeMode == Silvia::RANGE_LFO) {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 0.01f);
				float fCoarse = targetHz - fine * 20.0f;
				if (fCoarse <= 0.01f) {
					newCoarse = 0.0f;
				} else {
					newCoarse = std::sqrt((fCoarse - 0.01f) / 199.99f);
				}
			} else {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 100.f);
				float fineFactor = 1.0f + fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float fCoarse = targetHz / fineFactor;
				if (fCoarse <= 1.0f) fCoarse = 1.0f;

				float ratio = fCoarse / 150.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(2000.0f / 150.0f);
			}

			setValue(clampf(newCoarse, 0.0f, 1.0f));
		}

		void setDisplayValue(float displayValue) override {
			Silvia* mod = getSilvia();
			if (!mod) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (mod->rangeMode == Silvia::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Silvia* mod = getSilvia();
			if (!mod) {
				ParamQuantity::setDisplayValueString(s);
				return;
			}

			std::string str = trimCopy(s);
			if (str.empty()) return;

			std::string lowerStr = str;
			for (char& c : lowerStr) {
				c = (char)std::tolower((unsigned char)c);
			}

			enum UnitType { UNIT_DEFAULT, UNIT_HZ, UNIT_KHZ, UNIT_MHZ, UNIT_SEC, UNIT_MS };
			UnitType unitType = UNIT_DEFAULT;

			if (lowerStr.length() >= 4 && lowerStr.substr(lowerStr.length() - 4) == "secs") {
				unitType = UNIT_SEC;
				str = str.substr(0, str.length() - 4);
			} else if (lowerStr.length() >= 3 && lowerStr.substr(lowerStr.length() - 3) == "sec") {
				unitType = UNIT_SEC;
				str = str.substr(0, str.length() - 3);
			} else if (lowerStr.length() >= 3 && lowerStr.substr(lowerStr.length() - 3) == "khz") {
				unitType = UNIT_KHZ;
				str = str.substr(0, str.length() - 3);
			} else if (lowerStr.length() >= 3 && lowerStr.substr(lowerStr.length() - 3) == "mhz") {
				unitType = UNIT_MHZ;
				str = str.substr(0, str.length() - 3);
			} else if (lowerStr.length() >= 2 && lowerStr.substr(lowerStr.length() - 2) == "hz") {
				unitType = UNIT_HZ;
				str = str.substr(0, str.length() - 2);
			} else if (lowerStr.length() >= 2 && lowerStr.substr(lowerStr.length() - 2) == "ms") {
				unitType = UNIT_MS;
				str = str.substr(0, str.length() - 2);
			} else if (!lowerStr.empty() && lowerStr.back() == 's') {
				unitType = UNIT_SEC;
				str.pop_back();
			} else if (!lowerStr.empty() && lowerStr.back() == 'k') {
				unitType = UNIT_KHZ;
				str.pop_back();
			}

			str = trimCopy(str);
			char* endPtr = nullptr;
			float rawVal = std::strtof(str.c_str(), &endPtr);
			if (endPtr == str.c_str()) return;

			bool isPeriod;
			float finalVal;

			if (unitType == UNIT_SEC) {
				isPeriod = true;
				finalVal = rawVal;
			} else if (unitType == UNIT_MS) {
				isPeriod = true;
				finalVal = rawVal * 0.001f;
			} else if (unitType == UNIT_HZ) {
				isPeriod = false;
				finalVal = rawVal;
			} else if (unitType == UNIT_KHZ) {
				isPeriod = false;
				finalVal = rawVal * 1000.0f;
			} else if (unitType == UNIT_MHZ) {
				isPeriod = false;
				finalVal = rawVal * 1000000.0f;
			} else {
				isPeriod = (mod->rangeMode == Silvia::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Silvia* mod = getSilvia();
			if (!mod) return "";
			return (mod->rangeMode == Silvia::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	Silvia() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Standard Freq, Time-scale (Range), Fine
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		// Row 2: Scale (up to 3x), Rotate (±180°), Revolve (fake 3D)
		configParam(SCALE_PARAM, 0.f, 3.f, 1.f, "Scale", "×");
		configParam(ROTATE_PARAM, -180.f, 180.f, 0.f, "Rotate", "°");
		configParam(REVOLVE_PARAM, -180.f, 180.f, 0.f, "Revolve", "°");

		// Zone 3: CV Attenuverters (strictly '<Parameter> CV depth')
		configParam(SCALE_X_TRIM_PARAM, -1.f, 1.f, 0.f, "Scale X CV depth", "%", 0.f, 100.f);
		configParam(SCALE_Y_TRIM_PARAM, -1.f, 1.f, 0.f, "Scale Y CV depth", "%", 0.f, 100.f);
		configParam(ROTATE_TRIM_PARAM, -1.f, 1.f, 0.f, "Rotate CV depth", "%", 0.f, 100.f);
		configParam(REVOLVE_TRIM_PARAM, -1.f, 1.f, 0.f, "Revolve CV depth", "%", 0.f, 100.f);

		// Zone 4: Inputs (strictly avoiding redundant 'input'/'in')
		configInput(SCALE_X_CV_INPUT, "Scale X CV");
		configInput(SCALE_Y_CV_INPUT, "Scale Y CV");
		configInput(ROTATE_CV_INPUT, "Rotate CV");
		configInput(REVOLVE_CV_INPUT, "Revolve CV");
		configInput(SYNC_INPUT, "Sync");

		// Zone 4: Outputs (strictly avoiding redundant 'output'/'out')
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(SYNC_OUTPUT, "Sync");

		// Load default garmire.svg trajectory
		loadDefaultSvg();
	}

	// Mandatory helper per docs/classes/generate.md
	void getRangeVoltages(float& vMin, float& vMax) const {
		switch (voltageRange) {
			case RANGE_UNIPOLAR_1V:  vMin =  0.0f; vMax =  1.0f; break;
			case RANGE_BIPOLAR_1V:   vMin = -1.0f; vMax =  1.0f; break;
			case RANGE_UNIPOLAR_5V:  vMin =  0.0f; vMax =  5.0f; break;
			case RANGE_BIPOLAR_5V:   vMin = -5.0f; vMax =  5.0f; break;
			case RANGE_UNIPOLAR_10V: vMin =  0.0f; vMax = 10.0f; break;
			case RANGE_BIPOLAR_10V:
			default:                 vMin = -10.0f; vMax = 10.0f; break;
		}
	}

	float calculateBaseFrequency(float coarse, float fine) const {
		if (rangeMode == RANGE_VERY_SLOW) {
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float t = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			return 1.0f / t;
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + 199.99f * coarse * coarse;
			return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
		} else {
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
		}
	}

	// ── SVG Parsing & Trajectory Extraction (UI / Loader Thread Only) ──
	static float getPaintLuma(const NSVGpaint& paint, bool& valid) {
		if (paint.type == NSVG_PAINT_NONE) {
			valid = false;
			return 0.f;
		}
		unsigned int col = 0;
		if (paint.type == NSVG_PAINT_COLOR) {
			col = paint.color;
		} else if ((paint.type == NSVG_PAINT_LINEAR_GRADIENT || paint.type == NSVG_PAINT_RADIAL_GRADIENT) &&
				   paint.gradient && paint.gradient->nstops > 0) {
			col = paint.gradient->stops[0].color;
		} else {
			valid = false;
			return 0.f;
		}
		unsigned int a = (col >> 24) & 0xff;
		if (a == 0) {
			valid = false;
			return 0.f;
		}
		float r = (float)(col & 0xff) / 255.f;
		float g = (float)((col >> 8) & 0xff) / 255.f;
		float b = (float)((col >> 16) & 0xff) / 255.f;
		valid = true;
		return 0.2126f * r + 0.7152f * g + 0.0722f * b;
	}

	void buildTrajectoryFromNsvgLocked(NSVGimage* image, TrajectoryData& out) {
		out.count = 0;
		if (!image || !image->shapes) {
			// Fallback unit circle if image is empty
			for (int i = 0; i < MAX_TRAJ_POINTS; ++i) {
				float ang = 2.f * (float)M_PI * (float)i / (float)MAX_TRAJ_POINTS;
				out.pts[i] = {std::cos(ang), std::sin(ang), false};
			}
			out.count = MAX_TRAJ_POINTS;
			return;
		}

		// 1. Scan luminance polarity distribution across visible shapes
		int darkCount = 0;
		int lightCount = 0;
		for (NSVGshape* shape = image->shapes; shape != nullptr; shape = shape->next) {
			if (!(shape->flags & NSVG_FLAGS_VISIBLE) || !shape->paths) continue;
			bool hasStroke = false;
			float lumaStroke = getPaintLuma(shape->stroke, hasStroke);
			bool hasFill = false;
			float lumaFill = getPaintLuma(shape->fill, hasFill);
			if (!hasStroke && !hasFill) continue;
			float luma = hasStroke ? lumaStroke : lumaFill;
			if (luma < 0.5f) darkCount++;
			else lightCount++;
		}

		bool filterByPolarity = (darkCount > 0 && lightCount > 0);

		// 2. Collect flattened polylines per subpath
		struct RawPt { float x, y; };
		std::vector<std::vector<RawPt>> subpaths;

		for (NSVGshape* shape = image->shapes; shape != nullptr; shape = shape->next) {
			if (!(shape->flags & NSVG_FLAGS_VISIBLE) || !shape->paths) continue;
			bool hasStroke = false;
			float lumaStroke = getPaintLuma(shape->stroke, hasStroke);
			bool hasFill = false;
			float lumaFill = getPaintLuma(shape->fill, hasFill);
			if (!hasStroke && !hasFill) continue;
			float luma = hasStroke ? lumaStroke : lumaFill;

			if (filterByPolarity) {
				bool isLight = (luma >= 0.5f);
				if (invertSvg != isLight) continue;
			}

			for (NSVGpath* path = shape->paths; path != nullptr; path = path->next) {
				if (path->npts < 2 || !path->pts) continue;
				std::vector<RawPt> sub;
				sub.push_back({path->pts[0], path->pts[1]});

				for (int i = 0; i < path->npts - 1; i += 3) {
					float* p = &path->pts[i * 2];
					float x0 = p[0], y0 = p[1];
					float x1 = p[2], y1 = p[3];
					float x2 = p[4], y2 = p[5];
					float x3 = p[6], y3 = p[7];

					float polyLen = std::hypot(x1 - x0, y1 - y0) + std::hypot(x2 - x1, y2 - y1) + std::hypot(x3 - x2, y3 - y2);
					float chord = std::hypot(x3 - x0, y3 - y0);
					int steps = (polyLen - chord < 0.05f) ? 2 : (int)clampf(std::ceil(polyLen * 0.5f), 4.f, 48.f);

					for (int s = 1; s <= steps; ++s) {
						float t = (float)s / (float)steps;
						float u = 1.f - t;
						float tt = t * t;
						float uu = u * u;
						float uuu = uu * u;
						float ttt = tt * t;
						float bx = uuu * x0 + 3.f * uu * t * x1 + 3.f * u * tt * x2 + ttt * x3;
						float by = uuu * y0 + 3.f * uu * t * y1 + 3.f * u * tt * y2 + ttt * y3;
						sub.push_back({bx, by});
					}
				}

				if (path->closed && sub.size() >= 2) {
					float dx = sub.front().x - sub.back().x;
					float dy = sub.front().y - sub.back().y;
					if (dx * dx + dy * dy > 1e-6f) {
						sub.push_back(sub.front());
					}
				}

				if (sub.size() >= 2) {
					subpaths.push_back(std::move(sub));
				}
			}
		}

		if (subpaths.empty()) {
			for (int i = 0; i < MAX_TRAJ_POINTS; ++i) {
				float ang = 2.f * (float)M_PI * (float)i / (float)MAX_TRAJ_POINTS;
				out.pts[i] = {std::cos(ang), std::sin(ang), false};
			}
			out.count = MAX_TRAJ_POINTS;
			return;
		}

		// 3. Compute bounding box across selected subpaths and normalize to [-1, +1] (Y flipped up)
		float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
		for (const auto& sub : subpaths) {
			for (const auto& pt : sub) {
				if (pt.x < minX) minX = pt.x;
				if (pt.y < minY) minY = pt.y;
				if (pt.x > maxX) maxX = pt.x;
				if (pt.y > maxY) maxY = pt.y;
			}
		}

		float cx = 0.5f * (minX + maxX);
		float cy = 0.5f * (minY + maxY);
		float halfSpan = 0.5f * std::max(maxX - minX, maxY - minY);
		if (halfSpan < 1e-6f) halfSpan = 1.f;

		for (auto& sub : subpaths) {
			for (auto& pt : sub) {
				pt.x = (pt.x - cx) / halfSpan;
				pt.y = -(pt.y - cy) / halfSpan; // Flip SVG Y-down to Eurorack Y-up
			}
		}

		// 4. Resample by uniform arc length into MAX_TRAJ_POINTS
		struct Seg {
			float x0, y0, x1, y1;
			float len;
			bool isJumpStart;
		};
		std::vector<Seg> segs;
		double totalLen = 0.0;

		for (size_t sIdx = 0; sIdx < subpaths.size(); ++sIdx) {
			const auto& sub = subpaths[sIdx];

			// In Slewed mode (Option A), add weighted travel segments connecting subpaths
			if (traversalMode == TRAVERSAL_SLEWED && sIdx > 0) {
				const auto& prevEnd = subpaths[sIdx - 1].back();
				const auto& currStart = sub.front();
				float d = std::hypot(currStart.x - prevEnd.x, currStart.y - prevEnd.y);
				if (d > 1e-5f) {
					float wLen = d * 0.35f; // Fast slew across blank gaps
					segs.push_back({prevEnd.x, prevEnd.y, currStart.x, currStart.y, wLen, false});
					totalLen += wLen;
				}
			}

			for (size_t i = 1; i < sub.size(); ++i) {
				float d = std::hypot(sub[i].x - sub[i - 1].x, sub[i].y - sub[i - 1].y);
				if (d > 1e-6f) {
					bool jumpFlag = (traversalMode == TRAVERSAL_DIRECT_JUMP && i == 1 && sIdx > 0);
					segs.push_back({sub[i - 1].x, sub[i - 1].y, sub[i].x, sub[i].y, d, jumpFlag});
					totalLen += d;
				}
			}
		}

		// Close loop from last subpath back to first subpath in Slewed mode if endpoints differ
		if (traversalMode == TRAVERSAL_SLEWED && !subpaths.empty()) {
			const auto& lastEnd = subpaths.back().back();
			const auto& firstStart = subpaths.front().front();
			float d = std::hypot(firstStart.x - lastEnd.x, firstStart.y - lastEnd.y);
			if (d > 1e-4f) {
				float wLen = d * 0.35f;
				segs.push_back({lastEnd.x, lastEnd.y, firstStart.x, firstStart.y, wLen, false});
				totalLen += wLen;
			}
		}

		if (segs.empty() || totalLen < 1e-6) {
			for (int i = 0; i < MAX_TRAJ_POINTS; ++i) {
				out.pts[i] = {subpaths[0][0].x, subpaths[0][0].y, false};
			}
			out.count = MAX_TRAJ_POINTS;
			return;
		}

		// Build cumulative length table and sample uniformly
		std::vector<double> cumLen(segs.size() + 1, 0.0);
		for (size_t i = 0; i < segs.size(); ++i) {
			cumLen[i + 1] = cumLen[i] + segs[i].len;
		}

		size_t curSeg = 0;
		size_t lastJumpSegMarked = (size_t)-1;
		for (int i = 0; i < MAX_TRAJ_POINTS; ++i) {
			double target = ((double)i / (double)MAX_TRAJ_POINTS) * totalLen;
			while (curSeg + 1 < segs.size() && cumLen[curSeg + 1] <= target) {
				curSeg++;
			}
			double segStart = cumLen[curSeg];
			double segL = segs[curSeg].len;
			float frac = (segL > 1e-9) ? (float)((target - segStart) / segL) : 0.f;
			frac = clampf(frac, 0.f, 1.f);

			float sx = segs[curSeg].x0 + (segs[curSeg].x1 - segs[curSeg].x0) * frac;
			float sy = segs[curSeg].y0 + (segs[curSeg].y1 - segs[curSeg].y0) * frac;
			bool markJump = false;
			if (segs[curSeg].isJumpStart && lastJumpSegMarked != curSeg) {
				markJump = true;
				lastJumpSegMarked = curSeg;
			}
			out.pts[i] = {sx, sy, markJump};
		}
		out.count = MAX_TRAJ_POINTS;
	}

	void rebuildTrajectoryLocked() {
		std::string processed = preprocessSvgStyles(loadedRawSvg);
		NSVGimage* img = nullptr;

#ifndef METAMODULE
		window::Svg tempSvg;
		tempSvg.loadString(processed);
		img = tempSvg.handle;
		buildTrajectoryFromNsvgLocked(img, trajBuffers[uiBufIdx]);
#else
		std::string mutableCopy = processed;
		if (!mutableCopy.empty()) {
			img = silvia_nsvg::nsvgParse(&mutableCopy[0], "px", 96.f);
		}
		buildTrajectoryFromNsvgLocked((NSVGimage*)img, trajBuffers[uiBufIdx]);
		if (img) {
			silvia_nsvg::nsvgDelete(img);
		}
#endif

		// Update UI preview copy and publish to audio thread lock-free
		uiPreviewCopy = trajBuffers[uiBufIdx];
		uiBufIdx = midBufIdx.exchange(uiBufIdx | 0x10, std::memory_order_acq_rel) & 0x0F;
	}

	void loadDefaultSvg() {
		std::lock_guard<std::mutex> lock(uiMutex);
		svgPath.clear();
		loadedRawSvg = DEFAULT_GARMIRE_SVG;
		rebuildTrajectoryLocked();
	}

	bool loadSvgFromFile(const std::string& path) {
		std::FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) return false;
		std::string content;
		char buf[4096];
		while (true) {
			size_t n = std::fread(buf, 1, sizeof(buf), f);
			if (n > 0) content.append(buf, n);
			if (n < sizeof(buf)) break;
		}
		std::fclose(f);
		if (content.empty()) return false;

		std::lock_guard<std::mutex> lock(uiMutex);
		svgPath = path;
		loadedRawSvg = std::move(content);
		rebuildTrajectoryLocked();
		return true;
	}

	void setInvertSvg(bool inv) {
		std::lock_guard<std::mutex> lock(uiMutex);
		invertSvg = inv;
		rebuildTrajectoryLocked();
	}

	void setTraversalMode(TraversalMode mode) {
		std::lock_guard<std::mutex> lock(uiMutex);
		traversalMode = mode;
		rebuildTrajectoryLocked();
	}

	// Shared 2D+3D point transformation (used by both DSP and live preview display)
	inline void transformPoint(float inX, float inY,
							   float scaleX, float scaleY,
							   float rotDeg, float revDeg,
							   RevolveMode rMode,
							   float& outX, float& outY) const {
		// 1. Scale X and Scale Y
		float sx = inX * scaleX;
		float sy = inY * scaleY;

		// 2. Fake 3D Revolve around vertical Y axis
		float revRad = revDeg * (float)(M_PI / 180.0);
		float cosRev = std::cos(revRad);
		float sinRev = std::sin(revRad);
		float rx, ry;
		if (rMode == REVOLVE_PERSPECTIVE) {
			float xRot = sx * cosRev;
			float zRot = sx * sinRev;
			// Safe perspective denominator clamped to avoid divide-by-zero even at 3x scale
			float denom = std::max(0.35f, 1.0f + 0.25f * zRot);
			rx = xRot / denom;
			ry = sy / denom;
		} else {
			// Orthographic linear yaw projection
			rx = sx * cosRev;
			ry = sy;
		}

		// 3. 2D Rotation (Clockwise positive for +degrees)
		float rotRad = -rotDeg * (float)(M_PI / 180.0);
		float cosRot = std::cos(rotRad);
		float sinRot = std::sin(rotRad);
		outX = rx * cosRot - ry * sinRot;
		outY = rx * sinRot + ry * cosRot;
	}

	void process(const ProcessArgs& args) override {
		// Wait-free check for new trajectory from UI thread
		int m = midBufIdx.load(std::memory_order_relaxed);
		if (m & 0x10) {
			audioBufIdx = midBufIdx.exchange(audioBufIdx, std::memory_order_acq_rel) & 0x0F;
		}
		const TrajectoryData& traj = trajBuffers[audioBufIdx];
		int ptCount = traj.count;

		// Handle 3-state Range Button Trigger
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		// Update 3-Color Range Ring LED (Yellow = Very Slow, Teal = LFO, Magenta = VCO)
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		// Polyphony channel count
		int sxCh = inputs[SCALE_X_CV_INPUT].getChannels();
		int syCh = inputs[SCALE_Y_CV_INPUT].getChannels();
		int rCh  = inputs[ROTATE_CV_INPUT].getChannels();
		int vCh  = inputs[REVOLVE_CV_INPUT].getChannels();
		int sCh  = inputs[SYNC_INPUT].getChannels();
		int numChannels = std::max({sxCh, syCh, rCh, vCh, sCh, 1});
		numChannels = std::min(numChannels, 16);

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);
		outputs[SYNC_OUTPUT].setChannels(numChannels);

		// Base frequency from standard Freq, Time-scale (Range), Fine
		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam   = params[FINE_PARAM].getValue();
		float defaultF0   = calculateBaseFrequency(coarseParam, fineParam);

		float scaleParam   = params[SCALE_PARAM].getValue();
		float rotateParam  = params[ROTATE_PARAM].getValue();
		float revolveParam = params[REVOLVE_PARAM].getValue();

		float sxTrim  = params[SCALE_X_TRIM_PARAM].getValue();
		float syTrim  = params[SCALE_Y_TRIM_PARAM].getValue();
		float rotTrim = params[ROTATE_TRIM_PARAM].getValue();
		float revTrim = params[REVOLVE_TRIM_PARAM].getValue();

		bool syConnected   = inputs[SCALE_Y_CV_INPUT].isConnected();
		bool syncConnected = inputs[SYNC_INPUT].isConnected();

		float vMin, vMax;
		getRangeVoltages(vMin, vMax);
		float vMid = 0.5f * (vMin + vMax);
		float vHalfSpan = 0.5f * (vMax - vMin);

		for (int c = 0; c < numChannels; ++c) {
			VoiceState& vs = voices[c];

			// Bidirectional External Hard Frequency Sync
			bool syncTriggered = false;
			if (syncConnected) {
				vs.timeSinceSync += args.sampleTime;
				if (vs.syncTrigger.process(inputs[SYNC_INPUT].getPolyVoltage(c), 0.1f, 2.0f)) {
					if (vs.timeSinceSync > 0.0005f) {
						vs.syncPeriod = vs.timeSinceSync;
					}
					vs.timeSinceSync = 0.f;
					vs.basePhase = 0.f;
					vs.lastIdx = 0;
					syncTriggered = true;
					vs.syncPulse.trigger(1e-4f);
				}
			} else {
				vs.syncPeriod = 0.f;
			}

			float f0 = (syncConnected && vs.syncPeriod > 0.f) ? (1.f / vs.syncPeriod) : defaultF0;

			// Advance trajectory phase
			vs.basePhase += f0 * args.sampleTime;
			if (vs.basePhase >= 1.f) {
				vs.basePhase -= std::floor(vs.basePhase);
				if (!syncTriggered) {
					vs.syncPulse.trigger(1e-4f);
				}
			}

			if (c == 0) {
				displayPhase.store(vs.basePhase, std::memory_order_relaxed);
			}

			// Look up SVG trajectory coordinates
			float rawX = 0.f, rawY = 0.f;
			if (ptCount > 0) {
				float pos = vs.basePhase * (float)ptCount;
				int i0 = (int)pos % ptCount;
				if (i0 < 0) i0 += ptCount;
				int i1 = (i0 + 1) % ptCount;
				float frac = pos - std::floor(pos);

				const TrajPoint& p0 = traj.pts[i0];
				const TrajPoint& p1 = traj.pts[i1];

				// In Direct Jump mode, trigger Sync Out when stepping across a subpath jump
				if (traversalMode == TRAVERSAL_DIRECT_JUMP && i0 != vs.lastIdx) {
					int checkIdx = i0;
					if (traj.pts[checkIdx].isJump && !syncTriggered) {
						vs.syncPulse.trigger(1e-4f);
					}
					vs.lastIdx = i0;
				} else {
					vs.lastIdx = i0;
				}

				if (traversalMode == TRAVERSAL_DIRECT_JUMP && p1.isJump) {
					rawX = p0.x;
					rawY = p0.y;
				} else {
					rawX = p0.x + (p1.x - p0.x) * frac;
					rawY = p0.y + (p1.y - p0.y) * frac;
				}
			}

			// Compute modulated Scale X/Y, Rotate, Revolve
			// Scale Y CV normalizes from Scale X CV if unpatched
			float sxCv = inputs[SCALE_X_CV_INPUT].getPolyVoltage(c) / 5.f;
			float syCv = syConnected ? (inputs[SCALE_Y_CV_INPUT].getPolyVoltage(c) / 5.f) : sxCv;
			float scaleX = clampf(scaleParam + sxCv * sxTrim * 3.f, -3.f, 3.f);
			float scaleY = clampf(scaleParam + syCv * syTrim * 3.f, -3.f, 3.f);

			float rotCv = inputs[ROTATE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float rotDeg = rotateParam + rotCv * rotTrim * 180.f;

			float revCv = inputs[REVOLVE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float revDeg = revolveParam + revCv * revTrim * 180.f;

			float outX = 0.f, outY = 0.f;
			transformPoint(rawX, rawY, scaleX, scaleY, rotDeg, revDeg, revolveMode, outX, outY);

			// Map normalized coordinates to selected Generate VoltageRange (with ±12V galvo safety clamp)
			float voltX = clampf(vMid + outX * vHalfSpan, -12.f, 12.f);
			float voltY = clampf(vMid + outY * vHalfSpan, -12.f, 12.f);

			outputs[X_OUTPUT].setVoltage(voltX, c);
			outputs[Y_OUTPUT].setVoltage(voltY, c);
			outputs[SYNC_OUTPUT].setVoltage(vs.syncPulse.process(args.sampleTime) ? 10.f : 0.f, c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "traversalMode", json_integer((int)traversalMode));
		json_object_set_new(rootJ, "revolveMode", json_integer((int)revolveMode));
		{
			std::lock_guard<std::mutex> lock(uiMutex);
			json_object_set_new(rootJ, "invertSvg", json_boolean(invertSvg));
			json_object_set_new(rootJ, "svgPath", json_string(svgPath.c_str()));
		}
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) rangeMode = (RangeMode)clampf((float)json_integer_value(rmJ), 0.f, 2.f);

		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) voltageRange = (VoltageRange)clampf((float)json_integer_value(vrJ), 0.f, (float)(NUM_VOLTAGE_RANGES - 1));

		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)clampf((float)json_integer_value(stJ), 0.f, 2.f);

		json_t* tmJ = json_object_get(rootJ, "traversalMode");
		if (tmJ) traversalMode = (TraversalMode)clampf((float)json_integer_value(tmJ), 0.f, 1.f);

		json_t* revJ = json_object_get(rootJ, "revolveMode");
		if (revJ) revolveMode = (RevolveMode)clampf((float)json_integer_value(revJ), 0.f, 1.f);

		json_t* invJ = json_object_get(rootJ, "invertSvg");
		bool newInv = invJ ? json_boolean_value(invJ) : false;

		json_t* pathJ = json_object_get(rootJ, "svgPath");
		std::string newPath = pathJ ? json_string_value(pathJ) : "";

		{
			std::lock_guard<std::mutex> lock(uiMutex);
			invertSvg = newInv;
		}
		if (!newPath.empty()) {
			if (!loadSvgFromFile(newPath)) {
				loadDefaultSvg();
			}
		} else {
			loadDefaultSvg();
		}
	}
};

// ── Custom Hardware Widgets per docs/module_design.md ──
struct SilviaPort : app::SvgPort {
	SilviaPort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};

struct SilviaKnob : RoundKnob {
	SilviaKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct SilviaTrimpot : RoundKnob {
	SilviaTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct SilviaButton : app::SvgSwitch {
	SilviaButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Custom 3-Color Illuminated Range Ring ──
struct SilviaRangeLightRing : ModuleLightWidget {
	SilviaRangeLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff)); // Very Slow: Warm Gold
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // LFO: Teal
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff)); // VCO: Magenta
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

// ── Live Vector SVG Trajectory Preview Widget ──
struct SilviaDisplayWidget : widget::TransparentWidget {
	Silvia* module = nullptr;

	SilviaDisplayWidget(Silvia* m) : module(m) {
		// Box: X = 7.400..43.400 mm (36.000 mm), Y = 11.000..35.000 mm (24.000 mm)
		box.size = mm2px(Vec(36.000f, 24.000f));
	}

	void draw(const DrawArgs& args) override {
		float w = box.size.x;
		float h = box.size.y;
		float cx = w * 0.5f;
		float cy = h * 0.5f;
		float radius = std::min(w, h) * 0.38f;

		bool inv = false;
		std::string label = "garmire.svg";
		float scaleVal = 1.f, rotVal = 0.f, revVal = 0.f;
		Silvia::RevolveMode rMode = Silvia::REVOLVE_PERSPECTIVE;
		Silvia::TraversalMode tMode = Silvia::TRAVERSAL_SLEWED;
		float phase = 0.f;

		// Copy preview trajectory safely when module is present; fallback preview in module browser
		Silvia::TrajectoryData localTraj;
		bool hasModule = (module != nullptr);

		if (hasModule) {
			scaleVal = module->params[Silvia::SCALE_PARAM].getValue();
			rotVal = module->params[Silvia::ROTATE_PARAM].getValue();
			revVal = module->params[Silvia::REVOLVE_PARAM].getValue();
			rMode = module->revolveMode;
			tMode = module->traversalMode;
			phase = module->displayPhase.load(std::memory_order_relaxed);

			std::lock_guard<std::mutex> lock(module->uiMutex);
			inv = module->invertSvg;
			label = extractFilenameOnly(module->svgPath);
			localTraj = module->uiPreviewCopy;
		} else {
			// Preview circle in module browser when module == nullptr
			for (int i = 0; i < 128; ++i) {
				float a = 2.f * (float)M_PI * (float)i / 128.f;
				localTraj.pts[i] = {0.75f * std::cos(a), 0.75f * std::sin(a), false};
			}
			localTraj.count = 128;
		}

		nvgSave(args.vg);
		nvgScissor(args.vg, 0.f, 0.f, w, h);

		// Screen background (Black / White flips on Invert SVG)
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, w, h, mm2px(1.5f));
		nvgFillColor(args.vg, inv ? nvgRGBA(0xec, 0xec, 0xf0, 0xff) : nvgRGBA(0x12, 0x12, 0x16, 0xff));
		nvgFill(args.vg);

		// Subtle center crosshair
		NVGcolor gridCol = inv ? nvgRGBA(0x00, 0x00, 0x00, 0x22) : nvgRGBA(0xff, 0xff, 0xff, 0x1e);
		nvgStrokeColor(args.vg, gridCol);
		nvgStrokeWidth(args.vg, 0.75f);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, cx, 2.f);
		nvgLineTo(args.vg, cx, h - 2.f);
		nvgMoveTo(args.vg, 2.f, cy);
		nvgLineTo(args.vg, w - 2.f, cy);
		nvgStroke(args.vg);

		// Draw transformed SVG trajectory
		if (localTraj.count > 1) {
			NVGcolor traceCol = inv ? nvgRGBA(0x14, 0x14, 0x18, 0xf0) : nvgRGBA(0xf2, 0xf2, 0xf6, 0xf0);
			NVGcolor glowCol = inv ? nvgRGBA(0x40, 0xb0, 0xa6, 0x50) : nvgRGBA(0x40, 0xb0, 0xa6, 0x60);

			auto drawTrajectoryPath = [&](float strokeW, NVGcolor col) {
				nvgBeginPath(args.vg);
				int step = std::max(1, localTraj.count / 512);
				bool started = false;
				for (int i = 0; i < localTraj.count; i += step) {
					const auto& pt = localTraj.pts[i];
					float tx, ty;
					if (hasModule) {
						module->transformPoint(pt.x, pt.y, scaleVal, scaleVal, rotVal, revVal, rMode, tx, ty);
					} else {
						tx = pt.x;
						ty = pt.y;
					}
					float sx = cx + tx * radius;
					float sy = cy - ty * radius; // Screen Y is down

					if (!started || (tMode == Silvia::TRAVERSAL_DIRECT_JUMP && pt.isJump)) {
						nvgMoveTo(args.vg, sx, sy);
						started = true;
					} else {
						nvgLineTo(args.vg, sx, sy);
					}
				}
				if (tMode == Silvia::TRAVERSAL_SLEWED) {
					float tx, ty;
					const auto& pt0 = localTraj.pts[0];
					if (hasModule) {
						module->transformPoint(pt0.x, pt0.y, scaleVal, scaleVal, rotVal, revVal, rMode, tx, ty);
					} else {
						tx = pt0.x;
						ty = pt0.y;
					}
					nvgLineTo(args.vg, cx + tx * radius, cy - ty * radius);
				}
				nvgStrokeColor(args.vg, col);
				nvgStrokeWidth(args.vg, strokeW);
				nvgStroke(args.vg);
			};

			drawTrajectoryPath(2.6f, glowCol);
			drawTrajectoryPath(1.2f, traceCol);

			// Live beam spot indicator
			if (hasModule) {
				int idx = ((int)(phase * (float)localTraj.count)) % localTraj.count;
				if (idx < 0) idx += localTraj.count;
				const auto& bPt = localTraj.pts[idx];
				float bx, by;
				module->transformPoint(bPt.x, bPt.y, scaleVal, scaleVal, rotVal, revVal, rMode, bx, by);
				float dsx = cx + bx * radius;
				float dsy = cy - by * radius;
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, dsx, dsy, 2.2f);
				nvgFillColor(args.vg, nvgRGBA(0x40, 0xb0, 0xa6, 0xff));
				nvgFill(args.vg);
			}
		}

		// Filename watermark in bottom-left corner
		if (label.size() > 20) {
			label = "..." + label.substr(label.size() - 17);
		}
		nvgFontSize(args.vg, 8.5f);
		nvgFontFaceId(args.vg, APP->window->uiFont->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, inv ? nvgRGBA(0x33, 0x33, 0x33, 0xbb) : nvgRGBA(0xcc, 0xcc, 0xd4, 0xaa));
		nvgText(args.vg, 3.5f, h - 2.0f, label.c_str(), nullptr);

		nvgRestore(args.vg);
	}
};

struct SilviaWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Silvia::SkinTheme currentTheme = Silvia::THEME_MEDIUM;

	void setTheme(Silvia::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Silvia::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Silvia_Light.svg");
					break;
				case Silvia::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Silvia_Dark.svg");
					break;
				case Silvia::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Silvia.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Silvia*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	SilviaWidget(Silvia* module) {
		setModule(module);

		panel = new SvgPanel();
		Silvia::SkinTheme initTheme = module ? module->skinTheme : Silvia::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Silvia::THEME_LIGHT:  initPath = asset::plugin(pluginInstance, "res/Silvia_Light.svg"); break;
			case Silvia::THEME_DARK:   initPath = asset::plugin(pluginInstance, "res/Silvia_Dark.svg"); break;
			case Silvia::THEME_MEDIUM:
			default:                   initPath = asset::plugin(pluginInstance, "res/Silvia.svg"); break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// 3-Column Primary Grid (10 HP = 50.80 mm)
		const float col3_x[3] = {7.000f, 25.400f, 43.800f};
		// 4-Column Lower Grid
		const float col4_x[4] = {7.000f, 19.267f, 31.533f, 43.800f};

		// ── Live SVG Trajectory Preview Display (Above Parameter Knobs: X = 7.400 mm, Y = 11.000 mm) ──
		auto* display = new SilviaDisplayWidget(module);
		display->box.pos = mm2px(Vec(7.400f, 11.000f));
		addChild(display);

		// ── Zone 2: Primary Knobs ──
		// Row 1 (Center Y = 46.00 mm): FREQ, RANGE (Button + Ring), FINE
		addParam(createParamCentered<SilviaKnob>(mm2px(Vec(col3_x[0], 46.000f)), module, Silvia::FREQ_PARAM));
		addChild(createLightCentered<SilviaRangeLightRing>(mm2px(Vec(col3_x[1], 46.000f)), module, Silvia::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<SilviaButton>(mm2px(Vec(col3_x[1], 46.000f)), module, Silvia::RANGE_PARAM));
		addParam(createParamCentered<SilviaKnob>(mm2px(Vec(col3_x[2], 46.000f)), module, Silvia::FINE_PARAM));

		// Row 2 (Center Y = 62.00 mm): SCALE, ROTATE, REVOLVE
		addParam(createParamCentered<SilviaKnob>(mm2px(Vec(col3_x[0], 62.000f)), module, Silvia::SCALE_PARAM));
		addParam(createParamCentered<SilviaKnob>(mm2px(Vec(col3_x[1], 62.000f)), module, Silvia::ROTATE_PARAM));
		addParam(createParamCentered<SilviaKnob>(mm2px(Vec(col3_x[2], 62.000f)), module, Silvia::REVOLVE_PARAM));

		// ── Zone 3: CV Attenuverter Trimpots (Center Y = 82.000 mm) ──
		addParam(createParamCentered<SilviaTrimpot>(mm2px(Vec(col4_x[0], 82.000f)), module, Silvia::SCALE_X_TRIM_PARAM));
		addParam(createParamCentered<SilviaTrimpot>(mm2px(Vec(col4_x[1], 82.000f)), module, Silvia::SCALE_Y_TRIM_PARAM));
		addParam(createParamCentered<SilviaTrimpot>(mm2px(Vec(col4_x[2], 82.000f)), module, Silvia::ROTATE_TRIM_PARAM));
		addParam(createParamCentered<SilviaTrimpot>(mm2px(Vec(col4_x[3], 82.000f)), module, Silvia::REVOLVE_TRIM_PARAM));

		// ── Zone 4: I/O Jacks ──
		// Row 1 (CV Inputs, Center Y = 104.000 mm)
		addInput(createInputCentered<SilviaPort>(mm2px(Vec(col4_x[0], 104.000f)), module, Silvia::SCALE_X_CV_INPUT));
		addInput(createInputCentered<SilviaPort>(mm2px(Vec(col4_x[1], 104.000f)), module, Silvia::SCALE_Y_CV_INPUT));
		addInput(createInputCentered<SilviaPort>(mm2px(Vec(col4_x[2], 104.000f)), module, Silvia::ROTATE_CV_INPUT));
		addInput(createInputCentered<SilviaPort>(mm2px(Vec(col4_x[3], 104.000f)), module, Silvia::REVOLVE_CV_INPUT));

		// Row 2 (Sync & Outputs, Center Y = 117.000 mm)
		addInput(createInputCentered<SilviaPort>(mm2px(Vec(col4_x[0], 117.000f)), module, Silvia::SYNC_INPUT));
		addOutput(createOutputCentered<SilviaPort>(mm2px(Vec(col4_x[1], 117.000f)), module, Silvia::X_OUTPUT));
		addOutput(createOutputCentered<SilviaPort>(mm2px(Vec(col4_x[2], 117.000f)), module, Silvia::Y_OUTPUT));
		addOutput(createOutputCentered<SilviaPort>(mm2px(Vec(col4_x[3], 117.000f)), module, Silvia::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		auto* mod = dynamic_cast<Silvia*>(this->module);
		if (!mod) return;

		// ── SVG File Controls ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("SVG Source"));

		menu->addChild(createMenuItem("Load SVG File...", "", [=]() {
			osdialog_filters* filters = osdialog_filters_parse("SVG Files:svg");
#ifndef METAMODULE
			std::string dir;
			{
				std::lock_guard<std::mutex> lock(mod->uiMutex);
				if (!mod->svgPath.empty()) {
					dir = system::getDirectory(mod->svgPath);
				}
			}
			char* pathC = osdialog_file(OSDIALOG_OPEN, dir.empty() ? nullptr : dir.c_str(), nullptr, filters);
			if (pathC) {
				std::string chosen(pathC);
				std::free(pathC);
				mod->loadSvgFromFile(chosen);
			}
			osdialog_filters_free(filters);
#else
			async_osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters, [mod, filters](char* pathC) {
				if (pathC) {
					std::string chosen(pathC);
					std::free(pathC);
					mod->loadSvgFromFile(chosen);
				}
				osdialog_filters_free(filters);
			});
#endif
		}));

		menu->addChild(createMenuItem("Reset to Default (garmire.svg)", "", [=]() {
			mod->loadDefaultSvg();
		}));

		menu->addChild(createCheckMenuItem("Invert SVG (Black = White)", "",
			[=]() { return mod->invertSvg; },
			[=]() { mod->setInvertSvg(!mod->invertSvg); }
		));

		// ── Subpath Traversal Mode ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Subpath Traversal"));
		menu->addChild(createCheckMenuItem("Slewed (Continuous)", "",
			[=]() { return mod->traversalMode == Silvia::TRAVERSAL_SLEWED; },
			[=]() { mod->setTraversalMode(Silvia::TRAVERSAL_SLEWED); }
		));
		menu->addChild(createCheckMenuItem("Direct Jump (Sync Pulse)", "",
			[=]() { return mod->traversalMode == Silvia::TRAVERSAL_DIRECT_JUMP; },
			[=]() { mod->setTraversalMode(Silvia::TRAVERSAL_DIRECT_JUMP); }
		));

		// ── Revolve Projection Mode ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("3D Revolve Projection"));
		menu->addChild(createCheckMenuItem("3D Perspective", "",
			[=]() { return mod->revolveMode == Silvia::REVOLVE_PERSPECTIVE; },
			[=]() { mod->revolveMode = Silvia::REVOLVE_PERSPECTIVE; }
		));
		menu->addChild(createCheckMenuItem("Orthographic", "",
			[=]() { return mod->revolveMode == Silvia::REVOLVE_ORTHOGRAPHIC; },
			[=]() { mod->revolveMode = Silvia::REVOLVE_ORTHOGRAPHIC; }
		));

		// ── Oscillator Range ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));
		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};
		for (int i = 0; i < 3; ++i) {
			Silvia::RangeMode mode = (Silvia::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return mod->rangeMode == mode; },
				[=]() { mod->rangeMode = mode; }
			));
		}

		// ── Mandatory Generate Output Voltage Range ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Voltage Range"));
		const char* voltageLabels[] = {
			"0 to 1V",
			"±1V",
			"0 to 5V",
			"±5V",
			"0 to 10V",
			"±10V"
		};
		for (int i = 0; i < Silvia::NUM_VOLTAGE_RANGES; ++i) {
			Silvia::VoltageRange vr = (Silvia::VoltageRange)i;
			menu->addChild(createCheckMenuItem(voltageLabels[i], "",
				[=]() { return mod->voltageRange == vr; },
				[=]() { mod->voltageRange = vr; }
			));
		}

		// ── Skin Theme ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));
		const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};
		for (int i = 0; i < 3; ++i) {
			Silvia::SkinTheme theme = (Silvia::SkinTheme)i;
			menu->addChild(createCheckMenuItem(skinLabels[i], "",
				[=]() { return mod->skinTheme == theme; },
				[=]() {
					mod->skinTheme = theme;
					setTheme(theme);
				}
			));
		}
	}
};

Model* modelSilvia = createModel<Silvia, SilviaWidget>("Silvia");
