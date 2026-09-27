#include "plugin.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

static constexpr int MAX_BUF = 2048;
static constexpr int DEFAULT_BUF = 512;

// ── 1/2 Size 2-Position Toggle Switch (Tiny like Chromance) ────────
struct SmallCKSS : app::SvgSwitch {
	SmallCKSS() {
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSS_0.svg")));
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSS_1.svg")));
	}
	void draw(const DrawArgs& args) override {
		nvgSave(args.vg);
		nvgTranslate(args.vg, box.size.x * 0.25f, box.size.y * 0.25f);
		nvgScale(args.vg, 0.5f, 0.5f);
		app::SvgSwitch::draw(args);
		nvgRestore(args.vg);
	}
};

struct CopycatChannel {
	float bufferX[MAX_BUF] = {};
	float bufferY[MAX_BUF] = {};
	int writePos = 0;

	int samplesSinceZeroCross = 0;
	float prevInX = 0.f;
	int currentPeriod = DEFAULT_BUF;
	float smoothedPeriod = (float)DEFAULT_BUF;

	float framePhase = 0.f;

	void reset() {
		std::fill(bufferX, bufferX + MAX_BUF, 0.f);
		std::fill(bufferY, bufferY + MAX_BUF, 0.f);
		writePos = 0;
		samplesSinceZeroCross = 0;
		prevInX = 0.f;
		currentPeriod = DEFAULT_BUF;
		smoothedPeriod = (float)DEFAULT_BUF;
		framePhase = 0.f;
	}
};

struct Copycat : Module {
	enum ParamId {
		COPIES_PARAM,
		SCALE_PARAM,
		SHIFT_PARAM,
		ROTATE_PARAM,
		REFLECT_X_PARAM,
		REFLECT_Y_PARAM,
		COPIES_TRIM_PARAM,
		SCALE_TRIM_PARAM,
		SHIFT_TRIM_PARAM,
		ROTATE_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_INPUT,
		Y_INPUT,
		COPIES_CV_INPUT,
		SCALE_CV_INPUT,
		SHIFT_CV_INPUT,
		ROTATE_CV_INPUT,
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

	CopycatChannel channels[16];

	Copycat() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// COPIES Param (1 to 8 discrete copies, default 1)
		configParam(COPIES_PARAM, 1.f, 8.f, 1.f, "Copies", "", 0.f, 1.f, 0.f);
		paramQuantities[COPIES_PARAM]->snapEnabled = true;

		// SCALE Param (Scale Cascade, range -1.0 to +1.0, default 0.0)
		configParam(SCALE_PARAM, -1.f, 1.f, 0.f, "Scale Cascade", "%", 0.f, 100.f);

		// SHIFT Param (0% to 100%, default 0%)
		configParam(SHIFT_PARAM, 0.f, 1.f, 0.f, "Shift", "%", 0.f, 100.f);

		// ROTATE Param (-1.0 to +1.0 turn, default 0)
		configParam(ROTATE_PARAM, -1.f, 1.f, 0.f, "Rotation Offset", " turns", 0.f, 1.f);

		// Reflect Switches
		configSwitch(REFLECT_X_PARAM, 0.f, 1.f, 0.f, "Reflect X Axis", {"Off", "On"});
		configSwitch(REFLECT_Y_PARAM, 0.f, 1.f, 0.f, "Reflect Y Axis", {"Off", "On"});

		// Attenuverters (default 0%)
		configParam(COPIES_TRIM_PARAM, -1.f, 1.f, 0.f, "Copies CV depth", "%", 0.f, 100.f);
		configParam(SCALE_TRIM_PARAM, -1.f, 1.f, 0.f, "Scale CV depth", "%", 0.f, 100.f);
		configParam(SHIFT_TRIM_PARAM, -1.f, 1.f, 0.f, "Shift CV depth", "%", 0.f, 100.f);
		configParam(ROTATE_TRIM_PARAM, -1.f, 1.f, 0.f, "Rotation CV depth", "%", 0.f, 100.f);

		// Inputs
		configInput(X_INPUT, "X Signal");
		configInput(Y_INPUT, "Y Signal (Normalizes from X)");
		configInput(COPIES_CV_INPUT, "Copies CV");
		configInput(SCALE_CV_INPUT, "Scale CV");
		configInput(SHIFT_CV_INPUT, "Shift CV");
		configInput(ROTATE_CV_INPUT, "Rotation CV");

		// Outputs
		configOutput(X_OUTPUT, "X Signal");
		configOutput(Y_OUTPUT, "Y Signal");

		for (int c = 0; c < 16; c++) {
			channels[c].reset();
		}
	}

	void process(const ProcessArgs& args) override {
		int xChannels = inputs[X_INPUT].getChannels();
		int yChannels = inputs[Y_INPUT].getChannels();
		int copiesCvChannels = inputs[COPIES_CV_INPUT].getChannels();
		int scaleCvChannels = inputs[SCALE_CV_INPUT].getChannels();
		int shiftCvChannels = inputs[SHIFT_CV_INPUT].getChannels();
		int rotCvChannels = inputs[ROTATE_CV_INPUT].getChannels();

		int numChannels = std::max({xChannels, yChannels, copiesCvChannels, scaleCvChannels, shiftCvChannels, rotCvChannels, 1});

		outputs[X_OUTPUT].setChannels(numChannels);
		outputs[Y_OUTPUT].setChannels(numChannels);

		float copiesParam = params[COPIES_PARAM].getValue();
		float scaleParam = params[SCALE_PARAM].getValue();
		float shiftParam = params[SHIFT_PARAM].getValue();
		float rotParam = params[ROTATE_PARAM].getValue();

		float copiesTrim = params[COPIES_TRIM_PARAM].getValue();
		float scaleTrim = params[SCALE_TRIM_PARAM].getValue();
		float shiftTrim = params[SHIFT_TRIM_PARAM].getValue();
		float rotTrim = params[ROTATE_TRIM_PARAM].getValue();

		bool reflectX = params[REFLECT_X_PARAM].getValue() > 0.5f;
		bool reflectY = params[REFLECT_Y_PARAM].getValue() > 0.5f;

		bool yInputConnected = inputs[Y_INPUT].isConnected();

		for (int c = 0; c < numChannels; c++) {
			auto& chan = channels[c];

			float inX = inputs[X_INPUT].getPolyVoltage(c);
			float inY = yInputConnected ? inputs[Y_INPUT].getPolyVoltage(c) : inX;

			float copiesCV = inputs[COPIES_CV_INPUT].getPolyVoltage(c) / 5.f;
			float scaleCV = inputs[SCALE_CV_INPUT].getPolyVoltage(c) / 5.f;
			float shiftCV = inputs[SHIFT_CV_INPUT].getPolyVoltage(c) / 5.f;
			float rotCV = inputs[ROTATE_CV_INPUT].getPolyVoltage(c) / 5.f;

			float rawCopies = copiesParam + copiesCV * copiesTrim * 3.5f;
			int numCopies = std::max(1, std::min(8, (int)std::round(rawCopies)));

			float shift = clamp(shiftParam + shiftCV * shiftTrim, 0.f, 1.f);
			float rotation = rotParam + rotCV * rotTrim;
			float scaleVal = clamp(scaleParam + scaleCV * scaleTrim, -1.f, 1.f);

			// Write incoming audio vector to ring buffer
			chan.bufferX[chan.writePos] = inX;
			chan.bufferY[chan.writePos] = inY;

			// Auto-sync zero-crossing period detector for periodic shapes
			if (chan.prevInX <= 0.f && inX > 0.05f && chan.samplesSinceZeroCross > 64) {
				chan.currentPeriod = chan.samplesSinceZeroCross;
				chan.samplesSinceZeroCross = 0;
			}
			chan.samplesSinceZeroCross++;
			chan.prevInX = inX;

			int targetPeriod = std::max(128, std::min(1024, chan.currentPeriod));
			chan.smoothedPeriod += 0.005f * (targetPeriod - chan.smoothedPeriod);
			int bufLen = std::max(128, std::min(1024, (int)std::round(chan.smoothedPeriod)));

			chan.writePos = (chan.writePos + 1) % bufLen;

			// Pass-through when 1 copy or 0% shift
			if (numCopies <= 1 || shift <= 0.001f) {
				outputs[X_OUTPUT].setVoltage(clamp(inX, -12.f, 12.f), c);
				outputs[Y_OUTPUT].setVoltage(clamp(inY, -12.f, 12.f), c);
				continue;
			}

			// Sub-block time-compressed shape duplication
			chan.framePhase += 1.0f;
			if (chan.framePhase >= bufLen) {
				chan.framePhase -= bufLen;
			}

			float subBlockLen = (float)bufLen / (float)numCopies;
			int copyIdx = (int)(chan.framePhase / subBlockLen);
			copyIdx = std::max(0, std::min(numCopies - 1, copyIdx));

			float subPhase = (chan.framePhase - copyIdx * subBlockLen) / subBlockLen;
			subPhase = clamp(subPhase, 0.f, 1.f);

			auto sampleBuffer = [&](float phase) -> std::pair<float, float> {
				float readPos = phase * bufLen;
				float readOffset = bufLen - readPos;
				float readIndexFloat = chan.writePos - readOffset;
				while (readIndexFloat < 0.f) readIndexFloat += bufLen;
				while (readIndexFloat >= bufLen) readIndexFloat -= bufLen;

				int idx0 = (int)readIndexFloat;
				int idx1 = (idx0 + 1) % bufLen;
				float frac = readIndexFloat - idx0;

				float sx = (1.f - frac) * chan.bufferX[idx0] + frac * chan.bufferX[idx1];
				float sy = (1.f - frac) * chan.bufferY[idx0] + frac * chan.bufferY[idx1];
				return {sx, sy};
			};

			auto getCopyPosition = [&](int copyK, float phase) -> std::pair<float, float> {
				auto s = sampleBuffer(phase);
				float sx = s.first;
				float sy = s.second;

				// Scale Cascade (Zoom)
				float scaleK = std::max(0.05f, 1.0f + copyK * scaleVal * 0.25f);
				sx *= scaleK;
				sy *= scaleK;

				// Apply axis reflection toggle to alternating (odd) copies
				if (copyK % 2 == 1) {
					if (reflectX) sx = -sx;
					if (reflectY) sy = -sy;
				}

				float shiftVolts = shift * 5.f;
				float dx = 0.f;
				float dy = 0.f;

				if (numCopies == 2) {
					// Symmetrical placement beside each other across origin
					float angle = rotation * 2.f * (float)M_PI + (copyK == 0 ? 0.f : (float)M_PI);
					dx = shiftVolts * std::cos(angle);
					dy = shiftVolts * std::sin(angle);
				} else if (numCopies > 2) {
					// Polygon vertex placement around a circle of radius shiftVolts
					float angle = rotation * 2.f * (float)M_PI + copyK * (2.f * (float)M_PI / (float)numCopies);
					dx = shiftVolts * std::cos(angle);
					dy = shiftVolts * std::sin(angle);
				}

				return {sx + dx, sy + dy};
			};

			auto currentPos = getCopyPosition(copyIdx, subPhase);
			float outX = currentPos.first;
			float outY = currentPos.second;

			// Smooth blanking slew transition between sub-blocks
			float samplesToSubEnd = (copyIdx + 1) * subBlockLen - chan.framePhase;
			float transLen = std::min(8.0f, subBlockLen * 0.2f);

			if (samplesToSubEnd < transLen) {
				int nextCopyIdx = (copyIdx + 1) % numCopies;
				auto nextPos = getCopyPosition(nextCopyIdx, 0.f);

				float normT = 1.0f - (samplesToSubEnd / transLen);
				float blend = 0.5f * (1.0f - std::cos(normT * (float)M_PI));

				outX = (1.0f - blend) * outX + blend * nextPos.first;
				outY = (1.0f - blend) * outY + blend * nextPos.second;
			}

			// Safety clamping for laser galvo drives (-12V to +12V)
			outputs[X_OUTPUT].setVoltage(clamp(outX, -12.f, 12.f), c);
			outputs[Y_OUTPUT].setVoltage(clamp(outY, -12.f, 12.f), c);
		}
	}
};

struct CopycatWidget : ModuleWidget {
	CopycatWidget(Copycat* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Copycat.svg")));

		// 6HP Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Main Controls
		// Row 1: COPIES (X = 7.62 mm) & SCALE (X = 22.86 mm) Knobs (Center Y = 21.59 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 21.59)), module, Copycat::COPIES_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 21.59)), module, Copycat::SCALE_PARAM));

		// Row 2: SHIFT (X = 7.62 mm) & ROTATE (X = 22.86 mm) Knobs (Center Y = 43.00 mm)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 43.00)), module, Copycat::SHIFT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.86, 43.00)), module, Copycat::ROTATE_PARAM));

		// Attenuverter Trimpots & Tiny Toggle Switches
		// Trimpot Row 1: COPIES CV (X = 7.62 mm), SCALE CV (X = 15.24 mm), FLIP X Toggle (X = 22.86 mm) (Center Y = 61.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 61.00)), module, Copycat::COPIES_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(15.24, 61.00)), module, Copycat::SCALE_TRIM_PARAM));
		addParam(createParamCentered<SmallCKSS>(mm2px(Vec(22.86, 61.00)), module, Copycat::REFLECT_X_PARAM));

		// Trimpot Row 2: SHIFT CV (X = 7.62 mm), ROTATE CV (X = 15.24 mm), FLIP Y Toggle (X = 22.86 mm) (Center Y = 73.00 mm)
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.62, 73.00)), module, Copycat::SHIFT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(15.24, 73.00)), module, Copycat::ROTATE_TRIM_PARAM));
		addParam(createParamCentered<SmallCKSS>(mm2px(Vec(22.86, 73.00)), module, Copycat::REFLECT_Y_PARAM));

		// Bottom I/O Jacks
		// Row 1: Signal Inputs (Center Y = 89.50 mm): X IN (7.62), Y IN (22.86)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.50)), module, Copycat::X_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 89.50)), module, Copycat::Y_INPUT));

		// Row 2: COPIES CV (X = 7.62 mm), SCALE CV (X = 22.86 mm) (Center Y = 99.00 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 99.00)), module, Copycat::COPIES_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 99.00)), module, Copycat::SCALE_CV_INPUT));

		// Row 3: SHIFT CV (X = 7.62 mm), ROTATE CV (X = 22.86 mm) (Center Y = 108.50 mm)
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 108.50)), module, Copycat::SHIFT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 108.50)), module, Copycat::ROTATE_CV_INPUT));

		// Row 4: Signal Outputs (Center Y = 118.00 mm): X OUT (7.62), Y OUT (22.86)
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 118.00)), module, Copycat::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 118.00)), module, Copycat::Y_OUTPUT));
	}
};

Model* modelCopycat = createModel<Copycat, CopycatWidget>("Copycat");
