#include "plugin.hpp"
#include <cstdio>

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	printf("[Garmire] === init() started ===\n");

	#define REGISTER_MODEL(m, name) \
		do { \
			printf("[Garmire] Registering " name " (model=%p)...\n", (void*)m); \
			if (m) { \
				p->addModel(m); \
			} else { \
				printf("[Garmire] ERROR: " name " model pointer is NULL!\n"); \
			} \
		} while (0)

	// Register modules
	REGISTER_MODEL(modelChromance, "Chromance");
	REGISTER_MODEL(modelAnts, "Ants");
	REGISTER_MODEL(modelHsvRgb, "HsvRgb");
	REGISTER_MODEL(modelOklchRgb, "OklchRgb");
	REGISTER_MODEL(modelRgb, "Rgb");
	REGISTER_MODEL(modelInstability, "Instability");
	REGISTER_MODEL(modelScale, "Scale");
	REGISTER_MODEL(modelPosition, "Position");
	REGISTER_MODEL(modelRotate, "Rotate");
	REGISTER_MODEL(modelSpin, "Spin");
	REGISTER_MODEL(modelFold, "Fold");
	REGISTER_MODEL(modelStretch, "Stretch");
	REGISTER_MODEL(modelShear, "Shear");
	REGISTER_MODEL(modelSmooth, "Smooth");
	REGISTER_MODEL(modelWiggle, "Wiggle");
	REGISTER_MODEL(modelCrest, "Crest");
	REGISTER_MODEL(modelQuake, "Quake");
	REGISTER_MODEL(modelSteps, "Steps");
	REGISTER_MODEL(modelCopycat, "Copycat");
	REGISTER_MODEL(modelPetals, "Petals");
	REGISTER_MODEL(modelKnots, "Knots");
	REGISTER_MODEL(modelVortex, "Vortex");
	REGISTER_MODEL(modelTrough, "Trough");
	REGISTER_MODEL(modelSwitch, "Switch");
	REGISTER_MODEL(modelRoute, "Route");
	REGISTER_MODEL(modelSumMult, "SumMult");
	REGISTER_MODEL(modelSwitchXL, "SwitchXL");
	REGISTER_MODEL(modelRouteXL, "RouteXL");
	REGISTER_MODEL(modelSumMixXL, "SumMixXL");
	REGISTER_MODEL(modelAndxy, "Andxy");
	REGISTER_MODEL(modelOrxy, "Orxy");
	REGISTER_MODEL(modelXorxy, "Xorxy");
	REGISTER_MODEL(modelChopXL, "ChopXL");
	REGISTER_MODEL(modelRescale, "Rescale");
	REGISTER_MODEL(modelBleed, "Bleed");
	REGISTER_MODEL(modelLisa, "Lisa");
	REGISTER_MODEL(modelDaisy, "Daisy");
	REGISTER_MODEL(modelPolly, "Polly");
	REGISTER_MODEL(modelCirce, "Circe");
	REGISTER_MODEL(modelMaude, "Maude");
	REGISTER_MODEL(modelEunice, "Eunice");
	REGISTER_MODEL(modelSteppedSlew, "SteppedSlew");
	REGISTER_MODEL(modelRgbIVca, "RgbIVca");
	REGISTER_MODEL(modelMatrix3x3, "Matrix3x3");
	REGISTER_MODEL(modelBitMorton, "BitMorton");
	REGISTER_MODEL(modelBitReverse, "BitReverse");
	REGISTER_MODEL(modelBitTranspose, "BitTranspose");
	REGISTER_MODEL(modelBitValanche, "BitValanche");
	REGISTER_MODEL(modelBitPermute, "BitPermute");
	REGISTER_MODEL(modelBitGrayBin, "BitGrayBin");
	REGISTER_MODEL(modelBitGalois, "BitGalois");
	REGISTER_MODEL(modelBitomata, "Bitomata");
	REGISTER_MODEL(modelBitHamming, "BitHamming");
	REGISTER_MODEL(modelAmpara, "Ampara");
	REGISTER_MODEL(modelGeorgia, "Georgia");
	REGISTER_MODEL(modelPanopia, "Panopia");
	REGISTER_MODEL(modelPanopticon, "Panopticon");
	REGISTER_MODEL(modelCrissy, "Crissy");
	REGISTER_MODEL(modelZiggy, "Ziggy");
	REGISTER_MODEL(modelAnnette, "Annette");
	REGISTER_MODEL(modelEris, "Eris");
	REGISTER_MODEL(modelAion, "Aion");
	REGISTER_MODEL(modelAionidion, "Aionidion");
	REGISTER_MODEL(modelAiona, "Aiona");
	REGISTER_MODEL(modelBeatrice, "Beatrice");
	REGISTER_MODEL(modelReverie, "Reverie");
	REGISTER_MODEL(modelOctavia, "Octavia");
	REGISTER_MODEL(modelRandi, "Randi");
	REGISTER_MODEL(modelGrainne, "Grainne");
	REGISTER_MODEL(modelLain, "Lain");
	REGISTER_MODEL(modelSalus, "Salus");
	REGISTER_MODEL(modelSerqet, "Serqet");
	REGISTER_MODEL(modelJoy, "Joy");
	REGISTER_MODEL(modelHecate, "Hecate");

	#undef REGISTER_MODEL
	printf("[Garmire] === init() completed successfully! ===\n");
}

