#include <bitset>

#include "common.hpp"

#include "plugin.hpp"
#include "Common.hpp"
#include "Rainbow.hpp"

#include "dsp/noise.hpp"

extern float exp_1voct[4096];

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
		SCALE_PARAM,
		FREQ_PARAM,
		NOTE_PARAM,
		OCTAVE_PARAM,
		ET_ROOT_PARAM,
		ET_SEMITONE_PARAM,
		ET_CENTS_PARAM,
		JI_ROOT_PARAM,
		JI_UPPER_PARAM,
		JI_LOWER_PARAM,
		SET_FROM_FREQ_PARAM,
		SET_FROM_ET_PARAM,
		SET_FROM_JI_PARAM,
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

	enum slotState {
		LOADED,
		EDITED,
		FRESH
	};

	float currFreqs[NUM_BANKNOTES];
	int currState[NUM_BANKNOTES];
	int currScale;
	int currNote;
	int lastScale;

	int exUpdates = 0;
	bool updated = false;
	float Ctof = 96000.0f / (2.0f * core::PI);
	float ftoC = (2.0f * core::PI) / 96000.0f;

	rack::dsp::SchmittTrigger loadTrigger;
	rack::dsp::SchmittTrigger freqSetTrigger;
	rack::dsp::SchmittTrigger noteSetTrigger;

	const float octaves[11] = {1,2,4,8,16,32,64,128,256,512,1024};

	const float TwelfthRootTwo[12] = {
		1.0, 
		1.05946309436, 
		1.12246204831087, 
		1.1892071150051, 
		1.25992104989823, 
		1.33483985417448, 
		1.41421356237875, 
		1.49830707688367, 
		1.58740105197666, 
		1.68179283051751, 
		1.78179743629254, 
		1.88774862537721};

	RainbowExpander() : core::PrismModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {

		configParam(LOAD_PARAM, 0, 1, 0, "Load scales into Rainbow");
		configParam(SET_FROM_FREQ_PARAM, 0, 1, 0, "Load frequency into slot");
		configParam(SCALE_PARAM, 0, 10, 0, "Select scale from bank");
		configParam(NOTE_PARAM, 0, 20, 0, "Select note in scale");
		configParam(FREQ_PARAM, 0, 20000.0f, 440.0f, "Note frequency");

		configParam(OCTAVE_PARAM, 0, 10, 4, "Octave");

		configParam(ET_ROOT_PARAM, 0, 10, 0, "Root note for interval");
		configParam(ET_SEMITONE_PARAM, 0, 11, 0, "Interval in Semitones");
		configParam(ET_CENTS_PARAM, -100, 100, 0, "Cents");

		configParam(JI_ROOT_PARAM, 0, 10, 0, "Root for JI interval");
		configParam(JI_UPPER_PARAM, 1, 10000, 3, "Ratio numerator");
		configParam(JI_LOWER_PARAM, 1, 10000, 2, "Ratio denominator");

		configParam(SET_FROM_ET_PARAM, 0, 1, 0, "Load ET note into slot");
		configParam(SET_FROM_JI_PARAM, 0, 1, 0, "Load JI note into slot");

		float hz = 100.0f;

		for (int j = 0; j < NUM_SCALES; j++) {
			for (int i = 0; i < NUM_SCALENOTES; i++) {
				// currFreqs[i + j * NUM_SCALENOTES] = expander_default_user_scalebank[i] * Ctof;
				currFreqs[i + j * NUM_SCALENOTES] = hz;
				currState[i + j * NUM_SCALENOTES] = FRESH;
				hz += 20.0f;
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

	currScale = params[SCALE_PARAM].getValue();
	currNote = params[NOTE_PARAM].getValue();

	if (freqSetTrigger.process(params[SET_FROM_FREQ_PARAM].getValue())) {
		currFreqs[currNote + currScale * NUM_SCALENOTES] = params[FREQ_PARAM].getValue();
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;
	} 

	if (noteSetTrigger.process(params[SET_FROM_ET_PARAM].getValue())) {

		int oct = params[OCTAVE_PARAM].getValue();
		int root = params[ET_ROOT_PARAM].getValue();
		int semi = params[ET_SEMITONE_PARAM].getValue();
		float cents = params[ET_CENTS_PARAM].getValue();
		
		float root2 = pow(2.0, (root + semi) / 12.0f);

		float freq = ROOT * octaves[oct] * root2 * pow(2.0f, cents / 1200.0f);

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

	} 

	if (noteSetTrigger.process(params[SET_FROM_JI_PARAM].getValue())) {

		int oct = params[OCTAVE_PARAM].getValue();
		int root = params[JI_ROOT_PARAM].getValue();
		int upper = params[JI_UPPER_PARAM].getValue();
		int lower = params[JI_LOWER_PARAM].getValue();
		
		float cents = 100.0;
		
		float freq = ROOT * octaves[oct] * pow(2.0f, cents / 1200.0f);

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

	} 


	PrismModule::step();

	if (leftExpander.module && leftExpander.module->model == modelRainbow) {
		RainbowExpanderMessage *pM = (RainbowExpanderMessage*)leftExpander.module->rightExpander.producerMessage;
		if (updated) {
			for (int i = 0; i < NUM_BANKNOTES; i++) {
				pM->coeffs[i] = currFreqs[i] * ftoC;
				currState[i] = LOADED;
			}
			pM->updated = true;
		} else {
			pM->updated = false;
		}
		leftExpander.module->rightExpander.messageFlipRequested = true;
	}
}

struct FrquencyDisplay : TransparentWidget {
	
	RainbowExpander *module;
	std::shared_ptr<Font> font;
	
	FrquencyDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/BarlowCondensed-Bold.ttf"));
	}

	void draw(const DrawArgs &ctx) override {
		if (!module->stepX % 60 != 0) {
			return;
		}

		nvgFontSize(ctx.vg, 14);
		nvgFontFaceId(ctx.vg, font->handle);
		nvgTextLetterSpacing(ctx.vg, -1);

		char text[128];

		for (int i = 0; i < NUM_SCALENOTES; i++) {
			int index = i + module->currScale * NUM_SCALENOTES;

			switch(module->currState[i]) {
				case RainbowExpander::LOADED:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0xFF, 0x80, 0xFF));
					break;
				case RainbowExpander::EDITED:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0x80, 0xFF, 0xFF));
					break;
				case RainbowExpander::FRESH:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0xFF, 0xFF, 0xFF));
					break;
				default:
					nvgFillColor(ctx.vg, nvgRGBA(0xFF, 0x80, 0x80, 0xFF));
			}

			if (module->currNote == i) {
				snprintf(text, sizeof(text), "> %.3f", module->currFreqs[index]);
			} else { 
				snprintf(text, sizeof(text), "%.3f", module->currFreqs[index]);
			}

			nvgText(ctx.vg, box.pos.x, box.pos.y + i * 15, text, NULL);

		}
	}
	
};


struct RainbowExpanderWidget : ModuleWidget {
	
	RainbowExpanderWidget(RainbowExpander *module) {

		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RainbowExpander.svg")));



			addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(35.234, 15.823)), module, RainbowExpander::SCALE_PARAM));
			addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(51.848, 15.823)), module, RainbowExpander::NOTE_PARAM));
			addParam(createParam<gui::FloatReadout>(mm2px(Vec(30.284, 35.774)), module, RainbowExpander::FREQ_PARAM));
			addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(119.792, 39.024)), module, RainbowExpander::SET_FROM_FREQ_PARAM));
			addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(35.234, 59.132)), module, RainbowExpander::OCTAVE_PARAM));
			addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(51.848, 59.132)), module, RainbowExpander::ET_ROOT_PARAM));
			addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(68.463, 59.132)), module, RainbowExpander::ET_SEMITONE_PARAM));
			addParam(createParam<gui::FloatReadout>(mm2px(Vec(80.127, 55.882)), module, RainbowExpander::ET_CENTS_PARAM));
			addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(119.792, 59.132)), module, RainbowExpander::SET_FROM_ET_PARAM));
			addParam(createParam<gui::IntegerReadout>(mm2px(Vec(80.127, 68.582)), module, RainbowExpander::JI_UPPER_PARAM));
			addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(43.382, 76.595)), module, RainbowExpander::JI_ROOT_PARAM));
			addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(119.792, 76.595)), module, RainbowExpander::SET_FROM_JI_PARAM));
			addParam(createParam<gui::IntegerReadout>(mm2px(Vec(80.127, 78.108)), module, RainbowExpander::JI_LOWER_PARAM));
			addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(17.0, 120.069)), module, RainbowExpander::LOAD_PARAM));


		if (module != NULL) {
			FrquencyDisplay *displayW = createWidget<FrquencyDisplay>(mm2px(Vec(5.0f, 3.5f)));
			displayW->box.size = mm2px(Vec(20.0f, 110.6f));
			displayW->module = module;
			addChild(displayW);
		}

	}
};

Model *modelRainbowExpander = createModel<RainbowExpander, RainbowExpanderWidget>("RainbowExpander");
