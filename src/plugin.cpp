#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	// Register modules
	p->addModel(modelChromance);
	p->addModel(modelAnts);
	p->addModel(modelHsvRgb);
	p->addModel(modelOklchRgb);
	p->addModel(modelRgb);
	p->addModel(modelInstability);
	p->addModel(modelScale);
	p->addModel(modelPosition);
	p->addModel(modelRotate);
	p->addModel(modelSpin);
	p->addModel(modelFold);
	p->addModel(modelStretch);
	p->addModel(modelShear);
	p->addModel(modelSmooth);
	p->addModel(modelWiggle);
	p->addModel(modelCrest);
	p->addModel(modelQuake);
	p->addModel(modelSteps);
	p->addModel(modelCopycat);
	p->addModel(modelPetals);
	p->addModel(modelKnots);
	p->addModel(modelVortex);
	p->addModel(modelTrough);
	p->addModel(modelSwitch);
	p->addModel(modelRoute);
	p->addModel(modelSumMult);
	p->addModel(modelSwitchXL);
	p->addModel(modelRouteXL);
	p->addModel(modelSumMixXL);
	p->addModel(modelAndxy);
	p->addModel(modelOrxy);
	p->addModel(modelXorxy);
	p->addModel(modelChopXL);
	p->addModel(modelRescale);
	p->addModel(modelBleed);
	p->addModel(modelLisa);
}
