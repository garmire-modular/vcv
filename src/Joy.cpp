#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────
//  JOY — 12 HP 2D Ridgeline Trajectory & Waterfall Audio Vector Synthesizer
//  Transform class module converting incoming audio into stacked
//  waterfall vector trajectories with real-time crest occlusion,
//  galvanometer deflection outputs, and laser beam intensity (Z) modulation.
//  Inspired by the iconic pulsar visualization of Unknown Pleasures.
// ─────────────────────────────────────────────────────────────────────

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Soft-knee ceiling to ensure outputs never flat-top hard clip on scope
inline float softKneeClamp(float v, float threshold, float maxLimit) {
	float sign = (v < 0.0f) ? -1.0f : 1.0f;
	float absV = std::abs(v);
	if (absV <= threshold) return v;
	float span = maxLimit - threshold;
	float compressed = threshold + span * std::tanh((absV - threshold) / span);
	return sign * compressed;
}
} // namespace

struct Joy : Module {
	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT,
		THEME_DARK
	};
	SkinTheme skinTheme = THEME_MEDIUM;

	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};
	RangeMode rangeMode = RANGE_LFO;
	dsp::BooleanTrigger rangeTrigger;

	// Voltage range standards strictly per docs/classes/generate.md
	enum VoltageRange {
		RANGE_UNIPOLAR_1V = 0,
		RANGE_BIPOLAR_1V,
		RANGE_UNIPOLAR_5V,
		RANGE_BIPOLAR_5V,
		RANGE_UNIPOLAR_10V,
		RANGE_BIPOLAR_10V,
		NUM_VOLTAGE_RANGES
	};
	VoltageRange voltageRange = RANGE_BIPOLAR_5V;

	enum ZScaleMode {
		Z_SCALE_MATCH_RANGE = 0, // Defaults to vMax of active voltage range (5V for ±5V, 10V for ±10V, 1V for ±1V)
		Z_SCALE_5V = 1,
		Z_SCALE_10V = 2
	};
	ZScaleMode zScaleMode = Z_SCALE_MATCH_RANGE;

	enum FadeMode {
		FADE_TOP = 0,    // Fades towards top (distant horizon dims)
		FADE_BOTTOM = 1, // Fades towards bottom (foreground dims)
		FADE_EDGES = 2   // Fades towards left/right edges (center is highest intensity)
	};
	FadeMode fadeMode = FADE_TOP;

	enum ScanMode {
		SCAN_RASTER = 0,
		SCAN_SERPENTINE = 1
	};
	ScanMode scanMode = SCAN_RASTER;

	enum ParamId {
		// Row 1 Knobs: FREQ, RANGE (Button), FINE, HEIGHT
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,
		HEIGHT_PARAM,

		// Row 2 Knobs: SMOOTH, REFRESH, LINES, DECAY
		SMOOTH_PARAM,
		REFRESH_PARAM,
		LINES_PARAM,
		DECAY_PARAM,

		// Row 3 Knobs: WINDOW, TILT, OCCLUSION, ZFADE
		WINDOW_PARAM,
		TILT_PARAM,
		OCCLUSION_PARAM,
		ZFADE_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 1
		FREQ_TRIM_PARAM,
		AUDIO_TRIM_PARAM,
		FINE_TRIM_PARAM,
		HEIGHT_TRIM_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 2
		SMOOTH_TRIM_PARAM,
		REFRESH_TRIM_PARAM,
		LINES_TRIM_PARAM,
		DECAY_TRIM_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 3
		WINDOW_TRIM_PARAM,
		TILT_TRIM_PARAM,
		OCCLUSION_TRIM_PARAM,
		ZFADE_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Jack Row 1 (Inputs)
		FREQ_CV_INPUT,
		AUDIO_INPUT,
		FINE_CV_INPUT,
		HEIGHT_CV_INPUT,

		// Jack Row 2 (Inputs)
		SMOOTH_CV_INPUT,
		REFRESH_CV_INPUT,
		LINES_CV_INPUT,
		DECAY_CV_INPUT,

		// Jack Row 3 (Inputs)
		WINDOW_CV_INPUT,
		TILT_CV_INPUT,
		OCCLUSION_CV_INPUT,
		ZFADE_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Jack Row 4 (Outputs)
		X_OUTPUT,
		Y_OUTPUT,
		Z_OUTPUT,
		SYNC_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_TEAL,
		RANGE_LIGHT_MAGENTA,
		LIGHTS_LEN
	};

	// ── Custom Frequency Param Quantity matching Garmire Timebase Standard ──
	struct FreqParamQuantity : ParamQuantity {
		Joy* getJoy() {
			return dynamic_cast<Joy*>(module);
		}

		float getDisplayValue() override {
			Joy* joy = getJoy();
			if (!joy) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (joy->paramQuantities.size() > Joy::FINE_PARAM) ? joy->params[Joy::FINE_PARAM].getValue() : 0.f;

			if (joy->rangeMode == Joy::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (joy->rangeMode == Joy::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Joy* joy = getJoy();
			if (!joy) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (joy->rangeMode == Joy::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Joy* joy = getJoy();
			if (!joy) return;

			float fine = (joy->paramQuantities.size() > Joy::FINE_PARAM) ? joy->params[Joy::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (joy->rangeMode == Joy::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (joy->rangeMode == Joy::RANGE_LFO) {
				float targetHz = !isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 0.01f);
				float fCoarse = targetHz - fine * 20.0f;
				if (fCoarse <= 0.01f) {
					newCoarse = 0.0f;
				} else {
					newCoarse = std::sqrt((fCoarse - 0.01f) / 199.99f);
				}
			} else { // RANGE_VCO
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
			Joy* joy = getJoy();
			if (!joy) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (joy->rangeMode == Joy::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Joy* joy = getJoy();
			if (!joy) {
				ParamQuantity::setDisplayValueString(s);
				return;
			}

			std::string str = s;
			std::string lowerStr = s;
			for (char& c : lowerStr) c = (char)std::tolower((unsigned char)c);

			enum UnitType { UNIT_NONE, UNIT_HZ, UNIT_KHZ, UNIT_MHZ, UNIT_SEC, UNIT_MS };
			UnitType unitType = UNIT_NONE;

			if (lowerStr.find("khz") != std::string::npos) {
				unitType = UNIT_KHZ;
			} else if (lowerStr.find("mhz") != std::string::npos) {
				unitType = UNIT_MHZ;
			} else if (lowerStr.find("hz") != std::string::npos) {
				unitType = UNIT_HZ;
			} else if (lowerStr.find("ms") != std::string::npos) {
				unitType = UNIT_MS;
			} else if (lowerStr.find("s") != std::string::npos || lowerStr.find("sec") != std::string::npos) {
				unitType = UNIT_SEC;
			}

			while (!str.empty() && (std::isalpha((unsigned char)str.back()) || std::isspace((unsigned char)str.back()))) {
				str.pop_back();
			}

			char* endPtr = nullptr;
			float rawVal = std::strtof(str.c_str(), &endPtr);
			if (endPtr == str.c_str()) return;

			bool isPeriod = false;
			switch (unitType) {
				case UNIT_KHZ: rawVal *= 1000.0f; isPeriod = false; break;
				case UNIT_MHZ: rawVal *= 1000000.0f; isPeriod = false; break;
				case UNIT_HZ:  isPeriod = false; break;
				case UNIT_MS:  rawVal /= 1000.0f; isPeriod = true; break;
				case UNIT_SEC: isPeriod = true; break;
				case UNIT_NONE:
				default:
					isPeriod = (joy->rangeMode == Joy::RANGE_VERY_SLOW);
					break;
			}

			setFrequencyValue(rawVal, isPeriod);
		}
	};

	struct IntegerParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int val = (int)std::round(getValue());
			return std::to_string(val);
		}
		void setDisplayValueString(std::string s) override {
			char* endPtr = nullptr;
			float val = std::strtof(s.c_str(), &endPtr);
			if (endPtr != s.c_str()) {
				setValue(clampf(std::round(val), getMinValue(), getMaxValue()));
			}
		}
	};

	// ── Pre-allocated Ring Buffers for History & Ridge Lines ──
	static constexpr int MAX_LINES = 64;
	static constexpr int SAMPLES_PER_LINE = 512;

	float history[MAX_LINES][SAMPLES_PER_LINE];
	float currentLine[SAMPLES_PER_LINE];
	float occlusionSilhouette[SAMPLES_PER_LINE];

	int historyHead = 0;
	int writeIndex = 0;

	float audioFilterState = 0.0f;
	float refreshPhase = 0.0f;
	float scanPhase = 0.0f;

	dsp::PulseGenerator syncPulse;

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

	float getZMaxVoltage() const {
		if (zScaleMode == Z_SCALE_5V) return 5.0f;
		if (zScaleMode == Z_SCALE_10V) return 10.0f;
		// Default Z_SCALE_MATCH_RANGE: strictly defaults to vMax of active voltage range
		float vMin = -5.0f, vMax = 5.0f;
		getRangeVoltages(vMin, vMax);
		return vMax;
	}

	Joy() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1 Knobs
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency");
		configParam(RANGE_PARAM, 0.f, 1.f, 0.f, "Range");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine tune");
		configParam(HEIGHT_PARAM, 0.f, 2.f, 1.0f, "Waveform height");

		// Row 2 Knobs: Smooth default strictly 0% per directive
		configParam(SMOOTH_PARAM, 0.f, 1.f, 0.0f, "Smoothness", "%", 0.f, 100.f);
		configParam(REFRESH_PARAM, 0.f, 1.f, 0.5f, "Refresh rate");
		configParam<IntegerParamQuantity>(LINES_PARAM, 1.f, 64.f, 32.f, "History lines");
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay spacing");

		// Row 3 Knobs
		configParam(WINDOW_PARAM, 0.f, 1.f, 0.35f, "Active window width", "%", 0.f, 100.f);
		configParam(TILT_PARAM, 0.f, 1.f, 0.60f, "Perspective tilt", "%", 0.f, 100.f);
		configParam(OCCLUSION_PARAM, 0.f, 1.f, 1.0f, "Crest occlusion", "%", 0.f, 100.f);
		configParam(ZFADE_PARAM, 0.f, 1.f, 0.50f, "Z-Fade depth", "%", 0.f, 100.f);

		// Zone 3 Attenuverter Trimpots (Strictly '<Parameter> CV depth' and sentence case per AGENTS.md 7.5.4)
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth");
		configParam(AUDIO_TRIM_PARAM, 0.f, 2.f, 1.f, "Audio level");
		configParam(FINE_TRIM_PARAM, -1.f, 1.f, 0.f, "Fine tune CV depth");
		configParam(HEIGHT_TRIM_PARAM, -1.f, 1.f, 0.f, "Height CV depth");

		configParam(SMOOTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Smooth CV depth");
		configParam(REFRESH_TRIM_PARAM, -1.f, 1.f, 0.f, "Refresh CV depth");
		configParam(LINES_TRIM_PARAM, -1.f, 1.f, 0.f, "Lines CV depth");
		configParam(DECAY_TRIM_PARAM, -1.f, 1.f, 0.f, "Decay CV depth");

		configParam(WINDOW_TRIM_PARAM, -1.f, 1.f, 0.f, "Window CV depth");
		configParam(TILT_TRIM_PARAM, -1.f, 1.f, 0.f, "Tilt CV depth");
		configParam(OCCLUSION_TRIM_PARAM, -1.f, 1.f, 0.f, "Occlusion CV depth");
		configParam(ZFADE_TRIM_PARAM, -1.f, 1.f, 0.f, "Z-Fade CV depth");

		// Zone 4 Jacks: Row 1
		configInput(FREQ_CV_INPUT, "Frequency");
		configInput(AUDIO_INPUT, "Audio");
		configInput(FINE_CV_INPUT, "Fine tune");
		configInput(HEIGHT_CV_INPUT, "Height");

		// Zone 4 Jacks: Row 2
		configInput(SMOOTH_CV_INPUT, "Smooth");
		configInput(REFRESH_CV_INPUT, "Refresh");
		configInput(LINES_CV_INPUT, "Lines");
		configInput(DECAY_CV_INPUT, "Decay");

		// Zone 4 Jacks: Row 3
		configInput(WINDOW_CV_INPUT, "Window");
		configInput(TILT_CV_INPUT, "Tilt");
		configInput(OCCLUSION_CV_INPUT, "Occlusion");
		configInput(ZFADE_CV_INPUT, "Z-Fade");

		// Zone 4 Jacks: Row 4 Outputs
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(Z_OUTPUT, "Z");
		configOutput(SYNC_OUTPUT, "Sync");

		// Initialize pre-allocated buffers
		std::memset(history, 0, sizeof(history));
		std::memset(currentLine, 0, sizeof(currentLine));
		for (int i = 0; i < SAMPLES_PER_LINE; ++i) {
			occlusionSilhouette[i] = -10.0f;
		}
	}

	void onReset() override {
		std::memset(history, 0, sizeof(history));
		std::memset(currentLine, 0, sizeof(currentLine));
		for (int i = 0; i < SAMPLES_PER_LINE; ++i) {
			occlusionSilhouette[i] = -10.0f;
		}
		historyHead = 0;
		writeIndex = 0;
		audioFilterState = 0.0f;
		refreshPhase = 0.0f;
		scanPhase = 0.0f;
		fadeMode = FADE_TOP;
	}

	void process(const ProcessArgs& args) override {
		// 1. Range Mode Pushbutton Toggle
		if (rangeTrigger.process(params[RANGE_PARAM].getValue())) {
			rangeMode = (RangeMode)(((int)rangeMode + 1) % 3);
		}

		// Update Range Lights (Concentric illuminated ring under button)
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_TEAL].setBrightness(rangeMode == RANGE_LFO ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_MAGENTA].setBrightness(rangeMode == RANGE_VCO ? 1.0f : 0.0f);

		// 2. Incoming Audio Sampling & Lowpass Smoothing
		float audioIn = inputs[AUDIO_INPUT].getVoltage();
		float audioGain = params[AUDIO_TRIM_PARAM].getValue();
		float rawAudio = audioIn * audioGain * 0.2f; // Scaled to ~ ±1.0 nominal

		float smoothParam = clampf(params[SMOOTH_PARAM].getValue() + inputs[SMOOTH_CV_INPUT].getVoltage() * params[SMOOTH_TRIM_PARAM].getValue() * 0.1f, 0.0f, 1.0f);
		if (smoothParam <= 0.0001f) {
			// Zero smoothing default: raw audio waveform
			audioFilterState = rawAudio;
		} else {
			// Exponential cutoff smoothing filter
			float cutoffHz = 20.0f + 18000.0f * std::pow(1.0f - smoothParam, 2.5f);
			float alpha = clampf(2.0f * (float)M_PI * cutoffHz * args.sampleTime, 0.0001f, 1.0f);
			audioFilterState += alpha * (rawAudio - audioFilterState);
		}

		currentLine[writeIndex] = audioFilterState;
		writeIndex = (writeIndex + 1) % SAMPLES_PER_LINE;

		// 3. Line Advance & History Refresh Rate
		// Refresh knob ranges from 0.02 Hz (stroboscopic stepping) up to 100 Hz (flowing waterfall)
		float refreshParam = clampf(params[REFRESH_PARAM].getValue() + inputs[REFRESH_CV_INPUT].getVoltage() * params[REFRESH_TRIM_PARAM].getValue() * 0.1f, 0.0f, 1.0f);
		float refreshHz = 0.02f * std::pow(5000.0f, refreshParam);

		refreshPhase += refreshHz * args.sampleTime;
		if (refreshPhase >= 1.0f) {
			refreshPhase -= 1.0f;
			// Push current live line into circular history ring buffer
			historyHead = (historyHead + 1) % MAX_LINES;
			for (int s = 0; s < SAMPLES_PER_LINE; ++s) {
				history[historyHead][s] = currentLine[s];
			}
		}

		// 4. Trajectory Frequency & Scanning Speed
		float scanHz = 1.0f;
		float coarse = params[FREQ_PARAM].getValue();
		float fine = params[FINE_PARAM].getValue() + inputs[FINE_CV_INPUT].getVoltage() * params[FINE_TRIM_PARAM].getValue() * 0.1f;
		float freqCV = inputs[FREQ_CV_INPUT].getVoltage() * params[FREQ_TRIM_PARAM].getValue();

		if (rangeMode == RANGE_VERY_SLOW) {
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, clampf(coarse + freqCV * 0.1f, 0.f, 1.f));
			float period = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			scanHz = 1.0f / period;
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + 199.99f * std::pow(clampf(coarse + freqCV * 0.1f, 0.f, 1.f), 2.0f);
			scanHz = clampf(fCoarse + fine * 20.0f, 0.005f, 300.0f);
		} else { // RANGE_VCO
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, clampf(coarse + freqCV * 0.1f, 0.f, 1.f));
			scanHz = clampf(fCoarse * (1.0f + fine * 0.10f), 50.0f, 3000.0f);
		}

		// 5. Active Lines Parameter
		float linesParam = clampf(params[LINES_PARAM].getValue() + inputs[LINES_CV_INPUT].getVoltage() * params[LINES_TRIM_PARAM].getValue() * 6.4f, 1.0f, (float)MAX_LINES);
		int activeLines = (int)std::round(linesParam);
		if (activeLines < 1) activeLines = 1;
		if (activeLines > MAX_LINES) activeLines = MAX_LINES;

		// Total frame phase accumulation: sweeps through all active lines
		scanPhase += scanHz * args.sampleTime;
		if (scanPhase >= 1.0f) {
			scanPhase -= std::floor(scanPhase);
			// Emit 1 ms sync trigger pulse at frame completion
			syncPulse.trigger(0.001f);
			// Reset crest occlusion silhouette for next frame
			for (int s = 0; s < SAMPLES_PER_LINE; ++s) {
				occlusionSilhouette[s] = -10.0f;
			}
		}

		// Map scanPhase across activeLines
		float totalPos = scanPhase * (float)activeLines;
		int lineIdx = (int)totalPos;
		if (lineIdx >= activeLines) lineIdx = activeLines - 1;
		float lineFraction = totalPos - (float)lineIdx; // intra-line position 0..1

		// Horizontal sample position u in [-1.0, 1.0]
		float u = lineFraction;
		bool isRetrace = false;

		if (scanMode == SCAN_SERPENTINE) {
			// Bidirectional serpentine scanning
			if (lineIdx % 2 == 1) {
				u = 1.0f - u;
			}
		} else {
			// Raster scanning with flyback blanking at line boundaries
			if (u > 0.96f) {
				isRetrace = true; // Flyback retrace blanking
			}
		}

		float xNorm = u * 2.0f - 1.0f; // -1.0 to +1.0

		// Sample from historical line buffer
		// lineIdx 0 is foreground (closest/newest), activeLines-1 is background/horizon
		int ringSlot = (historyHead - lineIdx + MAX_LINES * 2) % MAX_LINES;
		int samplePos = (int)clampf(u * (SAMPLES_PER_LINE - 1), 0.0f, (float)(SAMPLES_PER_LINE - 1));
		float waveVal = history[ringSlot][samplePos];

		// 6. Active Window Aperture (Central Wave Clustering)
		// Window knob allows shrinking the active audio wave down to a narrow central band (12% to 100%)
		// with dead-flat zero baseline wings on either side
		float winParam = clampf(params[WINDOW_PARAM].getValue() + inputs[WINDOW_CV_INPUT].getVoltage() * params[WINDOW_TRIM_PARAM].getValue() * 0.1f, 0.0f, 1.0f);
		float winHalfWidth = 0.06f + 0.44f * winParam; // Ranges from 0.06 (12% width) to 0.50 (100% width)
		float absX = std::abs(xNorm);
		float windowTaper = 0.0f;
		if (absX < winHalfWidth) {
			// Smooth Hann cosine taper to zero baseline
			windowTaper = 0.5f * (1.0f + std::cos((float)M_PI * absX / winHalfWidth));
		}
		float disp = waveVal * windowTaper;

		// 7. Star Wars Crawl 3D Perspective Tilt
		// zDepth = 0 at foreground (bottom line), 1 at horizon (top line)
		float zDepth = (activeLines > 1) ? ((float)lineIdx / (float)(activeLines - 1)) : 0.0f;
		float tiltParam = clampf(params[TILT_PARAM].getValue() + inputs[TILT_CV_INPUT].getVoltage() * params[TILT_TRIM_PARAM].getValue() * 0.1f, 0.0f, 1.0f);

		// True 3D perspective scaling: foreground expands wide and tall, horizon tapers to vanishing point
		float frontScale = 1.0f + 2.0f * tiltParam;
		float perspFactor = frontScale / (1.0f + 4.0f * tiltParam * zDepth);

		// Star Wars crawl vertical baseline compression: wide line spacing at bottom, compressed at horizon
		float decayParam = clampf(params[DECAY_PARAM].getValue() + inputs[DECAY_CV_INPUT].getVoltage() * params[DECAY_TRIM_PARAM].getValue() * 0.1f, 0.0f, 1.0f);
		float gamma = 1.0f + 2.5f * tiltParam;
		float crawlY = 1.0f - std::pow(clampf(1.0f - zDepth, 0.0f, 1.0f), gamma);
		float yBaseline = -0.70f + 1.40f * crawlY * decayParam;

		// Wave height scaling
		float heightParam = clampf(params[HEIGHT_PARAM].getValue() + inputs[HEIGHT_CV_INPUT].getVoltage() * params[HEIGHT_TRIM_PARAM].getValue() * 0.2f, 0.0f, 2.0f);
		float yPeak = disp * heightParam * 0.65f * perspFactor;

		// Combined (X, Y) normalized coordinates [-1.0, 1.0]
		float xOutNorm = xNorm * perspFactor;
		float yOutNorm = yBaseline + yPeak;

		// 8. Crest Occlusion (Foreground Mountain Silhouette Blanking)
		float occlusionParam = clampf(params[OCCLUSION_PARAM].getValue() + inputs[OCCLUSION_CV_INPUT].getVoltage() * params[OCCLUSION_TRIM_PARAM].getValue() * 0.1f, 0.0f, 1.0f);
		float zOcclude = 1.0f;
		if (occlusionParam > 0.001f) {
			if (yOutNorm <= occlusionSilhouette[samplePos]) {
				// Point is occluded behind a foreground crest
				zOcclude = 1.0f - occlusionParam; // At 100% occlusion, blanks beam completely (0.0)
			} else {
				// Update silhouette with new peak
				occlusionSilhouette[samplePos] = yOutNorm;
			}
		}

		// 9. Z-Fade Depth and Direction
		// User selectable: Fade to Top, Fade to Bottom, or Fade to Edges
		// At 100% depth, fully extends to complete extinction (0.0)
		float zFadeParam = clampf(params[ZFADE_PARAM].getValue() + inputs[ZFADE_CV_INPUT].getVoltage() * params[ZFADE_TRIM_PARAM].getValue() * 0.1f, 0.0f, 1.0f);
		float fadeFactor = 1.0f;

		switch (fadeMode) {
			case FADE_BOTTOM:
				// Fades towards foreground (bottom of screen dims)
				fadeFactor = 1.0f - zFadeParam * std::pow(clampf(1.0f - zDepth, 0.0f, 1.0f), 0.75f);
				break;
			case FADE_EDGES:
				// Fades towards left and right edges (center bright)
				fadeFactor = 1.0f - zFadeParam * std::pow(clampf(absX, 0.0f, 1.0f), 1.0f);
				break;
			case FADE_TOP:
			default:
				// Fades towards distant horizon (top of screen dims)
				fadeFactor = 1.0f - zFadeParam * std::pow(clampf(zDepth, 0.0f, 1.0f), 0.75f);
				break;
		}

		float zNormalized = clampf(fadeFactor * zOcclude, 0.0f, 1.0f);
		if (isRetrace) {
			zNormalized = 0.0f; // Blank laser during raster flyback retrace
		}

		// 10. Output Voltage Bounds & Soft-Knee Scaling
		float vMin = -5.0f, vMax = 5.0f;
		getRangeVoltages(vMin, vMax);
		float vMid = 0.5f * (vMin + vMax);
		float vSpan = 0.5f * (vMax - vMin);

		float voltX = vMid + xOutNorm * vSpan;
		float voltY = vMid + yOutNorm * vSpan;

		// Soft knee ceiling prevents flat-top clipping on scope/lasers
		voltX = softKneeClamp(voltX, vMax * 0.95f, vMax);
		voltY = softKneeClamp(voltY, vMax * 0.95f, vMax);

		// Z Output Voltage
		float zVolt = zNormalized * getZMaxVoltage();

		// Write outputs
		outputs[X_OUTPUT].setVoltage(voltX);
		outputs[Y_OUTPUT].setVoltage(voltY);
		outputs[Z_OUTPUT].setVoltage(zVolt);
		outputs[SYNC_OUTPUT].setVoltage(syncPulse.process(args.sampleTime) ? 10.0f : 0.0f);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		json_object_set_new(rootJ, "zScaleMode", json_integer((int)zScaleMode));
		json_object_set_new(rootJ, "fadeMode", json_integer((int)fadeMode));
		json_object_set_new(rootJ, "scanMode", json_integer((int)scanMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);

		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) rangeMode = (RangeMode)json_integer_value(rmJ);

		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) voltageRange = (VoltageRange)json_integer_value(vrJ);

		json_t* zsJ = json_object_get(rootJ, "zScaleMode");
		if (zsJ) zScaleMode = (ZScaleMode)json_integer_value(zsJ);

		json_t* fmJ = json_object_get(rootJ, "fadeMode");
		if (fmJ) fadeMode = (FadeMode)json_integer_value(fmJ);

		json_t* smJ = json_object_get(rootJ, "scanMode");
		if (smJ) scanMode = (ScanMode)json_integer_value(smJ);
	}
};

// ── Custom Hardware Widgets ──
struct JoyRangeLightRing : ModuleLightWidget {
	JoyRangeLightRing() {
		// Box size (11.0 mm x 11.0 mm) encompasses R=4.82 mm ring plus optical halo
		box.size = mm2px(Vec(11.0f, 11.0f));

		// Base colors for 3-Color Range Engine
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff)); // State 0: Warm Gold (Very Slow)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff)); // State 1: Teal (LFO)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff)); // State 2: Magenta (VCO)
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

		// 50% Visibility Rule: Nominal 0.35f alpha scaled down to 0.175f
		haloColor.a *= 0.175f;

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, haloColor);
		nvgStrokeWidth(args.vg, mm2px(1.40f) + haloW);
		nvgStroke(args.vg);
	}
};

struct JoyCustomButton : app::SvgSwitch {
	JoyCustomButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

struct JoyWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Joy::SkinTheme currentTheme = Joy::THEME_MEDIUM;

	void setTheme(Joy::SkinTheme theme) {
		currentTheme = theme;
		if (panel) {
			std::string path;
			switch (theme) {
				case Joy::THEME_LIGHT:
					path = asset::plugin(pluginInstance, "res/Joy_Light.svg");
					break;
				case Joy::THEME_DARK:
					path = asset::plugin(pluginInstance, "res/Joy_Dark.svg");
					break;
				case Joy::THEME_MEDIUM:
				default:
					path = asset::plugin(pluginInstance, "res/Joy.svg");
					break;
			}
			panel->setBackground(Svg::load(path));
		}
	}

	void step() override {
		ModuleWidget::step();
		auto* mod = dynamic_cast<Joy*>(module);
		if (mod && mod->skinTheme != currentTheme) {
			setTheme(mod->skinTheme);
		}
	}

	JoyWidget(Joy* module) {
		setModule(module);
		panel = new SvgPanel();
		Joy::SkinTheme initTheme = module ? module->skinTheme : Joy::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Joy::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Joy_Light.svg");
				break;
			case Joy::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Joy_Dark.svg");
				break;
			case Joy::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Joy.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// 4 Standard Columns matching MAUDE (7.62, 22.86, 38.10, 53.34 mm)
		const float col_x[4] = {7.62f, 22.86f, 38.10f, 53.34f};

		// ── Zone 1 & 2: Primary Parameter Knobs ──
		// Row 1: FREQ, RANGE (Button + Ring), FINE, HEIGHT (Center Y = 18.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 18.50f)), module, Joy::FREQ_PARAM));
		// Concentric illuminated ring added first (in background under button cap)
		addChild(createLightCentered<JoyRangeLightRing>(mm2px(Vec(col_x[1], 18.50f)), module, Joy::RANGE_LIGHT_YELLOW));
		// Button added second (captures mouse events directly)
		addParam(createParamCentered<JoyCustomButton>(mm2px(Vec(col_x[1], 18.50f)), module, Joy::RANGE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 18.50f)), module, Joy::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 18.50f)), module, Joy::HEIGHT_PARAM));

		// Row 2: SMOOTH, REFRESH, LINES, DECAY (Center Y = 32.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 32.00f)), module, Joy::SMOOTH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 32.00f)), module, Joy::REFRESH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 32.00f)), module, Joy::LINES_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 32.00f)), module, Joy::DECAY_PARAM));

		// Row 3: WINDOW, TILT, OCCLUSION, ZFADE (Center Y = 45.50 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[0], 45.50f)), module, Joy::WINDOW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[1], 45.50f)), module, Joy::TILT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[2], 45.50f)), module, Joy::OCCLUSION_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(col_x[3], 45.50f)), module, Joy::ZFADE_PARAM));

		// ── Zone 3: CV Attenuverter Trimpots (3 Rows x 4 Trimpots) ──
		// Trim Row 1: FREQ, AUDIO, FINE, HEIGHT (Center Y = 56.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 56.50f)), module, Joy::FREQ_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 56.50f)), module, Joy::AUDIO_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 56.50f)), module, Joy::FINE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 56.50f)), module, Joy::HEIGHT_TRIM_PARAM));

		// Trim Row 2: SMOOTH, REFRESH, LINES, DECAY (Center Y = 64.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 64.50f)), module, Joy::SMOOTH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 64.50f)), module, Joy::REFRESH_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 64.50f)), module, Joy::LINES_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 64.50f)), module, Joy::DECAY_TRIM_PARAM));

		// Trim Row 3: WINDOW, TILT, OCCLUSION, ZFADE (Center Y = 72.50 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[0], 72.50f)), module, Joy::WINDOW_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[1], 72.50f)), module, Joy::TILT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[2], 72.50f)), module, Joy::OCCLUSION_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(col_x[3], 72.50f)), module, Joy::ZFADE_TRIM_PARAM));

		// ── Zone 4: I/O Jacks (4 Rows x 4 Jacks) ──
		// Jack Row 1 (Inputs): FREQ, AUDIO, FINE, HEIGHT (Center Y = 85.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 85.00f)), module, Joy::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 85.00f)), module, Joy::AUDIO_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 85.00f)), module, Joy::FINE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 85.00f)), module, Joy::HEIGHT_CV_INPUT));

		// Jack Row 2 (Inputs): SMOOTH, REFRESH, LINES, DECAY (Center Y = 96.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 96.00f)), module, Joy::SMOOTH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 96.00f)), module, Joy::REFRESH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 96.00f)), module, Joy::LINES_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 96.00f)), module, Joy::DECAY_CV_INPUT));

		// Jack Row 3 (Inputs): WINDOW, TILT, OCCLUSION, ZFADE (Center Y = 107.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 107.00f)), module, Joy::WINDOW_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 107.00f)), module, Joy::TILT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 107.00f)), module, Joy::OCCLUSION_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 107.00f)), module, Joy::ZFADE_CV_INPUT));

		// Jack Row 4 (Fixed Bottom Signal/Sync Row: X OUT, Y OUT, Z OUT, SYNC OUT, Center Y = 118.00 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[0], 118.00f)), module, Joy::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[1], 118.00f)), module, Joy::Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[2], 118.00f)), module, Joy::Z_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col_x[3], 118.00f)), module, Joy::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Joy* module = dynamic_cast<Joy*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Oscillator Range"));
		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};
		for (int i = 0; i < 3; i++) {
			Joy::RangeMode mode = (Joy::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return module->rangeMode == mode; },
				[=]() { module->rangeMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Voltage Range"));
		const char* vRangeLabels[] = {
			"0 to 1V",
			"±1V",
			"0 to 5V",
			"±5V",
			"0 to 10V",
			"±10V"
		};
		for (int i = 0; i < Joy::NUM_VOLTAGE_RANGES; i++) {
			Joy::VoltageRange mode = (Joy::VoltageRange)i;
			menu->addChild(createCheckMenuItem(vRangeLabels[i], "",
				[=]() { return module->voltageRange == mode; },
				[=]() { module->voltageRange = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Z Voltage Scale"));
		const char* zLabels[] = {
			"Match Output Range (Default)",
			"Unipolar 5V",
			"Unipolar 10V"
		};
		for (int i = 0; i < 3; i++) {
			Joy::ZScaleMode mode = (Joy::ZScaleMode)i;
			menu->addChild(createCheckMenuItem(zLabels[i], "",
				[=]() { return module->zScaleMode == mode; },
				[=]() { module->zScaleMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Z-Fade Direction"));
		const char* fadeLabels[] = {
			"Fade towards Top (Horizon)",
			"Fade towards Bottom (Foreground)",
			"Fade towards Edges (Center Bright)"
		};
		for (int i = 0; i < 3; i++) {
			Joy::FadeMode mode = (Joy::FadeMode)i;
			menu->addChild(createCheckMenuItem(fadeLabels[i], "",
				[=]() { return module->fadeMode == mode; },
				[=]() { module->fadeMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Scanning Trajectory"));
		const char* scanLabels[] = {
			"Raster (Unidirectional)",
			"Serpentine (Bidirectional)"
		};
		for (int i = 0; i < 2; i++) {
			Joy::ScanMode mode = (Joy::ScanMode)i;
			menu->addChild(createCheckMenuItem(scanLabels[i], "",
				[=]() { return module->scanMode == mode; },
				[=]() { module->scanMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Skin"));
		static const char* skinLabels[] = {
			"Medium (Default)",
			"Light",
			"Dark"
		};
		for (int i = 0; i < 3; ++i) {
			Joy::SkinTheme theme = (Joy::SkinTheme)i;
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

Model* modelJoy = createModel<Joy, JoyWidget>("Joy");
