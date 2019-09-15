
#include <string>
#include <sstream>
#include <algorithm> 
#include <locale>
#include <iterator>
#include <bitset>
#include <vector>
#include <iostream>
#include <fstream>
#include <cctype>
#include <osdialog.h>

#include "common.hpp"

#include "plugin.hpp"
#include "Common.hpp"
#include "Rainbow.hpp"
#include "scales/Scales.hpp"

#include "dsp/noise.hpp"

extern float exp_1voct[4096];

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

template <class Container>
void split(const std::string& str, Container& cont, char delim = ' ') {
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        cont.push_back(token);
    }
}

struct ScalaDef {
	int upper;
	int lower;
	float cents;
	std::string description;
	bool isRatio;
};

struct ScalaFile {
	std::vector<ScalaDef *> notes;
	std::string description;
	bool isValid;
	std::string lastError;

	ScalaDef *parseNote(std::string text) {

		if (text.find('/') != std::string::npos) {

			std::vector<std::string> ratios;
			split<std::vector<std::string>>(text,ratios,'/');
			if (ratios.size() != 2) {
				lastError = "Invalid ratio " + text;
				return NULL;
			}

			ScalaDef *d = new ScalaDef();

			try {
				d->upper = std::stoi(ratios[0]);
				d->lower = std::stoi(ratios[1]);
			} catch (std::exception &e) {
				lastError = "Cannot convert " + text + " to integer";
				return NULL;
			}

			d->isRatio = true;
			return d;

		} else if (text.find_first_of('.') != std::string::npos) {

			ScalaDef *d = new ScalaDef();
			try {
				d->cents = std::stof(text);
			} catch (std::exception &e) {
				lastError = "Cannot convert " + text + " to float";
				return NULL;
			}

			d->isRatio = false;
			return d;

		} else {

			ScalaDef *d = new ScalaDef();

			try {
				d->upper = std::stoi(text);
				d->lower = 1;
			} catch (std::exception &e) {
				lastError = "Cannot convert " + text + " to integer";
				return NULL;
			}

			d->isRatio = true;
			return d;

		}

		lastError = "Unknown error";
		return NULL;

	}

	bool load(const char *path) {

		bool readNumNotes = false;
		bool readDescription = false;
		
		unsigned int nNotes = 0;

		std::string line;
		std::ifstream file(path);
		if (!file.is_open()) {
			lastError = "Could not load Scala file '" + std::string(path) + "'";
			return false;
		}
		DEFER({
		    file.close();
		});

		reset();

		bool failed = false;

		while (std::getline(file,line)) {

			trim(line);

			if (line[0] == '!') {
				continue;
			}
			if (!readDescription) {
				description = line;
				readDescription = true;
				continue;
			}
			if (!readNumNotes) {
				try {
					nNotes = std::stoi(line);
				} catch (std::exception &e) {
					lastError = "Invalid number of notes '" + line + "'";
					return false;
				}
				readNumNotes = true;
				continue;
			}

			std::vector<std::string> tokens;
			split<std::vector<std::string>>(line,tokens);

			ScalaDef *n = parseNote(tokens[0]);
			n->description = line;
			if (n != NULL) {
				notes.push_back(n);
			} else {
				lastError = "Failed to parse line '" + tokens[0] + "'";
				failed = true;
			}

    	}

		if (notes.size() != nNotes) {
			lastError = "Number of notes " + std::to_string(notes.size()) + " found does not match declared value of " + std::to_string(nNotes);
			failed = true;
		}

		if (failed) {
			WARN("Loading Scala file failed");
			reset();
			return false;
		}

    	file.close();
		isValid = true;
		return true;
	}

	void reset () {
		isValid = false;
		for (unsigned int i = 0; i < notes.size(); i++) {
			delete notes[i];
		}
		notes.clear();
	}

};


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
		CALC_PARAM,
		EXECUTE_PARAM,
		ENUMS(PARAMETER_PARAM, 10),
		STACKMODE_PARAM,
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

	int stackMode = 0;

	const static int NUM_PAGES = 3;

	float minFreq = 13.75f; 	// A0
	float maxFreq = 28160.0f; 	// A10

	float parameterValues[NUM_PAGES][NUM_PARAMETERS] = {};
	bool parameterActive[NUM_PAGES][NUM_PARAMETERS] = {};
	std::string parameterLabels[NUM_PAGES][NUM_PARAMETERS] = {};
	std::string parameterDescriptions[NUM_PAGES][NUM_PARAMETERS] = {};

	prism::gui::PrismReadoutParam *widgetRef[NUM_PARAMETERS];

	std::string path;

	const float CtoF96 = 96000.0f / (2.0f * core::PI);
	const float FtoC96 = (2.0f * core::PI) / 96000.0f;

	const float CtoF48 = 48000.0f / (2.0f * core::PI);
	const float FtoC48 = (2.0f * core::PI) / 48000.0f;


	float currFreqs[NUM_BANKNOTES];
	int currState[NUM_BANKNOTES];
	int currScale = 0;
	int currNote = 0;
	int currBank = 0;

	int currPage = 0; // Freq = 0, ET = 1, JI = 2
	int prevPage = 0;

	std::string name;
	std::string description;
	std::string scalename[11];
	std::string notedesc[231];

    ScaleSet scales;

	ScalaFile scala;

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
					if (f < minFreq || f > maxFreq) {
						currFreqs[i] = clamp(f, minFreq, maxFreq);
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

		scala.reset();

		for (int j = 0; j < NUM_BANKNOTES; j++) {
			currFreqs[j] = scales.presets[NUM_SCALEBANKS - 1]->c_maxq96000[j] * CtoF96;
			currState[j] = FRESH;
			notedesc[j] = "";
		}

		name = scales.presets[NUM_SCALEBANKS - 1]->name;
		description = scales.presets[NUM_SCALEBANKS - 1]->description;
		for (int j = 0; j < NUM_SCALES; j++) {
			scalename[j] = scales.presets[NUM_SCALEBANKS - 1]->scalename[j];
		}

		// P0 Frequency		/ A			/ f0		P5				/ EDO			/				
		// P1 				/ Octave	/ Octave	P6	Cents		/ Cents			/ Cents	
		// P2  				/ 	 		/ 			P7	Slot Step	/ Slot Step		/ Slot step
		// P3 				/ Interval 	/ Upper		P8				/ 				/ 	
		// P4  				/ 			/ Lower		p9	Max steps	/ Max Steps		/ Max Steps

		// Frequency page
		parameterValues[0][0] = 261.6256f; 	// Frequency	
		parameterValues[0][1] = 0.0f;		// -	
		parameterValues[0][2] = 0.0f;		// -
		parameterValues[0][3] = 0.0f;		// -
		parameterValues[0][4] = 1.0f;		// Slot step
		parameterValues[0][5] = 0.0f;		// -
		parameterValues[0][6] = 0.0f;		// Cents
		parameterValues[0][7] = 0.0f;		// -
		parameterValues[0][8] = 0.0f;		// -
		parameterValues[0][9] = 21.0f;		// Max steps

		parameterActive[0][0] = true;		// Frequency
		parameterActive[0][1] = false;		// -
		parameterActive[0][2] = false;		// -
		parameterActive[0][3] = false;  	// -
		parameterActive[0][4] = true;  		// Slot step
		parameterActive[0][5] = false; 		// -
		parameterActive[0][6] = true;		// Cents
		parameterActive[0][7] = false;		// -
		parameterActive[0][8] = false;		// -
		parameterActive[0][9] = true;		// Max steps

		parameterLabels[0][0] =	"Frequency";
		parameterLabels[0][1] = "";
		parameterLabels[0][2] =	"";
		parameterLabels[0][3] =	"";
		parameterLabels[0][4] =	"Slot step";
		parameterLabels[0][5] =	"";
		parameterLabels[0][6] = "Cents";
		parameterLabels[0][7] =	"";
		parameterLabels[0][8] =	"";
		parameterLabels[0][9] =	"Max steps";

		parameterDescriptions[0][0] =	"Frequency";
		parameterDescriptions[0][1] = 	"";
		parameterDescriptions[0][2] =	"";
		parameterDescriptions[0][3] =	"";
		parameterDescriptions[0][4] =	"Number of slots to jump after each calculation step";
		parameterDescriptions[0][5] =	"";
		parameterDescriptions[0][6] = 	"Cents to be added to frequency";
		parameterDescriptions[0][7] =	"";
		parameterDescriptions[0][8] =	"";
		parameterDescriptions[0][9] =	"Maximum number of steps to apply";

		// ET page
		parameterValues[1][0] = 440.0f;		// Root A	
		parameterValues[1][1] = 4.0f;		// Octave
		parameterValues[1][2] = 1.0f;		// Interval	
		parameterValues[1][3] = 0.0f;		// -
		parameterValues[1][4] = 1.0f;		// Slot step
		parameterValues[1][5] = 12.0f;		// EDO
		parameterValues[1][6] = 0.0f;		// Cents
		parameterValues[1][7] = 0.0f;		// Offset Interval
		parameterValues[1][8] = 0.0f;		// -
		parameterValues[1][9] = 21.0f;		// Max steps

		parameterActive[1][0] = true;		// Root A
		parameterActive[1][1] = true;		// Octave
		parameterActive[1][2] = true;		// Interval
		parameterActive[1][3] = false;		// -
		parameterActive[1][4] = true;		// Slot step
		parameterActive[1][5] = true;		// EDO
		parameterActive[1][6] = true;		// Cents
		parameterActive[1][7] = true;		// Offset Interval
		parameterActive[1][8] = false;		// -
		parameterActive[1][9] = true;		// Max steps

		parameterLabels[1][0] =	"A = ";
		parameterLabels[1][1] =	"Octave";
		parameterLabels[1][2] =	"Interval";
		parameterLabels[1][3] =	"";
		parameterLabels[1][4] =	"Slot step";
		parameterLabels[1][5] =	"EDO";
		parameterLabels[1][6] =	"Cents";
		parameterLabels[1][7] =	"Offset";
		parameterLabels[1][8] =	"";
		parameterLabels[1][9] =	"Max steps";

		parameterDescriptions[1][0] =	"Frequency of pitch standard A4";
		parameterDescriptions[1][1] =	"Octave";
		parameterDescriptions[1][2] =	"Interval to add";
		parameterDescriptions[1][3] =	"";
		parameterDescriptions[1][4] =	"Number of slots to jump after each calculation step";
		parameterDescriptions[1][5] =	"Equal Division of Octave, how many intervals are in 1 octave";
		parameterDescriptions[1][6] =	"Cents to be added to the final interval";
		parameterDescriptions[1][7] =	"Initial offset interval from octave";
		parameterDescriptions[1][8] =	"";
		parameterDescriptions[1][9] =	"Maximum number of steps to apply";

		// JI page
		parameterValues[2][0] = 16.5f;		// f0
		parameterValues[2][1] = 4.0f;		// Octave
		parameterValues[2][2] = 3.0f;		// Upper
		parameterValues[2][3] = 2.0f;		// Lower
		parameterValues[2][4] = 1.0f;		// Slot step
		parameterValues[2][5] = 0.0f;		// -
		parameterValues[2][6] = 0.0f;		// Cents
		parameterValues[2][7] = 1.0f;		// Upper offset
		parameterValues[2][8] = 1.0f;		// Lower offset
		parameterValues[2][9] = 21.0f;		// Max steps

		parameterActive[2][0] = true;		// f0
		parameterActive[2][1] = true;		// Octave
		parameterActive[2][2] = true;		// Upper
		parameterActive[2][3] = true;		// Lower
		parameterActive[2][4] = true;		// Slot step
		parameterActive[2][5] = false;		// -
		parameterActive[2][6] = true;		// Cents
		parameterActive[2][7] = true;		// Upper offset
		parameterActive[2][8] = true;		// Lower offset
		parameterActive[2][9] = true;		// Max steps

		parameterLabels[2][0] =	"f0";
		parameterLabels[2][1] =	"Octave";
		parameterLabels[2][2] =	"Upper";
		parameterLabels[2][3] =	"Lower";
		parameterLabels[2][4] =	"Slot step";
		parameterLabels[2][5] =	"";
		parameterLabels[2][6] =	"Cents";
		parameterLabels[2][7] =	"Upper offset";
		parameterLabels[2][8] =	"Lower offset";
		parameterLabels[2][9] =	"Max steps";

		parameterDescriptions[2][0] =	"Fundamental frequency; JI octaves are calculated w.r.t. this frequency";
		parameterDescriptions[2][1] =	"Octave";
		parameterDescriptions[2][2] =	"Denominator of the interval ratio";
		parameterDescriptions[2][3] =	"Numerator of the interval ratio";
		parameterDescriptions[2][4] =	"Number of slots to jump after each calculation step";
		parameterDescriptions[2][5] =	"";
		parameterDescriptions[2][6] =	"Cents to be added to the final interval";
		parameterDescriptions[2][7] =	"Denominator of initial offet interval";
		parameterDescriptions[2][8] =	"Numerator of initial offet interval";
		parameterDescriptions[2][9] =	"Maximum number of steps to apply";

		for (int i = 0; i < NUM_PARAMETERS; i++) {
			params[PARAMETER_PARAM + i].setValue(parameterValues[currPage][i]);
		}

	}

	rack::dsp::SchmittTrigger transferTrigger;
	rack::dsp::SchmittTrigger loadBankTrigger;
	rack::dsp::SchmittTrigger executeTrigger;

	RainbowScaleExpander() : core::PrismModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {

		configParam(TRANSFER_PARAM, 0, 1, 0, "Load scales into Rainbow");
		configParam(SCALE_PARAM, 0, 10, 0, "Select scale from bank");
		configParam(SLOT_PARAM, 0, 20, 0, "Select note in scale");

		configParam(BANK_PARAM, 0, 21, 0, "Bank presets"); 
		configParam(BANKLOAD_PARAM, 0, 1, 0, "Load preset"); 

		configParam(PAGE_PARAM, 0, NUM_PAGES - 1, 1, "Select page: Frequency, ET, JI"); 
		configParam(CALC_PARAM, 0, 1, 0, "Set Single note/in page"); 
		configParam(STACKMODE_PARAM, 0, 1, 0, "Calculate interval per octave or stack intervals"); 
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

	void calculateRoot(float A440, float edo, float *root, float *distance) {

		if (A440 == 440.0f && edo == 12) {
			*root = 16.3516f;
		} else {
			float rootA = A440 / 32.0f;
			float m3 = 0.0f;
			float finalm3 = 0.0f;
			float closest = 1000000.0f;
			float diff;

			for (int i = 1; i < edo; i++) {
				m3 = pow(2.0, (float)i / (float)edo);
				diff = fabs(1.2f - m3);
				if (diff < closest) {
					closest = diff;
					finalm3 = m3;
					*distance = 1200.0f * log2f(m3) - 1200.0f * log2f(1.2f);
				}
			}
			*root = rootA * finalm3;
		}

	}

	void setFromFrequency() {

		float freq 	= params[PARAMETER_PARAM + 0].getValue();
		float cents	= params[PARAMETER_PARAM + 6].getValue();

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq * pow(2.0f, cents / 1200.0f);
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

		char text[20];

		notedesc[currNote + currScale * NUM_SCALENOTES] = "";

		if (cents != 0.0f) {
			snprintf(text, sizeof(text), "%.2f", cents);
			notedesc[currNote + currScale * NUM_SCALENOTES] += "/c=" + std::string(text);
		}

		this->moveNote();

	}

	void setFromET() {
		float A440		= params[PARAMETER_PARAM + 0].getValue();
		int oct 		= params[PARAMETER_PARAM + 1].getValue();
		int interval	= params[PARAMETER_PARAM + 2].getValue();
		int offset		= params[PARAMETER_PARAM + 7].getValue();
		int edo			= params[PARAMETER_PARAM + 5].getValue();
		float cents		= params[PARAMETER_PARAM + 6].getValue();

		float root = 0.0;
		float distance = 0.0;
		calculateRoot(A440, edo, &root, &distance);

		float freq = root * pow(2, oct) * pow(2.0, (float)(interval + offset) / (float)edo) * pow(2.0f, cents / 1200.0f);

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

		char text[20];

		scalename[currScale] = "";
		if (A440 != 440.0f || edo != 12) {
			snprintf(text, sizeof(text), "%.1f(%.1fc)", root, distance);
			scalename[currScale] += "/C0=" + std::string(text);

			snprintf(text, sizeof(text), "%d", edo);
			scalename[currScale] += "/edo=" + std::string(text);
		}

		snprintf(text, sizeof(text), "%d", interval);
		notedesc[currNote + currScale * NUM_SCALENOTES] = "/int=" + std::string(text); 

		snprintf(text, sizeof(text), "%d", oct); 
		notedesc[currNote + currScale * NUM_SCALENOTES] += "/oct=" + std::string(text);

		if (offset != 0) {
			snprintf(text, sizeof(text), "%d", offset);
			notedesc[currNote + currScale * NUM_SCALENOTES] += "/off=" + std::string(text);
		}
		if (cents != 0.0f) {
			snprintf(text, sizeof(text), "%.2f", cents);
			notedesc[currNote + currScale * NUM_SCALENOTES] += "/c=" + std::string(text);
		}

		this->moveNote();

	}

	void setFromJI() {
		float f0		= params[PARAMETER_PARAM + 0].getValue();
		int oct 		= params[PARAMETER_PARAM + 1].getValue();
		float upper		= params[PARAMETER_PARAM + 2].getValue();
		float lower		= params[PARAMETER_PARAM + 3].getValue();
		float upperO	= params[PARAMETER_PARAM + 7].getValue();
		float lowerO	= params[PARAMETER_PARAM + 8].getValue();
		float cents		= params[PARAMETER_PARAM + 6].getValue();
		
		float freq = f0 * pow(2, oct) * fabs(upperO / lowerO) * fabs(upper / lower) * pow(2.0f, cents / 1200.0f);

		currFreqs[currNote + currScale * NUM_SCALENOTES] = freq;
		currState[currNote + currScale * NUM_SCALENOTES] = EDITED;

		char text[20];

		snprintf(text, sizeof(text), "%.2f", f0);
		scalename[currScale] = "/f0=" + std::string(text);

		snprintf(text, sizeof(text), "%.1f:%.1f", upper, lower);
		notedesc[currNote + currScale * NUM_SCALENOTES] = "/int=" + std::string(text);

		snprintf(text, sizeof(text), "%d", oct);
		notedesc[currNote + currScale * NUM_SCALENOTES] += "/oct=" + std::string(text);

		if (upperO != 1.0f || lowerO != 1.0f) {
			snprintf(text, sizeof(text), "%.1f:%.1f", upperO, lowerO);
			notedesc[currNote + currScale * NUM_SCALENOTES] += "/off=" + std::string(text);
		}
		if (cents != 0.0) {
			snprintf(text, sizeof(text), "%.2f", cents);
			notedesc[currNote + currScale * NUM_SCALENOTES] += "/c=" + std::string(text);
		}

		this->moveNote();
	}

	void executeFromFrequency() {
		int currPosinBank = currNote + currScale * NUM_SCALENOTES;

		float f0		 	= params[PARAMETER_PARAM + 0].getValue();
		float cents			= params[PARAMETER_PARAM + 6].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 4].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 9].getValue();

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		char text[20];

		float frequency = f0;

		for (int i = 0; i < maxSteps; i++) {

			float dCents = cents * i;
			float freq = frequency * pow(2.0f, dCents / 1200.0f);

			if (freq > maxFreq) {
				break;
			}

			currFreqs[currPosinBank] = freq;
			currState[currPosinBank] = EDITED;

			snprintf(text, sizeof(text), "/f0=%.2f", f0);
			scalename[currScale] = text;

			snprintf(text, sizeof(text), "%.2f", dCents);
			notedesc[currPosinBank] = "/c=" + std::string(text);

			currPosinBank += nStepsinBank;

			if (currPosinBank < minSlot || currPosinBank > maxSlot) {
				break;
			} 
		}

	}

	void executeFromET() {
		int currPosinBank = currNote + currScale * NUM_SCALENOTES;

		float A440			= params[PARAMETER_PARAM + 0].getValue();
		int oct 			= params[PARAMETER_PARAM + 1].getValue(); 
		int interval		= params[PARAMETER_PARAM + 2].getValue();
		int offset			= params[PARAMETER_PARAM + 7].getValue();
		int edo				= params[PARAMETER_PARAM + 5].getValue();
		float cents			= params[PARAMETER_PARAM + 6].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 4].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 9].getValue();

		float root = 0.0;
		float distance = 0.0;
		calculateRoot(A440, edo, &root, &distance);

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		char text[20];

		scalename[currScale] = "";
		if (A440 != 440.0f || edo != 12) {
			snprintf(text, sizeof(text), "%.1f(%.1fc)", root, distance);
			scalename[currScale] += "/C0=" + std::string(text);

			snprintf(text, sizeof(text), "%d", edo);
			scalename[currScale] += "/edo=" + std::string(text);
		}
		if (stackMode) {
			snprintf(text, sizeof(text), "%d", oct); 
			scalename[currScale] += "/oct=" + std::string(text);
		}
	
		int intv = stackMode ? 0 : interval;

		float dCents = pow(2.0f, cents / 1200.0f);

		for (int i = 0; i < maxSteps; i++) {

			float freq = fabs(root * pow(2, oct) * pow(2.0, (float)(intv + offset) / (float)edo) * dCents);

			if (freq > maxFreq) {
				break;
			}

			currFreqs[currPosinBank] = freq;
			currState[currPosinBank] = EDITED;

			notedesc[currPosinBank] = "";

			if (stackMode) {
				snprintf(text, sizeof(text), "%d", intv);
				notedesc[currPosinBank] = "/int=" + std::string(text); 

				intv += interval;				
			} else {
				snprintf(text, sizeof(text), "%d", interval);
				notedesc[currPosinBank] = "/int=" + std::string(text); 

				// Add octave to text
				snprintf(text, sizeof(text), "%d", oct);
				notedesc[currPosinBank] += "/oct=" + std::string(text);			

				// Then increment octave
				oct++;
			}
			if (offset != 0) {
				snprintf(text, sizeof(text), "%d", offset);
				notedesc[currPosinBank] += "/off=" + std::string(text);
			}
			if (cents != 0.0) {
				snprintf(text, sizeof(text), "%.2f", cents);
				notedesc[currPosinBank] += "/c=" + std::string(text);
			}

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
		float upper			= params[PARAMETER_PARAM + 2].getValue();
		float lower			= params[PARAMETER_PARAM + 3].getValue();
		float upperO		= params[PARAMETER_PARAM + 7].getValue();
		float lowerO		= params[PARAMETER_PARAM + 8].getValue();
		float cents			= params[PARAMETER_PARAM + 6].getValue();
		int nStepsinBank 	= params[PARAMETER_PARAM + 4].getValue();
		int maxSteps 		= params[PARAMETER_PARAM + 9].getValue();

		// Only update within current scale
		int minSlot = currScale * NUM_SCALENOTES;
		int maxSlot = std::min((currScale + 1) * NUM_SCALENOTES - 1, NUM_BANKNOTES);

		char text[20];

		snprintf(text, sizeof(text), "%.2f", f0);
		scalename[currScale] = "/C0=" + std::string(text);
		if (stackMode) {
			snprintf(text, sizeof(text), "%d", oct);
			scalename[currScale] += "/oct=" + std::string(text);
		}

		int nOcts = 0;
		float offsetRatio = fabs(upperO / lowerO);
		float dCents = pow(2.0f, cents / 1200.0f);

		for (int i = 0; i < maxSteps; i++) {

			float ratio = fabs(pow(upper, nOcts) / pow(lower, nOcts));	
			float freq = fabs(f0 * pow(2, oct) * offsetRatio * ratio * dCents);

			if (freq > maxFreq) {
				break;
			}

			currFreqs[currPosinBank] = freq;
			currState[currPosinBank] = EDITED;

			snprintf(text, sizeof(text), "%.1f:%.1f", upper, lower);
			notedesc[currPosinBank] = "/int=" + std::string(text); 

			if (stackMode) {
				nOcts++;
			} else {
				// Add octave to text
				snprintf(text, sizeof(text), "%d", oct);
				notedesc[currPosinBank] += "/oct=" + std::string(text);			

				// Then increment octave
				oct++;
			}
			if (upperO != 1.0f || lowerO != 1.0f) {
				snprintf(text, sizeof(text), "%.1f:%.1f", upperO, lowerO);
				notedesc[currPosinBank] += "/off=" + std::string(text);
			}
			if (cents != 0.0) {
				snprintf(text, sizeof(text), "%.2f", cents);
				notedesc[currPosinBank] += "/c=" + std::string(text);
			}

			currPosinBank += nStepsinBank;

			if (currPosinBank < minSlot || currPosinBank > maxSlot) {
				break;
			} 
		}

	}

	void applyScale() {

		float f0;

		float f			= params[PARAMETER_PARAM + 0].getValue();
		float A440		= params[PARAMETER_PARAM + 0].getValue();
		int oct 		= params[PARAMETER_PARAM + 1].getValue();
		int offset		= params[PARAMETER_PARAM + 7].getValue();
		int edo			= params[PARAMETER_PARAM + 5].getValue();
		float upperO	= params[PARAMETER_PARAM + 7].getValue();
		float lowerO	= params[PARAMETER_PARAM + 8].getValue();

		if (currPage == 0) {
			f0 = f;
		} else if (currPage == 1) {
			float root = 0.0;
			float distance = 0.0;
			calculateRoot(A440, edo, &root, &distance);
			f0 = root * pow(2, oct) * pow(2.0, (float)offset / (float)edo);
		} else if (currPage == 2) {
			f0 = f * pow(2, oct) * fabs(upperO / lowerO);
		} else {
			f0 = 16.3516f;
		}

		int currPosinBank = currNote + currScale * NUM_SCALENOTES;
		int maxSlot = NUM_BANKNOTES;

		unsigned int scalaPos = 0;

		// populate with implicit 1/1 interval
		currFreqs[currPosinBank] = f0;
		currState[currPosinBank] = EDITED;
		notedesc[currPosinBank] = "1/1";
		currPosinBank++;

		while (currPosinBank < maxSlot) {

			ScalaDef *note = scala.notes[scalaPos];
			float freq;
			if (note->isRatio) {
				freq = f0 * ((float)note->upper / (float)note->lower);
			} else {
				freq = f0 * pow(2.0f, note->cents / 1200.0f);
			}

			if (freq > maxFreq) {
				break;
			}

			currFreqs[currPosinBank] = freq;
			currState[currPosinBank] = EDITED;
			notedesc[currPosinBank] = note->description;

			int scale = currPosinBank / 21;
		 	scalename[scale] = string::filename(path) + ", Page " + std::to_string(scale + 1);

			currPosinBank++;

			// Wrap
			if (++scalaPos == scala.notes.size()) {
				f0 *= 2.0f;
				scalaPos = 0;
			} 

		}

		description = scala.description;

	}

	void process(const ProcessArgs &args) override {
		PrismModule::step();

		currScale = params[SCALE_PARAM].getValue();
		currNote = params[SLOT_PARAM].getValue();
		currBank = params[BANK_PARAM].getValue();
		stackMode = params[STACKMODE_PARAM].getValue();

		if (loadBankTrigger.process(params[BANKLOAD_PARAM].getValue())) {
			int bank = params[BANK_PARAM].getValue();

			name = scales.full[bank]->name;
			description = scales.full[bank]->description;

			for (int i = 0; i < NUM_BANKNOTES; i++) {
				currFreqs[i] = scales.full[bank]->c_maxq96000[i] * CtoF96;
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

		int calc = params[CALC_PARAM].getValue();

		if (executeTrigger.process(params[EXECUTE_PARAM].getValue())) {
			if (calc == 0) {
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
			} else {
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
		}

		if (leftExpander.module) {
			if (leftExpander.module->model == modelRainbow) {
				RainbowScaleExpanderMessage *pM = (RainbowScaleExpanderMessage*)leftExpander.module->rightExpander.producerMessage;
				if (transferTrigger.process(params[TRANSFER_PARAM].getValue())) {
					for (int i = 0; i < NUM_BANKNOTES; i++) {
						pM->maxq96[i] = currFreqs[i] * FtoC96;
						pM->maxq48[i] = currFreqs[i] * FtoC48;
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

static void loadFile(RainbowScaleExpander *module) {

	std::string dir;
	std::string filename;
	if (module->path != "") {
		dir = string::directory(module->path);
		filename = string::filename(module->path);
	}
	else {
		dir = asset::user("");
		filename = "";
	}

	osdialog_filters *filter = osdialog_filters_parse("Scala file:scl");
	char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), filename.c_str(), filter);
	if (path) {
		module->path = path;
		if (!module->scala.load(path)) {
			std::string message = module->scala.lastError;
			osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, message.c_str());
		}
		free(path);
	}
	osdialog_filters_free(filter);
}

static void applyFile(RainbowScaleExpander *module) {

	if (module->scala.isValid) {
		module->applyScale();
	} else {
		std::string message = "No Scala file loaded";
		osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, message.c_str());
	}

}

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
		addParam(createParamCentered<gui::PrismSSwitch>(mm2px(Vec(107.39, 94.118)), module, RainbowScaleExpander::CALC_PARAM));
		addParam(createParamCentered<gui::PrismSSwitch>(mm2px(Vec(122.39, 94.118)), module, RainbowScaleExpander::STACKMODE_PARAM));
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

	void appendContextMenu(Menu *menu) override {

		RainbowScaleExpander *spectrum = dynamic_cast<RainbowScaleExpander*>(module);
		assert(spectrum);

		struct LoadItem : MenuItem {
			RainbowScaleExpander *module;
			void onAction(const event::Action &e) override {
				loadFile(module);
			}
		};

		struct ApplyItem : MenuItem {
			RainbowScaleExpander *module;
			void onAction(const event::Action &e) override {
				applyFile(module);
			}
		};

		menu->addChild(construct<MenuLabel>());

		LoadItem *loadItem = new LoadItem;
		loadItem->text = "Load Scala file";
		loadItem->module = spectrum;
		menu->addChild(loadItem);

		ApplyItem *applyItem = new ApplyItem;
		applyItem->text = "Apply Scala file";
		applyItem->module = spectrum;
		menu->addChild(applyItem);

	 }

};

Model *modelRainbowScaleExpander = createModel<RainbowScaleExpander, RainbowScaleExpanderWidget>("RainbowScaleExpander");
