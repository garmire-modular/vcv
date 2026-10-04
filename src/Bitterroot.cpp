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
	IntDisplayParamQuantity(const std::string& p = "", const std::string& s = "")
		: prefix(p), suffix(s) {}
	std::string getDisplayValueString() override {
		int v = static_cast<int>(std::round(getValue()));
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%d%s", prefix.c_str(), v, suffix.c_str());
		return std::string(buf);
	}
};

struct HexMaskParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int v = static_cast<int>(std::round(getValue())) & 0x03FF;
		char buf[32];
		snprintf(buf, sizeof(buf), "0x%03X (%d)", v, v);
		return std::string(buf);
	}
};

struct PercentParamQuantity : ParamQuantity {
	std::string prefix;
	PercentParamQuantity(const std::string& p = "") : prefix(p) {}
	std::string getDisplayValueString() override {
		float v = getValue() * 100.0f;
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%.1f%%", prefix.c_str(), v);
		return std::string(buf);
	}
};

struct SignedIntParamQuantity : ParamQuantity {
	std::string prefix;
	SignedIntParamQuantity(const std::string& p = "") : prefix(p) {}
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
		// Row 1: Blocks A, B, C
		BLOCK_A_P1, BLOCK_A_P2, BLOCK_A_P3, BLOCK_A_ACTIVE,
		BLOCK_B_P1, BLOCK_B_P2, BLOCK_B_P3, BLOCK_B_ACTIVE,
		BLOCK_C_P1, BLOCK_C_P2, BLOCK_C_P3, BLOCK_C_ACTIVE,

		// Row 2: Blocks D, E, F
		BLOCK_D_P1, BLOCK_D_P2, BLOCK_D_P3, BLOCK_D_ACTIVE,
		BLOCK_E_P1, BLOCK_E_P2, BLOCK_E_P3, BLOCK_E_ACTIVE,
		BLOCK_F_P1, BLOCK_F_P2, BLOCK_F_P3, BLOCK_F_ACTIVE,

		// Row 3: Blocks G, H, I
		BLOCK_G_P1, BLOCK_G_P2, BLOCK_G_P3, BLOCK_G_ACTIVE,
		BLOCK_H_P1, BLOCK_H_P2, BLOCK_H_P3, BLOCK_H_ACTIVE,
		BLOCK_I_P1, BLOCK_I_P2, BLOCK_I_P3, BLOCK_I_ACTIVE,

		// Master Section
		SCAN_X_PARAM,
		SCAN_Y_PARAM,
		ROUTE_PARAM,
		Z_BLANK_PARAM,

		PARAMS_LEN
	};

	enum InputId {
		// Row 1 CVs (Blocks A, B, C)
		BLOCK_A_P1_CV, BLOCK_A_P2_CV, BLOCK_A_P3_CV,
		BLOCK_B_P1_CV, BLOCK_B_P2_CV, BLOCK_B_P3_CV,
		BLOCK_C_P1_CV, BLOCK_C_P2_CV, BLOCK_C_P3_CV,

		// Row 2 CVs (Blocks D, E, F)
		BLOCK_D_P1_CV, BLOCK_D_P2_CV, BLOCK_D_P3_CV,
		BLOCK_E_P1_CV, BLOCK_E_P2_CV, BLOCK_E_P3_CV,
		BLOCK_F_P1_CV, BLOCK_F_P2_CV, BLOCK_F_P3_CV,

		// Row 3 CVs (Blocks G, H, I)
		BLOCK_G_P1_CV, BLOCK_G_P2_CV, BLOCK_G_P3_CV,
		BLOCK_H_P1_CV, BLOCK_H_P2_CV, BLOCK_H_P3_CV,
		BLOCK_I_P1_CV, BLOCK_I_P2_CV, BLOCK_I_P3_CV,

		// Master Inputs
		IN_X_INPUT,
		IN_Y_INPUT,
		SCAN_X_CV_INPUT,
		SCAN_Y_CV_INPUT,
		ROUTE_CV_INPUT,

		INPUTS_LEN
	};

	enum OutputId {
		OUT_X_OUTPUT,
		OUT_Y_OUTPUT,
		OUT_Z_OUTPUT,

		OUTPUTS_LEN
	};

	enum LightId {
		// 9 Per-block status lights
		BLOCK_A_LIGHT, BLOCK_B_LIGHT, BLOCK_C_LIGHT,
		BLOCK_D_LIGHT, BLOCK_E_LIGHT, BLOCK_F_LIGHT,
		BLOCK_G_LIGHT, BLOCK_H_LIGHT, BLOCK_I_LIGHT,

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

		// Configure Row 1: Blocks A, B, C
		configParam<IntDisplayParamQuantity>(BLOCK_A_P1, 0.f, 19.f, 0.f, "Morton Bit Rotation", " bits");
		configParam<ListParamQuantity>(BLOCK_A_P2, 0.f, 2.f, 0.f, "Morton Interleave Mode", "", 0.f, 1.f, 0.f)->labels = {"Standard", "Inverted", "2-Bit Block"};
		configParam<PercentParamQuantity>(BLOCK_A_P3, 0.f, 1.f, 0.f, "Hilbert Morph");
		configSwitch(BLOCK_A_ACTIVE, 0.f, 1.f, 1.f, "Morton Active", {"Bypassed", "Active"});

		configParam<IntDisplayParamQuantity>(BLOCK_B_P1, 1.f, 10.f, 10.f, "Reversal Width", " bits");
		configParam<HexMaskParamQuantity>(BLOCK_B_P2, 0.f, 1023.f, 0.f, "Dyadic Phase Mask");
		configParam<SignedIntParamQuantity>(BLOCK_B_P3, -5.f, 5.f, 0.f, "Axis Skew", " bits");
		configSwitch(BLOCK_B_ACTIVE, 0.f, 1.f, 1.f, "Reverse Active", {"Bypassed", "Active"});

		configParam<IntDisplayParamQuantity>(BLOCK_C_P1, 0.f, 9.f, 7.f, "Swap Plane A", " bit");
		configParam<IntDisplayParamQuantity>(BLOCK_C_P2, 0.f, 9.f, 3.f, "Swap Plane B", " bit");
		configParam<IntDisplayParamQuantity>(BLOCK_C_P3, 0.f, 9.f, 5.f, "Cycle Plane C", " bit");
		configSwitch(BLOCK_C_ACTIVE, 0.f, 1.f, 1.f, "Transpose Active", {"Bypassed", "Active"});

		// Configure Row 2: Blocks D, E, F
		configParam<HexMaskParamQuantity>(BLOCK_D_P1, 0.f, 1023.f, 511.f, "Carry Mask");
		configParam<IntDisplayParamQuantity>(BLOCK_D_P2, 0.f, 4.f, 1.f, "Cascade Shift", " bits");
		configParam<PercentParamQuantity>(BLOCK_D_P3, 0.f, 1.f, 0.f, "Borrow Polarity");
		configSwitch(BLOCK_D_ACTIVE, 0.f, 1.f, 1.f, "Avalanche Active", {"Bypassed", "Active"});

		configParam<ListParamQuantity>(BLOCK_E_P1, 0.f, 3.f, 0.f, "Permutation Mode", "", 0.f, 1.f, 0.f)->labels = {"Odd/Even", "Shuffle", "Inversion", "Quadrant"};
		configParam<IntDisplayParamQuantity>(BLOCK_E_P2, 0.f, 19.f, 0.f, "Register Rotation", " steps");
		configParam<ListParamQuantity>(BLOCK_E_P3, 0.f, 5.f, 0.f, "Stride Step", "", 0.f, 1.f, 0.f)->labels = {"1", "3", "5", "7", "9", "11"};
		configSwitch(BLOCK_E_ACTIVE, 0.f, 1.f, 1.f, "Permute Active", {"Bypassed", "Active"});

		configParam<IntDisplayParamQuantity>(BLOCK_F_P1, 1.f, 10.f, 10.f, "Gray Bit Depth", " bits");
		configParam<ListParamQuantity>(BLOCK_F_P2, 0.f, 2.f, 0.f, "Gray Code Mode", "", 0.f, 1.f, 0.f)->labels = {"Binary to Gray", "Gray to Binary", "Dual Reflected"};
		configParam<IntDisplayParamQuantity>(BLOCK_F_P3, 1.f, 9.f, 1.f, "XOR Tap Distance", " bits");
		configSwitch(BLOCK_F_ACTIVE, 0.f, 1.f, 1.f, "Gray Active", {"Bypassed", "Active"});

		// Configure Row 3: Blocks G, H, I
		configParam<ListParamQuantity>(BLOCK_G_P1, 0.f, 7.f, 0.f, "Irreducible Poly", "", 0.f, 1.f, 0.f)->labels = {
			"0x409", "0x481", "0x611", "0x50D", "0x46F", "0x425", "0x679", "0x4D5"
		};
		configParam<HexMaskParamQuantity>(BLOCK_G_P2, 1.f, 1023.f, 3.f, "Galois Multiplier Alpha");
		configParam<ListParamQuantity>(BLOCK_G_P3, 0.f, 3.f, 0.f, "Galois Power Mode", "", 0.f, 1.f, 0.f)->labels = {"Linear", "Inversion", "Cube", "S-Box Quintic"};
		configSwitch(BLOCK_G_ACTIVE, 0.f, 1.f, 1.f, "Galois Active", {"Bypassed", "Active"});

		configParam<IntDisplayParamQuantity>(BLOCK_H_P1, 0.f, 255.f, 90.f, "Wolfram Rule");
		configParam<IntDisplayParamQuantity>(BLOCK_H_P2, 1.f, 4.f, 1.f, "CA Steps", " steps");
		configParam<ListParamQuantity>(BLOCK_H_P3, 0.f, 2.f, 0.f, "Seed Coupling", "", 0.f, 1.f, 0.f)->labels = {"Edge", "Center", "Full XOR"};
		configSwitch(BLOCK_H_ACTIVE, 0.f, 1.f, 1.f, "Automata Active", {"Bypassed", "Active"});

		configParam<IntDisplayParamQuantity>(BLOCK_I_P1, 0.f, 32.f, 8.f, "Hamming Weight Gain");
		configParam<ListParamQuantity>(BLOCK_I_P2, 0.f, 2.f, 0.f, "Parity Mode", "", 0.f, 1.f, 0.f)->labels = {"Sign Flip", "Shear", "Jump"};
		configParam<PercentParamQuantity>(BLOCK_I_P3, 0.f, 1.f, 0.f, "Mutual Coupling");
		configSwitch(BLOCK_I_ACTIVE, 0.f, 1.f, 1.f, "Hamming Active", {"Bypassed", "Active"});

		// Master Section
		configParam(SCAN_X_PARAM, -1.f, 1.f, 0.f, "Scan X Focus");
		configParam(SCAN_Y_PARAM, -1.f, 1.f, 0.f, "Scan Y Focus");
		configSwitch(ROUTE_PARAM, 0.f, 1.f, 0.f, "Route Mode", {"Cascade Serial", "Matrix Scan"});
		configParam<PercentParamQuantity>(Z_BLANK_PARAM, 0.f, 1.f, 0.5f, "Z Blanking Threshold");

		// Inputs
		configInput(IN_X_INPUT, "X Coordinate");
		configInput(IN_Y_INPUT, "Y Coordinate");
		configInput(SCAN_X_CV_INPUT, "Scan X CV");
		configInput(SCAN_Y_CV_INPUT, "Scan Y CV");
		configInput(ROUTE_CV_INPUT, "Route Mode CV");

		// Outputs
		configOutput(OUT_X_OUTPUT, "X Coordinate");
		configOutput(OUT_Y_OUTPUT, "Y Coordinate");
		configOutput(OUT_Z_OUTPUT, "Z Intensity / Blanking");
	}

	void process(const ProcessArgs& args) override {
		// Read Master Inputs
		float inX_V = inputs[IN_X_INPUT].isConnected() ? inputs[IN_X_INPUT].getVoltage() : 0.0f;
		float inY_V = inputs[IN_Y_INPUT].isConnected() ? inputs[IN_Y_INPUT].getVoltage() : 0.0f;

		float scanX = params[SCAN_X_PARAM].getValue();
		if (inputs[SCAN_X_CV_INPUT].isConnected()) {
			scanX += inputs[SCAN_X_CV_INPUT].getVoltage() * 0.2f;
		}
		scanX = rack::math::clamp(scanX, -1.0f, 1.0f);

		float scanY = params[SCAN_Y_PARAM].getValue();
		if (inputs[SCAN_Y_CV_INPUT].isConnected()) {
			scanY += inputs[SCAN_Y_CV_INPUT].getVoltage() * 0.2f;
		}
		scanY = rack::math::clamp(scanY, -1.0f, 1.0f);

		float routeVal = params[ROUTE_PARAM].getValue();
		if (inputs[ROUTE_CV_INPUT].isConnected()) {
			routeVal = (inputs[ROUTE_CV_INPUT].getVoltage() > 1.5f) ? 1.0f : 0.0f;
		}
		engine.routeMode = (routeVal > 0.5f) ? bitterroot::ROUTE_MATRIX_SCAN : bitterroot::ROUTE_CASCADE_SERIAL;
		lights[ROUTE_LIGHT].setBrightness(routeVal > 0.5f ? 1.0f : 0.0f);

		float zBlank = params[Z_BLANK_PARAM].getValue();

		// Populate 9 block parameter sets with CV modulation
		bitterroot::BlockParams bp[9];

		auto readBlock = [&](int blockIdx, int p1Id, int p2Id, int p3Id, int actId,
		                     int cv1Id, int cv2Id, int cv3Id, int lightId,
		                     float p1Min, float p1Max, float p2Min, float p2Max, float p3Min, float p3Max) {
			float p1 = params[p1Id].getValue();
			if (inputs[cv1Id].isConnected()) p1 += inputs[cv1Id].getVoltage() * 0.2f * (p1Max - p1Min);
			p1 = rack::math::clamp(p1, p1Min, p1Max);

			float p2 = params[p2Id].getValue();
			if (inputs[cv2Id].isConnected()) p2 += inputs[cv2Id].getVoltage() * 0.2f * (p2Max - p2Min);
			p2 = rack::math::clamp(p2, p2Min, p2Max);

			float p3 = params[p3Id].getValue();
			if (inputs[cv3Id].isConnected()) p3 += inputs[cv3Id].getVoltage() * 0.2f * (p3Max - p3Min);
			p3 = rack::math::clamp(p3, p3Min, p3Max);

			bool act = params[actId].getValue() > 0.5f;

			bp[blockIdx].p1 = p1;
			bp[blockIdx].p2 = p2;
			bp[blockIdx].p3 = p3;
			bp[blockIdx].active = act;

			lights[lightId].setBrightness(act ? 1.0f : 0.0f);
		};

		// Row 1
		readBlock(0, BLOCK_A_P1, BLOCK_A_P2, BLOCK_A_P3, BLOCK_A_ACTIVE, BLOCK_A_P1_CV, BLOCK_A_P2_CV, BLOCK_A_P3_CV, BLOCK_A_LIGHT, 0.f, 19.f, 0.f, 2.f, 0.f, 1.f);
		readBlock(1, BLOCK_B_P1, BLOCK_B_P2, BLOCK_B_P3, BLOCK_B_ACTIVE, BLOCK_B_P1_CV, BLOCK_B_P2_CV, BLOCK_B_P3_CV, BLOCK_B_LIGHT, 1.f, 10.f, 0.f, 1023.f, -5.f, 5.f);
		readBlock(2, BLOCK_C_P1, BLOCK_C_P2, BLOCK_C_P3, BLOCK_C_ACTIVE, BLOCK_C_P1_CV, BLOCK_C_P2_CV, BLOCK_C_P3_CV, BLOCK_C_LIGHT, 0.f, 9.f, 0.f, 9.f, 0.f, 9.f);

		// Row 2
		readBlock(3, BLOCK_D_P1, BLOCK_D_P2, BLOCK_D_P3, BLOCK_D_ACTIVE, BLOCK_D_P1_CV, BLOCK_D_P2_CV, BLOCK_D_P3_CV, BLOCK_D_LIGHT, 0.f, 1023.f, 0.f, 4.f, 0.f, 1.f);
		readBlock(4, BLOCK_E_P1, BLOCK_E_P2, BLOCK_E_P3, BLOCK_E_ACTIVE, BLOCK_E_P1_CV, BLOCK_E_P2_CV, BLOCK_E_P3_CV, BLOCK_E_LIGHT, 0.f, 3.f, 0.f, 19.f, 0.f, 5.f);
		readBlock(5, BLOCK_F_P1, BLOCK_F_P2, BLOCK_F_P3, BLOCK_F_ACTIVE, BLOCK_F_P1_CV, BLOCK_F_P2_CV, BLOCK_F_P3_CV, BLOCK_F_LIGHT, 1.f, 10.f, 0.f, 2.f, 1.f, 9.f);

		// Row 3
		readBlock(6, BLOCK_G_P1, BLOCK_G_P2, BLOCK_G_P3, BLOCK_G_ACTIVE, BLOCK_G_P1_CV, BLOCK_G_P2_CV, BLOCK_G_P3_CV, BLOCK_G_LIGHT, 0.f, 7.f, 1.f, 1023.f, 0.f, 3.f);
		readBlock(7, BLOCK_H_P1, BLOCK_H_P2, BLOCK_H_P3, BLOCK_H_ACTIVE, BLOCK_H_P1_CV, BLOCK_H_P2_CV, BLOCK_H_P3_CV, BLOCK_H_LIGHT, 0.f, 255.f, 1.f, 4.f, 0.f, 2.f);
		readBlock(8, BLOCK_I_P1, BLOCK_I_P2, BLOCK_I_P3, BLOCK_I_ACTIVE, BLOCK_I_P1_CV, BLOCK_I_P2_CV, BLOCK_I_P3_CV, BLOCK_I_LIGHT, 0.f, 32.f, 0.f, 2.f, 0.f, 1.f);

		// Process DSP core
		auto out = engine.process(inX_V, inY_V, bp, scanX, scanY, zBlank, args.sampleRate);

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
		json_object_set_new(rootJ, "slewMode", json_integer((int)engine.slewMode));
		json_object_set_new(rootJ, "decimationRatio", json_integer(engine.decimationRatio));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) engine.voltageRange = (bitterroot::VoltageRange)json_integer_value(vrJ);

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

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Columns (Center of each 3-knob block)
		// Col 1: 24.50 mm (12.50, 24.50, 36.50)
		// Col 2: 71.12 mm (59.12, 71.12, 83.12)
		// Col 3: 117.74 mm (105.74, 117.74, 129.74)
		float colX[3][3] = {
			{12.50f, 24.50f, 36.50f},
			{59.12f, 71.12f, 83.12f},
			{105.74f, 117.74f, 129.74f}
		};

		// ---------------- Zone 2: Rows 1, 2, 3 Blocks ----------------
		float rowY[3] = {22.00f, 43.00f, 64.00f};
		float switchY[3] = {29.50f, 50.50f, 71.50f};

		int pIds[9][3] = {
			{Bitterroot::BLOCK_A_P1, Bitterroot::BLOCK_A_P2, Bitterroot::BLOCK_A_P3},
			{Bitterroot::BLOCK_B_P1, Bitterroot::BLOCK_B_P2, Bitterroot::BLOCK_B_P3},
			{Bitterroot::BLOCK_C_P1, Bitterroot::BLOCK_C_P2, Bitterroot::BLOCK_C_P3},
			{Bitterroot::BLOCK_D_P1, Bitterroot::BLOCK_D_P2, Bitterroot::BLOCK_D_P3},
			{Bitterroot::BLOCK_E_P1, Bitterroot::BLOCK_E_P2, Bitterroot::BLOCK_E_P3},
			{Bitterroot::BLOCK_F_P1, Bitterroot::BLOCK_F_P2, Bitterroot::BLOCK_F_P3},
			{Bitterroot::BLOCK_G_P1, Bitterroot::BLOCK_G_P2, Bitterroot::BLOCK_G_P3},
			{Bitterroot::BLOCK_H_P1, Bitterroot::BLOCK_H_P2, Bitterroot::BLOCK_H_P3},
			{Bitterroot::BLOCK_I_P1, Bitterroot::BLOCK_I_P2, Bitterroot::BLOCK_I_P3}
		};

		int actIds[9] = {
			Bitterroot::BLOCK_A_ACTIVE, Bitterroot::BLOCK_B_ACTIVE, Bitterroot::BLOCK_C_ACTIVE,
			Bitterroot::BLOCK_D_ACTIVE, Bitterroot::BLOCK_E_ACTIVE, Bitterroot::BLOCK_F_ACTIVE,
			Bitterroot::BLOCK_G_ACTIVE, Bitterroot::BLOCK_H_ACTIVE, Bitterroot::BLOCK_I_ACTIVE
		};

		int lightIds[9] = {
			Bitterroot::BLOCK_A_LIGHT, Bitterroot::BLOCK_B_LIGHT, Bitterroot::BLOCK_C_LIGHT,
			Bitterroot::BLOCK_D_LIGHT, Bitterroot::BLOCK_E_LIGHT, Bitterroot::BLOCK_F_LIGHT,
			Bitterroot::BLOCK_G_LIGHT, Bitterroot::BLOCK_H_LIGHT, Bitterroot::BLOCK_I_LIGHT
		};

		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int b = r * 3 + c;
				// 3 Knobs
				addParam(createParamCentered<Trimpot>(mm2px(Vec(colX[c][0], rowY[r])), module, pIds[b][0]));
				addParam(createParamCentered<Trimpot>(mm2px(Vec(colX[c][1], rowY[r])), module, pIds[b][1]));
				addParam(createParamCentered<Trimpot>(mm2px(Vec(colX[c][2], rowY[r])), module, pIds[b][2]));

				// Bypass Switch & LED (LED to the right of the switch)
				float switchCenter = colX[c][1];
				addParam(createParamCentered<CKSS>(mm2px(Vec(switchCenter - 3.0f, switchY[r])), module, actIds[b]));
				addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(switchCenter + 5.0f, switchY[r])), module, lightIds[b]));
			}
		}

		// ---------------- Zone 3: Master Controls & 3x3 LED Matrix ----------------
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(18.00f, 79.50f)), module, Bitterroot::SCAN_X_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(34.00f, 79.50f)), module, Bitterroot::SCAN_Y_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(50.00f, 79.50f)), module, Bitterroot::ROUTE_PARAM));
		addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(55.50f, 79.50f)), module, Bitterroot::ROUTE_LIGHT));

		// 3x3 Central Vector Activity Matrix (Centered at 71.12 mm, 79.50 mm)
		float ledGridX[3] = {67.62f, 71.12f, 74.62f};
		float ledGridY[3] = {76.00f, 79.50f, 83.00f};
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int idx = r * 3 + c;
				addChild(createLightCentered<SmallSimpleLight<GreenLight>>(mm2px(Vec(ledGridX[c], ledGridY[r])), module, Bitterroot::GRID_LED_0 + idx));
			}
		}

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(124.00f, 79.50f)), module, Bitterroot::Z_BLANK_PARAM));

		// ---------------- Zone 4: Patch Bay ----------------
		// Jack Rows 1, 2, 3 (9 CVs each)
		int cvIds[9][3] = {
			{Bitterroot::BLOCK_A_P1_CV, Bitterroot::BLOCK_A_P2_CV, Bitterroot::BLOCK_A_P3_CV},
			{Bitterroot::BLOCK_B_P1_CV, Bitterroot::BLOCK_B_P2_CV, Bitterroot::BLOCK_B_P3_CV},
			{Bitterroot::BLOCK_C_P1_CV, Bitterroot::BLOCK_C_P2_CV, Bitterroot::BLOCK_C_P3_CV},
			{Bitterroot::BLOCK_D_P1_CV, Bitterroot::BLOCK_D_P2_CV, Bitterroot::BLOCK_D_P3_CV},
			{Bitterroot::BLOCK_E_P1_CV, Bitterroot::BLOCK_E_P2_CV, Bitterroot::BLOCK_E_P3_CV},
			{Bitterroot::BLOCK_F_P1_CV, Bitterroot::BLOCK_F_P2_CV, Bitterroot::BLOCK_F_P3_CV},
			{Bitterroot::BLOCK_G_P1_CV, Bitterroot::BLOCK_G_P2_CV, Bitterroot::BLOCK_G_P3_CV},
			{Bitterroot::BLOCK_H_P1_CV, Bitterroot::BLOCK_H_P2_CV, Bitterroot::BLOCK_H_P3_CV},
			{Bitterroot::BLOCK_I_P1_CV, Bitterroot::BLOCK_I_P2_CV, Bitterroot::BLOCK_I_P3_CV}
		};

		float jackRowY[3] = {89.50f, 99.00f, 108.50f};
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				int b = r * 3 + c;
				addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[c][0], jackRowY[r])), module, cvIds[b][0]));
				addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[c][1], jackRowY[r])), module, cvIds[b][1]));
				addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX[c][2], jackRowY[r])), module, cvIds[b][2]));
			}
		}

		// Jack Row 4 (Fixed Y = 118.00 mm): Master I/O
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(13.00f, 118.00f)), module, Bitterroot::IN_X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(27.50f, 118.00f)), module, Bitterroot::IN_Y_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(47.00f, 118.00f)), module, Bitterroot::SCAN_X_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(63.00f, 118.00f)), module, Bitterroot::SCAN_Y_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(79.00f, 118.00f)), module, Bitterroot::ROUTE_CV_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(99.00f, 118.00f)), module, Bitterroot::OUT_X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(114.50f, 118.00f)), module, Bitterroot::OUT_Y_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(130.00f, 118.00f)), module, Bitterroot::OUT_Z_OUTPUT));
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
