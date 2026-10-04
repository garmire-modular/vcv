#include "plugin.hpp"
#include "core/BitterrootCore.hpp"
#include <cmath>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────────────
// Custom ParamQuantity Subclasses
// ─────────────────────────────────────────────────────────────────────

struct IntDisplayParamQuantity : ParamQuantity {
	std::string prefix;
	std::string suffix;
	IntDisplayParamQuantity() = default;
	IntDisplayParamQuantity(const std::string& p, const std::string& s = "")
		: prefix(p), suffix(s) {}
	std::string getDisplayValueString() override {
		int v = static_cast<int>(std::round(getValue()));
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%d%s", prefix.c_str(), v, suffix.c_str());
		return std::string(buf);
	}
};

struct HexMaskParamQuantity : ParamQuantity {
	HexMaskParamQuantity() = default;
	std::string getDisplayValueString() override {
		int v = static_cast<int>(std::round(getValue())) & 0x03FF;
		char buf[32];
		snprintf(buf, sizeof(buf), "0x%03X (%d)", v, v);
		return std::string(buf);
	}
};

struct PercentParamQuantity : ParamQuantity {
	std::string prefix;
	PercentParamQuantity() = default;
	PercentParamQuantity(const std::string& p) : prefix(p) {}
	std::string getDisplayValueString() override {
		float v = getValue() * 100.0f;
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%.1f%%", prefix.c_str(), v);
		return std::string(buf);
	}
};

struct SignedIntParamQuantity : ParamQuantity {
	std::string prefix;
	SignedIntParamQuantity() = default;
	SignedIntParamQuantity(const std::string& p) : prefix(p) {}
	std::string getDisplayValueString() override {
		int v = static_cast<int>(std::round(getValue()));
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%+d", prefix.c_str(), v);
		return std::string(buf);
	}
};

struct ListParamQuantity : ParamQuantity {
	std::vector<std::string> labels;
	ListParamQuantity() = default;
	ListParamQuantity(const std::vector<std::string>& l) : labels(l) {}
	std::string getDisplayValueString() override {
		int idx = static_cast<int>(std::round(getValue()));
		if (idx >= 0 && idx < (int)labels.size()) {
			return labels[idx];
		}
		return std::to_string(idx);
	}
};

// ─────────────────────────────────────────────────────────────────────
// Bitterroot Module
// ─────────────────────────────────────────────────────────────────────

struct Bitterroot : Module {
	enum ParamId {
		// Row 1: Morton, Reverse, Transpose (P1, P2, P3, Mix)
		MORTON_P1, MORTON_P2, MORTON_P3, MORTON_MIX,
		REVERSE_P1, REVERSE_P2, REVERSE_P3, REVERSE_MIX,
		TRANSPOSE_P1, TRANSPOSE_P2, TRANSPOSE_P3, TRANSPOSE_MIX,

		// Row 2: Avalanche, Permute, Gray (P1, P2, P3, Mix)
		AVALANCHE_P1, AVALANCHE_P2, AVALANCHE_P3, AVALANCHE_MIX,
		PERMUTE_P1, PERMUTE_P2, PERMUTE_P3, PERMUTE_MIX,
		GRAY_P1, GRAY_P2, GRAY_P3, GRAY_MIX,

		// Row 3: Galois, Automata, Hamming (P1, P2, P3, Mix)
		GALOIS_P1, GALOIS_P2, GALOIS_P3, GALOIS_MIX,
		AUTOMATA_P1, AUTOMATA_P2, AUTOMATA_P3, AUTOMATA_MIX,
		HAMMING_P1, HAMMING_P2, HAMMING_P3, HAMMING_MIX,

		// Master Section
		SCAN_X_PARAM,
		SCAN_Y_PARAM,
		ROUTE_PARAM,
		Z_BLANK_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Row 1 CVs: Morton, Reverse, Transpose (P1, P2, P3, Mix)
		MORTON_P1_CV, MORTON_P2_CV, MORTON_P3_CV, MORTON_MIX_CV,
		REVERSE_P1_CV, REVERSE_P2_CV, REVERSE_P3_CV, REVERSE_MIX_CV,
		TRANSPOSE_P1_CV, TRANSPOSE_P2_CV, TRANSPOSE_P3_CV, TRANSPOSE_MIX_CV,

		// Row 2 CVs: Avalanche, Permute, Gray (P1, P2, P3, Mix)
		AVALANCHE_P1_CV, AVALANCHE_P2_CV, AVALANCHE_P3_CV, AVALANCHE_MIX_CV,
		PERMUTE_P1_CV, PERMUTE_P2_CV, PERMUTE_P3_CV, PERMUTE_MIX_CV,
		GRAY_P1_CV, GRAY_P2_CV, GRAY_P3_CV, GRAY_MIX_CV,

		// Row 3 CVs: Galois, Automata, Hamming (P1, P2, P3, Mix)
		GALOIS_P1_CV, GALOIS_P2_CV, GALOIS_P3_CV, GALOIS_MIX_CV,
		AUTOMATA_P1_CV, AUTOMATA_P2_CV, AUTOMATA_P3_CV, AUTOMATA_MIX_CV,
		HAMMING_P1_CV, HAMMING_P2_CV, HAMMING_P3_CV, HAMMING_MIX_CV,

		// Master Inputs
		IN_X_INPUT,
		IN_Y_INPUT,
		SCAN_X_CV_INPUT,
		SCAN_Y_CV_INPUT,
		Z_BLANK_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		OUT_X_OUTPUT,
		OUT_Y_OUTPUT,
		OUT_Z_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		// Route mode light
		ROUTE_LIGHT,

		// 3x3 Central Vector Activity LED Matrix
		GRID_LED_0, GRID_LED_1, GRID_LED_2,
		GRID_LED_3, GRID_LED_4, GRID_LED_5,
		GRID_LED_6, GRID_LED_7, GRID_LED_8,

		LIGHTS_LEN
	};

	bitterroot::CoreEngine engine;

	Bitterroot() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// ──────────────── Row 1: Morton, Reverse, Transpose ────────────────
		configParam<IntDisplayParamQuantity>(MORTON_P1, 0.f, 19.f, 0.f, "Morton shift", " bits");
		configParam<ListParamQuantity>(MORTON_P2, 0.f, 2.f, 0.f, "Morton stride", "", 0.f, 1.f, 0.f)->labels = {"Standard", "Inverted", "2-bit block"};
		configParam<PercentParamQuantity>(MORTON_P3, 0.f, 1.f, 0.f, "Morton morph");
		configParam<PercentParamQuantity>(MORTON_MIX, 0.f, 1.f, 0.f, "Morton mix");

		configParam<IntDisplayParamQuantity>(REVERSE_P1, 1.f, 10.f, 10.f, "Reverse width", " bits");
		configParam<HexMaskParamQuantity>(REVERSE_P2, 0.f, 1023.f, 0.f, "Reverse mask");
		configParam<SignedIntParamQuantity>(REVERSE_P3, -5.f, 5.f, 0.f, "Reverse skew", " bits");
		configParam<PercentParamQuantity>(REVERSE_MIX, 0.f, 1.f, 0.f, "Reverse mix");

		configParam<IntDisplayParamQuantity>(TRANSPOSE_P1, 0.f, 9.f, 7.f, "Transpose plane A", " bit");
		configParam<IntDisplayParamQuantity>(TRANSPOSE_P2, 0.f, 9.f, 3.f, "Transpose plane B", " bit");
		configParam<IntDisplayParamQuantity>(TRANSPOSE_P3, 0.f, 9.f, 5.f, "Transpose cycle C", " bit");
		configParam<PercentParamQuantity>(TRANSPOSE_MIX, 0.f, 1.f, 0.f, "Transpose mix");

		// ──────────────── Row 2: Avalanche, Permute, Gray ────────────────
		configParam<HexMaskParamQuantity>(AVALANCHE_P1, 0.f, 1023.f, 511.f, "Avalanche mask");
		configParam<IntDisplayParamQuantity>(AVALANCHE_P2, 0.f, 4.f, 1.f, "Avalanche shift", " bits");
		configParam<PercentParamQuantity>(AVALANCHE_P3, 0.f, 1.f, 0.f, "Avalanche borrow");
		configParam<PercentParamQuantity>(AVALANCHE_MIX, 0.f, 1.f, 0.f, "Avalanche mix");

		configParam<ListParamQuantity>(PERMUTE_P1, 0.f, 3.f, 0.f, "Permute mode", "", 0.f, 1.f, 0.f)->labels = {"Odd/even", "Shuffle", "Inversion", "Quadrant"};
		configParam<IntDisplayParamQuantity>(PERMUTE_P2, 0.f, 19.f, 0.f, "Permute rotate", " steps");
		configParam<ListParamQuantity>(PERMUTE_P3, 0.f, 5.f, 0.f, "Permute stride", "", 0.f, 1.f, 0.f)->labels = {"1", "3", "5", "7", "9", "11"};
		configParam<PercentParamQuantity>(PERMUTE_MIX, 0.f, 1.f, 0.f, "Permute mix");

		configParam<IntDisplayParamQuantity>(GRAY_P1, 1.f, 10.f, 10.f, "Gray depth", " bits");
		configParam<ListParamQuantity>(GRAY_P2, 0.f, 2.f, 0.f, "Gray mode", "", 0.f, 1.f, 0.f)->labels = {"Binary to gray", "Gray to binary", "Dual reflected"};
		configParam<IntDisplayParamQuantity>(GRAY_P3, 1.f, 9.f, 1.f, "Gray tap", " bits");
		configParam<PercentParamQuantity>(GRAY_MIX, 0.f, 1.f, 0.f, "Gray mix");

		// ──────────────── Row 3: Galois, Automata, Hamming ────────────────
		configParam<ListParamQuantity>(GALOIS_P1, 0.f, 7.f, 0.f, "Galois poly", "", 0.f, 1.f, 0.f)->labels = {
			"0x409", "0x481", "0x611", "0x50D", "0x46F", "0x425", "0x679", "0x4D5"
		};
		configParam<HexMaskParamQuantity>(GALOIS_P2, 1.f, 1023.f, 3.f, "Galois alpha");
		configParam<ListParamQuantity>(GALOIS_P3, 0.f, 3.f, 0.f, "Galois mode", "", 0.f, 1.f, 0.f)->labels = {"Linear", "Inversion", "Cube", "S-box quintic"};
		configParam<PercentParamQuantity>(GALOIS_MIX, 0.f, 1.f, 0.f, "Galois mix");

		configParam<IntDisplayParamQuantity>(AUTOMATA_P1, 0.f, 255.f, 90.f, "Automata rule");
		configParam<IntDisplayParamQuantity>(AUTOMATA_P2, 1.f, 4.f, 1.f, "Automata steps", " steps");
		configParam<ListParamQuantity>(AUTOMATA_P3, 0.f, 2.f, 0.f, "Automata coupling", "", 0.f, 1.f, 0.f)->labels = {"Edge", "Center", "Full XOR"};
		configParam<PercentParamQuantity>(AUTOMATA_MIX, 0.f, 1.f, 0.f, "Automata mix");

		configParam<IntDisplayParamQuantity>(HAMMING_P1, 0.f, 32.f, 8.f, "Hamming gain");
		configParam<ListParamQuantity>(HAMMING_P2, 0.f, 2.f, 0.f, "Hamming mode", "", 0.f, 1.f, 0.f)->labels = {"Sign flip", "Shear", "Jump"};
		configParam<PercentParamQuantity>(HAMMING_P3, 0.f, 1.f, 0.f, "Hamming coupling");
		configParam<PercentParamQuantity>(HAMMING_MIX, 0.f, 1.f, 0.f, "Hamming mix");

		// ──────────────── Master Section ────────────────
		configParam<PercentParamQuantity>(SCAN_X_PARAM, 0.f, 1.f, 0.5f, "Scan X focus");
		configParam<PercentParamQuantity>(SCAN_Y_PARAM, 0.f, 1.f, 0.5f, "Scan Y focus");
		configSwitch(ROUTE_PARAM, 0.f, 1.f, 0.f, "Route mode", {"Serial", "Matrix scan"});
		configParam<PercentParamQuantity>(Z_BLANK_PARAM, 0.f, 1.f, 0.5f, "Z blanking threshold");

		// ──────────────── CV Inputs (Ending in CV depth) ────────────────
		// Row 1 CV Inputs
		configInput(MORTON_P1_CV, "Morton shift CV depth");
		configInput(MORTON_P2_CV, "Morton stride CV depth");
		configInput(MORTON_P3_CV, "Morton morph CV depth");
		configInput(MORTON_MIX_CV, "Morton mix CV depth");

		configInput(REVERSE_P1_CV, "Reverse width CV depth");
		configInput(REVERSE_P2_CV, "Reverse mask CV depth");
		configInput(REVERSE_P3_CV, "Reverse skew CV depth");
		configInput(REVERSE_MIX_CV, "Reverse mix CV depth");

		configInput(TRANSPOSE_P1_CV, "Transpose plane A CV depth");
		configInput(TRANSPOSE_P2_CV, "Transpose plane B CV depth");
		configInput(TRANSPOSE_P3_CV, "Transpose cycle C CV depth");
		configInput(TRANSPOSE_MIX_CV, "Transpose mix CV depth");

		// Row 2 CV Inputs
		configInput(AVALANCHE_P1_CV, "Avalanche mask CV depth");
		configInput(AVALANCHE_P2_CV, "Avalanche shift CV depth");
		configInput(AVALANCHE_P3_CV, "Avalanche borrow CV depth");
		configInput(AVALANCHE_MIX_CV, "Avalanche mix CV depth");

		configInput(PERMUTE_P1_CV, "Permute mode CV depth");
		configInput(PERMUTE_P2_CV, "Permute rotate CV depth");
		configInput(PERMUTE_P3_CV, "Permute stride CV depth");
		configInput(PERMUTE_MIX_CV, "Permute mix CV depth");

		configInput(GRAY_P1_CV, "Gray depth CV depth");
		configInput(GRAY_P2_CV, "Gray mode CV depth");
		configInput(GRAY_P3_CV, "Gray tap CV depth");
		configInput(GRAY_MIX_CV, "Gray mix CV depth");

		// Row 3 CV Inputs
		configInput(GALOIS_P1_CV, "Galois poly CV depth");
		configInput(GALOIS_P2_CV, "Galois alpha CV depth");
		configInput(GALOIS_P3_CV, "Galois mode CV depth");
		configInput(GALOIS_MIX_CV, "Galois mix CV depth");

		configInput(AUTOMATA_P1_CV, "Automata rule CV depth");
		configInput(AUTOMATA_P2_CV, "Automata steps CV depth");
		configInput(AUTOMATA_P3_CV, "Automata coupling CV depth");
		configInput(AUTOMATA_MIX_CV, "Automata mix CV depth");

		configInput(HAMMING_P1_CV, "Hamming gain CV depth");
		configInput(HAMMING_P2_CV, "Hamming mode CV depth");
		configInput(HAMMING_P3_CV, "Hamming coupling CV depth");
		configInput(HAMMING_MIX_CV, "Hamming mix CV depth");

		// Master Inputs
		configInput(IN_X_INPUT, "X coordinate input");
		configInput(IN_Y_INPUT, "Y coordinate input");
		configInput(SCAN_X_CV_INPUT, "Scan X coordinate CV depth");
		configInput(SCAN_Y_CV_INPUT, "Scan Y coordinate CV depth");
		configInput(Z_BLANK_CV_INPUT, "Z blanking CV depth");

		// Master Outputs
		configOutput(OUT_X_OUTPUT, "X coordinate output");
		configOutput(OUT_Y_OUTPUT, "Y coordinate output");
		configOutput(OUT_Z_OUTPUT, "Z intensity / blanking output");
	}

	void process(const ProcessArgs& args) override {
		// Read Master Inputs
		float inX_V = inputs[IN_X_INPUT].isConnected() ? inputs[IN_X_INPUT].getVoltage() : 0.0f;
		float inY_V = inputs[IN_Y_INPUT].isConnected() ? inputs[IN_Y_INPUT].getVoltage() : 0.0f;

		// Scan coordinates: param is 0% to 100% (default 50% = center 0.0)
		float scanX_norm = (params[SCAN_X_PARAM].getValue() - 0.5f) * 2.0f;
		if (inputs[SCAN_X_CV_INPUT].isConnected()) {
			scanX_norm += inputs[SCAN_X_CV_INPUT].getVoltage() * 0.1f;
		}
		scanX_norm = rack::math::clamp(scanX_norm, -1.0f, 1.0f);

		float scanY_norm = (params[SCAN_Y_PARAM].getValue() - 0.5f) * 2.0f;
		if (inputs[SCAN_Y_CV_INPUT].isConnected()) {
			scanY_norm += inputs[SCAN_Y_CV_INPUT].getVoltage() * 0.1f;
		}
		scanY_norm = rack::math::clamp(scanY_norm, -1.0f, 1.0f);

		float routeVal = params[ROUTE_PARAM].getValue();
		engine.routeMode = (routeVal > 0.5f) ? bitterroot::ROUTE_MATRIX_SCAN : bitterroot::ROUTE_SERIAL;
		lights[ROUTE_LIGHT].setBrightness(routeVal > 0.5f ? 1.0f : 0.0f);

		float zBlank = params[Z_BLANK_PARAM].getValue();
		if (inputs[Z_BLANK_CV_INPUT].isConnected()) {
			zBlank += inputs[Z_BLANK_CV_INPUT].getVoltage() * 0.1f;
		}
		zBlank = rack::math::clamp(zBlank, 0.0f, 1.0f);

		// Populate 9 effect parameter sets with CV modulation
		bitterroot::BlockParams bp[9];

		auto readBlock = [&](int blockIdx, int p1Id, int p2Id, int p3Id, int mixId,
		                     int cv1Id, int cv2Id, int cv3Id, int cvMixId,
		                     float p1Min, float p1Max, float p2Min, float p2Max, float p3Min, float p3Max) {
			float p1 = params[p1Id].getValue();
			if (inputs[cv1Id].isConnected()) p1 += inputs[cv1Id].getVoltage() * 0.1f * (p1Max - p1Min);
			p1 = rack::math::clamp(p1, p1Min, p1Max);

			float p2 = params[p2Id].getValue();
			if (inputs[cv2Id].isConnected()) p2 += inputs[cv2Id].getVoltage() * 0.1f * (p2Max - p2Min);
			p2 = rack::math::clamp(p2, p2Min, p2Max);

			float p3 = params[p3Id].getValue();
			if (inputs[cv3Id].isConnected()) p3 += inputs[cv3Id].getVoltage() * 0.1f * (p3Max - p3Min);
			p3 = rack::math::clamp(p3, p3Min, p3Max);

			float mix = params[mixId].getValue();
			if (inputs[cvMixId].isConnected()) mix += inputs[cvMixId].getVoltage() * 0.1f;
			mix = rack::math::clamp(mix, 0.0f, 1.0f);

			bp[blockIdx].p1 = p1;
			bp[blockIdx].p2 = p2;
			bp[blockIdx].p3 = p3;
			bp[blockIdx].mix = mix;
			bp[blockIdx].active = (mix > 0.0001f);
		};

		// Row 1: Morton, Reverse, Transpose
		readBlock(0, MORTON_P1, MORTON_P2, MORTON_P3, MORTON_MIX,
		          MORTON_P1_CV, MORTON_P2_CV, MORTON_P3_CV, MORTON_MIX_CV,
		          0.f, 19.f, 0.f, 2.f, 0.f, 1.f);
		readBlock(1, REVERSE_P1, REVERSE_P2, REVERSE_P3, REVERSE_MIX,
		          REVERSE_P1_CV, REVERSE_P2_CV, REVERSE_P3_CV, REVERSE_MIX_CV,
		          1.f, 10.f, 0.f, 1023.f, -5.f, 5.f);
		readBlock(2, TRANSPOSE_P1, TRANSPOSE_P2, TRANSPOSE_P3, TRANSPOSE_MIX,
		          TRANSPOSE_P1_CV, TRANSPOSE_P2_CV, TRANSPOSE_P3_CV, TRANSPOSE_MIX_CV,
		          0.f, 9.f, 0.f, 9.f, 0.f, 9.f);

		// Row 2: Avalanche, Permute, Gray
		readBlock(3, AVALANCHE_P1, AVALANCHE_P2, AVALANCHE_P3, AVALANCHE_MIX,
		          AVALANCHE_P1_CV, AVALANCHE_P2_CV, AVALANCHE_P3_CV, AVALANCHE_MIX_CV,
		          0.f, 1023.f, 0.f, 4.f, 0.f, 1.f);
		readBlock(4, PERMUTE_P1, PERMUTE_P2, PERMUTE_P3, PERMUTE_MIX,
		          PERMUTE_P1_CV, PERMUTE_P2_CV, PERMUTE_P3_CV, PERMUTE_MIX_CV,
		          0.f, 3.f, 0.f, 19.f, 0.f, 5.f);
		readBlock(5, GRAY_P1, GRAY_P2, GRAY_P3, GRAY_MIX,
		          GRAY_P1_CV, GRAY_P2_CV, GRAY_P3_CV, GRAY_MIX_CV,
		          1.f, 10.f, 0.f, 2.f, 1.f, 9.f);

		// Row 3: Galois, Automata, Hamming
		readBlock(6, GALOIS_P1, GALOIS_P2, GALOIS_P3, GALOIS_MIX,
		          GALOIS_P1_CV, GALOIS_P2_CV, GALOIS_P3_CV, GALOIS_MIX_CV,
		          0.f, 7.f, 1.f, 1023.f, 0.f, 3.f);
		readBlock(7, AUTOMATA_P1, AUTOMATA_P2, AUTOMATA_P3, AUTOMATA_MIX,
		          AUTOMATA_P1_CV, AUTOMATA_P2_CV, AUTOMATA_P3_CV, AUTOMATA_MIX_CV,
		          0.f, 255.f, 1.f, 4.f, 0.f, 2.f);
		readBlock(8, HAMMING_P1, HAMMING_P2, HAMMING_P3, HAMMING_MIX,
		          HAMMING_P1_CV, HAMMING_P2_CV, HAMMING_P3_CV, HAMMING_MIX_CV,
		          0.f, 32.f, 0.f, 2.f, 0.f, 1.f);

		// Process DSP core
		auto out = engine.process(inX_V, inY_V, bp, scanX_norm, scanY_norm, zBlank, args.sampleRate);

		// Set Outputs
		outputs[OUT_X_OUTPUT].setVoltage(out.outX);
		outputs[OUT_Y_OUTPUT].setVoltage(out.outY);
		outputs[OUT_Z_OUTPUT].setVoltage(out.outZ);

		// Update 3x3 Activity LEDs
		for (int i = 0; i < 9; ++i) {
			lights[GRID_LED_0 + i].setBrightness(out.cellActivity[i]);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "voltageRange", json_integer((int)engine.voltageRange));
		json_object_set_new(rootJ, "zScaleMode", json_integer((int)engine.zScaleMode));
		json_object_set_new(rootJ, "slewMode", json_integer((int)engine.slewMode));
		json_object_set_new(rootJ, "decimationRatio", json_integer(engine.decimationRatio));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) engine.voltageRange = (bitterroot::VoltageRange)json_integer_value(vrJ);

		json_t* zsJ = json_object_get(rootJ, "zScaleMode");
		if (zsJ) engine.zScaleMode = (bitterroot::ZScaleMode)json_integer_value(zsJ);

		json_t* smJ = json_object_get(rootJ, "slewMode");
		if (smJ) engine.slewMode = (bitterroot::SlewMode)json_integer_value(smJ);

		json_t* drJ = json_object_get(rootJ, "decimationRatio");
		if (drJ) engine.decimationRatio = json_integer_value(drJ);
	}
};

// ─────────────────────────────────────────────────────────────────────
// Bitterroot Widget
// ─────────────────────────────────────────────────────────────────────

struct BitterrootWidget : ModuleWidget {
	BitterrootWidget(Bitterroot* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bitterroot.svg")));

		// Screws for 32 HP (width = 162.56 mm)
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Columns of Knobs: 3 Effect Columns, each with 4 knobs (pitch 11.5 mm)
		float colX[3][4] = {
			{11.53f, 23.03f, 34.53f, 46.03f},
			{64.03f, 75.53f, 87.03f, 98.53f},
			{116.53f, 128.03f, 139.53f, 151.03f}
		};

		// ──────────────── Zone 2: Rows 1, 2, 3 Effect Knobs (Full Size Knobs) ────────────────
		float rowY[3] = {23.50f, 42.50f, 61.50f};

		int pIds[9][4] = {
			{Bitterroot::MORTON_P1, Bitterroot::MORTON_P2, Bitterroot::MORTON_P3, Bitterroot::MORTON_MIX},
			{Bitterroot::REVERSE_P1, Bitterroot::REVERSE_P2, Bitterroot::REVERSE_P3, Bitterroot::REVERSE_MIX},
			{Bitterroot::TRANSPOSE_P1, Bitterroot::TRANSPOSE_P2, Bitterroot::TRANSPOSE_P3, Bitterroot::TRANSPOSE_MIX},
			{Bitterroot::AVALANCHE_P1, Bitterroot::AVALANCHE_P2, Bitterroot::AVALANCHE_P3, Bitterroot::AVALANCHE_MIX},
			{Bitterroot::PERMUTE_P1, Bitterroot::PERMUTE_P2, Bitterroot::PERMUTE_P3, Bitterroot::PERMUTE_MIX},
			{Bitterroot::GRAY_P1, Bitterroot::GRAY_P2, Bitterroot::GRAY_P3, Bitterroot::GRAY_MIX},
			{Bitterroot::GALOIS_P1, Bitterroot::GALOIS_P2, Bitterroot::GALOIS_P3, Bitterroot::GALOIS_MIX},
			{Bitterroot::AUTOMATA_P1, Bitterroot::AUTOMATA_P2, Bitterroot::AUTOMATA_P3, Bitterroot::AUTOMATA_MIX},
			{Bitterroot::HAMMING_P1, Bitterroot::HAMMING_P2, Bitterroot::HAMMING_P3, Bitterroot::HAMMING_MIX}
		};

		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int b = r * 3 + c;
				for (int k = 0; k < 4; ++k) {
					addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colX[c][k], rowY[r])), module, pIds[b][k]));
				}
			}
		}

		// ──────────────── Zone 3: Master Controls & Master Jacks (Center Y = 79.50 mm) ────────────────
		// Left Side: Master Controls & Display
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(12.00f, 79.50f)), module, Bitterroot::SCAN_X_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.50f, 79.50f)), module, Bitterroot::SCAN_Y_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(37.00f, 79.50f)), module, Bitterroot::ROUTE_PARAM));
		addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(41.50f, 79.50f)), module, Bitterroot::ROUTE_LIGHT));

		// 3x3 Activity LED Matrix (Centered at 49.50 mm, 79.50 mm)
		float ledGridX[3] = {46.50f, 49.50f, 52.50f};
		float ledGridY[3] = {76.50f, 79.50f, 82.50f};
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int idx = r * 3 + c;
				addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(ledGridX[c], ledGridY[r])), module, Bitterroot::GRID_LED_0 + idx));
			}
		}

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(64.00f, 79.50f)), module, Bitterroot::Z_BLANK_PARAM));

		// Right Side: Master Jacks (Center Y = 79.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(80.00f, 79.50f)), module, Bitterroot::IN_X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(90.00f, 79.50f)), module, Bitterroot::IN_Y_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(103.00f, 79.50f)), module, Bitterroot::SCAN_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(113.00f, 79.50f)), module, Bitterroot::SCAN_Y_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(123.00f, 79.50f)), module, Bitterroot::Z_BLANK_CV_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(136.00f, 79.50f)), module, Bitterroot::OUT_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.00f, 79.50f)), module, Bitterroot::OUT_Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(156.00f, 79.50f)), module, Bitterroot::OUT_Z_OUTPUT));

		// ──────────────── Zone 4: 12 x 3 CV Depth Jack Matrix ────────────────
		int cvIds[9][4] = {
			{Bitterroot::MORTON_P1_CV, Bitterroot::MORTON_P2_CV, Bitterroot::MORTON_P3_CV, Bitterroot::MORTON_MIX_CV},
			{Bitterroot::REVERSE_P1_CV, Bitterroot::REVERSE_P2_CV, Bitterroot::REVERSE_P3_CV, Bitterroot::REVERSE_MIX_CV},
			{Bitterroot::TRANSPOSE_P1_CV, Bitterroot::TRANSPOSE_P2_CV, Bitterroot::TRANSPOSE_P3_CV, Bitterroot::TRANSPOSE_MIX_CV},
			{Bitterroot::AVALANCHE_P1_CV, Bitterroot::AVALANCHE_P2_CV, Bitterroot::AVALANCHE_P3_CV, Bitterroot::AVALANCHE_MIX_CV},
			{Bitterroot::PERMUTE_P1_CV, Bitterroot::PERMUTE_P2_CV, Bitterroot::PERMUTE_P3_CV, Bitterroot::PERMUTE_MIX_CV},
			{Bitterroot::GRAY_P1_CV, Bitterroot::GRAY_P2_CV, Bitterroot::GRAY_P3_CV, Bitterroot::GRAY_MIX_CV},
			{Bitterroot::GALOIS_P1_CV, Bitterroot::GALOIS_P2_CV, Bitterroot::GALOIS_P3_CV, Bitterroot::GALOIS_MIX_CV},
			{Bitterroot::AUTOMATA_P1_CV, Bitterroot::AUTOMATA_P2_CV, Bitterroot::AUTOMATA_P3_CV, Bitterroot::AUTOMATA_MIX_CV},
			{Bitterroot::HAMMING_P1_CV, Bitterroot::HAMMING_P2_CV, Bitterroot::HAMMING_P3_CV, Bitterroot::HAMMING_MIX_CV}
		};

		float jackRowY[3] = {98.00f, 108.00f, 118.00f};
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int b = r * 3 + c;
				for (int k = 0; k < 4; ++k) {
					addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[c][k], jackRowY[r])), module, cvIds[b][k]));
				}
			}
		}
	}

	void appendContextMenu(Menu* menu) override {
		Bitterroot* module = dynamic_cast<Bitterroot*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Voltage Range Standard"));

		const char* vrLabels[] = {
			"Bipolar ±5.0V (Default Eurorack/Laser)",
			"Unipolar 0–10.0V (Video Standard)"
		};

		for (int i = 0; i < 2; ++i) {
			bitterroot::VoltageRange vr = (bitterroot::VoltageRange)i;
			menu->addChild(createCheckMenuItem(vrLabels[i], "",
				[=]() { return module->engine.voltageRange == vr; },
				[=]() { module->engine.voltageRange = vr; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Z Full Intensity Scale"));

		const char* zScaleLabels[] = {
			"1.0V (Laser Diode / Logic Standard)",
			"5.0V (Eurorack Nominal Standard)",
			"10.0V (Full Video Standard)"
		};

		for (int i = 0; i < 3; ++i) {
			bitterroot::ZScaleMode zs = (bitterroot::ZScaleMode)i;
			menu->addChild(createCheckMenuItem(zScaleLabels[i], "",
				[=]() { return module->engine.zScaleMode == zs; },
				[=]() { module->engine.zScaleMode = zs; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Sample Rate Decimation"));

		const int decRatios[] = {1, 2, 4, 8, 16, 32, 96};
		const char* decLabels[] = {
			"Full Engine Rate (No Decimation)",
			"48.0 kHz", "24.0 kHz", "12.0 kHz",
			"6.0 kHz", "3.0 kHz", "1.0 kHz"
		};

		for (int i = 0; i < 7; ++i) {
			int ratio = decRatios[i];
			menu->addChild(createCheckMenuItem(decLabels[i], "",
				[=]() { return module->engine.decimationRatio == ratio; },
				[=]() { module->engine.decimationRatio = ratio; }
			));
		}

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Galvo-Safe Slew Limiter"));

		const char* slewLabels[] = {
			"Off (Raw DAC Glitches)",
			"Subtle 1-Pole (Corner Preserving)",
			"Medium Galvo Protection",
			"Heavy Slew (Acoustic Audio Smoothing)"
		};

		for (int i = 0; i < 4; ++i) {
			bitterroot::SlewMode sm = (bitterroot::SlewMode)i;
			menu->addChild(createCheckMenuItem(slewLabels[i], "",
				[=]() { return module->engine.slewMode == sm; },
				[=]() { module->engine.slewMode = sm; }
			));
		}
	}
};

Model* modelBitterroot = createModel<Bitterroot, BitterrootWidget>("Bitterroot");
