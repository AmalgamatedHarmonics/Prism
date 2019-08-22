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
	const static int NUM_PARAMETERS = 10;

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
		ENUMS(PARAMETER_PARAM, 10),
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

	float parameterValues[3][NUM_PARAMETERS] = {};
	bool parameterActive[3][NUM_PARAMETERS] = {};
	std::string parameterLabels[3][NUM_PARAMETERS] = {};
	std::string parameterDescriptions[3][NUM_PARAMETERS] = {};

	prism::gui::PrismReadoutParam *widgetRef[NUM_PARAMETERS];

	const float CtoF = 96000.0f / (2.0f * core::PI);
	const float FtoC = (2.0f * core::PI) / 96000.0f;

	float currFreqs[NUM_BANKNOTES];
	int currState[NUM_BANKNOTES];
	int currScale = 0;
	int currNote = 0;
	int currBank = 0;

	int currPage = 0; // Freq = 0, ET = 1, JI = 2
	int prevPage = 0;

	// float rootA;

	std::string name;
	std::string description;
	std::string scalename[11];
	std::string notedesc[231];

    ScaleSet scales;

	json_t *dataToJson() override {

        json_t *rootJ = json_object();

		// page
		json_t *ppageJ = json_integer(prevPage);
		json_object_set_new(rootJ, "ppage", ppageJ);

		// page
		json_t *pageJ = json_integer(currPage);
		json_object_set_new(rootJ, "page", pageJ);

		// name
		json_t *nameJ = json_string(name.c_str());
		json_object_set_new(rootJ, "name", nameJ);

		// description
		json_t *descriptionJ = json_string(description.c_str());
		json_object_set_new(rootJ, "description", descriptionJ);

		// scalename
		json_t *scalename_array = json_array();
		for (int i = 0; i < NUM_SCALES; i++) {
			json_t *scalenameJ = json_string(scalename[i].c_str());
			json_array_append_new(scalename_array, scalenameJ);
		}
		json_object_set_new(rootJ, "scalename",	scalename_array);

		// frequency
		json_t *frequency_array = json_array();
		for (int i = 0; i < NUM_BANKNOTES; i++) {
			json_t *frequencyJ = json_real(currFreqs[i]);
			json_array_append_new(frequency_array, frequencyJ);
		}
		json_object_set_new(rootJ, "frequency",	frequency_array);

		// notedesc
		json_t *notedesc_array = json_array();
		for (int i = 0; i < NUM_BANKNOTES; i++) {
			json_t *notedescJ = json_string(notedesc[i].c_str());
			json_array_append_new(notedesc_array, notedescJ);
		}
		json_object_set_new(rootJ, "notedesc",	notedesc_array);

        return rootJ;
    }

	void dataFromJson(json_t *rootJ) override {

		// ppage
		json_t *ppageJ = json_object_get(rootJ, "ppage");
		if (ppageJ)
			prevPage = json_integer_value(ppageJ);

		// page
		json_t *pageJ = json_object_get(rootJ, "page");
		if (pageJ)
			currPage = json_integer_value(pageJ);

		// name
		json_t *nameJ = json_object_get(rootJ, "name");
		if (nameJ)
			name = json_string_value(nameJ);

		// description
		json_t *descriptionJ = json_object_get(rootJ, "description");
		if (descriptionJ)
			description = json_string_value(descriptionJ);

		// frequency
		json_t *frequency_array = json_object_get(rootJ, "frequency");
		if (frequency_array) {
			for (int i = 0; i < NUM_BANKNOTES; i++) {
				json_t *frequencyJ = json_array_get(frequency_array, i);
				if (frequencyJ) {
					float f = json_real_value(frequencyJ);
					if (f < 13.75f || f > 30000.0f) {
						currFreqs[i] = clamp(f, 13.75f, 30000.0f);
						currState[i] = EDITED;
					} else {
						currFreqs[i] = f;
						currState[i] = FRESH;
					}
				}
			}
		}

		// scalename
		json_t *scalename_array = json_object_get(rootJ, "scalename");
		if (scalename_array) {
			for (int i = 0; i < NUM_SCALES; i++) {
				json_t *scalenameJ = json_array_get(scalename_array, i);
				if (scalenameJ) {
					scalename[i] = json_string_value(scalenameJ);
				}
			}
		}

		// notedesc
		json_t *notedesc_array = json_object_get(rootJ, "notedesc");
		if (notedesc_array) {
			for (int i = 0; i < NUM_BANKNOTES; i++) {
				json_t *notedescJ = json_array_get(notedesc_array, i);
				if (notedescJ) {
					notedesc[i] = json_string_value(notedescJ);
				}
			}
		}

		populateWidgetData();

	}

	void initialise() {
		for (int j = 0; j < NUM_BANKNOTES; j++) {
			currFreqs[j] = scales.presets[NUM_SCALEBANKS - 1]->c_maxq[j];
			currState[j] = FRESH;
		}

		// P0 Frequency		/ A			/ f0		P5				/ EDO			/				
		// P1 				/ Octave	/ Octave	P6	Cents		/ Cents			/ Cents	
		// P2  				/ Base 		/ Base		P7	Slot Step	/ Slot Step		/ Slot step
		// P3 				/ Interval 	/ Upper		P8				/ Interval Step	/ Cent step
		// P4  				/ 			/ Lower		p9	Max steps	/ Max Steps		/ Max Steps

		// Frequency page
		parameterValues[0][0] = 261.6256f; 	parameterActive[0][0] = true;	// Frequency
		parameterValues[0][1] = 0.0f;		parameterActive[0][1] = false;	// -
		parameterValues[0][2] = 0.0f;		parameterActive[0][2] = false;	// -
		parameterValues[0][3] = 0.0f;		parameterActive[0][3] = false;  // -
		parameterValues[0][4] = 0.0f;		parameterActive[0][4] = false;  // -
		parameterValues[0][5] = 0.0f;		parameterActive[0][5] = false;  // -
		parameterValues[0][6] = 0.0f;		parameterActive[0][6] = true;	// Cents
		parameterValues[0][7] = 1.0f;		parameterActive[0][7] = true;	// Slot step
		parameterValues[0][8] = 100.0f;		parameterActive[0][8] = true;	// Interval step (cents)
		parameterValues[0][9] = 21.0f;		parameterActive[0][9] = true;	// Max steps

		// ET page
		parameterValues[1][0] = 440.0f;		parameterActive[1][0] = true;	// Root A
		parameterValues[1][1] = 4.0f;		parameterActive[1][1] = true;	// Octave
		parameterValues[1][2] = 0.0f;		parameterActive[1][2] = true;	// Root Interval
		parameterValues[1][3] = 12.0f;		parameterActive[1][3] = true;	// Interval
		parameterValues[1][4] = 0.0f;		parameterActive[1][4] = false;	// -
		parameterValues[1][5] = 12.0f;		parameterActive[1][5] = true;	// EDO
		parameterValues[1][6] = 0.0f;		parameterActive[1][6] = true;	// Cents
		parameterValues[1][7] = 1.0f;		parameterActive[1][7] = true;	// Slot step
		parameterValues[1][8] = 1.0f;		parameterActive[1][8] = true;	// Interval step (semitone)
		parameterValues[1][9] = 21.0f;		parameterActive[1][9] = true;	// Max steps

		// JI page
		parameterValues[2][0] = 16.35f;		parameterActive[2][0] = true;	// f0
		parameterValues[2][1] = 4.0f;		parameterActive[2][1] = true;	// Octave
		parameterValues[2][2] = 1.0f;		parameterActive[2][2] = true;	// Base
		parameterValues[2][3] = 3.0f;		parameterActive[2][3] = true;	// Upper
		parameterValues[2][4] = 2.0f;		parameterActive[2][4] = true;	// Lower
		parameterValues[2][5] = 0.0f;		parameterActive[2][5] = false;	// -
		parameterValues[2][6] = 0.0f;		parameterActive[2][6] = true;	// Cents
		parameterValues[2][7] = 1.0f;		parameterActive[2][7] = true;	// Slot step
		parameterValues[2][8] = 0.0f;		parameterActive[2][8] = false;	// -
		parameterValues[2][9] = 21.0f;		parameterActive[2][9] = true;	// Max steps

		parameterLabels[0][0] =	"Frequency";
		parameterLabels[0][1] = "";
		parameterLabels[0][2] =	"";
		parameterLabels[0][3] =	"";
		parameterLabels[0][4] =	"";
		parameterLabels[0][5] =	"";
		parameterLabels[0][6] = "Cents";
		parameterLabels[0][7] =	"Slot step";
		parameterLabels[0][8] =	"Cent step";
		parameterLabels[0][9] =	"Max steps";

		parameterLabels[1][0] =	"A = ";
		parameterLabels[1][1] =	"Octave";
		parameterLabels[1][2] =	"Base intvl.";
		parameterLabels[1][3] =	"Interval";
		parameterLabels[1][4] =	"";
		parameterLabels[1][5] =	"EDO";
		parameterLabels[1][6] =	"Cents";
		parameterLabels[1][7] =	"Slot step";
		parameterLabels[1][8] =	"Intvl. step";
		parameterLabels[1][9] =	"Max steps";

		parameterLabels[2][0] =	"f0";
		parameterLabels[2][1] =	"Octave";
		parameterLabels[2][2] =	"Base intvl.";
		parameterLabels[2][3] =	"Upper";
		parameterLabels[2][4] =	"Lower";
		parameterLabels[2][5] =	"";
		parameterLabels[2][6] =	"Cents";
		parameterLabels[2][7] =	"Slot step";
		parameterLabels[2][8] =	"";
		parameterLabels[2][9] =	"Max steps";

		parameterDescriptions[0][0] =	"Frequency";
		parameterDescriptions[0][1] = 	"";
		parameterDescriptions[0][2] =	"";
		parameterDescriptions[0][3] =	"";
		parameterDescriptions[0][4] =	"";
		parameterDescriptions[0][5] =	"";
		parameterDescriptions[0][6] = 	"Cents to be added to frequency";
		parameterDescriptions[0][7] =	"Number of slots to jump after each calculation step";
		parameterDescriptions[0][8] =	"Additional cents to be added each step";
		parameterDescriptions[0][9] =	"Maximum number of steps to apply";

		parameterDescriptions[1][0] =	"Frequency of pitch standard A4";
		parameterDescriptions[1][1] =	"Octave";
		parameterDescriptions[1][2] =	"Base interval (semitones) to be added to the octave";
		parameterDescriptions[1][3] =	"Additional interval (semitones) to be added to the octave";
		parameterDescriptions[1][4] =	"";
		parameterDescriptions[1][5] =	"Equal Division of Octave, how many intervals are in 1 octave";
		parameterDescriptions[1][6] =	"Cents to be added to the final interval";
		parameterDescriptions[1][7] =	"Number of slots to jump after each calculation step";
		parameterDescriptions[1][8] =	"Additional intervals (semitones) to be added each step";
		parameterDescriptions[1][9] =	"Maximum number of steps to apply";

		parameterDescriptions[2][0] =	"Fundamental frequency; JI octaves are calculated w.r.t. this frequency";
		parameterDescriptions[2][1] =	"Octave";
		parameterDescriptions[2][2] =	"Base interval (ratio) to be added to the octave";
		parameterDescriptions[2][3] =	"Denominator of the interval ratio";
		parameterDescriptions[2][4] =	"Numerator of the interval ratio, set this to 1 to allow a precise numerical ratio in Upper";
		parameterDescriptions[2][5] =	"";
		parameterDescriptions[2][6] =	"Cents to be added to the final interval";
		parameterDescriptions[2][7] =	"Number of slots to jump after each calculation step";
		parameterDescriptions[2][8] =	"";
		parameterDescriptions[2][9] =	"Maximum number of steps to apply";

		for (int j = 0; j < 3; j++) {
			for (int i = 0; i < NUM_PARAMETERS; i++) {
				params[PARAMETER_PARAM + i].setValue(parameterValues[j][i]);
			}
		}
	}

	float JI5LimitIntervals[13] = {
		1.0f,				// P1
		16.0f/15.0f,		// m2
		9.0f/8.0f,			// M2
		6.0f/5.0f,			// m3
		5.0f/4.0f,			// M3
		4.0f/3.0f,			// P4
		45.0f/32.0f,		// A4
		64.0f/45.0f,		// d5
		3.0f/2.0f,			// P5
		8.0f/5.0f,			// m6
		5.0f/3.0f,			// M6
		16.0f/9.0f,			// m7
		15.0f/8.0f			// M7
	};

	rack::dsp::SchmittTrigger transferTrigger;
	rack::dsp::SchmittTrigger loadBankTrigger;
	rack::dsp::SchmittTrigger setTrigger;
	rack::dsp::SchmittTrigger executeTrigger;

	RainbowScaleExpander() : core::PrismModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {

		configParam(TRANSFER_PARAM, 0, 1, 0, "Load scales into Rainbow");
		configParam(SCALE_PARAM, 0, 10, 0, "Select scale from bank");
		configParam(SLOT_PARAM, 0, 20, 0, "Select note in scale");

		configParam(BANK_PARAM, 0, 21, 0, "Bank presets"); 
		configParam(BANKLOAD_PARAM, 0, 1, 0, "Load preset"); 

		configParam(PAGE_PARAM, 0, 2, 1, "Select page: Frequency, ET, JI"); 
		configParam(SET_PARAM, 0, 1, 0, "Set note frequency"); 
		configParam(EXECUTE_PARAM, 0, 1, 0, "Set frequencies in scale"); 

		for (int i = 0; i < NUM_PARAMETERS; i++) {
			configParam(PARAMETER_PARAM + i, -100000, 100000, 0, "Parameter");
		}

		initialise();

	}

	void onReset() override {
		initialise();
	}

	void populateWidgetData() {
		for (int i = 0; i < NUM_PARAMETERS; i++) {
			if (widgetRef[i]) {
				widgetRef[i]->isActive 	= parameterActive[currPage][i];
				widgetRef[i]->title 	= parameterLabels[currPage][i];
			}
			paramQuantities[PARAMETER_PARAM + i]->label = parameterLabels[currPage][i];
			paramQuantities[PARAMETER_PARAM + i]->description = parameterDescriptions[currPage][i];
		}
	}

	void setFromFrequency() {
		float freq 	= params[PARAMETER_PARAM + 0].getValue();
		float cents	= params[PARAMETER_PARAM + 6].getValue();
		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq * pow(2.0f, cents / 1200.0f);
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

		char fText[20];
		char cText[20];
		snprintf(fText, sizeof(fText), "%.2f", freq);
		snprintf(cText, sizeof(cText), "%.2f", cents);
		notedesc[currNote + currScale * NUM_SCALENOTES] = "f=" + std::string(fText);
		if (cents != 0.0) {
			notedesc[currNote + currScale * NUM_SCALENOTES] += "/c=" + std::string(cText);
		}
	}

	void setFromET() {
		float rootA	= params[PARAMETER_PARAM + 0].getValue() / 32.0f;
		int oct 	= params[PARAMETER_PARAM + 1].getValue();
		int root 	= params[PARAMETER_PARAM + 2].getValue();
		int semi	= params[PARAMETER_PARAM + 3].getValue();
		int edo		= params[PARAMETER_PARAM + 5].getValue();
		float cents	= params[PARAMETER_PARAM + 6].getValue();

		float root2 = pow(2.0, (root + semi) / (float)edo);
		float freq = rootA * pow(2, oct) * root2 * pow(2.0f, cents / 1200.0f);

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

		char aText[20];
		char oText[20];
		char rText[20];
		char sText[20];
		char eText[20];
		char cText[20];
		snprintf(aText, sizeof(aText), "%.1f", params[PARAMETER_PARAM + 0].getValue());
		snprintf(oText, sizeof(oText), "%d", oct);
		snprintf(rText, sizeof(rText), "%d", root);
		snprintf(sText, sizeof(sText), "%d", semi);
		snprintf(eText, sizeof(eText), "%d", edo);
		snprintf(cText, sizeof(cText), "%.2f", cents);

		notedesc[currNote + currScale * NUM_SCALENOTES] = "";

		if (rootA != 13.75f) {
			notedesc[currNote + currScale * NUM_SCALENOTES] += "A=" + std::string(aText) + "/";
		}
		if (edo != 12) {
			notedesc[currNote + currScale * NUM_SCALENOTES] += "e=" + std::string(eText) + "/";
		}
		notedesc[currNote + currScale * NUM_SCALENOTES] += "o=" + std::string(oText);
		notedesc[currNote + currScale * NUM_SCALENOTES] += "/r=" + std::string(rText); 
		notedesc[currNote + currScale * NUM_SCALENOTES] += "/i=" + std::string(sText); 
		if (cents != 0.0) {
			notedesc[currNote + currScale * NUM_SCALENOTES] += "/c=" + std::string(cText);
		}

		this->moveNote();
	}

	void setFromJI() {
		float f0	= params[PARAMETER_PARAM + 0].getValue();
		int oct 	= params[PARAMETER_PARAM + 1].getValue();
		float root 	= params[PARAMETER_PARAM + 2].getValue();
		float upper	= params[PARAMETER_PARAM + 3].getValue();
		float lower	= params[PARAMETER_PARAM + 4].getValue();
		float cents	= params[PARAMETER_PARAM + 6].getValue();
		
		float freq = f0 * pow(2, oct) * root * (upper / lower) * pow(2.0f, cents / 1200.0f);

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

		char aText[20];
		char oText[20];
		char rText[20];
		char iText[20];
		char cText[20];
		snprintf(aText, sizeof(aText), "%.2f", f0);
		snprintf(oText, sizeof(oText), "%d", oct);
		snprintf(rText, sizeof(rText), "%.2f", root);
		snprintf(iText, sizeof(iText), "%.1f/%.1f", upper, lower);
		snprintf(cText, sizeof(cText), "%.2f", cents);

		notedesc[currNote + currScale * NUM_SCALENOTES] = "";

		notedesc[currNote + currScale * NUM_SCALENOTES] += "f0=" + std::string(aText);
		notedesc[currNote + currScale * NUM_SCALENOTES] += "/o=" + std::string(oText);
		notedesc[currNote + currScale * NUM_SCALENOTES] += "/r=" + std::string(rText); 
		notedesc[currNote + currScale * NUM_SCALENOTES] += "/i=" + std::string(iText); 
		if (cents != 0.0) {
			notedesc[currNote + currScale * NUM_SCALENOTES] += "/c=" + std::string(cText);
		}

		this->moveNote();
	}

	void executeFromFrequency() {
		int currPosinBank = currNote + currScale * NUM_SCALENOTES;

		float frequency	 	= params[PARAMETER_PARAM + 0].getValue();
		float cents			= params[PARAMETER_PARAM + 6].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 7].getValue();
		float dCents		= params[PARAMETER_PARAM + 8].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 9].getValue();

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		char fText[100];
		char cText[100];

		for (int i = 0; i < maxSteps; i++) {
			float f2 = frequency * pow(2.0f, cents / 1200.0f) * pow(2.0f, dCents * i / 1200.0f);

			currFreqs[currPosinBank] = f2;
			currState[currPosinBank] = EDITED;
			snprintf(fText, sizeof(fText), "%.2f", frequency);
			snprintf(cText, sizeof(cText), "%.2f", cents + dCents * i);

			notedesc[currPosinBank] = "f=" + std::string(fText);
			if (cents + dCents * i != 0.0) {
				notedesc[currPosinBank] += "/c=" + std::string(cText);
			}

			currPosinBank += nStepsinBank;

			if (currPosinBank < minSlot || currPosinBank > maxSlot) {
				break;
			} 
		}
	}

	void executeFromET() {
		int currPosinBank = currNote + currScale * NUM_SCALENOTES;

		float rootA			= params[PARAMETER_PARAM + 0].getValue() / 32.0f;
		int oct 			= params[PARAMETER_PARAM + 1].getValue();
		int base 			= params[PARAMETER_PARAM + 2].getValue();
		int edo				= params[PARAMETER_PARAM + 5].getValue();
		float cents			= params[PARAMETER_PARAM + 6].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 7].getValue();
		int nSemitones 		= params[PARAMETER_PARAM + 8].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 9].getValue();

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		char aText[20];
		char oText[20];
		char bText[20];
		char iText[20];
		char eText[20];
		char cText[20];

		int interval = base;

		for (int i = 0; i < maxSteps; i++) {
			float r2 = pow(2.0, interval / (float)edo);
			float f2 = rootA * pow(2, oct) * r2 * pow(2.0f, cents / 1200.0f);

			currFreqs[currPosinBank] = f2;
			currState[currPosinBank] = EDITED;

			snprintf(aText, sizeof(aText), "%.1f", params[PARAMETER_PARAM + 0].getValue());
			snprintf(oText, sizeof(oText), "%d", oct);
			snprintf(bText, sizeof(bText), "%d", base);
			snprintf(iText, sizeof(iText), "%d", interval);
			snprintf(eText, sizeof(eText), "%d", edo);
			snprintf(cText, sizeof(cText), "%.2f", cents);

			notedesc[currPosinBank] = "";

			if (rootA != 13.75f) {
				notedesc[currPosinBank] += "A=" + std::string(aText) + "/";
			}
			if (edo != 12) {
				notedesc[currPosinBank] += "e=" + std::string(eText) + "/";
			}
			notedesc[currPosinBank] += "o=" + std::string(oText);
			notedesc[currPosinBank] += "/r=" + std::string(bText); 
			notedesc[currPosinBank] += "/i=" + std::string(iText); 
			if (cents != 0.0) {
				notedesc[currPosinBank] += "/c=" + std::string(cText);
			}

			interval += nSemitones;				
			currPosinBank += nStepsinBank;

			if (currPosinBank < minSlot || currPosinBank > maxSlot) {
				break;
			} 
		}
	}

	void executeFromJI() {
		int currPosinBank = currNote + currScale * NUM_SCALENOTES;

		float f0			= params[PARAMETER_PARAM + 0].getValue();
		int oct 			= params[PARAMETER_PARAM + 1].getValue();
		float root 			= params[PARAMETER_PARAM + 2].getValue();
		float upper			= params[PARAMETER_PARAM + 3].getValue();
		float lower			= params[PARAMETER_PARAM + 4].getValue();
		float cents			= params[PARAMETER_PARAM + 6].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 7].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 9].getValue();

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		for (int i = 0; i < maxSteps; i++) {

			float freq = f0 * pow(2, oct) * root * (upper / lower) * pow(2.0f, cents / 1200.0f);

			currFreqs[currPosinBank] = freq;
			currState[currPosinBank] = EDITED;

			char aText[20];
			char oText[20];
			char rText[20];
			char iText[20];
			char cText[20];
			snprintf(aText, sizeof(aText), "%.2f", f0);
			snprintf(oText, sizeof(oText), "%d", oct);
			snprintf(rText, sizeof(rText), "%.2f", root);
			snprintf(iText, sizeof(iText), "%.1f/%.1f", upper, lower);
			snprintf(cText, sizeof(cText), "%.2f", cents);

			notedesc[currPosinBank] = "";

			notedesc[currPosinBank] += "f0=" + std::string(aText);
			notedesc[currPosinBank] += "/o=" + std::string(oText);
			notedesc[currPosinBank] += "/r=" + std::string(rText); 
			notedesc[currPosinBank] += "/i=" + std::string(iText); 
			if (cents != 0.0) {
				notedesc[currPosinBank] += "/c=" + std::string(cText);
			}

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

		if (loadBankTrigger.process(params[BANKLOAD_PARAM].getValue())) {
			int bank = params[BANK_PARAM].getValue();

			name = scales.full[bank]->name;
			description = scales.full[bank]->description;

			for (int i = 0; i < NUM_BANKNOTES; i++) {
				currFreqs[i] = scales.full[bank]->c_maxq[i] * CtoF;
				currState[i] = FRESH;
				notedesc[i] = scales.full[bank]->notedesc[i];
			}

			for (int i = 0; i < NUM_SCALES; i++) {
				scalename[i] = scales.full[bank]->scalename[i];
			}

		}

		currPage = params[PAGE_PARAM].getValue();
		if (prevPage != currPage) {
			for (int i = 0; i < NUM_PARAMETERS; i++) {
				params[PARAMETER_PARAM + i].setValue(parameterValues[currPage][i]);
			}
			populateWidgetData();
			prevPage = currPage;
		} else {
			for (int i = 0; i < NUM_PARAMETERS; i++) {
				parameterValues[currPage][i] = params[PARAMETER_PARAM + i].getValue();
			}
		}

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
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/RobotoCondensed-Regular.ttf"));
	}

	void draw(const DrawArgs &ctx) override {
		if (!module->stepX % 60 != 0) {
			return;
		}

		nvgFontSize(ctx.vg, 14);
		nvgFontFaceId(ctx.vg, font->handle);
		nvgTextLetterSpacing(ctx.vg, -1);

		char text[128];

		snprintf(text, sizeof(text), "Bank: %s", module->name.c_str());
		nvgText(ctx.vg, box.pos.x + 7, box.pos.y + 0, text, NULL);

		switch(module->currPage) {
			case 0:
				snprintf(text, sizeof(text), "Mode: Frequency");
				break;
			case 1:
				snprintf(text, sizeof(text), "Mode: Equal Tem.");
				break;
			case 2:
				snprintf(text, sizeof(text), "Mode: Just Inton.");
				break;

		}
		nvgText(ctx.vg, box.pos.x + 120, box.pos.y + 0, text, NULL);

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
			nvgText(ctx.vg, box.pos.x + 26, (box.pos.y + 30) + (i * 15), text, NULL);

			if (module->notedesc[index].length() > 25) {
				snprintf(text, 25, "%s...", module->notedesc[index].substr(0, 20).c_str());
			} else {
				snprintf(text, 25, "%s", module->notedesc[index].c_str());
			}
			nvgText(ctx.vg, box.pos.x + 90, (box.pos.y + 30) + (i * 15), text, NULL);

		}
	}
	
};

struct ExpanderBankWidget : Widget {

	std::shared_ptr<Font> font;

	ExpanderBankWidget() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/RobotoCondensed-Regular.ttf"));
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

		gui::PrismReadoutParam *p0 = createParam<gui::FloatReadout>(mm2px(Vec(95.69, 9.268)), module, RainbowScaleExpander::PARAMETER_PARAM+0);
		gui::PrismReadoutParam *p1 = createParam<gui::FloatReadout>(mm2px(Vec(95.69, 24.268)), module, RainbowScaleExpander::PARAMETER_PARAM+1);
		gui::PrismReadoutParam *p2 = createParam<gui::FloatReadout>(mm2px(Vec(95.69, 39.268)), module, RainbowScaleExpander::PARAMETER_PARAM+2);
		gui::PrismReadoutParam *p3 = createParam<gui::FloatReadout>(mm2px(Vec(95.69, 54.268)), module, RainbowScaleExpander::PARAMETER_PARAM+3);
		gui::PrismReadoutParam *p4 = createParam<gui::FloatReadout>(mm2px(Vec(95.69, 69.268)), module, RainbowScaleExpander::PARAMETER_PARAM+4);
		gui::PrismReadoutParam *p5 = createParam<gui::FloatReadout>(mm2px(Vec(125.69, 9.268)), module, RainbowScaleExpander::PARAMETER_PARAM+5);
		gui::PrismReadoutParam *p6 = createParam<gui::FloatReadout>(mm2px(Vec(125.69, 24.268)), module, RainbowScaleExpander::PARAMETER_PARAM+6);
		gui::PrismReadoutParam *p7 = createParam<gui::FloatReadout>(mm2px(Vec(125.69, 39.268)), module, RainbowScaleExpander::PARAMETER_PARAM+7);
		gui::PrismReadoutParam *p8 = createParam<gui::FloatReadout>(mm2px(Vec(125.69, 54.268)), module, RainbowScaleExpander::PARAMETER_PARAM+8);
		gui::PrismReadoutParam *p9 = createParam<gui::FloatReadout>(mm2px(Vec(125.69, 69.268)), module, RainbowScaleExpander::PARAMETER_PARAM+9);

		if (module) {
			module->widgetRef[0] = p0;
			module->widgetRef[1] = p1;
			module->widgetRef[2] = p2;
			module->widgetRef[3] = p3;
			module->widgetRef[4] = p4;
			module->widgetRef[5] = p5;
			module->widgetRef[6] = p6;
			module->widgetRef[7] = p7;
			module->widgetRef[8] = p8;
			module->widgetRef[9] = p9;
			module->populateWidgetData();
		}

		addParam(p0);
		addParam(p1);
		addParam(p2);
		addParam(p3);
		addParam(p4);
		addParam(p5);
		addParam(p6);
		addParam(p7);
		addParam(p8);
		addParam(p9);

		addParam(createParamCentered<gui::PrismLargeKnobSnap>(mm2px(Vec(9.89, 19.118)), module, RainbowScaleExpander::SLOT_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(9.89, 79.118)), module, RainbowScaleExpander::TRANSFER_PARAM));
		addParam(createParamCentered<gui::PrismLargeKnobSnap>(mm2px(Vec(9.89, 49.118)), module, RainbowScaleExpander::SCALE_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(107.39, 94.118)), module, RainbowScaleExpander::SET_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(137.39, 94.118)), module, RainbowScaleExpander::EXECUTE_PARAM));
		addParam(createParamCentered<gui::PrismSSwitch3>(mm2px(Vec(9.89, 109.118)), module, RainbowScaleExpander::PAGE_PARAM));
		addParam(createParamCentered<gui::PrismLargeKnobSnap>(mm2px(Vec(98.328, 109.118)), module, RainbowScaleExpander::BANK_PARAM));
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
