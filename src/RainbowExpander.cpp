#include <bitset>

#include "common.hpp"

#include "plugin.hpp"
#include "Common.hpp"
#include "Rainbow.hpp"
#include "scales/Scales.hpp"

#include "dsp/noise.hpp"

extern float exp_1voct[4096];

using namespace prism;

struct BaseRainbowExpander : core::PrismModule {

	enum slotState {
		LOADED,
		EDITED,
		FRESH
	};

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

	const float octaves[11] = {1,2,4,8,16,32,64,128,256,512,1024};
	const float Ctof = 96000.0f / (2.0f * core::PI);
	const float ftoC = (2.0f * core::PI) / 96000.0f;

	float currFreqs[NUM_BANKNOTES];
	int currState[NUM_BANKNOTES];
	int currScale = 0;
	int currNote = 0;
	int currBank = 0;

	ScaleSet scales;

	BaseRainbowExpander(int P, int I, int O, int L) : core::PrismModule(P, I, O, L) { }

	json_t *dataToJson() override {

		json_t *rootJ = json_object();

		// userscale
		json_t *userscale_array = json_array();
		for (int i = 0; i < NUM_BANKNOTES; i++) {
			json_t *noteJ = json_real(currFreqs[i]);
			json_array_append_new(userscale_array, noteJ);
		}
		json_object_set_new(rootJ, "userscale",	userscale_array);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		// userscale
		json_t *uscale_array = json_object_get(rootJ, "userscale");
		if (uscale_array) {
			for (int i = 0; i < NUM_BANKNOTES; i++) {
				json_t *noteJ = json_array_get(uscale_array, i);
				if (noteJ) {
					currFreqs[i] = json_real_value(noteJ);
					currState[i] = FRESH;
				}
			}
		}
	}


	void initialise() {
		for (int j = 0; j < NUM_SCALES; j++) {
			for (int i = 0; i < NUM_SCALENOTES; i++) {
				currFreqs[i + j * NUM_SCALENOTES] = expander_default_user_scalebank[i] * Ctof;
				currState[i + j * NUM_SCALENOTES] = FRESH;
			}
		}
	}

	float *bankToCoeff(int bank) {
		float *coeff = (float *)(scales.full[bank]->c_maxq);
		return coeff;
	}
};

struct RainbowExpanderET : BaseRainbowExpander {

	enum ParamIds {
		LOAD_PARAM,
		SCALE_PARAM,
		FREQ_PARAM,
		NOTE_PARAM,
		OCTAVE_PARAM,
		CENTS_PARAM,
		ET_ROOT_PARAM,
		ET_SEMITONE_PARAM,
		ET_EDO_PARAM,
		SET_FROM_FREQ_PARAM,
		SET_FROM_ET_PARAM,
		ROOTA_PARAM,
		BANK_PARAM,
		SWITCHBANK_PARAM,
		ET_DSLOTS_PARAM,
		ET_DSEMITONE_PARAM,
		ET_MAXSTEPS_PARAM,
		LOAD_FROM_ET_PARAM,
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

	rack::dsp::SchmittTrigger loadTrigger;
	rack::dsp::SchmittTrigger freqSetTrigger;
	rack::dsp::SchmittTrigger noteETSetTrigger;
	rack::dsp::SchmittTrigger updateETSetTrigger;
	rack::dsp::SchmittTrigger loadPresetTrigger;

	RainbowExpanderET() : BaseRainbowExpander(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {

		configParam(LOAD_PARAM, 0, 1, 0, "Load scales into Rainbow");
		configParam(SET_FROM_FREQ_PARAM, 0, 1, 0, "Load frequency into slot");
		configParam(SCALE_PARAM, 0, 10, 0, "Select scale from bank");
		configParam(NOTE_PARAM, 0, 20, 0, "Select note in scale");
		configParam(FREQ_PARAM, 0, 20000.0f, 440.0f, "Note frequency");

		configParam(OCTAVE_PARAM, 0, 10, 4, "Octave");
		configParam(ROOTA_PARAM, 400, 500, 440, "Base tuning of A");

		configParam(ET_ROOT_PARAM, 0, 11, 0, "Root note for interval");
		configParam(ET_SEMITONE_PARAM, 0, 11, 0, "Interval in Semitones");
		configParam(ET_EDO_PARAM, 1, 100, 12, "Divisions of octave (EDO)");
		configParam(CENTS_PARAM, -2400, 2400, 0, "Cents");

		configParam(ET_DSLOTS_PARAM, -20, 20, 1, "Slot increment");
		configParam(ET_DSEMITONE_PARAM, 0, 11, 0, "Semitone increment");
		configParam(ET_MAXSTEPS_PARAM, 1, 21, 1, "Max number of steps");

		configParam(BANK_PARAM, 0, 21, 0, "Preset"); 
		configParam(SWITCHBANK_PARAM, 0, 1, 0, "Load preset"); 

		configParam(SET_FROM_ET_PARAM, 0, 1, 0, "Load ET note into slot");
		configParam(LOAD_FROM_ET_PARAM, 0, 1, 0, "Update ET notes in scale");

		initialise();

	}

	void onReset() override {
		initialise();
	}

	void process(const ProcessArgs &args) override {
		PrismModule::step();

		currScale = params[SCALE_PARAM].getValue();
		currNote = params[NOTE_PARAM].getValue();
		currBank = params[BANK_PARAM].getValue();

		float rootA = params[ROOTA_PARAM].getValue() / 32.0f;

		if (freqSetTrigger.process(params[SET_FROM_FREQ_PARAM].getValue())) {
			currFreqs[currNote + currScale * NUM_SCALENOTES] = params[FREQ_PARAM].getValue();
			currState[currNote + currScale * NUM_SCALENOTES] = EDITED;
		} 

		if (noteETSetTrigger.process(params[SET_FROM_ET_PARAM].getValue())) {

			int oct = params[OCTAVE_PARAM].getValue();
			int root = params[ET_ROOT_PARAM].getValue();
			int semi = params[ET_SEMITONE_PARAM].getValue();
			int edo = params[ET_EDO_PARAM].getValue();
			float cents = params[CENTS_PARAM].getValue();
			
			float root2 = pow(2.0, (root + semi) / (float)edo);
			float freq = rootA * octaves[oct] * root2 * pow(2.0f, cents / 1200.0f);

			currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
			currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

			this->moveNote();

		} 

		if (updateETSetTrigger.process(params[LOAD_FROM_ET_PARAM].getValue())) {

			int currPosinBank = currNote + currScale * NUM_SCALENOTES;
			int currST = params[ET_ROOT_PARAM].getValue();
			int octave = params[OCTAVE_PARAM].getValue();
			int edo = params[ET_EDO_PARAM].getValue();
			float cents = params[CENTS_PARAM].getValue();

			int nStepsinBank = params[ET_DSLOTS_PARAM].getValue();
			int nSemitones = params[ET_DSEMITONE_PARAM].getValue();
			int maxSteps = params[ET_MAXSTEPS_PARAM].getValue();

			// Only update within current scale
			int minSlot = currScale * NUM_SCALENOTES;
			int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

			for (int i = 0; i < maxSteps; i++) {
				float r2 = pow(2.0, currST / (float)edo);
				float f2 = rootA * octaves[octave] * r2 * pow(2.0f, cents / 1200.0f);

				currFreqs[currPosinBank] = f2;
				currState[currPosinBank] = EDITED;

				currST += nSemitones;				
				currPosinBank += nStepsinBank;

				if (currPosinBank < minSlot || currPosinBank > maxSlot) {
					break;
				} 
			}
		}

		if (loadPresetTrigger.process(params[SWITCHBANK_PARAM].getValue())) {
			float *coeff = bankToCoeff(params[BANK_PARAM].getValue());

			for (int i = 0; i < NUM_BANKNOTES; i++) {
				currFreqs[i] = coeff[i] * Ctof;
				currState[i] = FRESH;
			}
		}

		if (leftExpander.module) {
			if (leftExpander.module->model == modelRainbow) {
				RainbowExpanderMessage *pM = (RainbowExpanderMessage*)leftExpander.module->rightExpander.producerMessage;
				if (loadTrigger.process(params[LOAD_PARAM].getValue())) {
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
	}

	void moveNote(void) {
		int note = params[NOTE_PARAM].getValue();
		if (note < NUM_SCALENOTES - 1) {
			params[NOTE_PARAM].setValue(++note);
		}
	}

};

struct RainbowExpanderJI : BaseRainbowExpander {

	enum ParamIds {
		LOAD_PARAM,
		SCALE_PARAM,
		FREQ_PARAM,
		NOTE_PARAM,
		OCTAVE_PARAM,
		CENTS_PARAM,
		JI_ROOT_PARAM,
		JI_UPPER_PARAM,
		JI_LOWER_PARAM,
		SET_FROM_FREQ_PARAM,
		SET_FROM_JI_PARAM,
		ROOTA_PARAM,
		BANK_PARAM,
		SWITCHBANK_PARAM,
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

	rack::dsp::SchmittTrigger loadTrigger;
	rack::dsp::SchmittTrigger freqSetTrigger;
	rack::dsp::SchmittTrigger noteJISetTrigger;
	rack::dsp::SchmittTrigger loadPresetTrigger;

	RainbowExpanderJI() : BaseRainbowExpander(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {

		configParam(LOAD_PARAM, 0, 1, 0, "Load scales into Rainbow");
		configParam(SET_FROM_FREQ_PARAM, 0, 1, 0, "Load frequency into slot");
		configParam(SCALE_PARAM, 0, 10, 0, "Select scale from bank");
		configParam(NOTE_PARAM, 0, 20, 0, "Select note in scale");
		configParam(FREQ_PARAM, 0, 20000.0f, 440.0f, "Note frequency");

		configParam(OCTAVE_PARAM, 0, 10, 4, "Octave");
		configParam(ROOTA_PARAM, 400, 500, 440, "Base tuning of A");
		configParam(CENTS_PARAM, -2400, 2400, 0, "Cents");

		configParam(BANK_PARAM, 0, 18, 0, "Preset"); 
		configParam(SWITCHBANK_PARAM, 0, 1, 0, "Load preset"); 

		configParam(JI_ROOT_PARAM, 0, 10, 0, "Root for JI interval");
		configParam(JI_UPPER_PARAM, 1, 1000000, 3, "Ratio numerator");
		configParam(JI_LOWER_PARAM, 1, 1000000, 2, "Ratio denominator");

		configParam(SET_FROM_JI_PARAM, 0, 1, 0, "Load JI note into slot");

		initialise();

	}

	void onReset() override {
		initialise();
	}

	void process(const ProcessArgs &args) override {
		PrismModule::step();

		currScale = params[SCALE_PARAM].getValue();
		currNote = params[NOTE_PARAM].getValue();
		currBank = params[BANK_PARAM].getValue();

		float rootA = params[ROOTA_PARAM].getValue() / 32.0f;

		if (freqSetTrigger.process(params[SET_FROM_FREQ_PARAM].getValue())) {
			currFreqs[currNote + currScale * NUM_SCALENOTES] = params[FREQ_PARAM].getValue();
			currState[currNote + currScale * NUM_SCALENOTES] = EDITED;
		} 

		if (noteJISetTrigger.process(params[SET_FROM_JI_PARAM].getValue())) {

			int oct = params[OCTAVE_PARAM].getValue();
			int root = params[JI_ROOT_PARAM].getValue();
			float upper = params[JI_UPPER_PARAM].getValue();
			float lower = params[JI_LOWER_PARAM].getValue();
			float cents = params[CENTS_PARAM].getValue();
			
			float freq0 = rootA * pow(2,root/12.0);		
			float freq = freq0 * octaves[oct] * (upper / lower) * pow(2.0f, cents / 1200.0f);

			currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
			currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

			this->moveNote();

		} 

		if (loadPresetTrigger.process(params[SWITCHBANK_PARAM].getValue())) {
			float *coeff = bankToCoeff(params[BANK_PARAM].getValue());

			for (int i = 0; i < NUM_BANKNOTES; i++) {
				currFreqs[i] = coeff[i] * Ctof;
				currState[i] = FRESH;
			}
		}

		if (leftExpander.module) {
			if (leftExpander.module->model == modelRainbow) {
				RainbowExpanderMessage *pM = (RainbowExpanderMessage*)leftExpander.module->rightExpander.producerMessage;
				if (loadTrigger.process(params[LOAD_PARAM].getValue())) {
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
	}

	void moveNote(void) {
		int note = params[NOTE_PARAM].getValue();
		if (note < NUM_SCALENOTES - 1) {
			params[NOTE_PARAM].setValue(++note);
		}
	}

};


struct FrequencyDisplay : TransparentWidget {
	
	BaseRainbowExpander *module;
	std::shared_ptr<Font> font;
	
	FrequencyDisplay() {
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

			switch(module->currState[index]) {
				case BaseRainbowExpander::LOADED:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0xFF, 0x80, 0xFF));
					break;
				case BaseRainbowExpander::EDITED:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0x80, 0xFF, 0xFF));
					break;
				case BaseRainbowExpander::FRESH:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0xFF, 0xFF, 0xFF));
					break;
				default:
					nvgFillColor(ctx.vg, nvgRGBA(0xFF, 0x80, 0x80, 0xFF));
			}

			if (module->currNote == i) {
				snprintf(text, sizeof(text), "> %02d   %.3f", i+1, module->currFreqs[index]);
			} else { 
				snprintf(text, sizeof(text), "%02d   %.3f", i+1, module->currFreqs[index]);
			}

			nvgText(ctx.vg, box.pos.x, box.pos.y + i * 15, text, NULL);

		}
	}
	
};

struct ExpanderBankWidget : Widget {

	std::shared_ptr<Font> font;

	ExpanderBankWidget() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/BarlowCondensed-Bold.ttf"));
	}

	ScaleSet scales;

	BaseRainbowExpander *module = NULL;

	NVGcolor colors[NUM_SCALEBANKS] = {

		// Shades of Blue
		nvgRGBf( 255.0f/255.0f,	 070.0f/255.0f,  255.0f/255.0f ),
		nvgRGBf( 250.0f/255.0f,	 080.0f/255.0f,  250.0f/255.0f ),
		nvgRGBf( 245.0f/255.0f,	 090.0f/255.0f,  245.0f/255.0f ),
		nvgRGBf( 240.0f/255.0f,	 100.0f/255.0f,  240.0f/255.0f ),
		nvgRGBf( 235.0f/255.0f,	 110.0f/255.0f,  235.0f/255.0f ),
		nvgRGBf( 230.0f/255.0f,	 120.0f/255.0f,  230.0f/255.0f ),
						
		// Shades of Cyan
		nvgRGBf( 150.0f/255.0f,	 255.0f/255.0f,  255.0f/255.0f ),
		nvgRGBf( 130.0f/255.0f,	 245.0f/255.0f,  245.0f/255.0f ),
		nvgRGBf( 120.0f/255.0f,	 235.0f/255.0f,  235.0f/255.0f ),

		// Shades of Yellow
		nvgRGBf( 255.0f/255.0f,	 255.0f/255.0f,  150.0f/255.0f ),
		nvgRGBf( 255.0f/255.0f,	 245.0f/255.0f,  130.0f/255.0f ),
		nvgRGBf( 255.0f/255.0f,	 235.0f/255.0f,  120.0f/255.0f ),
		nvgRGBf( 255.0f/255.0f,	 225.0f/255.0f,  110.0f/255.0f ),

		// Shades of Green	
		nvgRGBf(  588.0f/1023.0f	, 954.0f/1023.0f	, 199.0f/1023.0f	),
		nvgRGBf(  274.0f/1023.0f	, 944.0f/1023.0f	, 67.0f/1023.0f		),
		nvgRGBf(  83.0f/1023.0f		, 934.0f/1023.0f	, 1.0f/1023.0f		),
		nvgRGBf(  1.0f/1023.0f		, 924.0f/1023.0f	, 1.0f/1023.0f		),
		nvgRGBf(  100.0f/1023.0f	, 824.0f/1023.0f	, 9.0f/1023.0f		),
		nvgRGBf(  100.0f/1023.0f	, 724.0f/1023.0f	, 4.0f/1023.0f		),

		// User Scale
		nvgRGBf( 900.0f/1023.0f		, 900.0f/1023.0f	, 900.0f/1023.0f	)
	};

	// Gamma
	NVGcolor extraColour = nvgRGBf(245.0f/255.0f,  245.0f/255.0f, 090.0f/255.0f );

	void draw(const DrawArgs &ctx) override {

		if (module == NULL) {
			return;
		}

		nvgFontSize(ctx.vg, 17.0f);
		nvgFontFaceId(ctx.vg, font->handle);

		char text[128];
		int index = module->currBank;
		if (index < NUM_SCALEBANKS) {
			nvgFillColor(ctx.vg, colors[index]);
		} else {
			nvgFillColor(ctx.vg, extraColour);
		}

		snprintf(text, sizeof(text), "%s", scales.full[index]->name.c_str());
		nvgText(ctx.vg, 0, box.pos.y, text, NULL);

	}

};

struct RainbowExpanderETWidget : ModuleWidget {
	
	RainbowExpanderETWidget(RainbowExpanderET *module) {

		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RainbowExpanderET.svg")));

		addParam(createParam<gui::IntegerReadout>(mm2px(Vec(35.686, 16.963)), module, RainbowExpanderET::ROOTA_PARAM));
		addParam(createParam<gui::IntegerReadout>(mm2px(Vec(52.09, 16.963)), module, RainbowExpanderET::ET_EDO_PARAM));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(30.284, 45.299)), module, RainbowExpanderET::FREQ_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(119.792, 48.549)), module, RainbowExpanderET::SET_FROM_FREQ_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(35.234, 68.657)), module, RainbowExpanderET::OCTAVE_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(52.998, 68.657)), module, RainbowExpanderET::ET_ROOT_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(70.763, 68.657)), module, RainbowExpanderET::ET_SEMITONE_PARAM));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(83.577, 65.407)), module, RainbowExpanderET::CENTS_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(119.792, 68.657)), module, RainbowExpanderET::SET_FROM_ET_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(5.849, 123.401)), module, RainbowExpanderET::NOTE_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(24.121, 123.401)), module, RainbowExpanderET::SCALE_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(43.246, 123.401)), module, RainbowExpanderET::LOAD_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(77.326, 123.401)), module, RainbowExpanderET::BANK_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(127.863, 123.401)), module, RainbowExpanderET::SWITCHBANK_PARAM));

		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(35.234, 88.765)), module, RainbowExpanderET::ET_DSLOTS_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(52.998, 88.765)), module, RainbowExpanderET::ET_DSEMITONE_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(70.763, 88.765)), module, RainbowExpanderET::ET_MAXSTEPS_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(119.792, 88.765)), module, RainbowExpanderET::LOAD_FROM_ET_PARAM));

		if (module != NULL) {
			FrequencyDisplay *displayW = createWidget<FrequencyDisplay>(mm2px(Vec(5.0f, 3.5f)));
			displayW->box.size = mm2px(Vec(20.0f, 110.6f));
			displayW->module = module;
			addChild(displayW);

			ExpanderBankWidget *bankW = new ExpanderBankWidget();
			bankW->module = module;
			bankW->box.pos = mm2px(Vec(86.5f, 62.5f));
			bankW->box.size = Vec(80.0, 20.0f);
			addChild(bankW);
		}
	}
};

struct RainbowExpanderJIWidget : ModuleWidget {
	
	RainbowExpanderJIWidget(RainbowExpanderJI *module) {

		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RainbowExpanderJI.svg")));

		addParam(createParam<gui::IntegerReadout>(mm2px(Vec(35.686, 16.963)), module, RainbowExpanderJI::ROOTA_PARAM));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(30.284, 45.299)), module, RainbowExpanderJI::FREQ_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(119.792, 48.549)), module, RainbowExpanderJI::SET_FROM_FREQ_PARAM));
		addParam(createParam<gui::IntegerReadout>(mm2px(Vec(64.563, 78.108)), module, RainbowExpanderJI::JI_UPPER_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(35.234, 86.12)), module, RainbowExpanderJI::OCTAVE_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(52.998, 86.12)), module, RainbowExpanderJI::JI_ROOT_PARAM));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(83.577, 82.87)), module, RainbowExpanderJI::CENTS_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(119.792, 86.12)), module, RainbowExpanderJI::SET_FROM_JI_PARAM));
		addParam(createParam<gui::IntegerReadout>(mm2px(Vec(64.563, 87.633)), module, RainbowExpanderJI::JI_LOWER_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(5.849, 123.401)), module, RainbowExpanderJI::NOTE_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(24.121, 123.401)), module, RainbowExpanderJI::SCALE_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(43.246, 123.401)), module, RainbowExpanderJI::LOAD_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(77.326, 123.401)), module, RainbowExpanderJI::BANK_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(127.863, 123.401)), module, RainbowExpanderJI::SWITCHBANK_PARAM));

		if (module != NULL) {
			FrequencyDisplay *displayW = createWidget<FrequencyDisplay>(mm2px(Vec(5.0f, 3.5f)));
			displayW->box.size = mm2px(Vec(20.0f, 110.6f));
			displayW->module = module;
			addChild(displayW);

			ExpanderBankWidget *bankW = new ExpanderBankWidget();
			bankW->module = module;
			bankW->box.pos = mm2px(Vec(86.5f, 62.5f));
			bankW->box.size = Vec(80.0, 20.0f);
			addChild(bankW);
		}
	}
};

Model *modelRainbowExpanderET = createModel<RainbowExpanderET, RainbowExpanderETWidget>("RainbowExpanderET");
Model *modelRainbowExpanderJI = createModel<RainbowExpanderJI, RainbowExpanderJIWidget>("RainbowExpanderJI");
