#include <bitset>

#include "common.hpp"

#include "plugin.hpp"
#include "Common.hpp"
#include "Rainbow.hpp"
#include "scales/Scales.hpp"

#include "dsp/noise.hpp"

extern float exp_1voct[4096];

using namespace prism;

struct RainbowScaleExpander : core::PrismModule {

	const static int MAX_OCTAVE = 11;

	enum slotState {
		LOADED,
		EDITED,
		FRESH
	};

	enum ParamIds {
		TRANSFER_PARAM,
		SCALE_PARAM,
		SLOT_PARAM,
		BANK_PARAM,
		BANKLOAD_PARAM,
		PAGE_PARAM,
		SET_PARAM,
		EXECUTE_PARAM,
		ROOTA_PARAM,
		ENUMS(PARAMETER_PARAM, 8),
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

	const float octaves[MAX_OCTAVE] = {1,2,4,8,16,32,64,128,256,512,1024};
	const float CtoF = 96000.0f / (2.0f * core::PI);
	const float FtoC = (2.0f * core::PI) / 96000.0f;

	float currFreqs[NUM_BANKNOTES];
	int currState[NUM_BANKNOTES];
	int currScale = 0;
	int currNote = 0;
	int currBank = 0;

	int currPage = 0; // Freq = 0, ET = 1, JI = 2
	int nextPage = 0;

	float rootA;

	std::string name;
	std::string description;
	std::string scalename[11];
	std::string notedesc[231];

    ScaleSet scales;

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
		for (int j = 0; j < NUM_BANKNOTES; j++) {
			currFreqs[j] = scales.presets[NUM_SCALEBANKS - 1]->c_maxq[j];
			currState[j] = FRESH;
		}
	}

	float *bankToCoeff(int bank) {
		float *coeff = (float *)(scales.full[bank]->c_maxq);
		return coeff;
	}

	rack::dsp::SchmittTrigger transferTrigger;
	rack::dsp::SchmittTrigger loadBankTrigger;
	rack::dsp::SchmittTrigger setTrigger;
	rack::dsp::SchmittTrigger executeTrigger;

	RainbowScaleExpander() : core::PrismModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {

		configParam(TRANSFER_PARAM, 0, 1, 0, "Load scales into Rainbow");
		configParam(SCALE_PARAM, 0, 10, 0, "Select scale from bank");
		configParam(SLOT_PARAM, 0, 20, 0, "Select note in scale");

		configParam(BANK_PARAM, 0, 21, 0, "Preset Bank"); 
		configParam(BANKLOAD_PARAM, 0, 1, 0, "Load preset"); 

		configParam(PAGE_PARAM, 0, 3, 0, "Select page"); 
		configParam(SET_PARAM, 0, 1, 0, "Set frequency"); 
		configParam(EXECUTE_PARAM, 0, 1, 0, "Set multiple frequencies"); 

		configParam(ROOTA_PARAM, 400, 500, 440, "Base tuning of A");

		for (int i = 0; i < 8; i++) {
			configParam(PARAMETER_PARAM + i, -100000, 100000, 0, "Parameter");
		}

		initialise();

	}

	void onReset() override {
		initialise();
	}

	// P0 Octave		/ Octave	/ Octave
	// P1 Frequency 	/ Root 		/ Key
	// P2 				/ Interval 	/ Upper
	// P3  				/ EDO		/ Lower
	// P4 Cents 		/ Cents 	/ Cents
	// P5 Slot step 	/ 			/ 
	// P6 Interval step	/ 			/ 
	// P7 Max steps		/			/

	void setFromFrequency() {
		currFreqs[currNote + currScale * NUM_SCALENOTES] = params[PARAMETER_PARAM + 1].getValue();
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;
	}

	void setFromET() {
		int oct 	= params[PARAMETER_PARAM + 0].getValue();
		int root 	= params[PARAMETER_PARAM + 1].getValue();
		int semi	= params[PARAMETER_PARAM + 2].getValue();
		int edo		= params[PARAMETER_PARAM + 3].getValue();
		float cents	= params[PARAMETER_PARAM + 4].getValue();
		
		float root2 = pow(2.0, (root + semi) / (float)edo);
		float freq = rootA * octaves[oct] * root2 * pow(2.0f, cents / 1200.0f);

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

		this->moveNote();
	}

	void setFromJI() {
		int oct 	= params[PARAMETER_PARAM + 0].getValue();
		int root 	= params[PARAMETER_PARAM + 1].getValue();
		float upper	= params[PARAMETER_PARAM + 2].getValue();
		float lower	= params[PARAMETER_PARAM + 3].getValue();
		float cents	= params[PARAMETER_PARAM + 4].getValue();
		
		float freq0 = rootA * pow(2,root/12.0);		
		float freq = freq0 * octaves[oct] * (upper / lower) * pow(2.0f, cents / 1200.0f);

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

		this->moveNote();
	}

	void executeFromFrequency() {
		int currPosinBank = currNote + currScale * NUM_SCALENOTES;

		int frequency	 	= params[PARAMETER_PARAM + 1].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 5].getValue();
		int nCents	 		= params[PARAMETER_PARAM + 6].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 7].getValue();

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		for (int i = 0; i < maxSteps; i++) {
			float f2 = frequency * pow(2.0f, nCents / 1200.0f);

			currFreqs[currPosinBank] = f2;
			currState[currPosinBank] = EDITED;

			frequency += f2;				
			currPosinBank += nStepsinBank;

			if (currPosinBank < minSlot || currPosinBank > maxSlot) {
				break;
			} 
		}
	}

	void executeFromET() {
		int currPosinBank = currNote + currScale * NUM_SCALENOTES;

		int oct 			= params[PARAMETER_PARAM + 0].getValue();
		int root 			= params[PARAMETER_PARAM + 1].getValue();
		int edo				= params[PARAMETER_PARAM + 3].getValue();
		float cents			= params[PARAMETER_PARAM + 4].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 5].getValue();
		int nSemitones 		= params[PARAMETER_PARAM + 6].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 7].getValue();

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		for (int i = 0; i < maxSteps; i++) {
			float r2 = pow(2.0, root / (float)edo);
			float f2 = rootA * octaves[oct] * r2 * pow(2.0f, cents / 1200.0f);

			currFreqs[currPosinBank] = f2;
			currState[currPosinBank] = EDITED;

			root += nSemitones;				
			currPosinBank += nStepsinBank;

			if (currPosinBank < minSlot || currPosinBank > maxSlot) {
				break;
			} 
		}
	}

	void executeFromJI() {
		int currPosinBank = currNote + currScale * NUM_SCALENOTES;

		int oct 			= params[PARAMETER_PARAM + 0].getValue();
		int root 			= params[PARAMETER_PARAM + 1].getValue();
		float upper			= params[PARAMETER_PARAM + 2].getValue();
		float lower			= params[PARAMETER_PARAM + 3].getValue();
		float cents			= params[PARAMETER_PARAM + 4].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 5].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 7].getValue();

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		for (int i = 0; i < maxSteps; i++) {

			float freq0 = rootA * pow(2,root/12.0);		
			float freq = freq0 * octaves[oct] * (upper / lower) * pow(2.0f, cents / 1200.0f);

			currFreqs[currPosinBank] = freq;
			currState[currPosinBank] = EDITED;

			oct++;
			currPosinBank += nStepsinBank;

			if (currPosinBank < minSlot || currPosinBank > maxSlot) {
				break;
			} 
		}
	}

	void process(const ProcessArgs &args) override {
		PrismModule::step();

		currScale = params[SCALE_PARAM].getValue();
		currNote = params[SLOT_PARAM].getValue();
		currBank = params[BANK_PARAM].getValue();

		rootA = params[ROOTA_PARAM].getValue() / 32.0f;

		if (loadBankTrigger.process(params[BANKLOAD_PARAM].getValue())) {
			int bank = params[BANK_PARAM].getValue();

			name = scales.full[bank]->name;
			description = scales.full[bank]->description;

			for (int i = 0; i < NUM_BANKNOTES; i++) {
				currFreqs[i] = scales.full[bank]->c_maxq[i] * CtoF;
				currState[i] = FRESH;
				notedesc[i] = "Hello!";
				// notedesc[i] = scales.full[bank]->notedesc[i];
			}

			for (int i = 0; i < NUM_SCALES; i++) {
				scalename[i] = scales.full[bank]->scalename[i];
			}

		}

		currPage = params[PAGE_PARAM].getValue();

		if (setTrigger.process(params[SET_PARAM].getValue())) {
			switch(currPage) {
				case 0:
					setFromFrequency();
					break;
				case 1:
					setFromET();
					break;
				case 2:
					setFromJI();
					break;
			}
		}

		if (executeTrigger.process(params[EXECUTE_PARAM].getValue())) {
			switch(currPage) {
				case 0:
					executeFromFrequency();
					break;
				case 1:
					executeFromET();
					break;
				case 2:
					executeFromJI();
					break;
			}
		}

		if (leftExpander.module) {
			if (leftExpander.module->model == modelRainbow) {
				RainbowScaleExpanderMessage *pM = (RainbowScaleExpanderMessage*)leftExpander.module->rightExpander.producerMessage;
				if (transferTrigger.process(params[TRANSFER_PARAM].getValue())) {
					for (int i = 0; i < NUM_BANKNOTES; i++) {
						pM->coeffs[i] = currFreqs[i] * FtoC;
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
		int note = params[SLOT_PARAM].getValue();
		if (note < NUM_SCALENOTES - 1) {
			params[SLOT_PARAM].setValue(++note);
		}
	}

};


struct FrequencyDisplay : TransparentWidget {
	
	RainbowScaleExpander *module;
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
		// nvgTextLetterSpacing(ctx.vg, -1);

		char text[128];

		snprintf(text, sizeof(text), "Bank: %s", module->name.c_str());
		nvgText(ctx.vg, box.pos.x + 7, box.pos.y + 0, text, NULL);

		snprintf(text, sizeof(text), "Scale: %s", module->scalename[module->currScale].c_str());
		nvgText(ctx.vg, box.pos.x + 7, box.pos.y + 15, text, NULL);

		for (int i = 0; i < NUM_SCALENOTES; i++) {
			int index = i + module->currScale * NUM_SCALENOTES;

			switch(module->currState[index]) {
				case RainbowScaleExpander::LOADED:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0xFF, 0x80, 0xFF));
					break;
				case RainbowScaleExpander::EDITED:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0x80, 0xFF, 0xFF));
					break;
				case RainbowScaleExpander::FRESH:
					nvgFillColor(ctx.vg, nvgRGBA(0x80, 0xFF, 0xFF, 0xFF));
					break;
				default:
					nvgFillColor(ctx.vg, nvgRGBA(0xFF, 0x80, 0x80, 0xFF));
			}

			if (module->currNote == i) {
				snprintf(text, sizeof(text), ">");
				nvgText(ctx.vg, box.pos.x + 2, (box.pos.y + 30) + (i * 15), text, NULL);
			}

			snprintf(text, sizeof(text), "%02d", i+1);
			nvgText(ctx.vg, box.pos.x + 9, (box.pos.y + 30) + (i * 15), text, NULL);

			if (module->currFreqs[index] > 100000.0f) {
				snprintf(text, sizeof(text), "%e", module->currFreqs[index]);
			} else {
				snprintf(text, sizeof(text), "%.3f", module->currFreqs[index]);
			}
			nvgText(ctx.vg, box.pos.x + 24, (box.pos.y + 30) + (i * 15), text, NULL);

			snprintf(text, sizeof(text), "%s", module->notedesc[index].c_str());
			nvgText(ctx.vg, box.pos.x + 90, (box.pos.y + 30) + (i * 15), text, NULL);

		}
	}
	
};

struct ExpanderBankWidget : Widget {

	std::shared_ptr<Font> font;

	ExpanderBankWidget() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/BarlowCondensed-Bold.ttf"));
	}

    ScaleSet scales;

	RainbowScaleExpander *module = NULL;

	NVGcolor colors[NUM_SCALEBANKS] = {

        // Shades of Blue
        nvgRGBf( 255.0f/255.0f,     070.0f/255.0f,  255.0f/255.0f ),
        nvgRGBf( 250.0f/255.0f,     080.0f/255.0f,  250.0f/255.0f ),
        nvgRGBf( 245.0f/255.0f,     090.0f/255.0f,  245.0f/255.0f ),
        nvgRGBf( 240.0f/255.0f,     100.0f/255.0f,  240.0f/255.0f ),
        nvgRGBf( 235.0f/255.0f,     110.0f/255.0f,  235.0f/255.0f ),
        nvgRGBf( 230.0f/255.0f,     120.0f/255.0f,  230.0f/255.0f ),
                        
        // Shades of Cyan
        nvgRGBf( 150.0f/255.0f,     255.0f/255.0f,  255.0f/255.0f ),
        nvgRGBf( 130.0f/255.0f,     245.0f/255.0f,  245.0f/255.0f ),
        nvgRGBf( 120.0f/255.0f,     235.0f/255.0f,  235.0f/255.0f ),

        // Shades of Yellow
        nvgRGBf( 255.0f/255.0f,     255.0f/255.0f,  150.0f/255.0f ),
        nvgRGBf( 255.0f/255.0f,     245.0f/255.0f,  130.0f/255.0f ),
        nvgRGBf( 255.0f/255.0f,     235.0f/255.0f,  120.0f/255.0f ),
        nvgRGBf( 255.0f/255.0f,     225.0f/255.0f,  110.0f/255.0f ),

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
		nvgText(ctx.vg, box.pos.x, box.pos.y + 15, text, NULL);

	}

};

struct RainbowScaleExpanderWidget : ModuleWidget {
	
	RainbowScaleExpanderWidget(RainbowScaleExpander *module) {

		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RainbowScaleExpander.svg")));

		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(9.89, 19.118)), module, RainbowScaleExpander::SLOT_PARAM));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(95.69, 15.868)), module, RainbowScaleExpander::PARAMETER_PARAM+0));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(125.69, 15.868)), module, RainbowScaleExpander::PARAMETER_PARAM+4));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(95.69, 30.837)), module, RainbowScaleExpander::PARAMETER_PARAM+1));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(125.69, 30.837)), module, RainbowScaleExpander::PARAMETER_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(9.89, 49.118)), module, RainbowScaleExpander::SCALE_PARAM));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(95.69, 45.868)), module, RainbowScaleExpander::PARAMETER_PARAM+2));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(125.69, 45.868)), module, RainbowScaleExpander::PARAMETER_PARAM+6));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(95.69, 60.868)), module, RainbowScaleExpander::PARAMETER_PARAM+3));
		addParam(createParam<gui::FloatReadout>(mm2px(Vec(125.69, 60.868)), module, RainbowScaleExpander::PARAMETER_PARAM+7));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(9.89, 79.118)), module, RainbowScaleExpander::TRANSFER_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(107.39, 79.118)), module, RainbowScaleExpander::PAGE_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(122.287, 79.118)), module, RainbowScaleExpander::SET_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(137.39, 79.118)), module, RainbowScaleExpander::EXECUTE_PARAM));
		addParam(createParam<gui::IntegerReadout>(mm2px(Vec(3.69, 105.868)), module, RainbowScaleExpander::ROOTA_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(98.328, 109.118)), module, RainbowScaleExpander::BANK_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(107.59, 109.118)), module, RainbowScaleExpander::BANKLOAD_PARAM));

		if (module != NULL) {
			FrequencyDisplay *displayW = createWidget<FrequencyDisplay>(ink2vcv(19.5f, 123.0f));
			displayW->box.size = mm2px(Vec(0.0f, 120.0f));
			displayW->module = module;
			addChild(displayW);

			ExpanderBankWidget *bankW = createWidget<ExpanderBankWidget>(ink2vcv(111.722f,24.382f));
			bankW->box.size = Vec(80.0, 20.0f);
			bankW->module = module;
			addChild(bankW);
		}
	}
};

Model *modelRainbowScaleExpander = createModel<RainbowScaleExpander, RainbowScaleExpanderWidget>("RainbowScaleExpander");
