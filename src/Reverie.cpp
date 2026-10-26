#include "plugin.hpp"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>

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
		RANGE_PARAM,
		FINE_PARAM,
		CYCLES_PARAM,
		MORPH_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		SYNC_INPUT,
		X_INPUT,
		Y_INPUT,
		FREQ_CV_INPUT,

		R_INPUT,
		G_INPUT,
		B_INPUT,
		MORPH_CV_INPUT,

		ZI_INPUT,
		SCAN_X_CV_INPUT,
		SCAN_Y_CV_INPUT,
		RECORD_INPUT,

		ENUMS(PRESET_GATE_INPUT, 16),
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
		int numCycles = 16;
		int totalSamples = 0;
		float baseFreq = 60.0f;
		std::vector<float> channels[NUM_CHANNELS];

		PresetSlot() {
			for (int c = 0; c < NUM_CHANNELS; c++) {
				channels[c].resize(SAMPLES_PER_CYCLE, 0.0f);
			}
		}

		void clear() {
			hasData = false;
			numCycles = 16;
			totalSamples = 0;
			baseFreq = 60.0f;
			for (int c = 0; c < NUM_CHANNELS; c++) {
				std::fill(channels[c].begin(), channels[c].end(), 0.0f);
			}
		}

		// Reads sample at currentCyclePhase where 1.0 unit of currentCyclePhase = 1 single recorded cycle
		float readSample(int channel, double currentCyclePhase) const {
			if (!hasData || totalSamples <= 0 || numCycles <= 0 || channel < 0 || channel >= NUM_CHANNELS) {
				return 0.0f;
			}
			const std::vector<float>& buf = channels[channel];
			if ((int)buf.size() < totalSamples) return 0.0f;

			double cycleInSlot = std::fmod(currentCyclePhase, (double)numCycles);
			if (cycleInSlot < 0.0) cycleInSlot += (double)numCycles;

			double pos = cycleInSlot * (double)SAMPLES_PER_CYCLE;
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
	int targetCycles = 16;
	int completedCycles = 0;
	float timeSinceSync = 0.0f;
	float lastMeasuredPeriod = 1.0f / 60.0f;

	// Scratch capture buffer for current cycle (pre-allocated)
	static constexpr int MAX_CYCLE_RAW_SAMPLES = 192000;
	std::vector<float> rawCycleBuffer[NUM_CHANNELS];
	int rawSampleCount = 0;

	// Selection & Playback state
	int selectedSlot = 0;   // Currently highlighted slot in the 4x4 grid (can be empty for live preview or stored)
	int activeSlot = -1;    // Currently playing stored preset (-1 if previewing an empty slot)
	int morphFromSlot = -1; // -1 means morphing from live input pass-through
	int morphToSlot = -1;
	float morphProgress = 1.0f;
	double cyclePhase = 0.0; // 1.0 = 1 single cycle (wraps modulo 384 = LCM of all cycle counts)

	dsp::BooleanTrigger rangeTrigger;
	dsp::SchmittTrigger syncInTrigger;
	dsp::SchmittTrigger recordGateTrigger;
	dsp::SchmittTrigger presetGateTriggers[NUM_SLOTS];
	dsp::PulseGenerator syncOutPulse;

	// Standardized FreqParamQuantity matching Lisa / Daisy / Circe
	struct FreqParamQuantity : ParamQuantity {
		Reverie* getReverie() {
			return dynamic_cast<Reverie*>(module);
		}

		float getDisplayValue() override {
			Reverie* rev = getReverie();
			if (!rev) return ParamQuantity::getDisplayValue();

			float coarse = getValue();
			float fine = (rev->paramQuantities.size() > Reverie::FINE_PARAM) ? rev->params[Reverie::FINE_PARAM].getValue() : 0.f;

			if (rev->rangeMode == Reverie::RANGE_VERY_SLOW) {
				// Period T in seconds: 600.0s to 0.1s
				float tCoarse = 600.0f * std::pow(0.1f / 600.0f, coarse);
				return clampf(tCoarse * (1.0f - fine * 0.10f), 0.05f, 1000.0f);
			} else if (rev->rangeMode == Reverie::RANGE_LFO) {
				// Frequency in Hz: 0.01 to 200.00 Hz
				float fCoarse = 0.01f + 199.99f * coarse * coarse;
				return clampf(fCoarse + fine * 20.0f, 0.01f, 250.0f);
			} else {
				// Frequency in Hz: 150.00 to 2000.00 Hz
				float fCoarse = 150.0f * std::pow(2000.0f / 150.0f, coarse);
				return clampf(fCoarse * (1.0f + fine * 0.10f), 100.0f, 2500.0f);
			}
		}

		std::string getDisplayValueString() override {
			Reverie* rev = getReverie();
			if (!rev) return ParamQuantity::getDisplayValueString();

			float val = getDisplayValue();
			char buf[32];
			if (rev->rangeMode == Reverie::RANGE_VERY_SLOW) {
				std::snprintf(buf, sizeof(buf), "%.1f", val);
			} else {
				std::snprintf(buf, sizeof(buf), "%.2f", val);
			}
			return std::string(buf);
		}

		void setFrequencyValue(float rawVal, bool isPeriod) {
			Reverie* rev = getReverie();
			if (!rev) return;

			float fine = (rev->paramQuantities.size() > Reverie::FINE_PARAM) ? rev->params[Reverie::FINE_PARAM].getValue() : 0.f;
			float newCoarse = getValue();

			if (rev->rangeMode == Reverie::RANGE_VERY_SLOW) {
				float targetPeriodS = isPeriod ? rawVal : (rawVal > 1e-6f ? 1.0f / rawVal : 1e6f);
				if (targetPeriodS <= 1e-5f) targetPeriodS = 1e-5f;

				float fineFactor = 1.0f - fine * 0.10f;
				if (std::abs(fineFactor) < 1e-4f) fineFactor = 1e-4f;
				float tCoarse = targetPeriodS / fineFactor;
				if (tCoarse <= 1e-6f) tCoarse = 1e-6f;

				float ratio = tCoarse / 600.0f;
				if (ratio <= 0.f) ratio = 1e-6f;
				newCoarse = std::log(ratio) / std::log(0.1f / 600.0f);
			} else if (rev->rangeMode == Reverie::RANGE_LFO) {
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
			Reverie* rev = getReverie();
			if (!rev) {
				ParamQuantity::setDisplayValue(displayValue);
				return;
			}
			bool isPeriod = (rev->rangeMode == Reverie::RANGE_VERY_SLOW);
			setFrequencyValue(displayValue, isPeriod);
		}

		void setDisplayValueString(std::string s) override {
			Reverie* rev = getReverie();
			if (!rev) {
				ParamQuantity::setDisplayValueString(s);
				return;
			}

			std::string str = s;
			while (!str.empty() && std::isspace((unsigned char)str.front())) {
				str.erase(str.begin());
			}
			while (!str.empty() && std::isspace((unsigned char)str.back())) {
				str.pop_back();
			}
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

			while (!str.empty() && std::isspace((unsigned char)str.back())) {
				str.pop_back();
			}

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
				isPeriod = (rev->rangeMode == Reverie::RANGE_VERY_SLOW);
				finalVal = rawVal;
			}

			setFrequencyValue(finalVal, isPeriod);
		}

		std::string getUnit() override {
			Reverie* rev = getReverie();
			if (!rev) return "";
			return (rev->rangeMode == Reverie::RANGE_VERY_SLOW) ? " s" : " Hz";
		}
	};

	struct CyclesParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int idx = (int)std::round(getValue());
			idx = std::max(0, std::min(6, idx));
			return std::to_string(CYCLE_OPTIONS[idx]) + (CYCLE_OPTIONS[idx] == 1 ? " cycle" : " cycles");
		}

		void setDisplayValueString(std::string s) override {
			char* endPtr = nullptr;
			float val = std::strtof(s.c_str(), &endPtr);
			if (endPtr != s.c_str()) {
				int target = (int)std::round(val);
				int bestIdx = 2;
				int bestDiff = 999999;
				for (int i = 0; i < 7; i++) {
					int d = std::abs(CYCLE_OPTIONS[i] - target);
					if (d < bestDiff) {
						bestDiff = d;
						bestIdx = i;
					}
				}
				setValue((float)bestIdx);
				return;
			}
			ParamQuantity::setDisplayValueString(s);
		}
	};

	Reverie() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Standardized Frequency Triple Controls (matching Lisa / Daisy / Circe)
		configParam<FreqParamQuantity>(FREQ_PARAM, 0.f, 1.f, 0.5477f, "Frequency", "");
		configButton(RANGE_PARAM, "Range time-scale");
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", "%", 0.f, 10.f);

		// Cycles (default = index 2 -> 16 cycles) & Morph Time
		configParam<CyclesParamQuantity>(CYCLES_PARAM, 0.f, 6.f, 2.f, "Cycles", "");
		paramQuantities[CYCLES_PARAM]->snapEnabled = true;
		configParam(MORPH_PARAM, 0.f, 10.f, 0.5f, "Morph time", " s");

		// Control & Signal Inputs (no redundant "input"/"in" words per AGENTS.md Section 7.5.1)
		configInput(SYNC_INPUT, "Sync");
		configInput(X_INPUT, "X");
		configInput(Y_INPUT, "Y");
		configInput(FREQ_CV_INPUT, "Frequency CV");

		configInput(R_INPUT, "R");
		configInput(G_INPUT, "G");
		configInput(B_INPUT, "B");
		configInput(MORPH_CV_INPUT, "Morph time CV");

		configInput(ZI_INPUT, "Z/I");
		configInput(SCAN_X_CV_INPUT, "X scan CV");
		configInput(SCAN_Y_CV_INPUT, "Y scan CV");
		configInput(RECORD_INPUT, "Record gate");

		for (int i = 0; i < NUM_SLOTS; i++) {
			configInput(PRESET_GATE_INPUT + i, "Preset " + std::to_string(i + 1) + " gate");
		}

		// Signal & Sync Outputs (no redundant "output"/"out" words per AGENTS.md Section 7.5.1)
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
		selectedSlot = 0;
		activeSlot = -1;
		morphFromSlot = -1;
		morphToSlot = -1;
		morphProgress = 1.0f;
		recState = REC_IDLE;
		armedSlot = -1;
		cyclePhase = 0.0;
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

	float getEffectiveMorphTime() {
		float morphSec = params[MORPH_PARAM].getValue();
		if (inputs[MORPH_CV_INPUT].isConnected()) {
			morphSec += inputs[MORPH_CV_INPUT].getVoltage();
		}
		return clampf(morphSec, 0.0f, 10.0f);
	}

	// Select an empty slot for live pass-through preview without recording
	void selectEmptySlotForPreview(int slotIndex) {
		if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
		if (recState == REC_WAIT_SYNC) {
			armedSlot = -1;
			recState = REC_IDLE;
		}
		selectedSlot = slotIndex;
		activeSlot = -1;
		morphFromSlot = -1;
		morphToSlot = -1;
		morphProgress = 1.0f;
	}

	// Arm a slot to record N cycles starting on the next Sync In rising edge
	void armSlot(int slotIndex) {
		if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
		int cycleOptIdx = (int)std::round(params[CYCLES_PARAM].getValue());
		cycleOptIdx = std::max(0, std::min(6, cycleOptIdx));
		targetCycles = CYCLE_OPTIONS[cycleOptIdx];
		completedCycles = 0;
		rawSampleCount = 0;

		selectedSlot = slotIndex;
		armedSlot = slotIndex;
		recState = REC_WAIT_SYNC;
	}

	// Clear and arm a slot for overwrite recording
	void overwriteSlot(int slotIndex) {
		if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
		slots[slotIndex].clear();
		if (activeSlot == slotIndex) {
			activeSlot = -1;
		}
		armSlot(slotIndex);
	}

	// Select a stored preset slot and initiate morphing
	void selectStoredSlot(int slotIndex) {
		if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
		if (!slots[slotIndex].hasData) {
			selectEmptySlotForPreview(slotIndex);
			return;
		}

		selectedSlot = slotIndex;
		if (activeSlot == slotIndex && morphProgress >= 1.0f) return;

		float morphSec = getEffectiveMorphTime();
		if (morphSec <= 0.001f) {
			activeSlot = slotIndex;
			morphFromSlot = slotIndex;
			morphToSlot = slotIndex;
			morphProgress = 1.0f;
		} else {
			// If activeSlot < 0 (was previewing an empty slot), morphFromSlot = -1 morphs smoothly from live input!
			morphFromSlot = (activeSlot >= 0 && slots[activeSlot].hasData) ? activeSlot : -1;
			morphToSlot = slotIndex;
			activeSlot = slotIndex;
			morphProgress = 0.0f;
		}
	}

	// Record gate input handler: records into the next free slot (if all slots full, nothing overwritten)
	void triggerRecordNextFreeSlot() {
		if (recState != REC_IDLE) return;
		int freeSlot = -1;
		// Check if currently selected slot is empty first; otherwise scan 0..15 for the first free slot
		for (int i = 0; i < NUM_SLOTS; i++) {
			if (!slots[i].hasData) {
				freeSlot = i;
				break;
			}
		}
		if (freeSlot >= 0) {
			armSlot(freeSlot);
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
		// 1. Handle Range Button cycling (Very Slow -> LFO -> VCO)
		if (rangeTrigger.process(params[RANGE_PARAM].getValue() > 0.5f)) {
			rangeMode = (RangeMode)((rangeMode + 1) % 3);
		}

		// Update 3-Color Range LED Ring (Warm Gold = Very Slow, Teal = LFO, Magenta = VCO)
		lights[RANGE_LIGHT_YELLOW].setBrightness(rangeMode == RANGE_VERY_SLOW ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_ORANGE].setBrightness(rangeMode == RANGE_LFO ? 1.0f : 0.0f);
		lights[RANGE_LIGHT_PURPLE].setBrightness(rangeMode == RANGE_VCO ? 1.0f : 0.0f);

		// 2. Check Record Gate Input (records into next free slot, never overwrites if full)
		if (inputs[RECORD_INPUT].isConnected()) {
			if (recordGateTrigger.process(inputs[RECORD_INPUT].getVoltage(), 0.1f, 2.0f)) {
				triggerRecordNextFreeSlot();
			}
		}

		// 3. Check 4x4 Preset Gate Input Grid
		for (int s = 0; s < NUM_SLOTS; s++) {
			if (inputs[PRESET_GATE_INPUT + s].isConnected()) {
				if (presetGateTriggers[s].process(inputs[PRESET_GATE_INPUT + s].getVoltage(), 0.1f, 2.0f)) {
					if (slots[s].hasData) {
						selectStoredSlot(s);
					} else {
						selectEmptySlotForPreview(s);
					}
				}
			}
		}

		// 4. Sync In edge detection & period measurement
		timeSinceSync += args.sampleTime;
		bool syncInPulse = false;
		if (inputs[SYNC_INPUT].isConnected()) {
			if (syncInTrigger.process(inputs[SYNC_INPUT].getVoltage(), 0.1f, 2.0f)) {
				if (timeSinceSync > 0.0002f) {
					lastMeasuredPeriod = timeSinceSync;
				}
				timeSinceSync = 0.0f;
				syncInPulse = true;
			}
		}

		// Read current live inputs (unpatched channels default to 0.0V)
		float liveIn[NUM_CHANNELS] = {
			inputs[X_INPUT].isConnected()  ? inputs[X_INPUT].getVoltage()  : 0.0f,
			inputs[Y_INPUT].isConnected()  ? inputs[Y_INPUT].getVoltage()  : 0.0f,
			inputs[R_INPUT].isConnected()  ? inputs[R_INPUT].getVoltage()  : 0.0f,
			inputs[G_INPUT].isConnected()  ? inputs[G_INPUT].getVoltage()  : 0.0f,
			inputs[B_INPUT].isConnected()  ? inputs[B_INPUT].getVoltage()  : 0.0f,
			inputs[ZI_INPUT].isConnected() ? inputs[ZI_INPUT].getVoltage() : 0.0f
		};

		// 5. Sync-Locked Recording State Machine
		if (recState == REC_WAIT_SYNC) {
			if (syncInPulse) {
				recState = REC_CAPTURING;
				completedCycles = 0;
				rawSampleCount = 0;
				// Capture first sample immediately at the sync edge
				for (int c = 0; c < NUM_CHANNELS; c++) {
					rawCycleBuffer[c][0] = liveIn[c];
				}
				rawSampleCount = 1;
			}
		} else if (recState == REC_CAPTURING) {
			if (syncInPulse && rawSampleCount > 2) {
				finalizeCycle(completedCycles);
				completedCycles++;
				rawSampleCount = 0;

				if (completedCycles >= targetCycles) {
					PresetSlot& slot = slots[armedSlot];
					slot.hasData = true;
					slot.numCycles = targetCycles;
					slot.totalSamples = targetCycles * SAMPLES_PER_CYCLE;
					slot.baseFreq = (lastMeasuredPeriod > 1e-5f) ? (1.0f / lastMeasuredPeriod) : 60.0f;

					selectedSlot = armedSlot;
					activeSlot = armedSlot;
					morphFromSlot = armedSlot;
					morphToSlot = armedSlot;
					morphProgress = 1.0f;
					cyclePhase = 0.0;

					armedSlot = -1;
					recState = REC_IDLE;
				}
			}

			if (recState == REC_CAPTURING) {
				if (rawSampleCount < MAX_CYCLE_RAW_SAMPLES) {
					for (int c = 0; c < NUM_CHANNELS; c++) {
						rawCycleBuffer[c][rawSampleCount] = liveIn[c];
					}
					rawSampleCount++;
				}
			}
		}

		// 6. Pass-through during recording or when an empty slot is selected for preview
		if (recState == REC_CAPTURING) {
			// Sync Out is silent while capturing
			outputs[SYNC_OUTPUT].setVoltage(0.0f);
			for (int c = 0; c < NUM_CHANNELS; c++) {
				outputs[X_OUTPUT + c].setVoltage(liveIn[c]);
			}
			return;
		}

		bool scanConnected = inputs[SCAN_X_CV_INPUT].isConnected() || inputs[SCAN_Y_CV_INPUT].isConnected();

		if (!scanConnected && (activeSlot < 0 || !slots[activeSlot].hasData)) {
			// Previewing an empty slot (or no preset active): live pass-through
			for (int c = 0; c < NUM_CHANNELS; c++) {
				outputs[X_OUTPUT + c].setVoltage(liveIn[c]);
			}
			outputs[SYNC_OUTPUT].setVoltage(inputs[SYNC_INPUT].getVoltage());
			return;
		}

		// 7. Playback Frequency Calculation (1 cycle per 1/fPlay seconds, matching external generator rate 1:1)
		float coarseParam = params[FREQ_PARAM].getValue();
		float fineParam = params[FINE_PARAM].getValue();
		float defaultF0 = calculateBaseFrequency(coarseParam, fineParam);

		float fPlay = defaultF0;
		if (inputs[FREQ_CV_INPUT].isConnected()) {
			fPlay *= std::pow(2.0f, inputs[FREQ_CV_INPUT].getVoltage());
		}
		fPlay = clampf(fPlay, 0.0001f, 20000.0f);

		// Advance cyclePhase (1.0 unit = 1 single recorded cycle)
		double prevCyclePhase = cyclePhase;
		cyclePhase += (double)fPlay * (double)args.sampleTime;
		if (std::floor(cyclePhase) > std::floor(prevCyclePhase)) {
			syncOutPulse.trigger(1e-4f);
		}
		// Wrap modulo 384.0 (exact LCM of 1, 8, 16, 24, 48, 96, 128)
		if (cyclePhase >= 384.0) {
			cyclePhase -= 384.0 * std::floor(cyclePhase / 384.0);
		}
		outputs[SYNC_OUTPUT].setVoltage(syncOutPulse.process(args.sampleTime) ? 10.0f : 0.0f);

		// 8. Morph Progression
		if (morphProgress < 1.0f) {
			float morphSec = std::max(0.001f, getEffectiveMorphTime());
			morphProgress += args.sampleTime / morphSec;
			if (morphProgress >= 1.0f) {
				morphProgress = 1.0f;
				activeSlot = morphToSlot;
				morphFromSlot = morphToSlot;
			}
		}

		// 9. Output Waveform Evaluation (2D CV Scan or Timed Preset Morph)
		if (scanConnected) {
			// Manual 2D CV Scan across the 4x4 grid (morph time ignored)
			float vMin = 0.0f, vMax = 10.0f;
			getRangeVoltages(vMin, vMax);
			float vRange = vMax - vMin;
			if (vRange <= 1e-4f) vRange = 1.0f;

			float scanX = 0.0f;
			if (inputs[SCAN_X_CV_INPUT].isConnected()) {
				float vx = inputs[SCAN_X_CV_INPUT].getVoltage();
				scanX = clampf((vx - vMin) / vRange, 0.0f, 1.0f) * 3.0f;
			}
			float scanY = 0.0f;
			if (inputs[SCAN_Y_CV_INPUT].isConnected()) {
				float vy = inputs[SCAN_Y_CV_INPUT].getVoltage();
				scanY = clampf((vy - vMin) / vRange, 0.0f, 1.0f) * 3.0f;
			}

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

			// Update selectedSlot to reflect nearest scanned cell
			int nearestCol = (int)std::round(scanX);
			int nearestRow = (int)std::round(scanY);
			selectedSlot = clamp(nearestRow * 4 + nearestCol, 0, 15);

			for (int c = 0; c < NUM_CHANNELS; c++) {
				float v00 = slots[s00].hasData ? slots[s00].readSample(c, cyclePhase) : liveIn[c];
				float v10 = slots[s10].hasData ? slots[s10].readSample(c, cyclePhase) : liveIn[c];
				float v01 = slots[s01].hasData ? slots[s01].readSample(c, cyclePhase) : liveIn[c];
				float v11 = slots[s11].hasData ? slots[s11].readSample(c, cyclePhase) : liveIn[c];

				float top = (1.0f - fracX) * v00 + fracX * v10;
				float bot = (1.0f - fracX) * v01 + fracX * v11;
				float outVal = (1.0f - fracY) * top + fracY * bot;

				outputs[X_OUTPUT + c].setVoltage(clampf(outVal, -12.0f, 12.0f));
			}
		} else {
			for (int c = 0; c < NUM_CHANNELS; c++) {
				float outVal = 0.0f;
				if (morphProgress >= 1.0f || morphToSlot < 0 || !slots[morphToSlot].hasData) {
					outVal = slots[activeSlot].readSample(c, cyclePhase);
				} else {
					float vFrom = (morphFromSlot >= 0 && slots[morphFromSlot].hasData)
						? slots[morphFromSlot].readSample(c, cyclePhase)
						: liveIn[c];
					float vTo = slots[morphToSlot].readSample(c, cyclePhase);
					outVal = (1.0f - morphProgress) * vFrom + morphProgress * vTo;
				}
				outputs[X_OUTPUT + c].setVoltage(clampf(outVal, -12.0f, 12.0f));
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "skinTheme", json_integer((int)skinTheme));
		json_object_set_new(rootJ, "rangeMode", json_integer((int)rangeMode));
		json_object_set_new(rootJ, "scanRange", json_integer((int)scanRange));
		json_object_set_new(rootJ, "selectedSlot", json_integer(selectedSlot));
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

		json_t* rmJ = json_object_get(rootJ, "rangeMode");
		if (rmJ) rangeMode = (RangeMode)json_integer_value(rmJ);

		json_t* rangeJ = json_object_get(rootJ, "scanRange");
		if (rangeJ) scanRange = (VoltageRange)json_integer_value(rangeJ);

		json_t* selJ = json_object_get(rootJ, "selectedSlot");
		if (selJ) selectedSlot = (int)json_integer_value(selJ);

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

// ── Custom Garmire Hardware Widgets ──
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

struct GarmireButton : app::SvgSwitch {
	GarmireButton() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/button_gray_1.svg")));
	}
};

// ── Standardized Concentric 3-Color Range Light Ring (matching Lisa / Daisy / Circe / Polly) ──
struct ReverieRangeLightRing : ModuleLightWidget {
	ReverieRangeLightRing() {
		box.size = mm2px(Vec(11.0f, 11.0f));
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
		haloColor.a *= 0.175f;

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r);
		nvgStrokeColor(args.vg, haloColor);
		nvgStrokeWidth(args.vg, mm2px(1.40f) + haloW);
		nvgStroke(args.vg);
	}
};

// ── 4x4 Preset Matrix Widget using res/preset-[empty|recording|stored|selected].png + Number Overlay ──
struct PresetGridWidget : OpaqueWidget {
	Reverie* module = nullptr;
	int pressedSlot = -1;
	double pressStartTime = 0.0;
	bool longPressTriggered = false;

	PresetGridWidget() {
		// 4 columns x 4 rows at 12.50 mm pitch (50.0 mm x 50.0 mm total box)
		box.size = mm2px(Vec(50.0f, 50.0f));
	}

	int getSlotAtPos(Vec pos) const {
		float pitchX = box.size.x / 4.0f;
		float pitchY = box.size.y / 4.0f;
		int col = clamp((int)std::floor(pos.x / pitchX), 0, 3);
		int row = clamp((int)std::floor(pos.y / pitchY), 0, 3);
		return row * 4 + col;
	}

	void step() override {
		OpaqueWidget::step();
		if (module && pressedSlot >= 0 && !longPressTriggered) {
			double elapsed = glfwGetTime() - pressStartTime;
			if (elapsed >= 0.45) {
				longPressTriggered = true;
				// Long-hold clears and arms slot for overwrite recording
				module->overwriteSlot(pressedSlot);
			}
		}
	}

	void onButton(const ButtonEvent& e) override {
		if (!module) {
			OpaqueWidget::onButton(e);
			return;
		}

		int slotIdx = getSlotAtPos(e.pos);

		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (e.action == GLFW_PRESS) {
				pressedSlot = slotIdx;
				pressStartTime = glfwGetTime();
				longPressTriggered = false;
				e.consume(this);
				return;
			} else if (e.action == GLFW_RELEASE) {
				if (pressedSlot >= 0 && !longPressTriggered) {
					int s = pressedSlot;
					if (e.mods & GLFW_MOD_SHIFT) {
						module->overwriteSlot(s);
					} else if (module->slots[s].hasData) {
						// Click a recorded slot to morph to that one
						module->selectStoredSlot(s);
					} else {
						// Empty slot workflow:
						// 1st click selects empty slot for preview without recording;
						// 2nd click on the same selected empty slot arms it to record.
						if (module->selectedSlot != s) {
							module->selectEmptySlotForPreview(s);
						} else {
							if (module->armedSlot == s) {
								module->armedSlot = -1;
								module->recState = Reverie::REC_IDLE;
							} else {
								module->armSlot(s);
							}
						}
					}
				}
				pressedSlot = -1;
				longPressTriggered = false;
				e.consume(this);
				return;
			}
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			int row = slotIdx / 4;
			int col = slotIdx % 4;
			ui::Menu* menu = createMenu();
			char titleBuf[64];
			std::snprintf(titleBuf, sizeof(titleBuf), "Preset Slot %d (Row %d, Col %d)", slotIdx + 1, row + 1, col + 1);
			menu->addChild(createMenuLabel(titleBuf));
			menu->addChild(new MenuSeparator());

			menu->addChild(createMenuItem("Select Slot (Preview / Morph)", "", [=]() {
				if (!module) return;
				if (module->slots[slotIdx].hasData) {
					module->selectStoredSlot(slotIdx);
				} else {
					module->selectEmptySlotForPreview(slotIdx);
				}
			}));

			menu->addChild(createMenuItem(module->slots[slotIdx].hasData ? "Overwrite / Re-Record Slot" : "Record Slot", "Hold 0.5s", [=]() {
				if (module) module->overwriteSlot(slotIdx);
			}));

			if (module->slots[slotIdx].hasData) {
				menu->addChild(createMenuItem("Clear Slot", "", [=]() {
					if (module) {
						module->slots[slotIdx].clear();
						if (module->activeSlot == slotIdx) {
							module->activeSlot = -1;
						}
					}
				}));
			}

			e.consume(this);
			return;
		}

		OpaqueWidget::onButton(e);
	}

	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);

		float pitchX = box.size.x / 4.0f; // 12.50 mm in px
		float pitchY = box.size.y / 4.0f; // 12.50 mm in px
		float btnW = mm2px(10.0f);
		float btnH = mm2px(10.0f);

		float curTime = (float)glfwGetTime();

		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				int s = r * 4 + c;
				float bx = c * pitchX + (pitchX - btnW) * 0.5f;
				float by = r * pitchY + (pitchY - btnH) * 0.5f;

				bool hasData = module ? module->slots[s].hasData : false;
				bool isArmed = module ? (module->armedSlot == s) : false;
				bool isSelected = module ? (module->selectedSlot == s) : (s == 0);
				bool isActiveStored = module ? (module->activeSlot == s && hasData) : false;

				const char* assetName = "res/preset-empty.png";
				NVGcolor edgeCol = nvgRGB(169, 169, 169);
				NVGcolor rimCol  = nvgRGB(234, 234, 234);
				NVGcolor capCol  = nvgRGB(255, 255, 255);
				float alpha = 1.0f;

				if (isArmed) {
					assetName = "res/preset-recording.png";
					edgeCol = nvgRGB(185, 121, 121);
					rimCol  = nvgRGB(212, 160, 160);
					capCol  = nvgRGB(229, 193, 193);
					alpha = 0.78f + 0.22f * (0.5f + 0.5f * std::sin(curTime * 8.0f));
				} else if (hasData) {
					assetName = "res/preset-stored.png";
					edgeCol = nvgRGB(118, 142, 185);
					rimCol  = nvgRGB(157, 177, 212);
					capCol  = nvgRGB(190, 204, 229);
				} else if (isSelected) {
					assetName = "res/preset-selected.png";
					edgeCol = nvgRGB(145, 145, 145);
					rimCol  = nvgRGB(214, 214, 214);
					capCol  = nvgRGB(244, 244, 244);
				}

				bool drewPng = false;
				if (APP && APP->window) {
					std::shared_ptr<window::Image> img = APP->window->loadImage(asset::plugin(pluginInstance, assetName));
					if (img && img->handle >= 0) {
						NVGpaint paint = nvgImagePattern(args.vg, bx, by, btnW, btnH, 0.0f, img->handle, alpha);
						nvgBeginPath(args.vg);
						nvgRect(args.vg, bx, by, btnW, btnH);
						nvgFillPaint(args.vg, paint);
						nvgFill(args.vg);
						drewPng = true;
					}
				}

				if (!drewPng) {
					// Fallback vector match if GL window image cache is unavailable
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, bx, by, btnW, btnH, mm2px(1.6f));
					nvgFillColor(args.vg, rimCol);
					nvgFill(args.vg);
					nvgStrokeWidth(args.vg, 1.0f);
					nvgStrokeColor(args.vg, edgeCol);
					nvgStroke(args.vg);

					float inset = mm2px(0.85f);
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, bx + inset, by + inset, btnW - 2.0f * inset, btnH - 2.0f * inset, mm2px(1.1f));
					nvgFillColor(args.vg, capCol);
					nvgFill(args.vg);
				}

				// Highlight border when a stored slot is currently active/selected
				if (isActiveStored && !isArmed) {
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, bx + 0.5f, by + 0.5f, btnW - 1.0f, btnH - 1.0f, mm2px(1.5f));
					nvgStrokeWidth(args.vg, 1.6f);
					nvgStrokeColor(args.vg, nvgRGBA(55, 95, 160, 255));
					nvgStroke(args.vg);
				}

				// Number Overlay (1..16)
				char numStr[8];
				std::snprintf(numStr, sizeof(numStr), "%d", s + 1);
				nvgFontSize(args.vg, 10.5f);
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				if (isSelected || isActiveStored) {
					nvgFillColor(args.vg, nvgRGB(20, 20, 20));
				} else {
					nvgFillColor(args.vg, nvgRGB(70, 70, 70));
				}
				nvgText(args.vg, bx + btnW * 0.5f, by + btnH * 0.5f, numStr, nullptr);
			}
		}

		nvgRestore(args.vg);
	}
};

// ── Context Menu Helper ──
static void appendReverieMenuItems(ui::Menu* menu, Reverie* module) {
	if (!module) return;

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("Oscillator Range"));

	const char* oscRangeLabels[] = {
		"Very Slow (600s - 0.1s)",
		"LFO (0.01 - 200 Hz)",
		"VCO (150 Hz - 2 kHz)"
	};

	for (int i = 0; i < 3; i++) {
		Reverie::RangeMode mode = (Reverie::RangeMode)i;
		menu->addChild(createCheckMenuItem(oscRangeLabels[i], "",
			[=]() { return module ? module->rangeMode == mode : false; },
			[=]() { if (module) module->rangeMode = mode; }
		));
	}

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("CV Scan Range"));

	const char* scanRangeLabels[] = {
		"+/-1V (-1V to +1V)",
		"0-1V (0V to +1V)",
		"+/-5V (-5V to +5V)",
		"0-5V (0V to +5V)",
		"+/-10V (-10V to +10V)",
		"0-10V (0V to +10V)"
	};

	for (int i = 0; i < Reverie::NUM_VOLTAGE_RANGES; i++) {
		Reverie::VoltageRange mode = (Reverie::VoltageRange)i;
		menu->addChild(createCheckMenuItem(scanRangeLabels[i], "",
			[=]() { return module ? module->scanRange == mode : false; },
			[=]() { if (module) module->scanRange = mode; }
		));
	}

	menu->addChild(new MenuSeparator());
	menu->addChild(createMenuLabel("Preset Management"));

	menu->addChild(createMenuItem("Record / Overwrite Selected Slot", "", [=]() {
		if (module && module->selectedSlot >= 0) {
			module->overwriteSlot(module->selectedSlot);
		}
	}));

	menu->addChild(createMenuItem("Clear Selected Slot", "", [=]() {
		if (module && module->selectedSlot >= 0) {
			module->slots[module->selectedSlot].clear();
			if (module->activeSlot == module->selectedSlot) {
				module->activeSlot = -1;
			}
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

// ── Module Widget (20 HP = 8 HP Controls + 12 HP 4x4 Matrix & Gate Grid) ──
struct ReverieWidget : ModuleWidget {
	SvgPanel* panel = nullptr;
	Reverie::SkinTheme currentTheme = Reverie::THEME_MEDIUM;

	void setTheme(Reverie::SkinTheme theme) {
		currentTheme = theme;
		if (!panel) return;
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

		// Zero virtual screws (AGENTS.md Section 7.5.6)

		// ── LEFT 8 HP SECTION: Primary Parameters ──
		// Row 1 (Y = 21.00 mm): FREQ (10.82), RANGE Button + Light Ring (20.32), FINE (29.82)
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(10.820f, 21.000f)), module, Reverie::FREQ_PARAM));
		addChild(createLightCentered<ReverieRangeLightRing>(mm2px(Vec(20.320f, 21.000f)), module, Reverie::RANGE_LIGHT_YELLOW));
		addParam(createParamCentered<GarmireButton>(mm2px(Vec(20.320f, 21.000f)), module, Reverie::RANGE_PARAM));
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(29.820f, 21.000f)), module, Reverie::FINE_PARAM));

		// Row 2 (Y = 40.00 mm): CYCLES (10.82), MORPH (29.82)
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(10.820f, 40.000f)), module, Reverie::CYCLES_PARAM));
		addParam(createParamCentered<GarmireKnob>(mm2px(Vec(29.820f, 40.000f)), module, Reverie::MORPH_PARAM));

		// ── LEFT 8 HP SECTION: 5-Row Control & Signal Jacks ──
		const float ctrl_x[4] = {6.800f, 16.000f, 25.200f, 34.400f};

		// Row 1 (Y = 65.00 mm): Sync In, X, Y, Frequency In
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[0], 65.000f)), module, Reverie::SYNC_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[1], 65.000f)), module, Reverie::X_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[2], 65.000f)), module, Reverie::Y_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[3], 65.000f)), module, Reverie::FREQ_CV_INPUT));

		// Row 2 (Y = 78.00 mm): R, G, B, Morph Time CV In
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[0], 78.000f)), module, Reverie::R_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[1], 78.000f)), module, Reverie::G_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[2], 78.000f)), module, Reverie::B_INPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[3], 78.000f)), module, Reverie::MORPH_CV_INPUT));

		// Row 3 (Y = 91.00 mm): Z/I In, X Out, Y Out, X Scan In
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[0], 91.000f)), module, Reverie::ZI_INPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(ctrl_x[1], 91.000f)), module, Reverie::X_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(ctrl_x[2], 91.000f)), module, Reverie::Y_OUTPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[3], 91.000f)), module, Reverie::SCAN_X_CV_INPUT));

		// Row 4 (Y = 104.00 mm): R Out, G Out, B Out, Y Scan In
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(ctrl_x[0], 104.000f)), module, Reverie::R_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(ctrl_x[1], 104.000f)), module, Reverie::G_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(ctrl_x[2], 104.000f)), module, Reverie::B_OUTPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(ctrl_x[3], 104.000f)), module, Reverie::SCAN_Y_CV_INPUT));

		// Row 5 (Y = 117.00 mm): Z Out (6.80), Sync Out (20.60), Record (34.40)
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(6.800f, 117.000f)), module, Reverie::ZI_OUTPUT));
		addOutput(createOutputCentered<GarmirePort>(mm2px(Vec(20.600f, 117.000f)), module, Reverie::SYNC_OUTPUT));
		addInput(createInputCentered<GarmirePort>(mm2px(Vec(34.400f, 117.000f)), module, Reverie::RECORD_INPUT));

		// ── RIGHT 12 HP SECTION: 4x4 Preset Button Matrix (Centered at X = 71.12 mm, Y = 38.25 mm) ──
		PresetGridWidget* gridWidget = new PresetGridWidget();
		gridWidget->module = module;
		gridWidget->box.pos = mm2px(Vec(71.120f - 25.000f, 38.250f - 25.000f));
		addChild(gridWidget);

		// ── RIGHT 12 HP SECTION: 4x4 Preset Gate Input Grid (Rows at 78, 91, 104, 117 mm) ──
		const float grid_x[4] = {52.370f, 64.870f, 77.370f, 89.870f};
		const float grid_jack_y[4] = {78.000f, 91.000f, 104.000f, 117.000f};
		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				int slotIdx = r * 4 + c;
				addInput(createInputCentered<GarmirePort>(mm2px(Vec(grid_x[c], grid_jack_y[r])), module, Reverie::PRESET_GATE_INPUT + slotIdx));
			}
		}
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
