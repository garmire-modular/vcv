#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────
//  JOY — 20 HP 2D Ridgeline Trajectory & Waterfall Audio Vector Synthesizer
//  Transform class module converting incoming audio into stacked
//  waterfall vector trajectories with real-time crest occlusion,
//  galvanometer deflection outputs, and laser beam intensity (Z) modulation.
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

	enum VoltageRange {
		RANGE_BIPOLAR_5V = 0,
		RANGE_BIPOLAR_10V,
		RANGE_UNIPOLAR_5V,
		RANGE_UNIPOLAR_10V,
		RANGE_BIPOLAR_1V,
		RANGE_UNIPOLAR_1V,
		NUM_VOLTAGE_RANGES
	};
	VoltageRange voltageRange = RANGE_BIPOLAR_5V;

	enum ScanMode {
		SCAN_RASTER = 0,
		SCAN_SERPENTINE = 1
	};
	ScanMode scanMode = SCAN_RASTER;

	enum ZScaleMode {
		Z_SCALE_10V = 0,
		Z_SCALE_5V = 1
	};
	ZScaleMode zScaleMode = Z_SCALE_10V;

	enum FadeMode {
		FADE_TOP = 0,    // Fades towards top (distant horizon dims)
		FADE_BOTTOM = 1, // Fades towards bottom (foreground dims)
		FADE_EDGES = 2   // Fades towards left/right edges (center is highest intensity)
	};
	FadeMode fadeMode = FADE_TOP;

	enum ParamId {
		// Row 1 Knobs: FREQ, RANGE (Button), FINE, WIDTH, HEIGHT
		FREQ_PARAM,
		RANGE_PARAM,
		FINE_PARAM,
		WIDTH_PARAM,
		HEIGHT_PARAM,

		// Row 2 Knobs: SMOOTH, REFRESH, LINES, DECAY, WINDOW
		SMOOTH_PARAM,
		REFRESH_PARAM,
		LINES_PARAM,
		DECAY_PARAM,
		WINDOW_PARAM,

		// Row 3 Knobs: TILT, OCCLUSION, ZFADE, FADEMODE, LOCK
		TILT_PARAM,
		OCCLUSION_PARAM,
		ZFADE_PARAM,
		FADEMODE_PARAM,
		LOCK_PARAM,

		// Zone 3 Attenuverter Trimpots: Row 1
		FREQ_TRIM_PARAM,
		REFRESH_TRIM_PARAM,
		TILT_TRIM_PARAM,
		WIDTH_TRIM_PARAM,
		HEIGHT_TRIM_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Jack Row 1 (Inputs)
		AUDIO_INPUT,
		ADV_INPUT,
		SYNC_INPUT,
		LOCK_INPUT,
		FADE_INPUT,

		// Jack Row 2 (CV Inputs)
		FREQ_CV_INPUT,
		REFRESH_CV_INPUT,
		TILT_CV_INPUT,
		WIDTH_CV_INPUT,
		HEIGHT_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		// Jack Row 3 (Outputs)
		X_OUTPUT,
		Y_OUTPUT,
		Z_OUTPUT,
		EOF_OUTPUT,
		EOR_OUTPUT,

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
			if (!joy) return;

			std::string lower;
			lower.reserve(s.size());
			for (char c : s) lower.push_back((char)::tolower(c));

			bool isPeriod = false;
			if (lower.find("s") != std::string::npos && lower.find("hz") == std::string::npos) {
				isPeriod = true;
			} else if (lower.find("hz") != std::string::npos) {
				isPeriod = false;
			} else {
				isPeriod = (joy->rangeMode == Joy::RANGE_VERY_SLOW);
			}

			float rawVal = 0.0f;
			if (std::sscanf(lower.c_str(), "%f", &rawVal) == 1) {
				setFrequencyValue(rawVal, isPeriod);
			}
		}

		std::string getUnit() override {
			Joy* joy = getJoy();
			if (!joy) return "";
			return (joy->rangeMode == Joy::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	// ── Custom Refresh Param Quantity with Strobe Sub-Hertz Scaling ──
	struct RefreshParamQuantity : ParamQuantity {
		float getDisplayValue() override {
			float norm = getValue();
			// 0.02 Hz (50s) to 100.0 Hz exponential curve
			return 0.02f * std::pow(5000.0f, norm);
		}

		std::string getDisplayValueString() override {
			float hz = getDisplayValue();
			char buf[32];
			if (hz < 0.1f) {
				std::snprintf(buf, sizeof(buf), "%.3f", hz);
			} else if (hz < 10.0f) {
				std::snprintf(buf, sizeof(buf), "%.2f", hz);
			} else {
				std::snprintf(buf, sizeof(buf), "%.1f", hz);
			}
			return std::string(buf);
		}
	};

	// ── Ridgeline Memory & Real-Time Scanning Storage ──
	static constexpr int MAX_LINES = 64;
	static constexpr int SAMPLES_PER_LINE = 256;

	float history[MAX_LINES][SAMPLES_PER_LINE] = {};
	float horizon[SAMPLES_PER_LINE] = {};
	float liveBuffer[SAMPLES_PER_LINE] = {};

	int headLine = 0;
	int liveWriteIdx = 0;
	bool armedTrigger = true;
	float prevAudioIn = 0.0f;

	// Beam traversal phase
	float linePhase = 0.0f;
	int displayLineIdx = 0;
	float refreshPhase = 0.0f;

	// Slew filter state for audio smoothing
	float smoothedAudio = 0.0f;

	// Pulse generators
	dsp::PulseGenerator eofPulse;
	dsp::PulseGenerator eorPulse;
	dsp::SchmittTrigger advTrigger;
	dsp::SchmittTrigger syncTrigger;

	Joy() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Row 1: Frequency, Range button, Fine, Width, Height
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);
		configParam(WIDTH_PARAM, 0.1f, 1.f, 0.8f, "Width", "%", 0.f, 100.f);
		configParam(HEIGHT_PARAM, 0.f, 2.f, 1.f, "Height", "%", 0.f, 100.f);

		// Row 2: Smoothness, Refresh rate (strobe capable down to 0.02 Hz), History lines, Decay offset, Window
		configParam(SMOOTH_PARAM, 0.f, 1.f, 0.2f, "Smoothness", "%", 0.f, 100.f);
		configParam<RefreshParamQuantity>(REFRESH_PARAM, 0.f, 1.f, 0.5f, "Refresh rate", " Hz");
		configParam(LINES_PARAM, 1.f, 64.f, 32.f, "History lines", "");
		paramQuantities[LINES_PARAM]->snapEnabled = true;
		configParam(DECAY_PARAM, 0.f, 2.f, 1.f, "Decay offset", "%", 0.f, 100.f);
		configParam(WINDOW_PARAM, 0.f, 1.f, 0.5f, "Border window", "%", 0.f, 100.f);

		// Row 3: Perspective tilt, Crest occlusion, Z-fade depth, Fade mode, Stabilize lock
		configParam(TILT_PARAM, -1.f, 1.f, 0.4f, "Perspective tilt", "%", 0.f, 100.f);
		configParam(OCCLUSION_PARAM, 0.f, 1.f, 0.8f, "Crest occlusion", "%", 0.f, 100.f);
		configParam(ZFADE_PARAM, 0.f, 1.f, 0.75f, "Z-fade depth", "%", 0.f, 100.f);
		configSwitch(FADEMODE_PARAM, 0.f, 2.f, 0.f, "Fade mode", {"Top", "Bottom", "Edges"});
		paramQuantities[FADEMODE_PARAM]->snapEnabled = true;
		configParam(LOCK_PARAM, 0.f, 1.f, 0.5f, "Stabilize threshold", "%", 0.f, 100.f);

		// Zone 3 Attenuverters (Mandatory naming: '<Parameter> CV depth')
		configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV depth", "%", 0.f, 100.f);
		configParam(REFRESH_TRIM_PARAM, -1.f, 1.f, 0.f, "Refresh CV depth", "%", 0.f, 100.f);
		configParam(TILT_TRIM_PARAM, -1.f, 1.f, 0.f, "Tilt CV depth", "%", 0.f, 100.f);
		configParam(WIDTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Width CV depth", "%", 0.f, 100.f);
		configParam(HEIGHT_TRIM_PARAM, -1.f, 1.f, 0.f, "Height CV depth", "%", 0.f, 100.f);

		// Zone 4 Jacks: strictly no redundant 'input' / 'output' / 'IN' / 'OUT' in tooltips
		configInput(AUDIO_INPUT, "Audio");
		configInput(ADV_INPUT, "Advance trigger");
		configInput(SYNC_INPUT, "Sync");
		configInput(LOCK_INPUT, "Stabilize gate");
		configInput(FADE_INPUT, "Fade mode CV");

		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(REFRESH_CV_INPUT, "Refresh CV");
		configInput(TILT_CV_INPUT, "Tilt CV");
		configInput(WIDTH_CV_INPUT, "Width CV");
		configInput(HEIGHT_CV_INPUT, "Height CV");

		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(Z_OUTPUT, "Z");
		configOutput(EOF_OUTPUT, "End of frame");
		configOutput(EOR_OUTPUT, "End of row");

		std::memset(history, 0, sizeof(history));
		for (int i = 0; i < SAMPLES_PER_LINE; ++i) horizon[i] = -2.0f;
	}

	void onReset() override {
		rangeMode = RANGE_LFO;
		voltageRange = RANGE_BIPOLAR_5V;
		scanMode = SCAN_RASTER;
		zScaleMode = Z_SCALE_10V;
		fadeMode = FADE_TOP;
		headLine = 0;
		liveWriteIdx = 0;
		linePhase = 0.0f;
		displayLineIdx = 0;
		refreshPhase = 0.0f;
		std::memset(history, 0, sizeof(history));
		for (int i = 0; i < SAMPLES_PER_LINE; ++i) horizon[i] = -2.0f;
	}

	void getVoltageBounds(float& vMin, float& vMax) const {
		switch (voltageRange) {
			case RANGE_BIPOLAR_10V: vMin = -10.0f; vMax = 10.0f; break;
			case RANGE_UNIPOLAR_5V: vMin =   0.0f; vMax =  5.0f; break;
			case RANGE_UNIPOLAR_10V:vMin =   0.0f; vMax = 10.0f; break;
			case RANGE_BIPOLAR_1V:  vMin =  -1.0f; vMax =  1.0f; break;
			case RANGE_UNIPOLAR_1V: vMin =   0.0f; vMax =  1.0f; break;
			case RANGE_BIPOLAR_5V:
			default:                vMin =  -5.0f; vMax =  5.0f; break;
		}
	}

	float getFrequencyHz() {
		float coarse = params[FREQ_PARAM].getValue();
		float fine = params[FINE_PARAM].getValue();

		if (rangeMode == RANGE_VERY_SLOW) {
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float t = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			return 1.0f / t;
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + 199.99f * coarse * coarse;
			return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
		} else { // RANGE_VCO
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
		}
	}

	float getRefreshHz() {
		float norm = clampf(params[REFRESH_PARAM].getValue(), 0.0f, 1.0f);
		float refreshBase = 0.02f * std::pow(5000.0f, norm);
		float refreshTrim = params[REFRESH_TRIM_PARAM].getValue();
		float refreshCv = inputs[REFRESH_CV_INPUT].getVoltage();
		// Exponential frequency modulation for refresh
		float modHz = refreshBase * std::pow(2.0f, (refreshCv / 5.0f) * refreshTrim * 3.0f);
		return clampf(modHz, 0.005f, 200.0f);
	}

	void process(const ProcessArgs& args) override {
		// Handle momentary Range Button
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		// Update 3-Color Range LED (Yellow = Very Slow, Teal = LFO, Magenta = VCO)
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.f : 0.f);
		lights[RANGE_LIGHT_TEAL].setBrightness(rangeMode == RANGE_LFO ? 1.f : 0.f);
		lights[RANGE_LIGHT_MAGENTA].setBrightness(rangeMode == RANGE_VCO ? 1.f : 0.f);

		// Handle Sync Reset
		if (inputs[SYNC_INPUT].isConnected() && syncTrigger.process(inputs[SYNC_INPUT].getVoltage())) {
			linePhase = 0.0f;
			displayLineIdx = 0;
			for (int s = 0; s < SAMPLES_PER_LINE; ++s) horizon[s] = -2.0f;
		}

		// Fade Mode selection from switch + CV input
		int fModeInt = (int)clampf(std::round(params[FADEMODE_PARAM].getValue()), 0.0f, 2.0f);
		if (inputs[FADE_INPUT].isConnected()) {
			float cv = inputs[FADE_INPUT].getVoltage();
			if (cv > 3.0f) fModeInt = 2;
			else if (cv > 1.0f) fModeInt = 1;
			else fModeInt = 0;
		}
		fadeMode = (FadeMode)fModeInt;

		// ── 1. Audio Ingestion & Smoothing ──
		float rawAudio = inputs[AUDIO_INPUT].getVoltage();
		float smoothParam = clampf(params[SMOOTH_PARAM].getValue(), 0.0f, 1.0f);
		// 1-pole filter: smoothParam=0 -> instant; at 1 -> heavy 15Hz cutoff
		float smoothAlpha = std::exp(-2.0f * (float)M_PI * (20000.0f * std::pow(0.00075f, smoothParam)) * args.sampleTime);
		smoothedAudio = smoothedAudio * smoothAlpha + rawAudio * (1.0f - smoothAlpha);

		// ── 2. Strobe Refresh & History Step Ingestion ──
		float lockThresh = clampf(params[LOCK_PARAM].getValue() * 0.5f, 0.01f, 0.5f);
		bool autoLock = inputs[LOCK_INPUT].isConnected() ? (inputs[LOCK_INPUT].getVoltage() > 1.0f) : (params[LOCK_PARAM].getValue() > 0.05f);
		bool manualAdv = inputs[ADV_INPUT].isConnected();
		bool triggerAdv = manualAdv && advTrigger.process(inputs[ADV_INPUT].getVoltage());

		float refreshRate = getRefreshHz();

		// Accumulate strobe refresh phase
		refreshPhase += refreshRate * args.sampleTime;
		bool timeToCapture = (refreshPhase >= 1.0f);
		if (timeToCapture) {
			refreshPhase -= 1.0f;
		}

		// Fill active liveBuffer slice
		if (liveWriteIdx < SAMPLES_PER_LINE) {
			liveBuffer[liveWriteIdx++] = smoothedAudio;
		}

		bool shouldCommitSlice = false;
		if (manualAdv) {
			shouldCommitSlice = triggerAdv;
		} else if (autoLock) {
			// Zero-crossing hysteresis trigger
			if (armedTrigger && rawAudio > lockThresh && prevAudioIn <= lockThresh) {
				shouldCommitSlice = (liveWriteIdx >= (SAMPLES_PER_LINE / 2));
				armedTrigger = false;
			} else if (!armedTrigger && rawAudio < -lockThresh) {
				armedTrigger = true;
			}
			if (timeToCapture && liveWriteIdx >= SAMPLES_PER_LINE) {
				shouldCommitSlice = true;
			}
		} else {
			shouldCommitSlice = timeToCapture;
		}

		prevAudioIn = rawAudio;

		if (shouldCommitSlice) {
			// Strobe snapshot: commit live slice to circular history array
			for (int s = 0; s < SAMPLES_PER_LINE; ++s) {
				history[headLine][s] = liveBuffer[s];
			}
			headLine = (headLine + 1) % MAX_LINES;
			liveWriteIdx = 0;
		}

		// ── 3. Scan Timebase & Beam Traversal ──
		float fCarrier = getFrequencyHz();
		float fTrim = params[FREQ_TRIM_PARAM].getValue();
		float fCv = inputs[FREQ_CV_INPUT].getVoltage();
		float scanFreq = std::max(0.0001f, fCarrier * std::pow(2.0f, (fCv / 5.0f) * fTrim));

		int numLines = (int)clampf(std::round(params[LINES_PARAM].getValue()), 1.0f, (float)MAX_LINES);

		// Advance line scan phase
		linePhase += scanFreq * args.sampleTime;
		if (linePhase >= 1.0f) {
			linePhase -= 1.0f;
			displayLineIdx++;
			eorPulse.trigger(5e-4f); // 0.5ms End of Row pulse
			if (displayLineIdx >= numLines) {
				displayLineIdx = 0;
				eofPulse.trigger(1e-3f); // 1ms End of Frame pulse
				// Reset horizon tracking at frame start
				for (int s = 0; s < SAMPLES_PER_LINE; ++s) horizon[s] = -2.0f;
			}
		}

		// ── 4. Coordinate Generation (X, Y) with Star Wars 3D Perspective ──
		float xNorm = 0.0f;
		float zRetrace = 1.0f;

		if (scanMode == SCAN_RASTER) {
			// Active sweep for 92% of period, rapid flyback for 8%
			if (linePhase < 0.92f) {
				xNorm = linePhase / 0.92f;
				zRetrace = 1.0f;
			} else {
				float tFly = (linePhase - 0.92f) / 0.08f;
				xNorm = 1.0f - tFly;
				zRetrace = 0.0f; // Blank beam during flyback
			}
		} else {
			// Serpentine (boustrophedon): even lines L->R, odd lines R->L
			bool isEven = (displayLineIdx % 2 == 0);
			xNorm = isEven ? linePhase : (1.0f - linePhase);
			zRetrace = 1.0f;
		}

		// Normalized coordinate centered around 0 (-1.0 to +1.0)
		float xCentered = xNorm * 2.0f - 1.0f;

		// ── Window Shrink: Compresses active waveform into center, flat zero baseline wings ──
		float winParam = clampf(params[WINDOW_PARAM].getValue(), 0.0f, 1.0f);
		// Active window width shrinks from 1.0 (full width) down to 0.15 (tight center cluster)
		float activeWidth = 1.0f - 0.85f * winParam;
		float yAudio = 0.0f;

		if (std::abs(xCentered) < activeWidth) {
			// Inside active window: map xCentered to [-1.0, 1.0] across activeWidth
			float u = xCentered / activeWidth; // -1.0 to +1.0
			float sampleFloat = clampf((u * 0.5f + 0.5f) * (SAMPLES_PER_LINE - 1), 0.0f, (float)(SAMPLES_PER_LINE - 1));
			int s0 = (int)sampleFloat;
			int s1 = std::min(s0 + 1, SAMPLES_PER_LINE - 1);
			float sFrac = sampleFloat - (float)s0;

			int histIdx = (headLine - 1 - displayLineIdx + MAX_LINES * 2) % MAX_LINES;
			float rawSample = history[histIdx][s0] * (1.0f - sFrac) + history[histIdx][s1] * sFrac;

			// Smooth Hann boundary taper at edges of active window
			float windowTaper = 0.5f * (1.0f + std::cos((float)M_PI * u));
			yAudio = rawSample * windowTaper;
		} else {
			// Outside active window: strictly 0.0 (dead-flat baseline margins)
			yAudio = 0.0f;
		}

		// Normalize incoming audio from ±5V nominal to ±1.0
		float yNormAudio = yAudio * 0.20f;

		// Height Control
		float heightBase = params[HEIGHT_PARAM].getValue();
		float heightTrim = params[HEIGHT_TRIM_PARAM].getValue();
		float heightCv = inputs[HEIGHT_CV_INPUT].getVoltage();
		float heightVal = clampf(heightBase + (heightCv / 5.0f) * heightTrim, 0.0f, 3.0f);

		// Decay & Baseline Spacing
		float decayVal = clampf(params[DECAY_PARAM].getValue(), 0.0f, 2.0f);

		// Normalized line position t: 0.0 at bottom (foreground), 1.0 at top (horizon)
		float tLine = (numLines > 1) ? ((float)displayLineIdx / (float)(numLines - 1)) : 0.0f;

		// ── Star Wars Crawl 3D Perspective Tilt ──
		float tiltBase = params[TILT_PARAM].getValue();
		float tiltTrim = params[TILT_TRIM_PARAM].getValue();
		float tiltCv = inputs[TILT_CV_INPUT].getVoltage();
		float tiltVal = clampf(tiltBase + (tiltCv / 5.0f) * tiltTrim, -1.0f, 1.0f);

		// 1. Horizontal Keystone Flare: wide at bottom (foreground), converges to vanishing point at top (horizon)
		float keystoneScale = 1.0f;
		if (tiltVal >= 0.0f) {
			// Flare out bottom up to 1.5x, narrow top down to 0.25x
			keystoneScale = (1.0f + 0.5f * tiltVal) - (1.25f * tiltVal) * tLine;
		} else {
			// Inverted tilt (wide at top, narrow at bottom)
			float absTilt = -tiltVal;
			keystoneScale = (1.0f - 0.75f * absTilt) + (1.25f * absTilt) * tLine;
		}
		keystoneScale = std::max(0.10f, keystoneScale);

		// 2. Vertical Foreshortening: hyperbolic pitch compression towards horizon
		float yPerspLine = tLine;
		if (std::abs(tiltVal) > 0.01f) {
			if (tiltVal > 0.0f) {
				float denom = 1.0f + 2.5f * tiltVal * (1.0f - tLine);
				yPerspLine = (denom > 1e-4f) ? (tLine / denom) : tLine;
			} else {
				float absTilt = -tiltVal;
				float denom = 1.0f + 2.5f * absTilt * tLine;
				yPerspLine = 1.0f - ((1.0f - tLine) / denom);
			}
		}

		// Baseline Y offset (covers -0.65 to +0.65 in display space)
		float yBase = (yPerspLine * 2.0f - 1.0f) * 0.65f * decayVal;

		// 3. Peak Height Foreshortening: tall towering peaks in foreground, small peaks at horizon
		float peakScale = 1.0f;
		if (tiltVal >= 0.0f) {
			peakScale = (1.0f + 0.5f * tiltVal) - (0.80f * tiltVal) * tLine;
		} else {
			float absTilt = -tiltVal;
			peakScale = (1.0f - 0.60f * absTilt) + (0.80f * absTilt) * tLine;
		}
		peakScale = std::max(0.10f, peakScale);

		// Older lines decay gently in peak amplitude if decayVal > 1.0
		float ageDecay = 1.0f - tLine * std::max(0.0f, decayVal - 1.0f) * 0.5f;

		// Composite Y coordinate before soft-knee
		float yDisplacement = yNormAudio * heightVal * peakScale * ageDecay * 0.35f;
		float yRaw = yBase + yDisplacement;
		float ySafe = softKneeClamp(yRaw, 0.85f, 0.98f);

		// Width Control: scales horizontal spread so it comfortably fits any display/scope
		float widthBase = params[WIDTH_PARAM].getValue();
		float widthTrim = params[WIDTH_TRIM_PARAM].getValue();
		float widthCv = inputs[WIDTH_CV_INPUT].getVoltage();
		float widthVal = clampf(widthBase + (widthCv / 5.0f) * widthTrim, 0.1f, 1.0f);

		float xRaw = xCentered * keystoneScale * widthVal * 0.90f;
		float xSafe = softKneeClamp(xRaw, 0.85f, 0.98f);

		// ── 5. Crest Occlusion & Z Output with 3 Fade Modes & Full Extinction ──
		float sampleIdxFloat = clampf(xNorm * (SAMPLES_PER_LINE - 1), 0.0f, (float)(SAMPLES_PER_LINE - 1));
		int s0 = (int)sampleIdxFloat;
		int s1 = std::min(s0 + 1, SAMPLES_PER_LINE - 1);
		float sFrac = sampleIdxFloat - (float)s0;

		float occlusionParam = clampf(params[OCCLUSION_PARAM].getValue(), 0.0f, 1.0f);
		float zOcclusion = 1.0f;

		if (occlusionParam > 0.0f && displayLineIdx > 0) {
			float prevHorizon = horizon[s0] * (1.0f - sFrac) + horizon[s1] * sFrac;
			if (ySafe < prevHorizon) {
				zOcclusion = 1.0f - occlusionParam;
			}
		}

		// Update horizon profile
		if (ySafe > horizon[s0]) horizon[s0] = ySafe;
		if (ySafe > horizon[s1]) horizon[s1] = ySafe;

		// ── Z-Fade Calculation: 3 selectable modes with deep exponential 100% extinction ──
		float zFadeParam = clampf(params[ZFADE_PARAM].getValue(), 0.0f, 1.0f);
		float fadeDist = 0.0f;

		switch (fadeMode) {
			case FADE_BOTTOM:
				// Foreground at bottom dims to black, top/horizon is brightest
				fadeDist = 1.0f - tLine;
				break;
			case FADE_EDGES:
				// Middle of screen (x=0) is brightest, left and right edges fade to black
				fadeDist = clampf(std::abs(xCentered), 0.0f, 1.0f);
				break;
			case FADE_TOP:
			default:
				// Distant horizon at top dims to black, foreground at bottom is brightest
				fadeDist = tLine;
				break;
		}

		// Exponential power extinction: at zFadeParam = 1.0, drops fully to 0.0 at fadeDist = 1.0
		float zDepth = 1.0f;
		if (zFadeParam > 0.001f) {
			float factor = clampf(1.0f - fadeDist * zFadeParam, 0.0f, 1.0f);
			float exponent = 1.0f + 3.5f * zFadeParam;
			zDepth = std::pow(factor, exponent);
		}

		// Composite Z Intensity
		float zTotal = zRetrace * zOcclusion * zDepth;
		float zMaxV = (zScaleMode == Z_SCALE_5V) ? 5.0f : 10.0f;
		float zOutV = clampf(zTotal * zMaxV, 0.0f, zMaxV);

		// ── 6. Voltage Scaling to Outputs ──
		float vMin = -5.0f, vMax = 5.0f;
		getVoltageBounds(vMin, vMax);
		float vCenter = 0.5f * (vMin + vMax);
		float vSpan = 0.5f * (vMax - vMin);

		float xOut = vCenter + xSafe * vSpan;
		float yOut = vCenter + ySafe * vSpan;

		outputs[X_OUTPUT].setVoltage(xOut);
		outputs[Y_OUTPUT].setVoltage(yOut);
		outputs[Z_OUTPUT].setVoltage(zOutV);

		bool eofActive = eofPulse.process(args.sampleTime);
		outputs[EOF_OUTPUT].setVoltage(eofActive ? 10.0f : 0.0f);

		bool eorActive = eorPulse.process(args.sampleTime);
		outputs[EOR_OUTPUT].setVoltage(eorActive ? 10.0f : 0.0f);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		json_object_set_new(rootJ, "scanMode", json_integer((int)scanMode));
		json_object_set_new(rootJ, "zScaleMode", json_integer((int)zScaleMode));
		json_object_set_new(rootJ, "fadeMode", json_integer((int)fadeMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) rangeMode = (RangeMode)json_integer_value(rmJ);

		json_t* stJ = json_object_get(rootJ, "skinTheme");
		if (stJ) skinTheme = (SkinTheme)json_integer_value(stJ);

		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) voltageRange = (VoltageRange)json_integer_value(vrJ);

		json_t* smJ = json_object_get(rootJ, "scanMode");
		if (smJ) scanMode = (ScanMode)json_integer_value(smJ);

		json_t* zmJ = json_object_get(rootJ, "zScaleMode");
		if (zmJ) zScaleMode = (ZScaleMode)json_integer_value(zmJ);

		json_t* fmJ = json_object_get(rootJ, "fadeMode");
		if (fmJ) fadeMode = (FadeMode)json_integer_value(fmJ);
	}
};

// ── Custom Hardware Widgets ──
struct JoyCustomKnob : RoundKnob {
	JoyCustomKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/knob_bg.svg")));
	}
};

struct JoyCustomTrimpot : RoundKnob {
	JoyCustomTrimpot() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/tinyknob_bg.svg")));
	}
};

struct JoyCustomPort : app::SvgPort {
	JoyCustomPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
};

struct JoyCustomButton : app::SvgSwitch {
	JoyCustomButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

struct JoyCustomSwitch : app::SvgSwitch {
	JoyCustomSwitch() {
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/switch_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/switch_1.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/switch_2.svg")));
	}
};

// ── Custom Concentric Range Light Ring ──
struct JoyRangeLightRing : ModuleLightWidget {
	JoyRangeLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));
		// Range 0: Very Slow = #e1be6a (Warm Gold)
		this->addBaseColor(nvgRGBA(0xe1, 0xbe, 0x6a, 0xff));
		// Range 1: LFO = #40b0a6 (Teal)
		this->addBaseColor(nvgRGBA(0x40, 0xb0, 0xa6, 0xff));
		// Range 2: VCO = #d35fb7 (Magenta)
		this->addBaseColor(nvgRGBA(0xd3, 0x5f, 0xb7, 0xff));
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
		nvgStrokeWidth(args.vg, haloW * 2.0f);
		nvgStroke(args.vg);
	}
};

// ── Module Widget (20 HP = 101.60 mm) ──
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

	JoyWidget(Joy* module) {
		setModule(module);
		panel = new SvgPanel();
		panel->box.size = box.size;
		setPanel(panel);
		setTheme(module ? module->skinTheme : Joy::THEME_MEDIUM);

		// 5 Column Grid across 20 HP: [7.000, 28.900, 50.800, 72.700, 94.600] mm
		const float colX[5] = {7.000f, 28.900f, 50.800f, 72.700f, 94.600f};

		// ── Row 1 Knobs: FREQ, RANGE, FINE, WIDTH, HEIGHT (Center Y = 18.00 mm) ──
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[0], 18.00f)), module, Joy::FREQ_PARAM));

		// Range Button at colX[1], Y = 20.00 mm with concentric Light Ring
		addChild(createLightCentered<JoyRangeLightRing>(mm2px(Vec(colX[1], 20.00f)), module, Joy::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<JoyCustomButton>(mm2px(Vec(colX[1], 20.00f)), module, Joy::RANGE_PARAM));

		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[2], 18.00f)), module, Joy::FINE_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[3], 18.00f)), module, Joy::WIDTH_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[4], 18.00f)), module, Joy::HEIGHT_PARAM));

		// ── Row 2 Knobs: SMOOTH, REFRESH, LINES, DECAY, WINDOW (Center Y = 34.00 mm) ──
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[0], 34.00f)), module, Joy::SMOOTH_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[1], 34.00f)), module, Joy::REFRESH_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[2], 34.00f)), module, Joy::LINES_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[3], 34.00f)), module, Joy::DECAY_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[4], 34.00f)), module, Joy::WINDOW_PARAM));

		// ── Row 3 Knobs: TILT, OCCLUSION, ZFADE, FADEMODE, LOCK (Center Y = 50.00 mm) ──
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[0], 50.00f)), module, Joy::TILT_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[1], 50.00f)), module, Joy::OCCLUSION_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[2], 50.00f)), module, Joy::ZFADE_PARAM));
		addParam(createParamCentered<JoyCustomSwitch>(mm2px(Vec(colX[3], 50.00f)), module, Joy::FADEMODE_PARAM));
		addParam(createParamCentered<JoyCustomKnob>(mm2px(Vec(colX[4], 50.00f)), module, Joy::LOCK_PARAM));

		// ── Zone 3 Attenuverter Trimpots (Center Y = 69.00 mm) ──
		addParam(createParamCentered<JoyCustomTrimpot>(mm2px(Vec(colX[0], 69.00f)), module, Joy::FREQ_TRIM_PARAM));
		addParam(createParamCentered<JoyCustomTrimpot>(mm2px(Vec(colX[1], 69.00f)), module, Joy::REFRESH_TRIM_PARAM));
		addParam(createParamCentered<JoyCustomTrimpot>(mm2px(Vec(colX[2], 69.00f)), module, Joy::TILT_TRIM_PARAM));
		addParam(createParamCentered<JoyCustomTrimpot>(mm2px(Vec(colX[3], 69.00f)), module, Joy::WIDTH_TRIM_PARAM));
		addParam(createParamCentered<JoyCustomTrimpot>(mm2px(Vec(colX[4], 69.00f)), module, Joy::HEIGHT_TRIM_PARAM));

		// ── Zone 4 Jacks ──
		// Jack Row 1 (Inputs, Center Y = 91.00 mm)
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[0], 91.00f)), module, Joy::AUDIO_INPUT));
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[1], 91.00f)), module, Joy::ADV_INPUT));
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[2], 91.00f)), module, Joy::SYNC_INPUT));
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[3], 91.00f)), module, Joy::LOCK_INPUT));
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[4], 91.00f)), module, Joy::FADE_INPUT));

		// Jack Row 2 (CV Inputs, Center Y = 104.00 mm)
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[0], 104.00f)), module, Joy::FREQ_CV_INPUT));
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[1], 104.00f)), module, Joy::REFRESH_CV_INPUT));
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[2], 104.00f)), module, Joy::TILT_CV_INPUT));
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[3], 104.00f)), module, Joy::WIDTH_CV_INPUT));
		addInput(createInputCentered<JoyCustomPort>(mm2px(Vec(colX[4], 104.00f)), module, Joy::HEIGHT_CV_INPUT));

		// Jack Row 3 (Outputs, Center Y = 117.00 mm)
		addOutput(createOutputCentered<JoyCustomPort>(mm2px(Vec(colX[0], 117.00f)), module, Joy::X_OUTPUT));
		addOutput(createOutputCentered<JoyCustomPort>(mm2px(Vec(colX[1], 117.00f)), module, Joy::Y_OUTPUT));
		addOutput(createOutputCentered<JoyCustomPort>(mm2px(Vec(colX[2], 117.00f)), module, Joy::Z_OUTPUT));
		addOutput(createOutputCentered<JoyCustomPort>(mm2px(Vec(colX[3], 117.00f)), module, Joy::EOF_OUTPUT));
		addOutput(createOutputCentered<JoyCustomPort>(mm2px(Vec(colX[4], 117.00f)), module, Joy::EOR_OUTPUT));
	}

	void step() override {
		ModuleWidget::step();
		auto* joy = dynamic_cast<Joy*>(module);
		if (joy && joy->skinTheme != currentTheme) {
			setTheme(joy->skinTheme);
		}
	}

	void appendContextMenu(Menu* menu) override {
		auto* joy = dynamic_cast<Joy*>(module);
		if (!joy) return;

		menu->addChild(new MenuSeparator());

		// Range Mode Submenu
		menu->addChild(createMenuLabel("Range"));
		const char* rangeLabels[] = {
			"Very Slow (600s - 0.1s)",
			"LFO (0.01 - 200 Hz)",
			"VCO (150 Hz - 2 kHz)"
		};
		for (int i = 0; i < 3; i++) {
			Joy::RangeMode mode = (Joy::RangeMode)i;
			menu->addChild(createCheckMenuItem(rangeLabels[i], "",
				[=]() { return joy->rangeMode == mode; },
				[=]() { joy->rangeMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());

		// Z-Fade Mode Submenu
		menu->addChild(createMenuLabel("Z-Fade Direction"));
		const char* fadeLabels[] = {
			"Fade Toward Top (Horizon Dims)",
			"Fade Toward Bottom (Foreground Dims)",
			"Fade Toward Edges (Center Bright)"
		};
		for (int i = 0; i < 3; i++) {
			Joy::FadeMode mode = (Joy::FadeMode)i;
			menu->addChild(createCheckMenuItem(fadeLabels[i], "",
				[=]() { return joy->fadeMode == mode; },
				[=]() { joy->fadeMode = mode; }
			));
		}

		menu->addChild(new MenuSeparator());

		// Scan Mode Submenu
		menu->addChild(createMenuLabel("Scan Algorithm"));
		menu->addChild(createCheckMenuItem("Raster Flyback (Blanked Retrace)", "",
			[=]() { return joy->scanMode == Joy::SCAN_RASTER; },
			[=]() { joy->scanMode = Joy::SCAN_RASTER; }
		));
		menu->addChild(createCheckMenuItem("Serpentine (Continuous Boustrophedon)", "",
			[=]() { return joy->scanMode == Joy::SCAN_SERPENTINE; },
			[=]() { joy->scanMode = Joy::SCAN_SERPENTINE; }
		));

		menu->addChild(new MenuSeparator());

		// Output Voltage Range Submenu
		menu->addChild(createMenuLabel("Voltage Range"));
		const char* vrLabels[] = {
			"±5V (Default)",
			"±10V",
			"0 to 5V",
			"0 to 10V",
			"±1V",
			"0 to 1V"
		};
		for (int i = 0; i < Joy::NUM_VOLTAGE_RANGES; i++) {
			Joy::VoltageRange mode = (Joy::VoltageRange)i;
			menu->addChild(createCheckMenuItem(vrLabels[i], "",
				[=]() { return joy->voltageRange == mode; },
				[=]() { joy->voltageRange = mode; }
			));
		}

		menu->addChild(new MenuSeparator());

		// Z Output Scale Submenu
		menu->addChild(createMenuLabel("Z Output Level"));
		menu->addChild(createCheckMenuItem("0 to 10V (Eurorack VCA standard)", "",
			[=]() { return joy->zScaleMode == Joy::Z_SCALE_10V; },
			[=]() { joy->zScaleMode = Joy::Z_SCALE_10V; }
		));
		menu->addChild(createCheckMenuItem("0 to 5V (Laser diode driver standard)", "",
			[=]() { return joy->zScaleMode == Joy::Z_SCALE_5V; },
			[=]() { joy->zScaleMode = Joy::Z_SCALE_5V; }
		));

		menu->addChild(new MenuSeparator());

		// Theme Skins
		menu->addChild(createMenuLabel("Skin"));
		menu->addChild(createCheckMenuItem("Medium Skin", "",
			[=]() { return joy->skinTheme == Joy::THEME_MEDIUM; },
			[=]() { joy->skinTheme = Joy::THEME_MEDIUM; setTheme(Joy::THEME_MEDIUM); }
		));
		menu->addChild(createCheckMenuItem("Light Skin", "",
			[=]() { return joy->skinTheme == Joy::THEME_LIGHT; },
			[=]() { joy->skinTheme = Joy::THEME_LIGHT; setTheme(Joy::THEME_LIGHT); }
		));
		menu->addChild(createCheckMenuItem("Dark Skin", "",
			[=]() { return joy->skinTheme == Joy::THEME_DARK; },
			[=]() { joy->skinTheme = Joy::THEME_DARK; setTheme(Joy::THEME_DARK); }
		));
	}
};

Model* modelJoy = createModel<Joy, JoyWidget>("Joy");
