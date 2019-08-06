#include <bitset>

#include "common.hpp"

#include "plugin.hpp"
#include "Common.hpp"
#include "Rainbow.hpp"

#include "dsp/noise.hpp"

using namespace prism;

struct RainbowExpander : core::PrismModule {

	float expander_default_user_scalebank[NUM_SCALENOTES] = {
		0.02094395102393198,
		0.0221893431599245,
		0.02350879016601388,
		0.02490669557392844,
		0.02638772476301919,
		0.02795682053052971,
		0.02961921958772248,
		0.03138047003691591,
		0.03324644988776009,
		0.03522338667454755,
		0.03731787824003011,
		0.03953691475510571,
		0.04188790204786397,
		0.04437868631984903,
		0.04701758033202778,
		0.0498133911478569,
		0.0527754495260384,
		0.05591364106105944,
		0.05923843917544499,
		0.06276094007383184,
		0.06649289977552018
	};

	enum ParamIds {
		LOAD_PARAM,
		ENUMS(FREQ_PARAM,NUM_SCALENOTES),
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float currCoeffs[NUM_BANKNOTES];
	int exUpdates = 0;
	bool updated = false;
	float Ctof = 96000.0f / (2.0f * core::PI);

	rack::dsp::SchmittTrigger loadTrigger;

	RainbowExpander() : core::PrismModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {

		configParam(LOAD_PARAM, 0, 1, 0, "Load scales into Rainbow");

		for (int i = 0; i < NUM_SCALENOTES; i++) {
			float f = expander_default_user_scalebank[i] * Ctof;
			configParam(FREQ_PARAM + i, 0, 20000.0, f, "Note frequency");
		}

		for (int j = 0; j < NUM_SCALES; j++) {
			for (int i = 0; i < NUM_SCALENOTES; i++) {
				currCoeffs[i + j * NUM_SCALENOTES] = expander_default_user_scalebank[i];
			}
		}

	}

	void onReset() override {
	}

	void process(const ProcessArgs &args) override;

};

void RainbowExpander::process(const ProcessArgs &args) {

	updated = false;
	if (loadTrigger.process(params[LOAD_PARAM].getValue())) {
		updated = true;
	} 

	PrismModule::step();

	if (leftExpander.module && leftExpander.module->model == modelRainbow) {
		RainbowExpanderMessage *pM = (RainbowExpanderMessage*)leftExpander.module->rightExpander.producerMessage;
		if (updated) {
			for (int i = 0; i < NUM_BANKNOTES; i++) {
				pM->coeffs[i] = currCoeffs[i];
			}
			pM->updated = true;
		} else {
			pM->updated = false;
		}
		leftExpander.module->rightExpander.messageFlipRequested = true;
	}
}

struct RainbowExpanderWidget : ModuleWidget {
	
	RainbowExpanderWidget(RainbowExpander *module) {

		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RainbowExpander.svg")));

		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(68.7, 15.77)), module, RainbowExpander::LOAD_PARAM));
		addParam(createParamCentered<gui::PrismReadoutParam>(mm2px(Vec(68.7, 35.77)), module, RainbowExpander::FREQ_PARAM));


	}
};

Model *modelRainbowExpander = createModel<RainbowExpander, RainbowExpanderWidget>("RainbowExpander");
