#include "plugin.hpp"
#include "core/BitDSPCore.hpp"
#include <cmath>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────────────
// Custom ParamQuantity Subclasses
// ─────────────────────────────────────────────────────────────────────

struct BitIntDisplayParamQuantity : ParamQuantity {
	std::string prefix;
	std::string suffix;
	BitIntDisplayParamQuantity() = default;
	BitIntDisplayParamQuantity(const std::string& p, const std::string& s = "")
		: prefix(p), suffix(s) {}
	std::string getDisplayValueString() override {
		int v = static_cast<int>(std::round(getValue()));
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%d%s", prefix.c_str(), v, suffix.c_str());
		return std::string(buf);
	}
};

struct BitHexMaskParamQuantity : ParamQuantity {
	BitHexMaskParamQuantity() = default;
	std::string getDisplayValueString() override {
		int v = static_cast<int>(std::round(getValue())) & 0x03FF;
		char buf[32];
		snprintf(buf, sizeof(buf), "0x%03X (%d)", v, v);
		return std::string(buf);
	}
};

struct BitPercentParamQuantity : ParamQuantity {
	std::string prefix;
	BitPercentParamQuantity() = default;
	BitPercentParamQuantity(const std::string& p) : prefix(p) {}
	std::string getDisplayValueString() override {
		float v = getValue() * 100.0f;
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%.1f%%", prefix.c_str(), v);
		return std::string(buf);
	}
};

struct BitListParamQuantity : ParamQuantity {
	std::vector<std::string> labels;
	BitListParamQuantity() = default;
	BitListParamQuantity(const std::vector<std::string>& l) : labels(l) {}
	std::string getDisplayValueString() override {
		int idx = static_cast<int>(std::round(getValue()));
		if (idx >= 0 && idx < (int)labels.size()) {
			return labels[idx];
		}
		return std::to_string(idx);
	}
};

// ─────────────────────────────────────────────────────────────────────
// Generic 6HP BitModule Base Class
// ─────────────────────────────────────────────────────────────────────

struct BaseBitModule : Module {
	enum ParamId {
		P1_PARAM,
		P2_PARAM,
		P3_PARAM,
		MIX_PARAM,
		P1_TRIM_PARAM,
		P2_TRIM_PARAM,
		P3_TRIM_PARAM,
		MIX_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		P1_CV_INPUT,
		P2_CV_INPUT,
		P3_CV_INPUT,
		MIX_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		X_OUTPUT,
		Y_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bitdsp::VoltageRange voltageRange = bitdsp::RANGE_BIPOLAR_5V;

	BaseBitModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}

	void configBasePorts(const char* p1Name, const char* p2Name, const char* p3Name) {
		// CV Attenuverters (ending in "CV depth")
		char trimBuf[64];
		snprintf(trimBuf, sizeof(trimBuf), "%s CV depth", p1Name);
		configParam<BitPercentParamQuantity>(P1_TRIM_PARAM, -1.f, 1.f, 1.0f, trimBuf);
		snprintf(trimBuf, sizeof(trimBuf), "%s CV depth", p2Name);
		configParam<BitPercentParamQuantity>(P2_TRIM_PARAM, -1.f, 1.f, 1.0f, trimBuf);
		snprintf(trimBuf, sizeof(trimBuf), "%s CV depth", p3Name);
		configParam<BitPercentParamQuantity>(P3_TRIM_PARAM, -1.f, 1.f, 1.0f, trimBuf);
		configParam<BitPercentParamQuantity>(MIX_TRIM_PARAM, -1.f, 1.f, 1.0f, "Mix CV depth");

		// Inputs
		configInput(X_INPUT, "X coordinate input");
		configInput(Y_INPUT, "Y coordinate input");
		char cvBuf[64];
		snprintf(cvBuf, sizeof(cvBuf), "%s CV depth", p1Name);
		configInput(P1_CV_INPUT, cvBuf);
		snprintf(cvBuf, sizeof(cvBuf), "%s CV depth", p2Name);
		configInput(P2_CV_INPUT, cvBuf);
		snprintf(cvBuf, sizeof(cvBuf), "%s CV depth", p3Name);
		configInput(P3_CV_INPUT, cvBuf);
		configInput(MIX_CV_INPUT, "Mix CV depth");

		// Outputs
		configOutput(X_OUTPUT, "X coordinate output");
		configOutput(Y_OUTPUT, "Y coordinate output");
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* vrJ = json_object_get(rootJ, "voltageRange");
		if (vrJ) {
			int vr = json_integer_value(vrJ);
			if (vr >= 0 && vr <= 1) {
				voltageRange = (bitdsp::VoltageRange)vr;
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────
// Generic 6HP BitModuleWidget (Ants Layout Template)
// ─────────────────────────────────────────────────────────────────────

struct BaseBitWidget : ModuleWidget {
	BaseBitWidget(BaseBitModule* module, const char* svgPath) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, svgPath)));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Row 1 Knobs (Center Y = 21.59 mm): P1 (7.62 mm), P2 (22.86 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, BaseBitModule::P1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, BaseBitModule::P2_PARAM));

		// Row 2 Knobs (Center Y = 43.00 mm): P3 (7.62 mm), MIX (22.86 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, BaseBitModule::P3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, BaseBitModule::MIX_PARAM));

		// Attenuverter Row 1 (Center Y = 61.00 mm): P1 Trim (7.62 mm), P2 Trim (22.86 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, BaseBitModule::P1_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 61.00)), module, BaseBitModule::P2_TRIM_PARAM));

		// Attenuverter Row 2 (Center Y = 73.00 mm): P3 Trim (7.62 mm), MIX Trim (22.86 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, BaseBitModule::P3_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.86, 73.00)), module, BaseBitModule::MIX_TRIM_PARAM));

		// Jack Row 1 (Center Y = 89.50 mm): X IN (7.62 mm), Y IN (22.86 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, BaseBitModule::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, BaseBitModule::Y_INPUT));

		// Jack Row 2 (Center Y = 99.00 mm): P1 CV (7.62 mm), P2 CV (22.86 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, BaseBitModule::P1_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, BaseBitModule::P2_CV_INPUT));

		// Jack Row 3 (Center Y = 108.50 mm): P3 CV (7.62 mm), MIX CV (22.86 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, BaseBitModule::P3_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, BaseBitModule::MIX_CV_INPUT));

		// Jack Row 4 (Center Y = 118.00 mm): X OUT (7.62 mm), Y OUT (22.86 mm)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, BaseBitModule::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, BaseBitModule::Y_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		BaseBitModule* module = dynamic_cast<BaseBitModule*>(this->module);
		if (!module) return;

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Voltage Range Standard"));

		const char* vrLabels[] = {
			"Bipolar ±5.0V (Default Eurorack/Laser)",
			"Unipolar 0–10.0V (Video Standard)"
		};

		for (int i = 0; i < 2; ++i) {
			bitdsp::VoltageRange vr = (bitdsp::VoltageRange)i;
			menu->addChild(createCheckMenuItem(vrLabels[i], "",
				[=]() { return module->voltageRange == vr; },
				[=]() { module->voltageRange = vr; }
			));
		}
	}
};

// ─────────────────────────────────────────────────────────────────────
// 1. BitMorton
// ─────────────────────────────────────────────────────────────────────
struct BitMortonModule : BaseBitModule {
	BitMortonModule() {
		configParam<BitIntDisplayParamQuantity>(P1_PARAM, 0.f, 19.f, 0.f, "Morton shift", " bits");
		configParam<BitListParamQuantity>(P2_PARAM, 0.f, 2.f, 0.f, "Morton stride", "", 0.f, 1.f, 0.f)->labels = {"Standard", "Inverted", "2-bit block"};
		configParam<BitPercentParamQuantity>(P3_PARAM, 0.f, 1.f, 0.f, "Morton morph");
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Morton mix");
		configBasePorts("Shift", "Stride", "Morph");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 19.0f;
			float p2 = p2Val + cv2 * 2.0f;
			float p3 = p3Val + cv3;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processMorton(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitMortonWidget : BaseBitWidget {
	BitMortonWidget(BitMortonModule* module) : BaseBitWidget(module, "res/BitMorton.svg") {}
};
Model* modelBitMorton = createModel<BitMortonModule, BitMortonWidget>("BitMorton");

// ─────────────────────────────────────────────────────────────────────
// 2. BitReverse
// ─────────────────────────────────────────────────────────────────────
struct BitReverseModule : BaseBitModule {
	BitReverseModule() {
		configParam<BitIntDisplayParamQuantity>(P1_PARAM, 1.f, 10.f, 10.f, "Reverse width", " bits");
		configParam<BitHexMaskParamQuantity>(P2_PARAM, 0.f, 1023.f, 0.f, "Reverse mask");
		configParam<BitIntDisplayParamQuantity>(P3_PARAM, -5.f, 5.f, 0.f, "Reverse skew");
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Reverse mix");
		configBasePorts("Width", "Offset", "Skew");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 9.0f;
			float p2 = p2Val + cv2 * 1023.0f;
			float p3 = p3Val + cv3 * 5.0f;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processReverse(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitReverseWidget : BaseBitWidget {
	BitReverseWidget(BitReverseModule* module) : BaseBitWidget(module, "res/BitReverse.svg") {}
};
Model* modelBitReverse = createModel<BitReverseModule, BitReverseWidget>("BitReverse");

// ─────────────────────────────────────────────────────────────────────
// 3. BitTranspose
// ─────────────────────────────────────────────────────────────────────
struct BitTransposeModule : BaseBitModule {
	BitTransposeModule() {
		configParam<BitIntDisplayParamQuantity>(P1_PARAM, 0.f, 9.f, 0.f, "Transpose plane A");
		configParam<BitIntDisplayParamQuantity>(P2_PARAM, 0.f, 9.f, 1.f, "Transpose plane B");
		configParam<BitIntDisplayParamQuantity>(P3_PARAM, 0.f, 9.f, 2.f, "Transpose cycle plane");
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Transpose mix");
		configBasePorts("Plane A", "Plane B", "Cycle");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 9.0f;
			float p2 = p2Val + cv2 * 9.0f;
			float p3 = p3Val + cv3 * 9.0f;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processTranspose(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitTransposeWidget : BaseBitWidget {
	BitTransposeWidget(BitTransposeModule* module) : BaseBitWidget(module, "res/BitTranspose.svg") {}
};
Model* modelBitTranspose = createModel<BitTransposeModule, BitTransposeWidget>("BitTranspose");

// ─────────────────────────────────────────────────────────────────────
// 4. BitValanche
// ─────────────────────────────────────────────────────────────────────
struct BitValancheModule : BaseBitModule {
	BitValancheModule() {
		configParam<BitHexMaskParamQuantity>(P1_PARAM, 0.f, 1023.f, 1023.f, "Avalanche mask");
		configParam<BitIntDisplayParamQuantity>(P2_PARAM, 0.f, 4.f, 1.f, "Avalanche shift", " bits");
		configParam<BitPercentParamQuantity>(P3_PARAM, 0.f, 1.f, 0.f, "Avalanche borrow");
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Avalanche mix");
		configBasePorts("Mask", "Shift", "Borrow");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 1023.0f;
			float p2 = p2Val + cv2 * 4.0f;
			float p3 = p3Val + cv3;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processValanche(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitValancheWidget : BaseBitWidget {
	BitValancheWidget(BitValancheModule* module) : BaseBitWidget(module, "res/BitValanche.svg") {}
};
Model* modelBitValanche = createModel<BitValancheModule, BitValancheWidget>("BitValanche");

// ─────────────────────────────────────────────────────────────────────
// 5. BitPermute
// ─────────────────────────────────────────────────────────────────────
struct BitPermuteModule : BaseBitModule {
	BitPermuteModule() {
		configParam<BitListParamQuantity>(P1_PARAM, 0.f, 3.f, 0.f, "Permute mode", "", 0.f, 1.f, 0.f)->labels = {
			"Odd/even split", "Perfect shuffle", "Inversion", "Quadrant rot"
		};
		configParam<BitIntDisplayParamQuantity>(P2_PARAM, 0.f, 19.f, 0.f, "Permute rotate", " bits");
		configParam<BitListParamQuantity>(P3_PARAM, 0.f, 5.f, 0.f, "Permute stride", "", 0.f, 1.f, 0.f)->labels = {
			"Stride 1", "Stride 3", "Stride 5", "Stride 7", "Stride 9", "Stride 11"
		};
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Permute mix");
		configBasePorts("Mode", "Rotate", "Stride");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 3.0f;
			float p2 = p2Val + cv2 * 19.0f;
			float p3 = p3Val + cv3 * 5.0f;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processPermute(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitPermuteWidget : BaseBitWidget {
	BitPermuteWidget(BitPermuteModule* module) : BaseBitWidget(module, "res/BitPermute.svg") {}
};
Model* modelBitPermute = createModel<BitPermuteModule, BitPermuteWidget>("BitPermute");

// ─────────────────────────────────────────────────────────────────────
// 6. BitGrayBin
// ─────────────────────────────────────────────────────────────────────
struct BitGrayBinModule : BaseBitModule {
	BitGrayBinModule() {
		configParam<BitIntDisplayParamQuantity>(P1_PARAM, 1.f, 10.f, 10.f, "Gray depth", " bits");
		configParam<BitListParamQuantity>(P2_PARAM, 0.f, 2.f, 0.f, "Gray mode", "", 0.f, 1.f, 0.f)->labels = {"Binary to gray", "Gray to binary", "Dual reflected"};
		configParam<BitIntDisplayParamQuantity>(P3_PARAM, 1.f, 9.f, 1.f, "Gray tap", " bits");
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Gray mix");
		configBasePorts("Depth", "Mode", "Tap");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 9.0f;
			float p2 = p2Val + cv2 * 2.0f;
			float p3 = p3Val + cv3 * 8.0f;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processGrayBin(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitGrayBinWidget : BaseBitWidget {
	BitGrayBinWidget(BitGrayBinModule* module) : BaseBitWidget(module, "res/BitGrayBin.svg") {}
};
Model* modelBitGrayBin = createModel<BitGrayBinModule, BitGrayBinWidget>("BitGrayBin");

// ─────────────────────────────────────────────────────────────────────
// 7. BitGalois
// ─────────────────────────────────────────────────────────────────────
struct BitGaloisModule : BaseBitModule {
	BitGaloisModule() {
		configParam<BitListParamQuantity>(P1_PARAM, 0.f, 7.f, 0.f, "Galois poly", "", 0.f, 1.f, 0.f)->labels = {
			"0x409", "0x481", "0x611", "0x50D", "0x46F", "0x425", "0x679", "0x4D5"
		};
		configParam<BitHexMaskParamQuantity>(P2_PARAM, 1.f, 1023.f, 3.f, "Galois alpha");
		configParam<BitListParamQuantity>(P3_PARAM, 0.f, 3.f, 0.f, "Galois mode", "", 0.f, 1.f, 0.f)->labels = {"Linear", "Inversion", "Cube", "S-box quintic"};
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Galois mix");
		configBasePorts("Poly", "Alpha", "Mode");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 7.0f;
			float p2 = p2Val + cv2 * 1023.0f;
			float p3 = p3Val + cv3 * 3.0f;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processGalois(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitGaloisWidget : BaseBitWidget {
	BitGaloisWidget(BitGaloisModule* module) : BaseBitWidget(module, "res/BitGalois.svg") {}
};
Model* modelBitGalois = createModel<BitGaloisModule, BitGaloisWidget>("BitGalois");

// ─────────────────────────────────────────────────────────────────────
// 8. Bitomata
// ─────────────────────────────────────────────────────────────────────
struct BitomataModule : BaseBitModule {
	BitomataModule() {
		configParam<BitIntDisplayParamQuantity>(P1_PARAM, 0.f, 255.f, 90.f, "Automata rule");
		configParam<BitIntDisplayParamQuantity>(P2_PARAM, 1.f, 4.f, 1.f, "Automata steps", " steps");
		configParam<BitListParamQuantity>(P3_PARAM, 0.f, 2.f, 0.f, "Automata coupling", "", 0.f, 1.f, 0.f)->labels = {"Edge", "Center", "Full XOR"};
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Automata mix");
		configBasePorts("Rule", "Steps", "Inject");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 255.0f;
			float p2 = p2Val + cv2 * 3.0f;
			float p3 = p3Val + cv3 * 2.0f;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processBitomata(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitomataWidget : BaseBitWidget {
	BitomataWidget(BitomataModule* module) : BaseBitWidget(module, "res/Bitomata.svg") {}
};
Model* modelBitomata = createModel<BitomataModule, BitomataWidget>("Bitomata");

// ─────────────────────────────────────────────────────────────────────
// 9. BitHamming
// ─────────────────────────────────────────────────────────────────────
struct BitHammingModule : BaseBitModule {
	BitHammingModule() {
		configParam<BitIntDisplayParamQuantity>(P1_PARAM, 0.f, 32.f, 8.f, "Hamming gain");
		configParam<BitListParamQuantity>(P2_PARAM, 0.f, 2.f, 0.f, "Hamming mode", "", 0.f, 1.f, 0.f)->labels = {"Sign flip", "Shear", "Jump"};
		configParam<BitPercentParamQuantity>(P3_PARAM, 0.f, 1.f, 0.f, "Hamming coupling");
		configParam<BitPercentParamQuantity>(MIX_PARAM, 0.f, 1.f, 0.f, "Hamming mix");
		configBasePorts("Gain", "Mode", "Mutual");
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[X_INPUT].getChannels(), inputs[Y_INPUT].getChannels());
		channels = std::max(channels, 1);
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);

		float p1Val = params[P1_PARAM].getValue();
		float p2Val = params[P2_PARAM].getValue();
		float p3Val = params[P3_PARAM].getValue();
		float mixVal = params[MIX_PARAM].getValue();

		float p1Trim = params[P1_TRIM_PARAM].getValue();
		float p2Trim = params[P2_TRIM_PARAM].getValue();
		float p3Trim = params[P3_TRIM_PARAM].getValue();
		float mixTrim = params[MIX_TRIM_PARAM].getValue();

		for (int c = 0; c < channels; ++c) {
			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = inputs[Y_INPUT].isConnected() ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float cv1 = inputs[P1_CV_INPUT].getPolyVoltage(c) * 0.2f * p1Trim;
			float cv2 = inputs[P2_CV_INPUT].getPolyVoltage(c) * 0.2f * p2Trim;
			float cv3 = inputs[P3_CV_INPUT].getPolyVoltage(c) * 0.2f * p3Trim;
			float cvMix = inputs[MIX_CV_INPUT].getPolyVoltage(c) * 0.1f * mixTrim;

			float p1 = p1Val + cv1 * 32.0f;
			float p2 = p2Val + cv2 * 2.0f;
			float p3 = p3Val + cv3;
			float mix = rack::math::clamp(mixVal + cvMix, 0.0f, 1.0f);

			bitdsp::PointXY out = bitdsp::processHamming(inX, inY, p1, p2, p3, mix, voltageRange);
			outputs[X_OUTPUT].setVoltage(out.x, c);
			outputs[Y_OUTPUT].setVoltage(out.y, c);
		}
	}
};
struct BitHammingWidget : BaseBitWidget {
	BitHammingWidget(BitHammingModule* module) : BaseBitWidget(module, "res/BitHamming.svg") {}
};
Model* modelBitHamming = createModel<BitHammingModule, BitHammingWidget>("BitHamming");
