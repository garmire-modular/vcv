#include "plugin.hpp"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <osdialog.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

// 4-point Hermite cubic interpolation for antialiased smooth wavetable read
inline float hermiteInterp(float y0, float y1, float y2, float y3, float frac) {
	float c0 = y1;
	float c1 = 0.5f * (y2 - y0);
	float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
	float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
	return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

static const int CYCLE_OPTIONS[7] = {1, 8, 16, 24, 48, 96, 128};

} // namespace

struct Reverie : Module {
	static constexpr int SAMPLES_PER_CYCLE = 4096;
	static constexpr int NUM_CHANNELS = 6; // X, Y, R, G, B, Z/I
	static constexpr int NUM_SLOTS = 16;

	enum RangeMode {
		RANGE_VERY_SLOW = 0, // 600s down to 0.1s
		RANGE_LFO,            // 0.01 - 200 Hz
		RANGE_VCO             // 150 Hz - 2 kHz
	};

	enum VoltageRange {
		RANGE_BIPOLAR_1V = 0,
		RANGE_UNIPOLAR_1V,
		RANGE_BIPOLAR_5V,
		RANGE_UNIPOLAR_5V,
		RANGE_BIPOLAR_10V,
		RANGE_UNIPOLAR_10V,
		NUM_VOLTAGE_RANGES
	};

	enum SkinTheme {
		THEME_MEDIUM = 0,
		THEME_LIGHT = 1,
		THEME_DARK = 2
	};

	enum RecordState {
		REC_IDLE = 0,
		REC_WAIT_SYNC,
		REC_CAPTURING
	};

	enum ParamId {
		FREQ_PARAM,
		FINE_PARAM,
		RANGE_PARAM,
		MORPH_PARAM,
		CYCLES_PARAM,
		FREQ_TRIM_PARAM,
		MORPH_TRIM_PARAM,
		SCAN_X_TRIM_PARAM,
		SCAN_Y_TRIM_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		FREQ_CV_INPUT,
		MORPH_CV_INPUT,
		SCAN_X_CV_INPUT,
		SCAN_Y_CV_INPUT,
		X_INPUT,
		Y_INPUT,
		R_INPUT,
		G_INPUT,
		B_INPUT,
		ZI_INPUT,
		SYNC_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		X_OUTPUT,
		Y_OUTPUT,
		R_OUTPUT,
		G_OUTPUT,
		B_OUTPUT,
		ZI_OUTPUT,
		SYNC_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		RANGE_LIGHT_YELLOW,
		RANGE_LIGHT_ORANGE,
		RANGE_LIGHT_PURPLE,
		LIGHTS_LEN
	};

	struct PresetSlot {
		bool hasData = false;
		int numCycles = 1;
		int totalSamples = 0;
		float baseFreq = 1.0f;
		std::vector<float> channels[NUM_CHANNELS];

		PresetSlot() {
			for (int c = 0; c < NUM_CHANNELS; c++) {
				channels[c].resize(SAMPLES_PER_CYCLE);
			}
		}

		void clear() {
			hasData = false;
			numCycles = 1;
			totalSamples = 0;
			baseFreq = 1.0f;
			for (int c = 0; c < NUM_CHANNELS; c++) {
				std::fill(channels[c].begin(), channels[c].end(), 0.0f);
			}
		}

		float readSample(int channel, double phase) const {
			if (!hasData || totalSamples <= 0 || channel < 0 || channel >= NUM_CHANNELS) {
				return 0.0f;
			}
			const std::vector<float>& buf = channels[channel];
			if (buf.empty()) return 0.0f;

			double pos = phase * (double)totalSamples;
			int idx1 = (int)std::floor(pos);
			float frac = (float)(pos - (double)idx1);

			int N = totalSamples;
			int idx0 = (idx1 - 1 + N) % N;
			idx1 = idx1 % N;
			int idx2 = (idx1 + 1) % N;
			int idx3 = (idx1 + 2) % N;

			return hermiteInterp(buf[idx0], buf[idx1], buf[idx2], buf[idx3], frac);
		}
	};

	PresetSlot slots[NUM_SLOTS];

	SkinTheme skinTheme = THEME_MEDIUM;
	RangeMode rangeMode = RANGE_LFO;
	VoltageRange scanRange = RANGE_UNIPOLAR_10V;

	// Recording state
	RecordState recState = REC_IDLE;
	int armedSlot = -1;
	int targetCycles = 1;
	int completedCycles = 0;
	float timeSinceSync = 0.0f;
	float lastMeasuredPeriod = 0.01f;

	// Scratch capture buffer for current cycle (pre-allocated)
	static constexpr int MAX_CYCLE_RAW_SAMPLES = 192000;
	std::vector<float> rawCycleBuffer[NUM_CHANNELS];
	int rawSampleCount = 0;

	// Playback state
	int activeSlot = -1;
	int morphFromSlot = -1;
	int morphToSlot = -1;
	float morphProgress = 1.0f;
	double playPhase = 0.0;

	dsp::BooleanTrigger rangeTrigger;
	dsp::SchmittTrigger syncInTrigger;
	dsp::PulseGenerator syncOutPulse;

	struct FreqParamQuantity : ParamQuantity {
		Reverie* getReverie() {
			return dynamic_cast<Reverie*>(module);
		}

		float getDisplayValue() override {
			Reverie* mod = getReverie();
			if (!mod) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (mod->paramQuantities.size() > Reverie::FINE_PARAM) ? mod->params[Reverie::FINE_PARAM].getValue() : 0.f;

			if (mod->rangeMode == Reverie::RANGE_VERY_SLOW) {
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (mod->rangeMode == Reverie::RANGE_LFO) {
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.001f, 500.0f);
			} else {
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * std::pow(2.0f, fine / 12.0f), 20.0f, 10000.0f);
			}
		}

		std::string getDisplayValueString() override {
			Reverie* mod = getReverie();
			if (!mod) return ParamQuantity::getDisplayValueString();
			float val = getDisplayValue();
			char buf[32];
			if (mod->rangeMode == Reverie::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			} else if (mod->rangeMode == Reverie::RANGE_LFO) {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			}
			return std::string(buf);
		}

		std::string getUnit() override {
			Reverie* mod = getReverie();
			if (!mod) return "";
			return (mod->rangeMode == Reverie::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	struct CyclesParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int idx = (int)std::round(getValue());
			idx = std::max(0, std::min(6, idx));
			return std::to_string(CYCLE_OPTIONS[idx]) + " cycles";
		}
	};

	Reverie() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Parameters
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.0f, 1.0f, 0.5f, "Playback frequency");
		configParam(FINE_PARAM, -1.0f, 1.0f, 0.0f, "Fine tune", "%", 0.0f, 100.0f);
		configParam(RANGE_PARAM, 0.0f, 1.0f, 0.0f, "Range mode");
		configParam(MORPH_PARAM, 0.0f, 10.0f, 0.5f, "Morph time", " s");
		configParam<CyclesParamQuantity>(CYCLES_PARAM, 0.0f, 6.0f, 0.0f, "Capture cycles");

		// Attenuverter parameters (sentence case: <Parameter> CV depth)
		configParam(FREQ_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Frequency CV depth", "%", 0.0f, 100.0f);
		configParam(MORPH_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Morph time CV depth", "%", 0.0f, 100.0f);
		configParam(SCAN_X_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Scan X CV depth", "%", 0.0f, 100.0f);
		configParam(SCAN_Y_TRIM_PARAM, -1.0f, 1.0f, 0.0f, "Scan Y CV depth", "%", 0.0f, 100.0f);

		// Inputs (no redundant "input" words)
		configInput(FREQ_CV_INPUT, "Frequency CV");
		configInput(MORPH_CV_INPUT, "Morph time CV");
		configInput(SCAN_X_CV_INPUT, "Scan X CV");
		configInput(SCAN_Y_CV_INPUT, "Scan Y CV");
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(R_INPUT, "R");
		configInput(G_INPUT, "G");
		configInput(B_INPUT, "B");
		configInput(ZI_INPUT, "Z/I");
		configInput(SYNC_INPUT, "Sync");

		// Outputs (no redundant "output" words)
		configOutput(X_OUTPUT, "X");
		configOutput(Y_OUTPUT, "Y");
		configOutput(R_OUTPUT, "R");
		configOutput(G_OUTPUT, "G");
		configOutput(B_OUTPUT, "B");
		configOutput(ZI_OUTPUT, "Z/I");
		configOutput(SYNC_OUTPUT, "Sync");

		// Pre-allocate raw cycle capture buffers
		for (int c = 0; c < NUM_CHANNELS; c++) {
			rawCycleBuffer[c].resize(MAX_CYCLE_RAW_SAMPLES, 0.0f);
		}
	}

	void onReset() override {
		for (int s = 0; s < NUM_SLOTS; s++) {
			slots[s].clear();
		}
		activeSlot = -1;
		morphFromSlot = -1;
		morphToSlot = -1;
		morphProgress = 1.0f;
		recState = REC_IDLE;
		armedSlot = -1;
	}

	float calculateBaseFrequency(float coarse, float fine, float cvMod) const {
		if (rangeMode == RANGE_VERY_SLOW) {
			float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
			float t = clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			float f = 1.0f / t;
			if (cvMod != 0.0f) {
				f *= std::pow(2.0f, cvMod);
			}
			return clampf(f, 0.0001f, 20.0f);
		} else if (rangeMode == RANGE_LFO) {
			float fCoarse = 0.01f + 199.99f * coarse * coarse;
			float f = clampf(fCoarse + fine * 20.0f, 0.001f, 500.0f);
			if (cvMod != 0.0f) {
				f *= std::pow(2.0f, cvMod);
			}
			return clampf(f, 0.001f, 1000.0f);
		} else {
			float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
			float f = fCoarse * std::pow(2.0f, fine / 12.0f);
			if (cvMod != 0.0f) {
				f *= std::pow(2.0f, cvMod);
			}
			return clampf(f, 1.0f, 20000.0f);
		}
	}

	void getRangeVoltages(float& vMin, float& vMax) const {
		switch (scanRange) {
			case RANGE_BIPOLAR_1V:   vMin = -1.0f; vMax = 1.0f;  break;
			case RANGE_UNIPOLAR_1V:  vMin =  0.0f; vMax = 1.0f;  break;
			case RANGE_BIPOLAR_5V:   vMin = -5.0f; vMax = 5.0f;  break;
			case RANGE_UNIPOLAR_5V:  vMin =  0.0f; vMax = 5.0f;  break;
			case RANGE_BIPOLAR_10V:  vMin = -10.0f; vMax = 10.0f; break;
			case RANGE_UNIPOLAR_10V:
			default:                 vMin =  0.0f; vMax = 10.0f; break;
		}
	}

	void armSlot(int slotIndex) {
		if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
		int cycleOptIdx = (int)std::round(params[CYCLES_PARAM].getValue());
		cycleOptIdx = std::max(0, std::min(6, cycleOptIdx));
		targetCycles = CYCLE_OPTIONS[cycleOptIdx];
		completedCycles = 0;
		rawSampleCount = 0;

		armedSlot = slotIndex;
		recState = REC_WAIT_SYNC;
	}

	void selectSlot(int slotIndex) {
		if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
		if (!slots[slotIndex].hasData) {
			armSlot(slotIndex);
			return;
		}
		if (activeSlot == slotIndex && morphProgress >= 1.0f) return;

		float morphSec = clampf(params[MORPH_PARAM].getValue(), 0.0f, 10.0f);
		if (inputs[MORPH_CV_INPUT].isConnected()) {
			morphSec = clampf(morphSec + (inputs[MORPH_CV_INPUT].getVoltage() / 5.0f) * params[MORPH_TRIM_PARAM].getValue() * 5.0f, 0.0f, 10.0f);
		}

		if (activeSlot < 0 || !slots[activeSlot].hasData || morphSec <= 0.001f) {
			activeSlot = slotIndex;
			morphFromSlot = slotIndex;
			morphToSlot = slotIndex;
			morphProgress = 1.0f;
		} else {
			morphFromSlot = activeSlot;
			morphToSlot = slotIndex;
			morphProgress = 0.0f;
		}
	}

	void finalizeCycle(int cycleIdx) {
		if (armedSlot < 0 || armedSlot >= NUM_SLOTS || rawSampleCount <= 1) return;
		PresetSlot& slot = slots[armedSlot];

		int reqTotal = targetCycles * SAMPLES_PER_CYCLE;
		for (int c = 0; c < NUM_CHANNELS; c++) {
			if ((int)slot.channels[c].size() != reqTotal) {
				slot.channels[c].resize(reqTotal, 0.0f);
			}
		}

		int startOffset = cycleIdx * SAMPLES_PER_CYCLE;
		int rawN = rawSampleCount;

		for (int i = 0; i < SAMPLES_PER_CYCLE; i++) {
			double pos = ((double)i / (double)SAMPLES_PER_CYCLE) * (double)rawN;
			int idx1 = (int)std::floor(pos);
			float frac = (float)(pos - (double)idx1);

			int idx0 = (idx1 - 1 + rawN) % rawN;
			idx1 = idx1 % rawN;
			int idx2 = (idx1 + 1) % rawN;
			int idx3 = (idx1 + 2) % rawN;

			for (int c = 0; c < NUM_CHANNELS; c++) {
				const std::vector<float>& src = rawCycleBuffer[c];
				slot.channels[c][startOffset + i] = hermiteInterp(src[idx0], src[idx1], src[idx2], src[idx3], frac);
			}
		}
	}

	void process(const ProcessArgs& args) override {
		// 1. Range Button Cycling
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.0f : 0.0f);

		// 2. Sync In edge detection & period measurement
		timeSinceSync += args.sampleTime;
		bool syncInPulse = syncInTrigger.process(inputs[SYNC_INPUT].getVoltage());
		if (syncInPulse) {
			if (timeSinceSync > 1e-5f) {
				lastMeasuredPeriod = timeSinceSync;
			}
			timeSinceSync = 0.0f;
		}

		// 3. Recording Engine
		if (recState == REC_WAIT_SYNC) {
			if (syncInPulse) {
				recState = REC_CAPTURING;
				completedCycles = 0;
				rawSampleCount = 0;
			}
		} else if (recState == REC_CAPTURING) {
			if (syncInPulse && rawSampleCount > 10) {
				finalizeCycle(completedCycles);
				completedCycles++;
				rawSampleCount = 0;

				if (completedCycles >= targetCycles) {
					// Finalize preset slot
					PresetSlot& slot = slots[armedSlot];
					slot.hasData = true;
					slot.numCycles = targetCycles;
					slot.totalSamples = targetCycles * SAMPLES_PER_CYCLE;
					slot.baseFreq = (lastMeasuredPeriod > 1e-5f) ? (1.0f / lastMeasuredPeriod) : 1.0f;

					if (activeSlot < 0) {
						activeSlot = armedSlot;
						morphProgress = 1.0f;
					}
					armedSlot = -1;
					recState = REC_IDLE;
				}
			}

			if (recState == REC_CAPTURING) {
				// Record input frame into raw cycle buffer
				if (rawSampleCount < MAX_CYCLE_RAW_SAMPLES) {
					rawCycleBuffer[0][rawSampleCount] = inputs[X_INPUT].isConnected() ? inputs[X_INPUT].getVoltage() : 0.0f;
					rawCycleBuffer[1][rawSampleCount] = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getVoltage() : 0.0f;
					rawCycleBuffer[2][rawSampleCount] = inputs[R_INPUT].isConnected() ? inputs[R_INPUT].getVoltage() : 0.0f;
					rawCycleBuffer[3][rawSampleCount] = inputs[G_INPUT].isConnected() ? inputs[G_INPUT].getVoltage() : 0.0f;
					rawCycleBuffer[4][rawSampleCount] = inputs[B_INPUT].isConnected() ? inputs[B_INPUT].getVoltage() : 0.0f;
					rawCycleBuffer[5][rawSampleCount] = inputs[ZI_INPUT].isConnected() ? inputs[ZI_INPUT].getVoltage() : 0.0f;
					rawSampleCount++;
				}
			}
		}

		// 4. Pass-through when recording or no active slot
		bool isRecording = (recState == REC_CAPTURING);
		if (isRecording) {
			// Directive: "Silent when capturing [Sync Out]"
			outputs[SYNC_OUTPUT].setVoltage(0.0f);

			// Pass through signals during capture
			outputs[X_OUTPUT].setVoltage(inputs[X_INPUT].getVoltage());
			outputs[Y_OUTPUT].setVoltage(inputs[Y_INPUT].getVoltage());
			outputs[R_OUTPUT].setVoltage(inputs[R_INPUT].getVoltage());
			outputs[G_OUTPUT].setVoltage(inputs[G_INPUT].getVoltage());
			outputs[B_OUTPUT].setVoltage(inputs[B_INPUT].getVoltage());
			outputs[ZI_OUTPUT].setVoltage(inputs[ZI_INPUT].getVoltage());
			return;
		}

		if (activeSlot < 0 || !slots[activeSlot].hasData) {
			// No preset playing: Pass-through
			outputs[X_OUTPUT].setVoltage(inputs[X_INPUT].getVoltage());
			outputs[Y_OUTPUT].setVoltage(inputs[Y_INPUT].getVoltage());
			outputs[R_OUTPUT].setVoltage(inputs[R_INPUT].getVoltage());
			outputs[G_OUTPUT].setVoltage(inputs[G_INPUT].getVoltage());
			outputs[B_OUTPUT].setVoltage(inputs[B_INPUT].getVoltage());
			outputs[ZI_OUTPUT].setVoltage(inputs[ZI_INPUT].getVoltage());
			outputs[SYNC_OUTPUT].setVoltage(inputs[SYNC_INPUT].getVoltage());
			return;
		}

		// 5. Playback Frequency Calculation
		float freqCoarse = params[FREQ_PARAM].getValue();
		float freqFine = params[FINE_PARAM].getValue();
		float freqCvMod = 0.0f;
		if (inputs[FREQ_CV_INPUT].isConnected()) {
			freqCvMod = inputs[FREQ_CV_INPUT].getVoltage() * params[FREQ_TRIM_PARAM].getValue();
		}
		float fPlay = calculateBaseFrequency(freqCoarse, freqFine, freqCvMod);

		// 6. Advance Phase Accumulator
		playPhase += (double)fPlay * args.sampleTime;
		if (playPhase >= 1.0) {
			playPhase -= 1.0;
			syncOutPulse.trigger(1e-3f); // 1 ms sync trigger
		}
		outputs[SYNC_OUTPUT].setVoltage(syncOutPulse.process(args.sampleTime) ? 10.0f : 0.0f);

		// 7. Morph Progression
		if (morphProgress < 1.0f) {
			float morphSec = clampf(params[MORPH_PARAM].getValue(), 0.001f, 10.0f);
			if (inputs[MORPH_CV_INPUT].isConnected()) {
				morphSec = clampf(morphSec + (inputs[MORPH_CV_INPUT].getVoltage() / 5.0f) * params[MORPH_TRIM_PARAM].getValue() * 5.0f, 0.001f, 10.0f);
			}
			morphProgress += args.sampleTime / morphSec;
			if (morphProgress >= 1.0f) {
				morphProgress = 1.0f;
				activeSlot = morphToSlot;
			}
		}

		// 8. CV Grid Scan (Directive: "CV sweeps are manual, morph time is ignored")
		bool scanConnected = inputs[SCAN_X_CV_INPUT].isConnected() || inputs[SCAN_Y_CV_INPUT].isConnected();
		if (scanConnected) {
			float vMin = 0.0f, vMax = 10.0f;
			getRangeVoltages(vMin, vMax);
			float vRange = vMax - vMin;
			if (vRange <= 1e-4f) vRange = 1.0f;

			float scanX = 0.0f;
			if (inputs[SCAN_X_CV_INPUT].isConnected()) {
				float vx = inputs[SCAN_X_CV_INPUT].getVoltage() * params[SCAN_X_TRIM_PARAM].getValue();
				scanX = clampf((vx - vMin) / vRange, 0.0f, 1.0f) * 3.0f;
			}
			float scanY = 0.0f;
			if (inputs[SCAN_Y_CV_INPUT].isConnected()) {
				float vy = inputs[SCAN_Y_CV_INPUT].getVoltage() * params[SCAN_Y_TRIM_PARAM].getValue();
				scanY = clampf((vy - vMin) / vRange, 0.0f, 1.0f) * 3.0f;
			}

			// 2D grid coordinates (4x4)
			int col0 = std::min(2, (int)std::floor(scanX));
			int col1 = col0 + 1;
			float fracX = scanX - (float)col0;

			int row0 = std::min(2, (int)std::floor(scanY));
			int row1 = row0 + 1;
			float fracY = scanY - (float)row0;

			int s00 = row0 * 4 + col0;
			int s10 = row0 * 4 + col1;
			int s01 = row1 * 4 + col0;
			int s11 = row1 * 4 + col1;

			for (int c = 0; c < NUM_CHANNELS; c++) {
				float v00 = slots[s00].readSample(c, playPhase);
				float v10 = slots[s10].readSample(c, playPhase);
				float v01 = slots[s01].readSample(c, playPhase);
				float v11 = slots[s11].readSample(c, playPhase);

				float top = (1.0f - fracX) * v00 + fracX * v10;
				float bot = (1.0f - fracX) * v01 + fracX * v11;
				float outVal = (1.0f - fracY) * top + fracY * bot;

				// Galvo safety clamp (-12V to +12V)
				outVal = clampf(outVal, -12.0f, 12.0f);
				outputs[X_OUTPUT + c].setVoltage(outVal);
			}
		} else {
			// Normal Timed Morph between active and target presets
			for (int c = 0; c < NUM_CHANNELS; c++) {
				float outVal = 0.0f;
				if (morphProgress >= 1.0f || morphFromSlot < 0 || !slots[morphFromSlot].hasData) {
					outVal = slots[activeSlot].readSample(c, playPhase);
				} else {
					float vFrom = slots[morphFromSlot].readSample(c, playPhase);
					float vTo = slots[morphToSlot].readSample(c, playPhase);
					outVal = (1.0f - morphProgress) * vFrom + morphProgress * vTo;
				}
				outVal = clampf(outVal, -12.0f, 12.0f);
				outputs[X_OUTPUT + c].setVoltage(outVal);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "scanRange", json_integer((int)scanRange));
		json_object_set_new(rootJ, "activeSlot", json_integer(activeSlot));

		json_t* slotsJ = json_array();
		for (int s = 0; s < NUM_SLOTS; s++) {
			const PresetSlot& slot = slots[s];
			json_t* slotJ = json_object();
			json_object_set_new(slotJ, "hasData", json_boolean(slot.hasData));
			if (slot.hasData) {
				json_object_set_new(slotJ, "numCycles", json_integer(slot.numCycles));
				json_object_set_new(slotJ, "totalSamples", json_integer(slot.totalSamples));
				json_object_set_new(slotJ, "baseFreq", json_real(slot.baseFreq));

				json_t* channelsJ = json_array();
				for (int c = 0; c < NUM_CHANNELS; c++) {
					json_t* chJ = json_array();
					for (float v : slot.channels[c]) {
						json_array_append_new(chJ, json_real(v));
					}
					json_array_append_new(channelsJ, chJ);
				}
				json_object_set_new(slotJ, "channels", channelsJ);
			}
			json_array_append_new(slotsJ, slotJ);
		}
		json_object_set_new(rootJ, "slots", slotsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* themeJ = json_object_get(rootJ, "skinTheme");
		if (themeJ) skinTheme = (SkinTheme)json_integer_value(themeJ);

		json_t* rangeJ = json_object_get(rootJ, "scanRange");
		if (rangeJ) scanRange = (VoltageRange)json_integer_value(rangeJ);

		json_t* activeJ = json_object_get(rootJ, "activeSlot");
		if (activeJ) activeSlot = (int)json_integer_value(activeJ);

		json_t* slotsJ = json_object_get(rootJ, "slots");
		if (slotsJ && json_is_array(slotsJ)) {
			size_t slotIdx;
			json_t* slotJ;
			json_array_foreach(slotsJ, slotIdx, slotJ) {
				if (slotIdx >= NUM_SLOTS) break;
				PresetSlot& slot = slots[slotIdx];
				json_t* hasDataJ = json_object_get(slotJ, "hasData");
				if (hasDataJ && json_is_true(hasDataJ)) {
					slot.hasData = true;
					json_t* numCyclesJ = json_object_get(slotJ, "numCycles");
					if (numCyclesJ) slot.numCycles = (int)json_integer_value(numCyclesJ);

					json_t* totalSamplesJ = json_object_get(slotJ, "totalSamples");
					if (totalSamplesJ) slot.totalSamples = (int)json_integer_value(totalSamplesJ);

					json_t* baseFreqJ = json_object_get(slotJ, "baseFreq");
					if (baseFreqJ) slot.baseFreq = (float)json_real_value(baseFreqJ);

					json_t* channelsJ = json_object_get(slotJ, "channels");
					if (channelsJ && json_is_array(channelsJ)) {
						size_t chIdx;
						json_t* chJ;
						json_array_foreach(channelsJ, chIdx, chJ) {
							if (chIdx >= NUM_CHANNELS) break;
							slot.channels[chIdx].clear();
							size_t smpIdx;
							json_t* smpJ;
							json_array_foreach(chJ, smpIdx, smpJ) {
								slot.channels[chIdx].push_back((float)json_real_value(smpJ));
							}
						}
					}
				} else {
					slot.clear();
				}
			}
		}
	}
};

// Custom Garmire Widgets
struct GarmirePort : app::SvgPort {
	GarmirePort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/3_5mm_jacks.svg")));
	}
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

struct GarmireButton : app::SvgSwitch {
	GarmireButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

struct ReverieYellowLight : ModuleLightWidget {
	ReverieYellowLight() {
		addBaseColor(nvgRGB(0xe1, 0xbe, 0x6a));
	}
};

struct ReverieOrangeLight : ModuleLightWidget {
	ReverieOrangeLight() {
		addBaseColor(nvgRGB(0x40, 0xb0, 0xa6));
	}
};

struct ReveriePurpleLight : ModuleLightWidget {
	ReveriePurpleLight() {
		addBaseColor(nvgRGB(0xd3, 0x5f, 0xb7));
	}
};

// 4x4 Preset Grid Widget
struct PresetGridWidget : OpaqueWidget {
	Reverie* module = nullptr;
	int hoveredSlot = -1;

	PresetGridWidget() {
		box.size = mm2px(Vec(54.0f, 54.0f));
	}

	void onButton(const ButtonEvent& e) override {
		if (!module) {
			OpaqueWidget::onButton(e);
			return;
		}

		float w = box.size.x;
		float h = box.size.y;
		float pitchX = w / 4.0f;
		float pitchY = h / 4.0f;

		int col = (int)std::floor(e.pos.x / pitchX);
		int row = (int)std::floor(e.pos.y / pitchY);
		col = std::max(0, std::min(3, col));
		row = std::max(0, std::min(3, row));
		int slotIdx = row * 4 + col;

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (e.mods & GLFW_MOD_SHIFT) {
				// Shift + Click: Arm slot for overwrite
				module->armSlot(slotIdx);
			} else {
				// Click: Select preset or Arm if empty
				module->selectSlot(slotIdx);
			}
			e.consume(this);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			char titleBuf[64];
			std::snprintf(titleBuf, sizeof(titleBuf), "Preset Slot %d (Row %d, Col %d)", slotIdx + 1, row + 1, col + 1);
			menu->addChild(createMenuLabel(titleBuf));
			menu->addChild(new MenuSeparator());

			menu->addChild(createMenuItem("Arm for Recording", "", [=]() {
				if (module) module->armSlot(slotIdx);
			}));

			if (module->slots[slotIdx].hasData) {
				menu->addChild(createMenuItem("Recall Preset", "", [=]() {
					if (module) module->selectSlot(slotIdx);
				}));

				menu->addChild(createMenuItem("Clear Preset", "", [=]() {
					if (module) {
						module->slots[slotIdx].clear();
						if (module->activeSlot == slotIdx) module->activeSlot = -1;
					}
				}));
			}

			e.consume(this);
			return;
		}

		OpaqueWidget::onButton(e);
	}

	void onHover(const HoverEvent& e) override {
		float w = box.size.x;
		float h = box.size.y;
		int col = (int)std::floor(e.pos.x / (w / 4.0f));
		int row = (int)std::floor(e.pos.y / (h / 4.0f));
		hoveredSlot = (col >= 0 && col < 4 && row >= 0 && row < 4) ? (row * 4 + col) : -1;
		OpaqueWidget::onHover(e);
	}

	void onLeave(const LeaveEvent& e) override {
		hoveredSlot = -1;
		OpaqueWidget::onLeave(e);
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float w = box.size.x;
		float h = box.size.y;
		float pitchX = w / 4.0f;
		float pitchY = h / 4.0f;
		float btnW = pitchX - 3.5f;
		float btnH = pitchY - 3.5f;

		float curTime = (float)glfwGetTime();

		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				int s = r * 4 + c;
				float bx = c * pitchX + (pitchX - btnW) * 0.5f;
				float by = r * pitchY + (pitchY - btnH) * 0.5f;

				bool isEmpty = module ? !module->slots[s].hasData : true;
				bool isArmed = module ? (module->armedSlot == s) : false;
				bool isActive = module ? (module->activeSlot == s) : false;
				bool isHovered = (hoveredSlot == s);

				// Button background
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, bx, by, btnW, btnH, 2.5f);

				if (isArmed) {
					// Dusty rose pulsing (#d4a0a0 = 212, 160, 160)
					float pulse = 0.5f + 0.5f * std::sin(curTime * 9.0f);
					nvgFillColor(args.vg, nvgRGBAf(0.83f, 0.63f, 0.63f, 0.70f + 0.30f * pulse));
					nvgFill(args.vg);

					nvgStrokeWidth(args.vg, 1.5f);
					nvgStrokeColor(args.vg, nvgRGBAf(1.0f, 0.88f, 0.88f, 0.95f));
					nvgStroke(args.vg);
				} else if (isActive) {
					// Active playing: Bright Azure (#4ac2ff) with glow border
					nvgFillColor(args.vg, nvgRGBAf(0.29f, 0.76f, 1.0f, isHovered ? 1.0f : 0.90f));
					nvgFill(args.vg);

					nvgStrokeWidth(args.vg, 1.8f);
					nvgStrokeColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 1.0f));
					nvgStroke(args.vg);
				} else if (!isEmpty) {
					// Stored: Azure (#7ab8d4 = 122, 184, 212)
					nvgFillColor(args.vg, nvgRGBAf(0.48f, 0.72f, 0.83f, isHovered ? 0.95f : 0.80f));
					nvgFill(args.vg);

					nvgStrokeWidth(args.vg, 1.0f);
					nvgStrokeColor(args.vg, nvgRGBAf(0.75f, 0.90f, 0.98f, 0.75f));
					nvgStroke(args.vg);
				} else {
					// Empty: White / translucent
					nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, isHovered ? 0.28f : 0.16f));
					nvgFill(args.vg);

					nvgStrokeWidth(args.vg, 0.8f);
					nvgStrokeColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, isHovered ? 0.60f : 0.40f));
					nvgStroke(args.vg);
				}

				// Slot Number (1..16)
				char numStr[8];
				std::snprintf(numStr, sizeof(numStr), "%d", s + 1);
				nvgFontSize(args.vg, 9.0f);
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				if (isActive) {
					nvgFillColor(args.vg, nvgRGB(255, 255, 255));
				} else if (!isEmpty || isArmed) {
					nvgFillColor(args.vg, nvgRGB(240, 245, 250));
				} else {
					nvgFillColor(args.vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.50f));
				}
				nvgText(args.vg, bx + btnW * 0.5f, by + btnH * 0.5f, numStr, nullptr);
			}
		}

		nvgRestore(args.vg);
	}
};

// Context Menu Helper
static void appendReverieMenuItems(ui::Menu* menu, Reverie* module) {
	if (!module) return;

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("CV Scan Range"));

	const char* rangeLabels[] = {
		"+/-1V (-1V to +1V)",
		"0-1V (0V to +1V)",
		"+/-5V (-5V to +5V)",
		"0-5V (0V to +5V)",
		"+/-10V (-10V to +10V)",
		"0-10V (0V to +10V)"
	};

	for (int i = 0; i < Reverie::NUM_VOLTAGE_RANGES; i++) {
		Reverie::VoltageRange mode = (Reverie::VoltageRange)i;
		menu->addChild(createCheckMenuItem(rangeLabels[i], "",
			[=]() { return module ? module->scanRange == mode : false; },
			[=]() { if (module) module->scanRange = mode; }
		));
	}

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("Preset Management"));

	menu->addChild(createMenuItem("Clear Active Preset", "", [=]() {
		if (module && module->activeSlot >= 0) {
			module->slots[module->activeSlot].clear();
			module->activeSlot = -1;
		}
	}));

	menu->addChild(createMenuItem("Clear All Presets", "", [=]() {
		if (module) {
			for (int s = 0; s < Reverie::NUM_SLOTS; s++) {
				module->slots[s].clear();
			}
			module->activeSlot = -1;
		}
	}));
}

// Module Widget
struct ReverieWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Reverie::SkinTheme currentTheme = Reverie::THEME_MEDIUM;

	void setTheme(Reverie::SkinTheme theme) {
		currentTheme = theme;
		std::string path;
		switch (theme) {
			case Reverie::THEME_LIGHT:
				path = asset::plugin(pluginInstance, "res/Reverie_Light.svg");
				break;
			case Reverie::THEME_DARK:
				path = asset::plugin(pluginInstance, "res/Reverie_Dark.svg");
				break;
			case Reverie::THEME_MEDIUM:
			default:
				path = asset::plugin(pluginInstance, "res/Reverie.svg");
				break;
		}
		panel->setBackground(Svg::load(path));
	}

	void step() override {
		ModuleWidget::step();
		Reverie* module = dynamic_cast<Reverie*>(this->module);
		if (module && module->skinTheme != currentTheme) {
			setTheme(module->skinTheme);
		}
	}

	ReverieWidget(Reverie* module) {
		setModule(module);
		panel = new SvgPanel();
		Reverie::SkinTheme initTheme = module ? module->skinTheme : Reverie::THEME_MEDIUM;
		currentTheme = initTheme;
		std::string initPath;
		switch (initTheme) {
			case Reverie::THEME_LIGHT:
				initPath = asset::plugin(pluginInstance, "res/Reverie_Light.svg");
				break;
			case Reverie::THEME_DARK:
				initPath = asset::plugin(pluginInstance, "res/Reverie_Dark.svg");
				break;
			case Reverie::THEME_MEDIUM:
			default:
				initPath = asset::plugin(pluginInstance, "res/Reverie.svg");
				break;
		}
		panel->setBackground(Svg::load(initPath));
		setPanel(panel);

		// Zero virtual screws (Section 7.5.6)

		// ZONE 2: Primary Controls (Left Side)
		// Row 1 Knobs: FREQ (X = 16.0), FINE (X = 39.0), RANGE (X = 62.0)
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(16.000, 20.000)), module, Reverie::FREQ_PARAM));
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(39.000, 20.000)), module, Reverie::FINE_PARAM));
		addParam(createParamCentered<GarmireButton>(mm2px(Vec(62.000, 20.000)), module, Reverie::RANGE_PARAM));

		// Range LEDs
		addChild(createLightCentered<SmallLight<ReverieYellowLight>>(mm2px(Vec(58.500, 27.500)), module, Reverie::RANGE_LIGHT_YELLOW));
		addChild(createLightCentered<SmallLight<ReverieOrangeLight>>(mm2px(Vec(62.000, 27.500)), module, Reverie::RANGE_LIGHT_ORANGE));
		addChild(createLightCentered<SmallLight<ReveriePurpleLight>>(mm2px(Vec(65.500, 27.500)), module, Reverie::RANGE_LIGHT_PURPLE));

		// Row 2 Knobs: MORPH (X = 22.0), CYCLES (X = 52.0)
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(22.000, 42.000)), module, Reverie::MORPH_PARAM));
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(52.000, 42.000)), module, Reverie::CYCLES_PARAM));

		// ZONE 3: CV Attenuverters (Center Y = 66.0 mm)
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(12.000, 66.000)), module, Reverie::FREQ_TRIM_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(28.373, 66.000)), module, Reverie::MORPH_TRIM_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(49.747, 66.000)), module, Reverie::SCAN_X_TRIM_PARAM));
		addParam(createParamCentered<GarmireTrimpot>(mm2px(Vec(71.120, 66.000)), module, Reverie::SCAN_Y_TRIM_PARAM));

		// ZONE 4: 4x4 Preset Grid Widget (Centered at 108.0 mm, 46.0 mm; W = 54 mm, H = 54 mm)
		PresetGridWidget* gridWidget = new PresetGridWidget();
		gridWidget->module = module;
		gridWidget->box.pos = mm2px(Vec(108.000 - 27.000, 46.000 - 27.000));
		addChild(gridWidget);

		// ZONE 5: I/O Ports & Jacks
		// Row 3: CV Inputs (Center Y = 91.0 mm)
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(12.000, 91.000)), module, Reverie::FREQ_CV_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(28.373, 91.000)), module, Reverie::MORPH_CV_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(49.747, 91.000)), module, Reverie::SCAN_X_CV_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(71.120, 91.000)), module, Reverie::SCAN_Y_CV_INPUT));

		// Row 2: Signal Inputs (Center Y = 104.0 mm)
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(7.000, 104.000)), module, Reverie::X_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(28.373, 104.000)), module, Reverie::Y_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(49.747, 104.000)), module, Reverie::R_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(71.120, 104.000)), module, Reverie::G_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(92.493, 104.000)), module, Reverie::B_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(113.867, 104.000)), module, Reverie::ZI_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(135.240, 104.000)), module, Reverie::SYNC_INPUT));

		// Row 1: Signal Outputs (Center Y = 117.0 mm)
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(7.000, 117.000)), module, Reverie::X_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(28.373, 117.000)), module, Reverie::Y_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(49.747, 117.000)), module, Reverie::R_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(71.120, 117.000)), module, Reverie::G_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(92.493, 117.000)), module, Reverie::B_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(113.867, 117.000)), module, Reverie::ZI_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(135.240, 117.000)), module, Reverie::SYNC_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Reverie* module = dynamic_cast<Reverie*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Theme"));

		menu->addChild(createCheckMenuItem("Medium Skin", "",
			[=]() { return module->skinTheme == Reverie::THEME_MEDIUM; },
			[=]() { module->skinTheme = Reverie::THEME_MEDIUM; }
		));
		menu->addChild(createCheckMenuItem("Light Skin", "",
			[=]() { return module->skinTheme == Reverie::THEME_LIGHT; },
			[=]() { module->skinTheme = Reverie::THEME_LIGHT; }
		));
		menu->addChild(createCheckMenuItem("Dark Skin", "",
			[=]() { return module->skinTheme == Reverie::THEME_DARK; },
			[=]() { module->skinTheme = Reverie::THEME_DARK; }
		));

		appendReverieMenuItems(menu, module);
	}
};

Model* modelReverie = createModel<Reverie, ReverieWidget>("Reverie");
