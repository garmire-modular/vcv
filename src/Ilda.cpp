#include "plugin.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
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
#endif

// ─────────────────────────────────────────────────────────────────────
//  ILDA — ILDA (.ild) & Multi-Channel WAV (.wav) Laser Player (14 HP, v1.0.1)
//  Generate class module implementing ILDA Image Data Transfer Format
//  (Rev 011 Formats 0, 1, 2, 4, 5) and 1–8 channel WAV playback:
//    - 8-ch WAV: 1:X, 2:Y, 3:R, 4:G, 5:B, 6:Z/Blank, 7:Audio L, 8:Audio R
//    - Large top full-color laser preview display with live scan beam
//    - Row 1 (4 cols): Standard Freq, Range, Fine, Frame Rate
//    - Zone 3: Freq CV depth, Reset button, FPS CV depth, Freeze button
//    - Zone 4 Row 1: Sync In (frame), Freq CV, Reset, FPS CV, Freeze Gate
//    - Zone 4 Row 2: Freq Sync In, X, Y, Z, Audio L
//    - Zone 4 Row 3: R, G, B, Sync Out, Audio R
//    - Right-click context menu for file, interpolation, frame/sync,
//      WAV mapping, voltage range, RGB scale, and skin settings.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

static std::string trimCopy(const std::string& s) {
	size_t b = 0;
	while (b < s.size() && std::isspace((unsigned char)s[b])) b++;
	size_t e = s.size();
	while (e > b && std::isspace((unsigned char)s[e - 1])) e--;
	return s.substr(b, e - b);
}

static std::string extractFilenameOnly(const std::string& path) {
	if (path.empty()) return "default.ild";
	size_t slash = path.find_last_of("/\\");
	return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

static std::string getLowerExtension(const std::string& path) {
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos) return "";
	std::string ext = path.substr(dot + 1);
	for (char& c : ext) c = (char)std::tolower((unsigned char)c);
	return ext;
}

// Big-endian integer helpers for ILDA Rev 011
inline uint16_t readU16BE(const uint8_t* p) {
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

inline int16_t readI16BE(const uint8_t* p) {
	return (int16_t)readU16BE(p);
}

inline void writeU16BE(std::vector<uint8_t>& dst, uint16_t v) {
	dst.push_back((uint8_t)((v >> 8) & 0xff));
	dst.push_back((uint8_t)(v & 0xff));
}

inline void writeI16BE(std::vector<uint8_t>& dst, int16_t v) {
	writeU16BE(dst, (uint16_t)v);
}

// Little-endian integer helpers for RIFF/WAV
inline uint16_t readU16LE(const uint8_t* p) {
	return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

inline uint32_t readU32LE(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline int32_t readI24LE(const uint8_t* p) {
	int32_t v = (int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16);
	if (v & 0x800000) v |= 0xff000000;
	return v;
}

inline int32_t readI32LE(const uint8_t* p) {
	return (int32_t)readU32LE(p);
}

// ILDA Spec Revision 011 Appendix A — Suggested Default Color Palette (Indices 0..63)
static const uint8_t ILDA_DEFAULT_PALETTE_64[64][3] = {
	{255,   0,   0}, {255,  16,   0}, {255,  32,   0}, {255,  48,   0},
	{255,  64,   0}, {255,  80,   0}, {255,  96,   0}, {255, 112,   0},
	{255, 128,   0}, {255, 144,   0}, {255, 160,   0}, {255, 176,   0},
	{255, 192,   0}, {255, 208,   0}, {255, 224,   0}, {255, 240,   0},
	{255, 255,   0}, {224, 255,   0}, {192, 255,   0}, {160, 255,   0},
	{128, 255,   0}, { 96, 255,   0}, { 64, 255,   0}, { 32, 255,   0},
	{  0, 255,   0}, {  0, 255,  36}, {  0, 255,  73}, {  0, 255, 109},
	{  0, 255, 146}, {  0, 255, 182}, {  0, 255, 219}, {  0, 255, 255},
	{  0, 227, 255}, {  0, 198, 255}, {  0, 170, 255}, {  0, 142, 255},
	{  0, 113, 255}, {  0,  85, 255}, {  0,  56, 255}, {  0,  28, 255},
	{  0,   0, 255}, { 32,   0, 255}, { 64,   0, 255}, { 96,   0, 255},
	{128,   0, 255}, {160,   0, 255}, {192,   0, 255}, {224,   0, 255},
	{255,   0, 255}, {255,  32, 255}, {255,  64, 255}, {255,  96, 255},
	{255, 128, 255}, {255, 160, 255}, {255, 192, 255}, {255, 224, 255},
	{255, 255, 255}, {255, 224, 224}, {255, 192, 192}, {255, 160, 160},
	{255, 128, 128}, {255,  96,  96}, {255,  64,  64}, {255,  32,  32}
};

static std::array<uint8_t, 768> makeInitialPalette() {
	std::array<uint8_t, 768> pal;
	for (int i = 0; i < 64; ++i) {
		pal[i * 3 + 0] = ILDA_DEFAULT_PALETTE_64[i][0];
		pal[i * 3 + 1] = ILDA_DEFAULT_PALETTE_64[i][1];
		pal[i * 3 + 2] = ILDA_DEFAULT_PALETTE_64[i][2];
	}
	for (int i = 64; i < 256; ++i) {
		pal[i * 3 + 0] = 255;
		pal[i * 3 + 1] = 255;
		pal[i * 3 + 2] = 255;
	}
	return pal;
}

// Programmatically builds a valid multi-frame ILDA Format 4 (3D True Color) binary stream
// showcasing 3D coordinates, full RGB hues, and blanking transitions out of the box.
static std::vector<uint8_t> buildDefaultIldaBinary() {
	std::vector<uint8_t> bin;
	const uint16_t numFrames = 36;
	bin.reserve(numFrames * 400);

	auto appendHeader = [&](uint8_t fmt, const char* name8, uint16_t numRec, uint16_t frameNum, uint16_t totalFrames) {
		bin.push_back('I'); bin.push_back('L'); bin.push_back('D'); bin.push_back('A');
		bin.push_back(0); bin.push_back(0); bin.push_back(0);
		bin.push_back(fmt);
		for (int i = 0; i < 8; ++i) bin.push_back((uint8_t)name8[i]);
		const char comp[9] = "GARMIRE ";
		for (int i = 0; i < 8; ++i) bin.push_back((uint8_t)comp[i]);
		writeU16BE(bin, numRec);
		writeU16BE(bin, frameNum);
		writeU16BE(bin, totalFrames);
		bin.push_back(0); // Projector 0
		bin.push_back(0); // Reserved
	};

	struct Pt {
		float x, y, z;
		uint8_t r, g, b;
		bool blank;
	};

	auto pushLinePts = [&](std::vector<Pt>& pts, float x0, float y0, float z0, float x1, float y1, float z1,
						   uint8_t r, uint8_t g, uint8_t b, int steps, bool moveFromPrev) {
		if (moveFromPrev) {
			pts.push_back({x0, y0, z0, 0, 0, 0, true});
			pts.push_back({x0, y0, z0, r, g, b, false});
		}
		for (int s = 1; s <= steps; ++s) {
			float t = (float)s / (float)steps;
			pts.push_back({x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t, r, g, b, false});
		}
	};

	// Stylized geometric G emblem outer vertices
	const float gVerts[7][2] = {
		{ 0.50f,  0.56f},
		{-0.22f,  0.76f},
		{-0.76f,  0.22f},
		{-0.76f, -0.44f},
		{-0.22f, -0.76f},
		{ 0.62f, -0.76f},
		{ 0.62f, -0.04f}
	};

	for (uint16_t f = 0; f < numFrames; ++f) {
		float ang = 2.f * (float)M_PI * ((float)f / (float)numFrames);
		float ca = std::cos(ang);
		float sa = std::sin(ang);

		std::vector<Pt> pts;
		pts.reserve(160);

		// Subpath 1: Colorful octagonal G frame with slight 3D wave
		for (int i = 0; i < 6; ++i) {
			float z0 = 0.25f * std::sin(ang + (float)i * 0.8f);
			float z1 = 0.25f * std::sin(ang + (float)(i + 1) * 0.8f);
			uint8_t idx = (uint8_t)(((i * 9 + f * 2) % 48));
			uint8_t r = ILDA_DEFAULT_PALETTE_64[idx][0];
			uint8_t g = ILDA_DEFAULT_PALETTE_64[idx][1];
			uint8_t b = ILDA_DEFAULT_PALETTE_64[idx][2];
			pushLinePts(pts, gVerts[i][0], gVerts[i][1], z0, gVerts[i + 1][0], gVerts[i + 1][1], z1, r, g, b, 6, i == 0);
		}
		// G horizontal crossbar inward
		pushLinePts(pts, gVerts[6][0], gVerts[6][1], 0.2f * std::sin(ang), 0.04f, -0.04f, 0.2f * std::cos(ang),
					64, 176, 166, 6, false);

		// Subpath 2: Rotating 3D inner diamond prism (blanked travel from crossbar)
		float dia[4][3] = {
			{0.0f * ca - 0.0f * sa,  0.34f, 0.34f * sa},
			{0.34f * ca,             0.0f,  0.34f * sa},
			{0.0f,                  -0.34f, -0.34f * sa},
			{-0.34f * ca,            0.0f, -0.34f * sa}
		};
		for (int i = 0; i < 4; ++i) {
			int ni = (i + 1) % 4;
			pushLinePts(pts, dia[i][0], dia[i][1], dia[i][2], dia[ni][0], dia[ni][1], dia[ni][2],
						255, 224, 64, 5, i == 0);
		}

		appendHeader(4, "GARMIRE ", (uint16_t)pts.size(), f, numFrames);
		for (size_t i = 0; i < pts.size(); ++i) {
			const Pt& p = pts[i];
			int16_t ix = (int16_t)clampf(p.x * 32767.f, -32768.f, 32767.f);
			int16_t iy = (int16_t)clampf(p.y * 32767.f, -32768.f, 32767.f);
			int16_t iz = (int16_t)clampf(p.z * 32767.f, -32768.f, 32767.f);
			uint8_t status = (uint8_t)((p.blank ? 0x40 : 0x00) | ((i + 1 == pts.size()) ? 0x80 : 0x00));
			writeI16BE(bin, ix);
			writeI16BE(bin, iy);
			writeI16BE(bin, iz);
			bin.push_back(status);
			// Format 4 order per Spec Fig 6: Status, Blue, Green, Red
			bin.push_back(p.b);
			bin.push_back(p.g);
			bin.push_back(p.r);
		}
	}

	// EOF Header (Number of Records = 0 per Spec §3.1 & §4.2.6)
	appendHeader(4, "EOF     ", 0, 0, numFrames);
	return bin;
}

} // namespace

struct Ilda : Module {
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

	// Mandatory 6 Generate Voltage Standards
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

	enum RgbVoltageScale {
		RGB_SCALE_1V = 0,
		RGB_SCALE_5V = 1,    // Default 0..+5V
		RGB_SCALE_10V = 2
	};
	RgbVoltageScale rgbVoltageScale = RGB_SCALE_5V;

	enum InterpMode {
		INTERP_LINEAR = 0,   // Smooth interpolation
		INTERP_NEAREST = 1   // Stepped corners
	};
	InterpMode interpMode = INTERP_LINEAR;

	enum FrameMode {
		FRAME_AUTO_FPS = 0,  // Auto-advance multi-frame .ild at FPS setting
		FRAME_MANUAL = 1,    // Hold selected manual frame
		FRAME_ALL_SEQ = 2    // Traverse all frames continuously per scan cycle
	};
	FrameMode frameMode = FRAME_AUTO_FPS;

	enum SyncInMode {
		SYNC_RESET_FILE = 0, // Restart animation frame to 0
		SYNC_STEP_FRAME = 1  // Advance 1 frame per trigger
	};
	SyncInMode syncInMode = SYNC_STEP_FRAME;

	enum WavSpeedMode {
		WAV_SPEED_VOCT = 0,  // Default: 1.0x native rate at Freq=0.5, V/Oct + Fine
		WAV_SINGLE_CYCLE = 1 // Sweep entire WAV as 1 frame at exact Freq Hz
	};
	WavSpeedMode wavSpeedMode = WAV_SPEED_VOCT;

	enum WavCh6Mode {
		WAV_CH6_Z_DEPTH = 0, // Default: Ch 6 -> Z Depth
		WAV_CH6_BLANK_GATE = 1 // Ch 6 -> Blanking gate (< 0.05 = blank)
	};
	WavCh6Mode wavCh6Mode = WAV_CH6_Z_DEPTH;

	enum WavColorEncoding {
		WAV_COLOR_UNIPOLAR = 0, // 0..+1 standard
		WAV_COLOR_BIPOLAR = 1   // -1..+1 shifted to 0..+1
	};
	WavColorEncoding wavColorEncoding = WAV_COLOR_UNIPOLAR;

	enum RevolvePreset {
		REVOLVE_OFF = 0,
		REVOLVE_TILT_POS_25,
		REVOLVE_TILT_POS_45,
		REVOLVE_TILT_NEG_25,
		REVOLVE_AUTO_SPIN
	};
	RevolvePreset revolvePreset = REVOLVE_OFF;

	enum ParamId {
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,
		FRAME_RATE_PARAM,

		FREQ_TRIM_PARAM,
		RESET_BUTTON_PARAM,
		FPS_TRIM_PARAM,
		FREEZE_BUTTON_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		FRAME_SYNC_INPUT,
		FREQ_CV_INPUT,
		RESET_INPUT,
		FPS_CV_INPUT,
		FREEZE_INPUT,
		FREQ_SYNC_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		X_OUTPUT,
		Y_OUTPUT,
		Z_OUTPUT,
		AUDIO_L_OUTPUT,

		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		SYNC_OUTPUT,
		AUDIO_R_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_ORANGE,
		RANGE_LIGHT_PURPLE,
		FREEZE_LIGHT,

		LIGHTS_LEN
	};

	struct ShowPoint {
		float x = 0.f;
		float y = 0.f;
		float z = 0.f;
		float r = 1.f;
		float g = 1.f;
		float b = 1.f;
		float al = 0.f;
		float ar = 0.f;
		bool blank = false;
	};

	struct FrameRange {
		int startIdx = 0;
		int pointCount = 0;
		std::string name;
	};

	struct ShowData {
		bool isWav = false;
		int numChannels = 0;
		uint32_t sampleRate = 48000;
		std::vector<ShowPoint> points;
		std::vector<FrameRange> frames;
		std::string infoSummary;
	};

	// Lock-free triple buffer (UI owns uiSlot, Audio owns audioSlot, atomic midSlot)
	ShowData showSlots[3];
	int uiSlot = 0;
	int audioSlot = 1;
	std::atomic<int> midSlot{2};

	// UI-thread only state (never touched by audio thread)
	std::mutex uiMutex;
	ShowData uiPreviewShow;
	std::string loadedFilePath;
	std::vector<uint8_t> loadedRawBytes;
	bool loadedIsWav = false;

	// Display toggles & telemetry (lock-free atomics for preview rendering)
	std::atomic<bool> showBlankedMoves{false};
	std::atomic<bool> flipX{false};
	std::atomic<bool> flipY{false};
	std::atomic<bool> masterFrozen{false};
	std::atomic<int> manualFrame{0};
	std::atomic<int> displayFrame{0};
	std::atomic<float> displayScanPos{0.f};
	std::atomic<float> displayRevolveDeg{0.f};

	dsp::BooleanTrigger rangeTrigger;
	dsp::BooleanTrigger resetBtnTrigger;
	dsp::BooleanTrigger freezeBtnTrigger;

	struct VoiceState {
		float scanPhase = 0.f;
		float wavSamplePos = 0.f;
		float wavFreezePos = 0.f;
		int curFrame = 0;
		bool frozen = false;
		float animTimer = 0.f;
		float timeSinceSync = 0.f;
		float syncPeriod = 0.f;
		float dcInX = 0.f, dcOutX = 0.f;
		float dcInY = 0.f, dcOutY = 0.f;
		dsp::SchmittTrigger frameSyncTrigger;
		dsp::SchmittTrigger freqSyncTrigger;
		dsp::SchmittTrigger resetTrigger;
		dsp::SchmittTrigger freezeTrigger;
		dsp::PulseGenerator syncPulse;
	};
	VoiceState voices[16];
	float autoSpinDeg = 0.f;

	// ── Standard Garmire Generate FreqParamQuantity ──
	struct FreqParamQuantity : ParamQuantity {
		Ilda* getIlda() { return dynamic_cast<Ilda*>(module); }

		float getDisplayValue() override {
			Ilda* mod = getIlda();
			if (!mod) return ParamQuantity::getDisplayValue();
			float coarse = getValue();
			float fine = (mod->paramQuantities.size() > Ilda::FINE_PARAM) ? mod->params[Ilda::FINE_PARAM].getValue() : 0.f;
			if (mod->rangeMode == Ilda::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (mod->rangeMode == Ilda::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Ilda* mod = getIlda();
			if (!mod) return ParamQuantity::getDisplayValueString();
			float val = getDisplayValue();
			char buf[32];
			if (mod->rangeMode == Ilda::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else if (val < 10.0f) {
				std::snprintf(buf, sizeof(buf), "%.3f", val);
			} else if (val < 100.0f) {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Ilda* mod = getIlda();
			if (!mod) return;
			float fine = (mod->paramQuantities.size() > Ilda::FINE_PARAM) ? mod->params[Ilda::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();
			if (mod->rangeMode == Ilda::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				targetPeriodS = std::max(targetPeriodS, 1e-5f);
				float fineFactor = std::max(1e-4f, std::abs(1.0f - fine * 0.10f));
				float tCoarse = std::max(targetPeriodS / fineFactor, 1e-6f);
				float ratio = std::max(tCoarse / 600.0f, 1e-6f);
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (mod->rangeMode == Ilda::RANGE_LFO) {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 0.01f);
				float fCoarse = targetHz - fine * 20.0f;
				newCoarse = (fCoarse <= 0.01f) ? 0.0f : std::sqrt((fCoarse - 0.01f) / 199.99f);
			} else {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 100.f);
				float fineFactor = std::max(1e-4f, std::abs(1.0f + fine * 0.10f));
				float fCoarse = std::max(targetHz / fineFactor, 1.0f);
				float ratio = std::max(fCoarse / 150.0f, 1e-6f);
				newCoarse = std::log(ratio) / std::log(2000.0f / 150.0f);
			}
			setValue(clampf(newCoarse, 0.0f, 1.0f));
		}

		void setDisplayValue(float displayValue) override {
			Ilda* mod = getIlda();
			if (!mod) { ParamQuantity::setDisplayValue(displayValue); return; }
			setFrequencyValue(displayValue, mod->rangeMode == Ilda::RANGE_VERY_SLOW);
		}

		void setDisplayValueString(std::string s) override {
			Ilda* mod = getIlda();
			if (!mod) { ParamQuantity::setDisplayValueString(s); return; }
			std::string str = trimCopy(s);
			if (str.empty()) return;
			std::string lowerStr = str;
			for (char& c : lowerStr) c = (char)std::tolower((unsigned char)c);

			enum UnitType { UNIT_DEFAULT, UNIT_HZ, UNIT_KHZ, UNIT_MHZ, UNIT_SEC, UNIT_MS };
			UnitType unitType = UNIT_DEFAULT;
			if (lowerStr.size() >= 4 && lowerStr.substr(lowerStr.size() - 4) == "secs") { unitType = UNIT_SEC; str.resize(str.size() - 4); }
			else if (lowerStr.size() >= 3 && lowerStr.substr(lowerStr.size() - 3) == "sec") { unitType = UNIT_SEC; str.resize(str.size() - 3); }
			else if (lowerStr.size() >= 3 && lowerStr.substr(lowerStr.size() - 3) == "khz") { unitType = UNIT_KHZ; str.resize(str.size() - 3); }
			else if (lowerStr.size() >= 3 && lowerStr.substr(lowerStr.size() - 3) == "mhz") { unitType = UNIT_MHZ; str.resize(str.size() - 3); }
			else if (lowerStr.size() >= 2 && lowerStr.substr(lowerStr.size() - 2) == "hz") { unitType = UNIT_HZ; str.resize(str.size() - 2); }
			else if (lowerStr.size() >= 2 && lowerStr.substr(lowerStr.size() - 2) == "ms") { unitType = UNIT_MS; str.resize(str.size() - 2); }
			else if (!lowerStr.empty() && lowerStr.back() == 's') { unitType = UNIT_SEC; str.pop_back(); }
			else if (!lowerStr.empty() && lowerStr.back() == 'k') { unitType = UNIT_KHZ; str.pop_back(); }

			str = trimCopy(str);
			if (str.empty()) return;
			char* endPtr = nullptr;
			float val = std::strtof(str.c_str(), &endPtr);
			if (endPtr == str.c_str()) return;

			if (unitType == UNIT_KHZ) { val *= 1000.0f; setFrequencyValue(val, false); }
			else if (unitType == UNIT_MHZ) { val *= 1000000.0f; setFrequencyValue(val, false); }
			else if (unitType == UNIT_HZ) { setFrequencyValue(val, false); }
			else if (unitType == UNIT_SEC) { setFrequencyValue(val, true); }
			else if (unitType == UNIT_MS) { setFrequencyValue(val * 0.001f, true); }
			else { setFrequencyValue(val, mod->rangeMode == Ilda::RANGE_VERY_SLOW); }
		}

		std::string getUnit() override {
			Ilda* mod = getIlda();
			if (!mod) return " Hz";
			return (mod->rangeMode == Ilda::RANGE_VERY_SLOW) ? " s" : " Hz";
		}

		std::string getLabel() override {
			Ilda* mod = getIlda();
			if (!mod) return "Frequency";
			return (mod->rangeMode == Ilda::RANGE_VERY_SLOW) ? "Cycle Period" : "Frequency";
		}
	};

	Ilda() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency");
		configButton(RANGE_PARAM, "Time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configParam(FRAME_RATE_PARAM, 0.f, 60.f, 24.f, "Frame rate", " fps");

		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configButton(RESET_BUTTON_PARAM, "Reset playback");
		configParam(FPS_TRIM_PARAM, -1.f, 1.f, 0.f, "Frame rate CV depth", "%", 0.f, 100.f);
		configButton(FREEZE_BUTTON_PARAM, "Freeze frame");

		configInput(FRAME_SYNC_INPUT, "Frame sync");
		configInput(FREQ_CV_INPUT, "Frequency (1V/oct)");
		configInput(RESET_INPUT, "Reset playback");
		configInput(FPS_CV_INPUT, "Frame rate CV");
		configInput(FREEZE_INPUT, "Freeze frame gate");
		configInput(FREQ_SYNC_INPUT, "Frequency sync");

		configOutput(X_OUTPUT, "X coordinate");
		configOutput(Y_OUTPUT, "Y coordinate");
		configOutput(Z_OUTPUT, "Z coordinate");
		configOutput(AUDIO_L_OUTPUT, "Left audio");

		configOutput(R_OUTPUT, "Red color");
		configOutput(G_OUTPUT, "Green color");
		configOutput(B_OUTPUT, "Blue color");
		configOutput(SYNC_OUTPUT, "Sync");
		configOutput(AUDIO_R_OUTPUT, "Right audio");

		loadDefaultSequence();
	}

	float calculateBaseFrequency(float coarse, float fine) const {
		if (rangeMode == RANGE_VERY_SLOW) {
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float periodSec = std::max(tCoarse * (1.0f - fine * 0.10f), 1e-4f);
			return 1.0f / periodSec;
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + 199.99f * coarse * coarse;
			return std::max(fCoarse + fine * 20.0f, 0.005f);
		} else {
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return std::max(fCoarse * (1.0f + fine * 0.10f), 1.0f);
		}
	}

	void getRangeVoltages(float& vMin, float& vMax) const {
		switch (voltageRange) {
			case RANGE_UNIPOLAR_1V:  vMin = 0.f;   vMax = 1.f;  break;
			case RANGE_BIPOLAR_1V:   vMin = -1.f;  vMax = 1.f;  break;
			case RANGE_UNIPOLAR_5V:  vMin = 0.f;   vMax = 5.f;  break;
			case RANGE_UNIPOLAR_10V: vMin = 0.f;   vMax = 10.f; break;
			case RANGE_BIPOLAR_10V:  vMin = -10.f; vMax = 10.f; break;
			case RANGE_BIPOLAR_5V:
			default:                 vMin = -5.f;  vMax = 5.f;  break;
		}
	}

	float getRgbScaleVoltage() const {
		switch (rgbVoltageScale) {
			case RGB_SCALE_1V:  return 1.f;
			case RGB_SCALE_10V: return 10.f;
			case RGB_SCALE_5V:
			default:            return 5.f;
		}
	}

	// Parse ILDA binary stream supporting Formats 0, 1, 2, 4, and 5 per ILDA Rev 011
	bool parseIldaBytes(const std::vector<uint8_t>& raw, ShowData& out) {
		out = ShowData();
		out.isWav = false;

		std::unordered_map<uint8_t, std::array<uint8_t, 768>> palettes;
		size_t off = 0;
		const size_t nBytes = raw.size();

		while (off + 32 <= nBytes) {
			if (std::memcmp(&raw[off], "ILDA", 4) != 0) {
				break;
			}
			uint8_t fmt = raw[off + 7];
			char nameBuf[9];
			for (int i = 0; i < 8; ++i) {
				char ch = (char)raw[off + 8 + i];
				nameBuf[i] = ch;
			}
			nameBuf[8] = '\0';
			std::string secName = trimCopy(std::string(nameBuf));

			uint16_t numRecords = readU16BE(&raw[off + 24]);
			uint8_t projNum = raw[off + 30];
			off += 32;

			// End-of-file frame header
			if (numRecords == 0 && (fmt == 0 || fmt == 1 || fmt == 4 || fmt == 5)) {
				break;
			}

			size_t recSize = 0;
			switch (fmt) {
				case 0: recSize = 8; break;
				case 1: recSize = 6; break;
				case 2: recSize = 3; break;
				case 4: recSize = 10; break;
				case 5: recSize = 8; break;
				default: recSize = 0; break;
			}
			if (recSize == 0 || off + (size_t)numRecords * recSize > nBytes) {
				break;
			}

			if (fmt == 2) {
				// Format 2: Color Palette (3 bytes per entry: R, G, B)
				auto it = palettes.find(projNum);
				if (it == palettes.end()) {
					it = palettes.emplace(projNum, makeInitialPalette()).first;
				}
				size_t count = std::min<size_t>(numRecords, 256);
				for (size_t i = 0; i < count; ++i) {
					const uint8_t* p = &raw[off + i * 3];
					it->second[i * 3 + 0] = p[0];
					it->second[i * 3 + 1] = p[1];
					it->second[i * 3 + 2] = p[2];
				}
				off += (size_t)numRecords * recSize;
				continue;
			}

			// Frame Formats 0, 1, 4, 5
			if (palettes.find(projNum) == palettes.end()) {
				palettes.emplace(projNum, makeInitialPalette());
			}
			const auto& pal = palettes[projNum];

			FrameRange fr;
			fr.startIdx = (int)out.points.size();
			fr.pointCount = (int)numRecords;
			fr.name = secName.empty() ? ("Frame " + std::to_string(out.frames.size() + 1)) : secName;

			out.points.reserve(out.points.size() + numRecords);
			for (uint16_t i = 0; i < numRecords; ++i) {
				const uint8_t* rec = &raw[off + (size_t)i * recSize];
				ShowPoint pt;
				pt.x = (float)readI16BE(&rec[0]) / 32768.f;
				pt.y = (float)readI16BE(&rec[2]) / 32768.f;
				pt.al = pt.x;
				pt.ar = pt.y;

				uint8_t status = 0;
				if (fmt == 0) {
					pt.z = (float)readI16BE(&rec[4]) / 32768.f;
					status = rec[6];
					uint8_t cIdx = rec[7];
					pt.r = (float)pal[cIdx * 3 + 0] / 255.f;
					pt.g = (float)pal[cIdx * 3 + 1] / 255.f;
					pt.b = (float)pal[cIdx * 3 + 2] / 255.f;
				} else if (fmt == 1) {
					pt.z = 0.f;
					status = rec[4];
					uint8_t cIdx = rec[5];
					pt.r = (float)pal[cIdx * 3 + 0] / 255.f;
					pt.g = (float)pal[cIdx * 3 + 1] / 255.f;
					pt.b = (float)pal[cIdx * 3 + 2] / 255.f;
				} else if (fmt == 4) {
					pt.z = (float)readI16BE(&rec[4]) / 32768.f;
					status = rec[6];
					// Format 4 order per Spec Fig 6: Status(7), Blue(8), Green(9), Red(10)
					pt.b = (float)rec[7] / 255.f;
					pt.g = (float)rec[8] / 255.f;
					pt.r = (float)rec[9] / 255.f;
				} else { // fmt == 5
					pt.z = 0.f;
					status = rec[4];
					// Format 5 order per Spec Fig 7: Status(5), Blue(6), Green(7), Red(8)
					pt.b = (float)rec[5] / 255.f;
					pt.g = (float)rec[6] / 255.f;
					pt.r = (float)rec[7] / 255.f;
				}

				// Status Code Bit 6 = Blanking Bit (takes precedence over RGB per §5.2.4)
				pt.blank = (status & 0x40) != 0;
				if (pt.blank) {
					pt.r = 0.f;
					pt.g = 0.f;
					pt.b = 0.f;
				}
				out.points.push_back(pt);
			}
			out.frames.push_back(fr);
			off += (size_t)numRecords * recSize;
		}

		if (out.frames.empty() || out.points.empty()) {
			return false;
		}

		char infoBuf[64];
		std::snprintf(infoBuf, sizeof(infoBuf), "ILDA (%d frame%s, %d pts)",
					  (int)out.frames.size(), out.frames.size() == 1 ? "" : "s", (int)out.points.size());
		out.infoSummary = infoBuf;
		return true;
	}

	// Parse PCM / IEEE Float / WAVE_FORMAT_EXTENSIBLE WAV (1–8 channels)
	bool parseWavBytes(const std::vector<uint8_t>& raw, ShowData& out) {
		out = ShowData();
		out.isWav = true;

		if (raw.size() < 44) return false;
		if (std::memcmp(&raw[0], "RIFF", 4) != 0 || std::memcmp(&raw[8], "WAVE", 4) != 0) {
			return false;
		}

		bool hasFmt = false;
		uint16_t audioFormat = 0;
		uint16_t numChannels = 0;
		uint32_t sampleRate = 48000;
		uint16_t bitsPerSample = 16;
		const uint8_t* dataPtr = nullptr;
		uint32_t dataBytes = 0;

		size_t pos = 12;
		while (pos + 8 <= raw.size()) {
			const uint8_t* cid = &raw[pos];
			uint32_t csize = readU32LE(&raw[pos + 4]);
			pos += 8;
			size_t avail = raw.size() - pos;
			if (csize > avail) csize = (uint32_t)avail;

			if (std::memcmp(cid, "fmt ", 4) == 0 && csize >= 16) {
				audioFormat = readU16LE(&raw[pos + 0]);
				numChannels = readU16LE(&raw[pos + 2]);
				sampleRate = readU32LE(&raw[pos + 4]);
				bitsPerSample = readU16LE(&raw[pos + 14]);
				if (audioFormat == 0xFFFE && csize >= 40) {
					audioFormat = readU16LE(&raw[pos + 24]); // SubFormat GUID first 2 bytes
				}
				hasFmt = true;
			} else if (std::memcmp(cid, "data", 4) == 0) {
				dataPtr = &raw[pos];
				dataBytes = csize;
			}

			pos += csize;
			if (csize & 1) pos++; // RIFF word alignment
		}

		if (!hasFmt || !dataPtr || dataBytes == 0 || numChannels < 1 || numChannels > 16) {
			return false;
		}
		if (audioFormat != 1 && audioFormat != 3) {
			return false; // Only PCM (1) and IEEE Float (3) supported
		}
		if ((audioFormat == 1 && bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) ||
			(audioFormat == 3 && bitsPerSample != 32 && bitsPerSample != 64)) {
			return false;
		}

		size_t bytesPerSample = bitsPerSample / 8;
		size_t frameStride = bytesPerSample * numChannels;
		size_t totalFrames = dataBytes / frameStride;
		if (totalFrames == 0) return false;

		// Cap huge multi-minute WAVs to keep RAM predictable (up to 4,800,000 samples)
		const size_t MAX_WAV_FRAMES = 4800000;
		if (totalFrames > MAX_WAV_FRAMES) totalFrames = MAX_WAV_FRAMES;

		out.numChannels = numChannels;
		out.sampleRate = (sampleRate > 0) ? sampleRate : 48000;
		out.points.resize(totalFrames);

		auto decodeSample = [&](const uint8_t* p) -> float {
			if (audioFormat == 1) {
				if (bitsPerSample == 8) {
					return ((float)p[0] - 128.f) / 128.f;
				} else if (bitsPerSample == 16) {
					int16_t v = (int16_t)readU16LE(p);
					return (float)v / 32768.f;
				} else if (bitsPerSample == 24) {
					return (float)readI24LE(p) / 8388608.f;
				} else {
					return (float)((double)readI32LE(p) / 2147483648.0);
				}
			} else {
				if (bitsPerSample == 32) {
					float f = 0.f;
					std::memcpy(&f, p, 4);
					return clampf(f, -1.f, 1.f);
				} else {
					double d = 0.0;
					std::memcpy(&d, p, 8);
					return clampf((float)d, -1.f, 1.f);
				}
			}
		};

		auto mapColor = [&](float v) -> float {
			if (wavColorEncoding == WAV_COLOR_BIPOLAR) {
				return clampf(0.5f * (v + 1.f), 0.f, 1.f);
			}
			return clampf(v, 0.f, 1.f);
		};

		for (size_t i = 0; i < totalFrames; ++i) {
			const uint8_t* framePtr = dataPtr + i * frameStride;
			float ch[8] = {0.f};
			int readCh = std::min<int>(numChannels, 8);
			for (int k = 0; k < readCh; ++k) {
				ch[k] = decodeSample(framePtr + (size_t)k * bytesPerSample);
			}

			ShowPoint pt;
			pt.x = ch[0];
			pt.y = (numChannels >= 2) ? ch[1] : 0.f;
			pt.z = 0.f;
			pt.blank = false;

			if (numChannels <= 2) {
				pt.r = 1.f;
				pt.g = 1.f;
				pt.b = 1.f;
				pt.al = ch[0];
				pt.ar = (numChannels >= 2) ? ch[1] : ch[0];
			} else if (numChannels == 3) {
				float lum = mapColor(ch[2]);
				pt.r = lum;
				pt.g = lum;
				pt.b = lum;
				pt.blank = (lum <= 0.002f);
				pt.al = ch[0];
				pt.ar = ch[1];
			} else if (numChannels == 4) {
				pt.r = mapColor(ch[2]);
				pt.g = mapColor(ch[3]);
				pt.b = 0.f;
				pt.blank = (pt.r <= 0.002f && pt.g <= 0.002f);
				pt.al = ch[0];
				pt.ar = ch[1];
			} else { // 5..8+ channels: 1:X, 2:Y, 3:R, 4:G, 5:B, 6:Z/Blank, 7:Audio L, 8:Audio R
				pt.r = mapColor(ch[2]);
				pt.g = mapColor(ch[3]);
				pt.b = mapColor(ch[4]);
				if (numChannels >= 6) {
					if (wavCh6Mode == WAV_CH6_BLANK_GATE) {
						if (ch[5] < 0.05f) {
							pt.blank = true;
							pt.r = 0.f;
							pt.g = 0.f;
							pt.b = 0.f;
						}
					} else {
						pt.z = ch[5];
					}
				}
				if (numChannels >= 8) {
					pt.al = ch[6];
					pt.ar = ch[7];
				} else if (numChannels == 7) {
					pt.al = ch[6];
					pt.ar = ch[6];
				} else {
					pt.al = ch[0];
					pt.ar = ch[1];
				}
			}

			out.points[i] = pt;
		}

		FrameRange fr;
		fr.startIdx = 0;
		fr.pointCount = (int)totalFrames;
		fr.name = "WAV";
		out.frames.push_back(fr);

		char infoBuf[64];
		std::snprintf(infoBuf, sizeof(infoBuf), "WAV (%d ch, %.1f kHz)",
					  (int)numChannels, (float)out.sampleRate * 0.001f);
		out.infoSummary = infoBuf;
		return true;
	}

	void publishShowLocked() {
		uiPreviewShow = showSlots[uiSlot];
		uiSlot = midSlot.exchange(uiSlot | 0x10, std::memory_order_acq_rel) & 0x0F;
		int maxF = std::max(1, (int)uiPreviewShow.frames.size());
		if (manualFrame.load() >= maxF) {
			manualFrame.store(0);
		}
	}

	void reparseCurrentBytesLocked() {
		if (loadedRawBytes.empty()) return;
		ShowData parsed;
		bool ok = loadedIsWav ? parseWavBytes(loadedRawBytes, parsed) : parseIldaBytes(loadedRawBytes, parsed);
		if (ok) {
			showSlots[uiSlot] = std::move(parsed);
			publishShowLocked();
		}
	}

	void loadDefaultSequence() {
		std::lock_guard<std::mutex> lock(uiMutex);
		loadedFilePath.clear();
		loadedIsWav = false;
		loadedRawBytes = buildDefaultIldaBinary();
		reparseCurrentBytesLocked();
	}

	bool loadFromFile(const std::string& path) {
		std::FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) return false;
		std::vector<uint8_t> bytes;
		uint8_t buf[8192];
		while (true) {
			size_t n = std::fread(buf, 1, sizeof(buf), f);
			if (n > 0) bytes.insert(bytes.end(), buf, buf + n);
			if (n < sizeof(buf)) break;
		}
		std::fclose(f);
		if (bytes.size() < 32) return false;

		std::string ext = getLowerExtension(path);
		bool isWav = (ext == "wav" || ext == "wave");
		if (!isWav && bytes.size() >= 12 && std::memcmp(&bytes[0], "RIFF", 4) == 0 && std::memcmp(&bytes[8], "WAVE", 4) == 0) {
			isWav = true;
		}

		std::lock_guard<std::mutex> lock(uiMutex);
		ShowData parsed;
		bool ok = isWav ? parseWavBytes(bytes, parsed) : parseIldaBytes(bytes, parsed);
		if (!ok) {
			// Fallback attempt with opposite parser in case extension was missing
			ok = isWav ? parseIldaBytes(bytes, parsed) : parseWavBytes(bytes, parsed);
			if (ok) isWav = !isWav;
		}
		if (!ok) return false;

		loadedFilePath = path;
		loadedIsWav = isWav;
		loadedRawBytes = std::move(bytes);
		manualFrame.store(0);
		showSlots[uiSlot] = std::move(parsed);
		publishShowLocked();
		return true;
	}

	void setWavCh6Mode(WavCh6Mode mode) {
		std::lock_guard<std::mutex> lock(uiMutex);
		if (wavCh6Mode == mode) return;
		wavCh6Mode = mode;
		if (loadedIsWav) reparseCurrentBytesLocked();
	}

	void setWavColorEncoding(WavColorEncoding enc) {
		std::lock_guard<std::mutex> lock(uiMutex);
		if (wavColorEncoding == enc) return;
		wavColorEncoding = enc;
		if (loadedIsWav) reparseCurrentBytesLocked();
	}

	inline void transformXYZ(float inX, float inY, float inZ,
							 float revDeg, bool fX, bool fY,
							 float& outX, float& outY, float& outZ) const {
		float sx = fX ? -inX : inX;
		float sy = fY ? -inY : inY;
		float sz = inZ;

		// Optional 3D XZ yaw revolve (driven from context menu Revolve preset)
		float rx = sx;
		float rz = sz;
		if (std::abs(revDeg) > 0.001f) {
			float revRad = revDeg * (float)(M_PI / 180.0);
			float cr = std::cos(revRad);
			float sr = std::sin(revRad);
			rx = sx * cr + sz * sr;
			rz = -sx * sr + sz * cr;
		}

		outX = rx;
		outY = sy;
		outZ = rz;
	}

	void process(const ProcessArgs& args) override {
		// Wait-free check for new ShowData from UI thread
		int m = midSlot.load(std::memory_order_relaxed);
		if (m & 0x10) {
			audioSlot = midSlot.exchange(audioSlot, std::memory_order_acq_rel) & 0x0F;
		}
		const ShowData& show = showSlots[audioSlot];

		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		int totalFrames = (int)show.frames.size();
		int totalPts = (int)show.points.size();

		// Front-panel Reset playback button
		if (resetBtnTrigger.process(params[RESET_BUTTON_PARAM].getValue() > 0.5f)) {
			for (int i = 0; i < 16; ++i) {
				voices[i].scanPhase = 0.f;
				voices[i].wavSamplePos = 0.f;
				voices[i].wavFreezePos = 0.f;
				voices[i].curFrame = 0;
				voices[i].animTimer = 0.f;
				voices[i].syncPulse.trigger(1e-4f);
			}
		}

		// Front-panel Freeze frame toggle button
		if (freezeBtnTrigger.process(params[FREEZE_BUTTON_PARAM].getValue() > 0.5f)) {
			bool nextFreeze = !masterFrozen.load(std::memory_order_relaxed);
			masterFrozen.store(nextFreeze, std::memory_order_relaxed);
			for (int i = 0; i < 16; ++i) {
				voices[i].frozen = nextFreeze;
				if (nextFreeze) {
					voices[i].wavFreezePos = voices[i].wavSamplePos;
				}
			}
		}

		// Update optional continuous 3D revolve
		if (revolvePreset == REVOLVE_AUTO_SPIN) {
			autoSpinDeg += 36.f * args.sampleTime;
			if (autoSpinDeg >= 180.f) autoSpinDeg -= 360.f;
		}
		float revDeg = 0.f;
		switch (revolvePreset) {
			case REVOLVE_TILT_POS_25: revDeg = 25.f; break;
			case REVOLVE_TILT_POS_45: revDeg = 45.f; break;
			case REVOLVE_TILT_NEG_25: revDeg = -25.f; break;
			case REVOLVE_AUTO_SPIN:   revDeg = autoSpinDeg; break;
			case REVOLVE_OFF:
			default:                  revDeg = 0.f; break;
		}
		displayRevolveDeg.store(revDeg, std::memory_order_relaxed);

		// Polyphony count across all 6 inputs
		int numChannels = 1;
		for (int inp = 0; inp < INPUTS_LEN; ++inp) {
			numChannels = std::max(numChannels, inputs[inp].getChannels());
		}
		numChannels = std::min(numChannels, 16);

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);
		outputs[Z_OUTPUT].setChannels(numChannels);
		outputs[R_OUTPUT].setChannels(numChannels);
		outputs[G_OUTPUT].setChannels(numChannels);
		outputs[B_OUTPUT].setChannels(numChannels);
		outputs[AUDIO_L_OUTPUT].setChannels(numChannels);
		outputs[AUDIO_R_OUTPUT].setChannels(numChannels);
		outputs[SYNC_OUTPUT].setChannels(numChannels);

		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam = params[FINE_PARAM].getValue();
		float baseFreqHz = calculateBaseFrequency(coarseParam, fineParam);
		float frameRateParam = params[FRAME_RATE_PARAM].getValue();

		float freqTrim = params[FREQ_TRIM_PARAM].getValue();
		float fpsTrim = params[FPS_TRIM_PARAM].getValue();

		float vMin, vMax;
		getRangeVoltages(vMin, vMax);
		float vMid = 0.5f * (vMin + vMax);
		float vHalfSpan = 0.5f * (vMax - vMin);
		float rgbMaxV = getRgbScaleVoltage();

		bool frameSyncConnected = inputs[FRAME_SYNC_INPUT].isConnected();
		bool freqSyncConnected = inputs[FREQ_SYNC_INPUT].isConnected();
		bool fX = flipX.load(std::memory_order_relaxed);
		bool fY = flipY.load(std::memory_order_relaxed);
		int manFrame = manualFrame.load(std::memory_order_relaxed);

		// Precompute 1-pole DC blocker coeff (~5 Hz) for AC audio output when show has < 7 channels
		float dcR = clampf(1.f - (2.f * (float)M_PI * 5.f * args.sampleTime), 0.90f, 0.9999f);

		for (int c = 0; c < numChannels; ++c) {
			VoiceState& vs = voices[c];
			if (vs.curFrame >= totalFrames) vs.curFrame = 0;

			float freqCvV = inputs[FREQ_CV_INPUT].getPolyVoltage(c);
			float freqOct = clampf(freqCvV * freqTrim, -6.f, 8.f);
			float voiceFreq = baseFreqHz * std::pow(2.f, freqOct);

			float fpsCvV = inputs[FPS_CV_INPUT].getPolyVoltage(c) / 5.f;
			float voiceFps = clampf(frameRateParam + fpsCvV * fpsTrim * 60.f, 0.f, 120.f);

			// Gate input toggles freezing the frame on rising edge
			if (inputs[FREEZE_INPUT].isConnected()) {
				if (vs.freezeTrigger.process(inputs[FREEZE_INPUT].getPolyVoltage(c), 0.1f, 2.0f)) {
					vs.frozen = !vs.frozen;
					if (vs.frozen) {
						vs.wavFreezePos = vs.wavSamplePos;
					}
					if (c == 0) {
						masterFrozen.store(vs.frozen, std::memory_order_relaxed);
					}
				}
			}

			// Reset trigger input
			bool resetTrig = false;
			if (inputs[RESET_INPUT].isConnected()) {
				if (vs.resetTrigger.process(inputs[RESET_INPUT].getPolyVoltage(c), 0.1f, 2.0f)) {
					vs.scanPhase = 0.f;
					vs.wavSamplePos = 0.f;
					vs.wavFreezePos = 0.f;
					vs.curFrame = 0;
					vs.animTimer = 0.f;
					resetTrig = true;
					vs.syncPulse.trigger(1e-4f);
				}
			}

			// Frame Sync In (Row 1 Col 0): steps/restarts animation frames
			bool frameSyncTrig = false;
			if (frameSyncConnected) {
				if (vs.frameSyncTrigger.process(inputs[FRAME_SYNC_INPUT].getPolyVoltage(c), 0.1f, 2.0f)) {
					frameSyncTrig = true;
					if (!vs.frozen) {
						if (show.isWav) {
							vs.wavSamplePos = 0.f;
						} else if (totalFrames > 0) {
							if (syncInMode == SYNC_STEP_FRAME) {
								vs.curFrame = (vs.curFrame + 1) % totalFrames;
							} else {
								vs.curFrame = 0;
							}
							vs.animTimer = 0.f;
						}
					}
				}
			}

			// Frequency Sync In (Row 2 Col 0): hard-syncs point scan phase & frequency
			bool freqSyncTrig = false;
			if (freqSyncConnected) {
				vs.timeSinceSync += args.sampleTime;
				if (vs.freqSyncTrigger.process(inputs[FREQ_SYNC_INPUT].getPolyVoltage(c), 0.1f, 2.0f)) {
					if (vs.timeSinceSync > 0.0005f) {
						vs.syncPeriod = vs.timeSinceSync;
					}
					vs.timeSinceSync = 0.f;
					vs.scanPhase = 0.f;
					freqSyncTrig = true;
					vs.syncPulse.trigger(1e-4f);
				}
			} else {
				vs.syncPeriod = 0.f;
			}

			ShowPoint sampled;
			int activeFrameIdx = 0;
			float activeScanPos = 0.f;

			if (totalPts > 0 && totalFrames > 0) {
				if (show.isWav) {
					if (wavSpeedMode == WAV_SINGLE_CYCLE) {
						float f0 = (freqSyncConnected && vs.syncPeriod > 0.f) ? (1.f / vs.syncPeriod) : voiceFreq;
						vs.scanPhase += f0 * args.sampleTime;
						if (vs.scanPhase >= 1.f) {
							vs.scanPhase -= std::floor(vs.scanPhase);
							if (!freqSyncTrig && !resetTrig) vs.syncPulse.trigger(1e-4f);
						}
						float pos = vs.scanPhase * (float)totalPts;
						int i0 = (int)pos % totalPts;
						int i1 = (i0 + 1) % totalPts;
						float frac = (interpMode == INTERP_LINEAR) ? (pos - std::floor(pos)) : 0.f;
						const ShowPoint& p0 = show.points[i0];
						const ShowPoint& p1 = show.points[i1];
						sampled.x = p0.x + (p1.x - p0.x) * frac;
						sampled.y = p0.y + (p1.y - p0.y) * frac;
						sampled.z = p0.z + (p1.z - p0.z) * frac;
						sampled.r = (p0.blank || p1.blank) ? p0.r : (p0.r + (p1.r - p0.r) * frac);
						sampled.g = (p0.blank || p1.blank) ? p0.g : (p0.g + (p1.g - p0.g) * frac);
						sampled.b = (p0.blank || p1.blank) ? p0.b : (p0.b + (p1.b - p0.b) * frac);
						sampled.al = p0.al + (p1.al - p0.al) * frac;
						sampled.ar = p0.ar + (p1.ar - p0.ar) * frac;
						sampled.blank = p0.blank;
						activeScanPos = vs.scanPhase;
					} else {
						// V/Oct speed relative to native WAV sampleRate (1.0x when Freq = 0.5 in LFO range)
						float rangeMult = (rangeMode == RANGE_VERY_SLOW) ? 0.1f : (rangeMode == RANGE_VCO ? 8.0f : 1.0f);
						float speed = rangeMult * std::pow(2.f, (coarseParam - 0.5f) * 8.f + fineParam * 0.5f + freqOct);
						float samplePosNow = 0.f;
						if (vs.frozen) {
							// Loop local frame window when Frozen on WAV so beam trace stays active
							int sliceLen = std::min(1024, totalPts);
							vs.scanPhase += voiceFreq * args.sampleTime;
							if (vs.scanPhase >= 1.f) {
								vs.scanPhase -= std::floor(vs.scanPhase);
								if (!freqSyncTrig && !resetTrig) vs.syncPulse.trigger(1e-4f);
							}
							samplePosNow = std::fmod(vs.wavFreezePos + vs.scanPhase * (float)sliceLen, (float)totalPts);
							activeScanPos = samplePosNow / (float)totalPts;
						} else {
							vs.wavSamplePos += speed * (float)show.sampleRate * args.sampleTime;
							if (vs.wavSamplePos >= (float)totalPts) {
								vs.wavSamplePos -= std::floor(vs.wavSamplePos / (float)totalPts) * (float)totalPts;
								if (!freqSyncTrig && !resetTrig && !frameSyncTrig) vs.syncPulse.trigger(1e-4f);
							}
							if (vs.wavSamplePos < 0.f) vs.wavSamplePos = 0.f;
							samplePosNow = vs.wavSamplePos;
							activeScanPos = vs.wavSamplePos / (float)totalPts;
						}
						int i0 = (int)samplePosNow % totalPts;
						int i1 = (i0 + 1) % totalPts;
						float frac = (interpMode == INTERP_LINEAR) ? (samplePosNow - std::floor(samplePosNow)) : 0.f;
						const ShowPoint& p0 = show.points[i0];
						const ShowPoint& p1 = show.points[i1];
						sampled.x = p0.x + (p1.x - p0.x) * frac;
						sampled.y = p0.y + (p1.y - p0.y) * frac;
						sampled.z = p0.z + (p1.z - p0.z) * frac;
						sampled.r = (p0.blank || p1.blank) ? p0.r : (p0.r + (p1.r - p0.r) * frac);
						sampled.g = (p0.blank || p1.blank) ? p0.g : (p0.g + (p1.g - p0.g) * frac);
						sampled.b = (p0.blank || p1.blank) ? p0.b : (p0.b + (p1.b - p0.b) * frac);
						sampled.al = p0.al + (p1.al - p0.al) * frac;
						sampled.ar = p0.ar + (p1.ar - p0.ar) * frac;
						sampled.blank = p0.blank;
					}
				} else {
					// ILDA Multi-Frame Show
					float f0 = (freqSyncConnected && vs.syncPeriod > 0.f) ? (1.f / vs.syncPeriod) : voiceFreq;
					vs.scanPhase += f0 * args.sampleTime;
					if (vs.scanPhase >= 1.f) {
						vs.scanPhase -= std::floor(vs.scanPhase);
						if (!freqSyncTrig && !resetTrig) vs.syncPulse.trigger(1e-4f);
					}

					if (frameMode == FRAME_ALL_SEQ && !vs.frozen) {
						float globalPos = vs.scanPhase * (float)totalPts;
						int i0 = (int)globalPos % totalPts;
						int i1 = (i0 + 1) % totalPts;
						float frac = (interpMode == INTERP_LINEAR) ? (globalPos - std::floor(globalPos)) : 0.f;
						const ShowPoint& p0 = show.points[i0];
						const ShowPoint& p1 = show.points[i1];
						bool noBlend = (p0.blank || p1.blank || interpMode == INTERP_NEAREST);
						sampled.x = p0.x + (p1.x - p0.x) * frac;
						sampled.y = p0.y + (p1.y - p0.y) * frac;
						sampled.z = p0.z + (p1.z - p0.z) * frac;
						sampled.r = noBlend ? p0.r : (p0.r + (p1.r - p0.r) * frac);
						sampled.g = noBlend ? p0.g : (p0.g + (p1.g - p0.g) * frac);
						sampled.b = noBlend ? p0.b : (p0.b + (p1.b - p0.b) * frac);
						sampled.blank = p0.blank;
						activeFrameIdx = clampf((float)(vs.scanPhase * totalFrames), 0.f, (float)(totalFrames - 1));
						activeScanPos = vs.scanPhase;
					} else {
						if (!vs.frozen) {
							if (frameMode == FRAME_MANUAL) {
								vs.curFrame = manFrame % totalFrames;
							} else if (!frameSyncConnected && totalFrames > 1 && voiceFps > 0.01f) {
								// Auto-advance frames using FRAME RATE knob + FPS CV
								float frameDur = 1.f / voiceFps;
								vs.animTimer += args.sampleTime;
								while (vs.animTimer >= frameDur) {
									vs.animTimer -= frameDur;
									vs.curFrame = (vs.curFrame + 1) % totalFrames;
								}
							}
						}
						activeFrameIdx = vs.curFrame;
						const FrameRange& fr = show.frames[activeFrameIdx];
						if (fr.pointCount > 0) {
							float fPos = vs.scanPhase * (float)fr.pointCount;
							int local0 = (int)fPos % fr.pointCount;
							int local1 = (local0 + 1) % fr.pointCount;
							float frac = (interpMode == INTERP_LINEAR) ? (fPos - std::floor(fPos)) : 0.f;
							const ShowPoint& p0 = show.points[fr.startIdx + local0];
							const ShowPoint& p1 = show.points[fr.startIdx + local1];
							bool noBlend = (p0.blank || p1.blank || interpMode == INTERP_NEAREST);
							sampled.x = p0.x + (p1.x - p0.x) * frac;
							sampled.y = p0.y + (p1.y - p0.y) * frac;
							sampled.z = p0.z + (p1.z - p0.z) * frac;
							sampled.r = noBlend ? p0.r : (p0.r + (p1.r - p0.r) * frac);
							sampled.g = noBlend ? p0.g : (p0.g + (p1.g - p0.g) * frac);
							sampled.b = noBlend ? p0.b : (p0.b + (p1.b - p0.b) * frac);
							sampled.blank = p0.blank;
						}
						activeScanPos = vs.scanPhase;
					}
				}
			}

			if (c == 0) {
				displayFrame.store(activeFrameIdx, std::memory_order_relaxed);
				displayScanPos.store(activeScanPos, std::memory_order_relaxed);
				lights[FREEZE_LIGHT].setBrightness(vs.frozen ? 1.f : 0.f);
			}

			float tx = 0.f, ty = 0.f, tz = 0.f;
			transformXYZ(sampled.x, sampled.y, sampled.z, revDeg, fX, fY, tx, ty, tz);

			// XYZ coordinate outputs (mapped through Generate voltage range, clamped ±12V)
			outputs[X_OUTPUT].setVoltage(clampf(vMid + tx * vHalfSpan, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clampf(vMid + ty * vHalfSpan, -12.f, 12.f), c);
			outputs[Z_OUTPUT].setVoltage(clampf(vMid + tz * vHalfSpan, -12.f, 12.f), c);

			// RGB color outputs (0V when blanked per ILDA Spec §5.2.4)
			float rV = sampled.blank ? 0.f : clampf(sampled.r, 0.f, 1.f) * rgbMaxV;
			float gV = sampled.blank ? 0.f : clampf(sampled.g, 0.f, 1.f) * rgbMaxV;
			float bV = sampled.blank ? 0.f : clampf(sampled.b, 0.f, 1.f) * rgbMaxV;
			outputs[R_OUTPUT].setVoltage(rV, c);
			outputs[G_OUTPUT].setVoltage(gV, c);
			outputs[B_OUTPUT].setVoltage(bV, c);

			// Left & Right Audio Out:
			// Use dedicated Ch 7 & Ch 8 when loading 7/8-ch WAV; otherwise AC-coupled X/Y deflection audio at ±5V
			float aL = 0.f, aR = 0.f;
			if (show.isWav && show.numChannels >= 7) {
				aL = clampf(sampled.al * 5.f, -10.f, 10.f);
				aR = clampf(sampled.ar * 5.f, -10.f, 10.f);
			} else {
				float acX = tx - vs.dcInX + dcR * vs.dcOutX;
				vs.dcInX = tx;
				vs.dcOutX = acX;
				float acY = ty - vs.dcInY + dcR * vs.dcOutY;
				vs.dcInY = ty;
				vs.dcOutY = acY;
				aL = clampf(acX * 5.f, -10.f, 10.f);
				aR = clampf(acY * 5.f, -10.f, 10.f);
			}
			outputs[AUDIO_L_OUTPUT].setVoltage(aL, c);
			outputs[AUDIO_R_OUTPUT].setVoltage(aR, c);

			outputs[SYNC_OUTPUT].setVoltage(vs.syncPulse.process(args.sampleTime) ? 10.f : 0.f, c);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		json_object_set_new(rootJ, "rgbVoltageScale", json_integer((int)rgbVoltageScale));
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "interpMode", json_integer((int)interpMode));
		json_object_set_new(rootJ, "frameMode", json_integer((int)frameMode));
		json_object_set_new(rootJ, "syncInMode", json_integer((int)syncInMode));
		json_object_set_new(rootJ, "wavSpeedMode", json_integer((int)wavSpeedMode));
		json_object_set_new(rootJ, "wavCh6Mode", json_integer((int)wavCh6Mode));
		json_object_set_new(rootJ, "wavColorEncoding", json_integer((int)wavColorEncoding));
		json_object_set_new(rootJ, "revolvePreset", json_integer((int)revolvePreset));
		json_object_set_new(rootJ, "showBlankedMoves", json_boolean(showBlankedMoves.load()));
		json_object_set_new(rootJ, "flipX", json_boolean(flipX.load()));
		json_object_set_new(rootJ, "flipY", json_boolean(flipY.load()));
		json_object_set_new(rootJ, "masterFrozen", json_boolean(masterFrozen.load()));
		json_object_set_new(rootJ, "manualFrame", json_integer(manualFrame.load()));
		{
			std::lock_guard<std::mutex> lock(uiMutex);
			json_object_set_new(rootJ, "filePath", json_string(loadedFilePath.c_str()));
		}
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) rangeMode = (RangeMode)clampf((float)json_integer_value(rmJ), 0.f, 2.f);

		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) voltageRange = (VoltageRange)clampf((float)json_integer_value(vrJ), 0.f, (float)(NUM_VOLTAGE_RANGES - 1));

		json_t* rgbJ = json_object_get(rootJ, "rgbVoltageScale");
		if (rgbJ) rgbVoltageScale = (RgbVoltageScale)clampf((float)json_integer_value(rgbJ), 0.f, 2.f);

		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)clampf((float)json_integer_value(stJ), 0.f, 2.f);

		json_t* imJ = json_object_get(rootJ, "interpMode");
		if (imJ) interpMode = (InterpMode)clampf((float)json_integer_value(imJ), 0.f, 1.f);

		json_t* fmJ = json_object_get(rootJ, "frameMode");
		if (fmJ) frameMode = (FrameMode)clampf((float)json_integer_value(fmJ), 0.f, 2.f);

		json_t* smJ = json_object_get(rootJ, "syncInMode");
		if (smJ) syncInMode = (SyncInMode)clampf((float)json_integer_value(smJ), 0.f, 1.f);

		json_t* wsJ = json_object_get(rootJ, "wavSpeedMode");
		if (wsJ) wavSpeedMode = (WavSpeedMode)clampf((float)json_integer_value(wsJ), 0.f, 1.f);

		json_t* wc6J = json_object_get(rootJ, "wavCh6Mode");
		if (wc6J) wavCh6Mode = (WavCh6Mode)clampf((float)json_integer_value(wc6J), 0.f, 1.f);

		json_t* wceJ = json_object_get(rootJ, "wavColorEncoding");
		if (wceJ) wavColorEncoding = (WavColorEncoding)clampf((float)json_integer_value(wceJ), 0.f, 1.f);

		json_t* rpJ = json_object_get(rootJ, "revolvePreset");
		if (rpJ) revolvePreset = (RevolvePreset)clampf((float)json_integer_value(rpJ), 0.f, 4.f);

		json_t* sbmJ = json_object_get(rootJ, "showBlankedMoves");
		if (sbmJ) showBlankedMoves.store(json_boolean_value(sbmJ));

		json_t* fxJ = json_object_get(rootJ, "flipX");
		if (fxJ) flipX.store(json_boolean_value(fxJ));

		json_t* fyJ = json_object_get(rootJ, "flipY");
		if (fyJ) flipY.store(json_boolean_value(fyJ));

		json_t* frzJ = json_object_get(rootJ, "masterFrozen");
		if (frzJ) {
			bool frz = json_boolean_value(frzJ);
			masterFrozen.store(frz);
			for (int i = 0; i < 16; ++i) voices[i].frozen = frz;
		}

		json_t* mfJ = json_object_get(rootJ, "manualFrame");
		if (mfJ) manualFrame.store(std::max(0, (int)json_integer_value(mfJ)));

		json_t* fpJ = json_object_get(rootJ, "filePath");
		std::string path = fpJ ? json_string_value(fpJ) : "";
		if (!path.empty()) {
			if (!loadFromFile(path)) {
				loadDefaultSequence();
			}
		} else {
			loadDefaultSequence();
		}
	}
};

// ── Custom Hardware Widgets per docs/module_design.md ──
struct IldaPort : app::SvgPort {
	IldaPort() { setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg"))); }
};

struct IldaKnob : RoundKnob {
	IldaKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct IldaTrimpot : RoundKnob {
	IldaTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct IldaButton : app::SvgSwitch {
	IldaButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Custom 3-Color Illuminated Range Ring ──
struct IldaRangeLightRing : ModuleLightWidget {
	IldaRangeLightRing() {
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

// ── Custom Illuminated Ring for Small Freeze Toggle Button ──
struct IldaFreezeLightRing : ModuleLightWidget {
	IldaFreezeLightRing() {
		box.size = mm2px(Vec(9.0f, 9.0f));
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // Frozen: Active Teal
	}

	void drawBackground(const DrawArgs& args) override {
		Vec c = box.size.div(2.f);
		float r = mm2px(3.20f);
		float w = mm2px(0.85f);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, nvgRGBA(0x22, 0x22, 0x22, 0x99));
		nvgStrokeWidth(args.vg, w);
		nvgStroke(args.vg);
	}

	void drawLight(const DrawArgs& args) override {
		if (color.a <= 0.0f) return;
		Vec c = box.size.div(2.f);
		float r = mm2px(3.20f);
		float w = mm2px(0.85f);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, color);
		nvgStrokeWidth(args.vg, w);
		nvgStroke(args.vg);
	}

	void drawHalo(const DrawArgs& args) override {
		if (color.a <= 0.0f) return;
		Vec c = box.size.div(2.f);
		float r = mm2px(3.20f);
		float haloW = mm2px(0.75f);
		NVGcolor haloColor = color;
		haloColor.a *= 0.175f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, haloColor);
		nvgStrokeWidth(args.vg, mm2px(0.85f) + haloW);
		nvgStroke(args.vg);
	}
};

// ── Live Full-Color ILDA / WAV Laser Preview Display ──
struct IldaDisplayWidget : widget::TransparentWidget {
	Ilda* module = nullptr;

	IldaDisplayWidget(Ilda* m) : module(m) {
		// Box: X = 7.400..63.720 mm (56.320 mm), Y = 11.000..41.000 mm (30.000 mm)
		box.size = mm2px(Vec(56.320f, 30.000f));
	}

	void draw(const DrawArgs& args) override {
		float w = box.size.x;
		float h = box.size.y;
		float cx = w * 0.5f;
		float cy = h * 0.5f;
		float radius = std::min(w, h) * 0.43f;

		float revDeg = 0.f;
		bool showBlank = false;
		bool fX = false, fY = false;
		bool isFrozen = false;
		int activeFrameIdx = 0;
		float scanPos = 0.f;
		std::string label = "default.ild";
		Ilda::ShowData localShow;
		bool hasModule = (module != nullptr);

		if (hasModule) {
			revDeg = module->displayRevolveDeg.load(std::memory_order_relaxed);
			showBlank = module->showBlankedMoves.load(std::memory_order_relaxed);
			fX = module->flipX.load(std::memory_order_relaxed);
			fY = module->flipY.load(std::memory_order_relaxed);
			isFrozen = module->masterFrozen.load(std::memory_order_relaxed);
			activeFrameIdx = module->displayFrame.load(std::memory_order_relaxed);
			scanPos = module->displayScanPos.load(std::memory_order_relaxed);

			std::lock_guard<std::mutex> lock(module->uiMutex);
			label = extractFilenameOnly(module->loadedFilePath);
			localShow = module->uiPreviewShow;
		} else {
			// Clean fallback emblem for Module Browser thumbnail
			Ilda::FrameRange pr;
			pr.startIdx = 0;
			pr.pointCount = 65;
			pr.name = "Preview";
			localShow.frames.push_back(pr);
			localShow.points.reserve(65);
			for (int i = 0; i <= 64; ++i) {
				float a = 2.f * (float)M_PI * ((float)i / 64.f);
				Ilda::ShowPoint pt;
				pt.x = 0.72f * std::cos(a);
				pt.y = 0.72f * std::sin(a);
				pt.r = 0.25f + 0.75f * (0.5f + 0.5f * std::cos(a));
				pt.g = 0.55f + 0.45f * std::sin(a);
				pt.b = 0.85f;
				localShow.points.push_back(pt);
			}
		}

		nvgSave(args.vg);
		nvgScissor(args.vg, 0.f, 0.f, w, h);

		// Deep dark laser chamber background
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, w, h, mm2px(1.5f));
		nvgFillColor(args.vg, nvgRGBA(0x10, 0x10, 0x14, 0xff));
		nvgFill(args.vg);

		// Subtle center crosshair
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x1a));
		nvgStrokeWidth(args.vg, 0.75f);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, cx, 3.f);
		nvgLineTo(args.vg, cx, h - 3.f);
		nvgMoveTo(args.vg, 3.f, cy);
		nvgLineTo(args.vg, w - 3.f, cy);
		nvgStroke(args.vg);

		// Draw active frame or WAV window
		if (!localShow.frames.empty() && !localShow.points.empty()) {
			int frIdx = activeFrameIdx % (int)localShow.frames.size();
			if (frIdx < 0) frIdx = 0;
			const auto& fr = localShow.frames[frIdx];
			int count = fr.pointCount;
			int startIdx = fr.startIdx;

			if (count > 1 && startIdx + count <= (int)localShow.points.size()) {
				int step = std::max(1, count / 768);
				float prevX = 0.f, prevY = 0.f;
				bool havePrev = false;

				auto xform = [&](const Ilda::ShowPoint& p, float& sx, float& sy) {
					float tx = p.x, ty = p.y, tz = p.z;
					if (hasModule) {
						module->transformXYZ(p.x, p.y, p.z, revDeg, fX, fY, tx, ty, tz);
					}
					sx = cx + tx * radius;
					sy = cy - ty * radius;
				};

				for (int i = 0; i < count; i += step) {
					const auto& pt = localShow.points[startIdx + i];
					float sx, sy;
					xform(pt, sx, sy);

					if (havePrev) {
						if (!pt.blank) {
							uint8_t cr = (uint8_t)clampf(pt.r * 255.f, 0.f, 255.f);
							uint8_t cg = (uint8_t)clampf(pt.g * 255.f, 0.f, 255.f);
							uint8_t cb = (uint8_t)clampf(pt.b * 255.f, 0.f, 255.f);
							if (cr + cg + cb > 4) {
								nvgBeginPath(args.vg);
								nvgMoveTo(args.vg, prevX, prevY);
								nvgLineTo(args.vg, sx, sy);
								nvgStrokeColor(args.vg, nvgRGBA(cr, cg, cb, 0x38));
								nvgStrokeWidth(args.vg, 3.0f);
								nvgStroke(args.vg);

								nvgBeginPath(args.vg);
								nvgMoveTo(args.vg, prevX, prevY);
								nvgLineTo(args.vg, sx, sy);
								nvgStrokeColor(args.vg, nvgRGBA(cr, cg, cb, 0xf2));
								nvgStrokeWidth(args.vg, 1.35f);
								nvgStroke(args.vg);
							}
						} else if (showBlank) {
							nvgBeginPath(args.vg);
							nvgMoveTo(args.vg, prevX, prevY);
							nvgLineTo(args.vg, sx, sy);
							nvgStrokeColor(args.vg, nvgRGBA(0x88, 0x88, 0x99, 0x45));
							nvgStrokeWidth(args.vg, 0.7f);
							nvgStroke(args.vg);
						}
					}
					prevX = sx;
					prevY = sy;
					havePrev = true;
				}

				// Active galvo beam spot
				if (hasModule) {
					int bOff = ((int)(scanPos * (float)count)) % count;
					if (bOff < 0) bOff += count;
					const auto& bPt = localShow.points[startIdx + bOff];
					float bx, by;
					xform(bPt, bx, by);
					nvgBeginPath(args.vg);
					nvgCircle(args.vg, bx, by, bPt.blank ? 1.5f : 2.3f);
					if (bPt.blank) {
						nvgFillColor(args.vg, nvgRGBA(0x99, 0x99, 0xaa, 0x80));
					} else {
						nvgFillColor(args.vg, nvgRGBA(
							(uint8_t)clampf(bPt.r * 255.f, 60.f, 255.f),
							(uint8_t)clampf(bPt.g * 255.f, 60.f, 255.f),
							(uint8_t)clampf(bPt.b * 255.f, 60.f, 255.f), 0xff));
					}
					nvgFill(args.vg);
				}
			}
		}

		// File & frame overlay text at bottom corners
		if (label.size() > 22) {
			label = "..." + label.substr(label.size() - 19);
		}
		nvgFontSize(args.vg, 8.5f);
		nvgFontFaceId(args.vg, APP->window->uiFont->handle);
		nvgFillColor(args.vg, nvgRGBA(0xcc, 0xcc, 0xd4, 0xb0));
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
		nvgText(args.vg, 4.f, h - 2.5f, label.c_str(), nullptr);

		if (hasModule && isFrozen) {
			nvgFillColor(args.vg, nvgRGBA(0x40, 0xb0, 0xa6, 0xdd));
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
			nvgText(args.vg, w - 4.f, 2.5f, "FROZEN", nullptr);
		}

		if (hasModule && localShow.frames.size() > 1) {
			char frBuf[24];
			std::snprintf(frBuf, sizeof(frBuf), "F%d/%d",
						  (activeFrameIdx % (int)localShow.frames.size()) + 1,
						  (int)localShow.frames.size());
			nvgFillColor(args.vg, nvgRGBA(0xcc, 0xcc, 0xd4, 0xb0));
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
			nvgText(args.vg, w - 4.f, h - 2.5f, frBuf, nullptr);
		} else if (hasModule && localShow.isWav) {
			char chBuf[24];
			std::snprintf(chBuf, sizeof(chBuf), "%dCH WAV", localShow.numChannels);
			nvgFillColor(args.vg, nvgRGBA(0xcc, 0xcc, 0xd4, 0xb0));
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
			nvgText(args.vg, w - 4.f, h - 2.5f, chBuf, nullptr);
		}

		nvgRestore(args.vg);
	}
};

struct IldaWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Ilda::SkinTheme currentTheme = Ilda::THEME_MEDIUM;

	void setTheme(Ilda::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Ilda::THEME_LIGHT:  path = asset::plugin(pluginInstance, "res/Ilda_Light.svg"); break;
				case Ilda::THEME_DARK:   path = asset::plugin(pluginInstance, "res/Ilda_Dark.svg"); break;
				case Ilda::THEME_MEDIUM:
				default:                 path = asset::plugin(pluginInstance, "res/Ilda.svg"); break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Ilda*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	IldaWidget(Ilda* module) {
		setModule(module);

		panel = new SvgPanel();
		Ilda::SkinTheme initTheme = module ? module->skinTheme : Ilda::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Ilda::THEME_LIGHT:  initPath = asset::plugin(pluginInstance, "res/Ilda_Light.svg"); break;
			case Ilda::THEME_DARK:   initPath = asset::plugin(pluginInstance, "res/Ilda_Dark.svg"); break;
			case Ilda::THEME_MEDIUM:
			default:                 initPath = asset::plugin(pluginInstance, "res/Ilda.svg"); break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// 4-Column Row 1 Grid & 5-Column Lower Grid on 14 HP (71.120 mm)
		const float col4_x[4] = {8.560f, 26.560f, 44.560f, 62.560f};
		const float col5_x[5] = {8.560f, 22.060f, 35.560f, 49.060f, 62.560f};

		// ── Large Top Laser Preview Display ──
		auto* display = new IldaDisplayWidget(module);
		display->box.pos = mm2px(Vec(7.400f, 11.000f));
		addChild(display);

		// ── Zone 2: Primary Knobs on One Line (Center Y = 52.000 mm) ──
		addParam(createParamCentered<IldaKnob>(mm2px(Vec(col4_x[0], 52.000f)), module, Ilda::FREQ_PARAM));
		addChild(createLightCentered<IldaRangeLightRing>(mm2px(Vec(col4_x[1], 52.000f)), module, Ilda::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<IldaButton>(mm2px(Vec(col4_x[1], 52.000f)), module, Ilda::RANGE_PARAM));
		addParam(createParamCentered<IldaKnob>(mm2px(Vec(col4_x[2], 52.000f)), module, Ilda::FINE_PARAM));
		addParam(createParamCentered<IldaKnob>(mm2px(Vec(col4_x[3], 52.000f)), module, Ilda::FRAME_RATE_PARAM));

		// ── Zone 3: Attenuverters & Small Control Buttons (Center Y = 73.000 mm) ──
		addParam(createParamCentered<IldaTrimpot>(mm2px(Vec(col5_x[1], 73.000f)), module, Ilda::FREQ_TRIM_PARAM));
		addParam(createParamCentered<IldaButton>(mm2px(Vec(col5_x[2], 73.000f)), module, Ilda::RESET_BUTTON_PARAM));
		addParam(createParamCentered<IldaTrimpot>(mm2px(Vec(col5_x[3], 73.000f)), module, Ilda::FPS_TRIM_PARAM));
		addChild(createLightCentered<IldaFreezeLightRing>(mm2px(Vec(col5_x[4], 73.000f)), module, Ilda::FREEZE_LIGHT));
		addParam(createParamCentered<IldaButton>(mm2px(Vec(col5_x[4], 73.000f)), module, Ilda::FREEZE_BUTTON_PARAM));

		// ── Zone 4: 3-Row x 5-Column Jack Matrix ──
		// Row 1 (Center Y = 91.000 mm): SYNC IN, FREQ, RESET, FPS, FREEZE
		addInput(createInputCentered<IldaPort>(mm2px(Vec(col5_x[0], 91.000f)), module, Ilda::FRAME_SYNC_INPUT));
		addInput(createInputCentered<IldaPort>(mm2px(Vec(col5_x[1], 91.000f)), module, Ilda::FREQ_CV_INPUT));
		addInput(createInputCentered<IldaPort>(mm2px(Vec(col5_x[2], 91.000f)), module, Ilda::RESET_INPUT));
		addInput(createInputCentered<IldaPort>(mm2px(Vec(col5_x[3], 91.000f)), module, Ilda::FPS_CV_INPUT));
		addInput(createInputCentered<IldaPort>(mm2px(Vec(col5_x[4], 91.000f)), module, Ilda::FREEZE_INPUT));

		// Row 2 (Center Y = 104.000 mm): (Freq) SYNC IN, X, Y, Z, AUDIO L
		addInput(createInputCentered<IldaPort>(mm2px(Vec(col5_x[0], 104.000f)), module, Ilda::FREQ_SYNC_INPUT));
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[1], 104.000f)), module, Ilda::X_OUTPUT));
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[2], 104.000f)), module, Ilda::Y_OUTPUT));
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[3], 104.000f)), module, Ilda::Z_OUTPUT));
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[4], 104.000f)), module, Ilda::AUDIO_L_OUTPUT));

		// Row 3 (Center Y = 117.000 mm): R, G, B, SYNC OUT, AUDIO R
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[0], 117.000f)), module, Ilda::R_OUTPUT));
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[1], 117.000f)), module, Ilda::G_OUTPUT));
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[2], 117.000f)), module, Ilda::B_OUTPUT));
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[3], 117.000f)), module, Ilda::SYNC_OUTPUT));
		addOutput(createOutputCentered<IldaPort>(mm2px(Vec(col5_x[4], 117.000f)), module, Ilda::AUDIO_R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		auto* mod = dynamic_cast<Ilda*>(this->module);
		if (!mod) return;

		std::string infoStr;
		int totalFrames = 1;
		bool isWav = false;
		{
			std::lock_guard<std::mutex> lock(mod->uiMutex);
			infoStr = mod->uiPreviewShow.infoSummary;
			totalFrames = std::max(1, (int)mod->uiPreviewShow.frames.size());
			isWav = mod->loadedIsWav;
		}

		// ── File Section ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel(infoStr.empty() ? "ILDA / WAV Source" : infoStr));

		menu->addChild(createMenuItem("Load .ild / .wav File...", "", [=]() {
			osdialog_filters* filters = osdialog_filters_parse("Laser & Audio:ild,wav;ILDA Files:ild;WAV Files:wav");
#ifndef METAMODULE
			std::string dir;
			{
				std::lock_guard<std::mutex> lock(mod->uiMutex);
				if (!mod->loadedFilePath.empty()) {
					dir = system::getDirectory(mod->loadedFilePath);
				}
			}
			char* pathC = osdialog_file(OSDIALOG_OPEN, dir.empty() ? nullptr : dir.c_str(), nullptr, filters);
			if (pathC) {
				std::string chosen(pathC);
				std::free(pathC);
				mod->loadFromFile(chosen);
			}
			osdialog_filters_free(filters);
#else
			async_osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters, [mod, filters](char* pathC) {
				if (pathC) {
					std::string chosen(pathC);
					std::free(pathC);
					mod->loadFromFile(chosen);
				}
				osdialog_filters_free(filters);
			});
#endif
		}));

		menu->addChild(createMenuItem("Reset to Default Sequence", "", [=]() {
			mod->loadDefaultSequence();
		}));

		// ── Point Interpolation & Geometry ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Interpolation & Preview"));

		menu->addChild(createCheckMenuItem("Linear Interpolation (Smooth)", "",
			[=]() { return mod->interpMode == Ilda::INTERP_LINEAR; },
			[=]() { mod->interpMode = Ilda::INTERP_LINEAR; }
		));
		menu->addChild(createCheckMenuItem("Nearest Point (Stepped Corners)", "",
			[=]() { return mod->interpMode == Ilda::INTERP_NEAREST; },
			[=]() { mod->interpMode = Ilda::INTERP_NEAREST; }
		));
		menu->addChild(createCheckMenuItem("Show Blanked Moves", "",
			[=]() { return mod->showBlankedMoves.load(); },
			[=]() { mod->showBlankedMoves.store(!mod->showBlankedMoves.load()); }
		));
		menu->addChild(createCheckMenuItem("Flip X", "",
			[=]() { return mod->flipX.load(); },
			[=]() { mod->flipX.store(!mod->flipX.load()); }
		));
		menu->addChild(createCheckMenuItem("Flip Y", "",
			[=]() { return mod->flipY.load(); },
			[=]() { mod->flipY.store(!mod->flipY.load()); }
		));

		menu->addChild(createSubmenuItem("3D Revolve", "", [=](Menu* sub) {
			const char* labels[] = {"Off (0°)", "+25° Tilt", "+45° Tilt", "-25° Tilt", "Auto Spin"};
			for (int i = 0; i < 5; ++i) {
				Ilda::RevolvePreset rp = (Ilda::RevolvePreset)i;
				sub->addChild(createCheckMenuItem(labels[i], "",
					[=]() { return mod->revolvePreset == rp; },
					[=]() { mod->revolvePreset = rp; }
				));
			}
		}));

		// ── Animation & Sync ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Animation & Sync"));

		menu->addChild(createSubmenuItem("Frame Playback Mode", "", [=](Menu* sub) {
			sub->addChild(createCheckMenuItem("Auto Advance (Frame Rate Knob)", "",
				[=]() { return mod->frameMode == Ilda::FRAME_AUTO_FPS; },
				[=]() { mod->frameMode = Ilda::FRAME_AUTO_FPS; }
			));
			sub->addChild(createCheckMenuItem("Manual Frame Hold", "",
				[=]() { return mod->frameMode == Ilda::FRAME_MANUAL; },
				[=]() { mod->frameMode = Ilda::FRAME_MANUAL; }
			));
			sub->addChild(createCheckMenuItem("All Frames Sequential", "",
				[=]() { return mod->frameMode == Ilda::FRAME_ALL_SEQ; },
				[=]() { mod->frameMode = Ilda::FRAME_ALL_SEQ; }
			));
		}));

		if (!isWav && totalFrames > 1) {
			menu->addChild(createSubmenuItem("Select Manual Frame", "", [=](Menu* sub) {
				sub->addChild(createMenuItem("Previous Frame", "", [=]() {
					int cur = mod->manualFrame.load();
					mod->manualFrame.store((cur + totalFrames - 1) % totalFrames);
					mod->frameMode = Ilda::FRAME_MANUAL;
				}));
				sub->addChild(createMenuItem("Next Frame", "", [=]() {
					int cur = mod->manualFrame.load();
					mod->manualFrame.store((cur + 1) % totalFrames);
					mod->frameMode = Ilda::FRAME_MANUAL;
				}));
				sub->addChild(new MenuSeparator());
				int shown = std::min(totalFrames, 36);
				for (int f = 0; f < shown; ++f) {
					sub->addChild(createCheckMenuItem("Frame " + std::to_string(f + 1), "",
						[=]() { return mod->manualFrame.load() == f && mod->frameMode == Ilda::FRAME_MANUAL; },
						[=]() {
							mod->manualFrame.store(f);
							mod->frameMode = Ilda::FRAME_MANUAL;
						}
					));
				}
			}));
		}

		menu->addChild(createSubmenuItem("Frame Sync In Mode", "", [=](Menu* sub) {
			sub->addChild(createCheckMenuItem("Advance 1 Frame per Trigger (Default)", "",
				[=]() { return mod->syncInMode == Ilda::SYNC_STEP_FRAME; },
				[=]() { mod->syncInMode = Ilda::SYNC_STEP_FRAME; }
			));
			sub->addChild(createCheckMenuItem("Restart to Frame 1", "",
				[=]() { return mod->syncInMode == Ilda::SYNC_RESET_FILE; },
				[=]() { mod->syncInMode = Ilda::SYNC_RESET_FILE; }
			));
		}));

		// ── WAV Options ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("WAV File Options"));

		menu->addChild(createSubmenuItem("WAV Speed Mode", "", [=](Menu* sub) {
			sub->addChild(createCheckMenuItem("V/Oct Speed (1.0x @ Center)", "",
				[=]() { return mod->wavSpeedMode == Ilda::WAV_SPEED_VOCT; },
				[=]() { mod->wavSpeedMode = Ilda::WAV_SPEED_VOCT; }
			));
			sub->addChild(createCheckMenuItem("Single-Cycle Scan (Freq Hz)", "",
				[=]() { return mod->wavSpeedMode == Ilda::WAV_SINGLE_CYCLE; },
				[=]() { mod->wavSpeedMode = Ilda::WAV_SINGLE_CYCLE; }
			));
		}));

		menu->addChild(createSubmenuItem("WAV Ch 6 Mode", "", [=](Menu* sub) {
			sub->addChild(createCheckMenuItem("Z Depth (Default)", "",
				[=]() { return mod->wavCh6Mode == Ilda::WAV_CH6_Z_DEPTH; },
				[=]() { mod->setWavCh6Mode(Ilda::WAV_CH6_Z_DEPTH); }
			));
			sub->addChild(createCheckMenuItem("Blanking Gate (< 0.05)", "",
				[=]() { return mod->wavCh6Mode == Ilda::WAV_CH6_BLANK_GATE; },
				[=]() { mod->setWavCh6Mode(Ilda::WAV_CH6_BLANK_GATE); }
			));
		}));

		menu->addChild(createSubmenuItem("WAV Color Encoding", "", [=](Menu* sub) {
			sub->addChild(createCheckMenuItem("0 to +1 (Standard Unipolar)", "",
				[=]() { return mod->wavColorEncoding == Ilda::WAV_COLOR_UNIPOLAR; },
				[=]() { mod->setWavColorEncoding(Ilda::WAV_COLOR_UNIPOLAR); }
			));
			sub->addChild(createCheckMenuItem("-1 to +1 (Full Bipolar)", "",
				[=]() { return mod->wavColorEncoding == Ilda::WAV_COLOR_BIPOLAR; },
				[=]() { mod->setWavColorEncoding(Ilda::WAV_COLOR_BIPOLAR); }
			));
		}));

		// ── Oscillator Range & Output Voltage Ranges ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator & Output Ranges"));

		menu->addChild(createSubmenuItem("Oscillator Range", "", [=](Menu* sub) {
			const char* rangeLabels[] = {
				"Very Slow (600s - 0.1s)",
				"LFO (0.01 - 200 Hz)",
				"VCO (150 Hz - 2 kHz)"
			};
			for (int i = 0; i < 3; ++i) {
				Ilda::RangeMode mode = (Ilda::RangeMode)i;
				sub->addChild(createCheckMenuItem(rangeLabels[i], "",
					[=]() { return mod->rangeMode == mode; },
					[=]() { mod->rangeMode = mode; }
				));
			}
		}));

		menu->addChild(createSubmenuItem("Voltage Range", "", [=](Menu* sub) {
			const char* voltageLabels[] = {"0 to 1V", "±1V", "0 to 5V", "±5V", "0 to 10V", "±10V"};
			for (int i = 0; i < Ilda::NUM_VOLTAGE_RANGES; ++i) {
				Ilda::VoltageRange vr = (Ilda::VoltageRange)i;
				sub->addChild(createCheckMenuItem(voltageLabels[i], "",
					[=]() { return mod->voltageRange == vr; },
					[=]() { mod->voltageRange = vr; }
				));
			}
		}));

		menu->addChild(createSubmenuItem("RGB Voltage Range", "", [=](Menu* sub) {
			const char* rgbLabels[] = {"0 to 1V", "0 to 5V (Default)", "0 to 10V"};
			for (int i = 0; i < 3; ++i) {
				Ilda::RgbVoltageScale rs = (Ilda::RgbVoltageScale)i;
				sub->addChild(createCheckMenuItem(rgbLabels[i], "",
					[=]() { return mod->rgbVoltageScale == rs; },
					[=]() { mod->rgbVoltageScale = rs; }
				));
			}
		}));

		// ── Skin Theme ──
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));
		const char* skinLabels[] = {"Medium (Default)", "Light", "Dark"};
		for (int i = 0; i < 3; ++i) {
			Ilda::SkinTheme theme = (Ilda::SkinTheme)i;
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

Model* modelIlda = createModel<Ilda, IldaWidget>("Ilda");
