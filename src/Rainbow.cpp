#include <bitset>

#include "common.hpp"

#include "plugin.hpp"
#include "Common.hpp"
#include "Rainbow.hpp"

#include "scales/Scales.hpp"

using namespace prism;
using namespace rainbow;

struct Rainbow;

struct LED : Widget {

	NVGcolor color;
	NVGcolor colorBorder;

	Rainbow *module = NULL;

	int id;

	float ledRadius = 5.0f;
	float ledStrokeWidth = 1.0f;
	float xCenter;
	float yCenter;

	LED(int i, float xPos, float yPos) {
		id = i;
		box.pos.x = xPos;
		box.pos.y = yPos;
		box.size.x = ledRadius * 2.0f + ledStrokeWidth * 2.0f;
		box.size.y = ledRadius * 2.0f + ledStrokeWidth * 2.0f;
		color = nvgRGB(255, 255, 255);
		Vec ctr = box.getCenter();
		xCenter = ctr.x / SVG_DPI;
		yCenter = ctr.y / SVG_DPI;
	}

	// void draw(const DrawArgs& args) override {
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;

		nvgGlobalTint(args.vg, color::WHITE);
		nvgFillColor(args.vg, color);
		nvgStrokeColor(args.vg, colorBorder);
		nvgStrokeWidth(args.vg, ledStrokeWidth);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0.0f, 0.0f);
		nvgCircle(args.vg, xCenter, yCenter, ledRadius);
		nvgFill(args.vg);
		nvgStroke(args.vg);

		Widget::drawLayer(args, layer);
	}

	void onButton(const event::Button &e) override;

};

struct Rainbow : core::PrismModule {

	enum ParamIds {
		MORPH_PARAM,
		GLOBAL_Q_PARAM,
		GLOBAL_LEVEL_PARAM,
		SPREAD_PARAM,
		ENUMS(CHANNEL_Q_PARAM,6),
		ENUMS(CHANNEL_LEVEL_PARAM,6),
		FREQNUDGE1_PARAM,
		FREQNUDGE6_PARAM,
		SLEW_PARAM,
		SLEWON_PARAM, // Obsolete
		ENUMS(CHANNEL_Q_ON_PARAM,6),
		FILTER_PARAM,
		MOD135_PARAM,
		MOD246_PARAM,
		SCALEROT_PARAM,
		PREPOST_PARAM,
		ENV_PARAM,
		ENUMS(LOCKON_PARAM,6),
		ROTCW_PARAM,
		ROTCCW_PARAM,
		SCALECW_PARAM,
		SCALECCW_PARAM,
		BANK_PARAM,
		SWITCHBANK_PARAM,
		ENUMS(TRANS_PARAM,6),
		VOCTGLIDE_PARAM,
		NOISE_PARAM,
		COMPRESS_PARAM, // Obsolete
		ENUMS(LEVEL_OUT_PARAM,6),
		OUTCHAN_PARAM,
		LOCK135_PARAM,
		LOCK246_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		MORPH_INPUT,
		POLY_Q_INPUT,
		POLY_LEVEL_INPUT,
		SCALE_INPUT,
		SPREAD_INPUT,
		ROTATECV_INPUT,
		FREQCV1_INPUT,
		FREQCV6_INPUT,
		ROTCW_INPUT,
		ROTCCW_INPUT,
		LOCK135_INPUT,
		LOCK246_INPUT,
		POLY_IN_INPUT,
		GLOBAL_Q_INPUT,
		GLOBAL_LEVEL_INPUT,
		ENUMS(MONO_Q_INPUT,6),
		ENUMS(MONO_LEVEL_INPUT,6),
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUT_OUTPUT,
		POLY_ENV_OUTPUT,
		POLY_VOCT_OUTPUT,
		POLY_DEBUG_OUTPUT,
		ENUMS(MONO_ENV_OUTPUT,6),
		ENUMS(MONO_VOCT_OUTPUT,6),
		NUM_OUTPUTS
	};
	enum LightIds {
		CLIP_LIGHT,
		ENUMS(LOCK_LIGHT,6),
		ENUMS(QLOCK_LIGHT,6),
		NOISE_LIGHT,
		SCALEROT_LIGHT,
		VOCTGLIDE_LIGHT,
		PREPOST_LIGHT,
		POLYCV1IN_LIGHT,
		POLYCV6IN_LIGHT,
		MONOIN_LIGHT,
		ENUMS(OEIN_LIGHT,2),
		POLYIN_LIGHT,
		CPUMODE_LIGHT,
		NUM_LIGHTS
	};

	LED *ringLEDs[NUM_FILTS] = {};
	LED *scaleLEDs[NUM_SCALES] = {};
	LED *envelopeLEDs[NUM_CHANNELS] = {};
	LED *qLEDs[NUM_CHANNELS] = {};
	LED *tuningLEDs[NUM_CHANNELS] = {};

	dsp::VuMeter2 vuMeters[6];
	dsp::ClockDivider lightDivider;
	uint32_t channelClipCnt[6];
	float clipLimit = -5.2895f; // Clip at 10V;
	int frameRate = 735; // 44100Hz / 60fps

	NVGcolor defaultBorder = nvgRGB(73, 73, 73);
	NVGcolor blockedBorder = nvgRGB(255, 0, 0);

	Rotation	rotation;
	Envelope 	envelope;
	LEDRing 	ring;
	FilterBank	filterbank;
	IO 			io;
	Q 			q;
	Tuning 		tuning;
	Levels 		levels;
	Inputs 		input;  
	State 		state;

	void set_default_param_values(void);
	void load_from_state(void);
	void populate_state(void);

	void initialise(void);
	void prepare(void);

	RainbowScaleExpanderMessage *pMessage = new RainbowScaleExpanderMessage;
	RainbowScaleExpanderMessage *cMessage = new RainbowScaleExpanderMessage;

	int currBank = 0; // TODO Move to State
	int nextBank = 0;

	int currFilter = 0; // TODO Move to State
	int nextFilter = 0;

	rack::dsp::SchmittTrigger lockTriggers[6];
	rack::dsp::SchmittTrigger qlockTriggers[6];
	rack::dsp::SchmittTrigger lock135Trigger;
	rack::dsp::SchmittTrigger lock246Trigger;

	rack::dsp::SchmittTrigger lock135ButtonTrigger;
	rack::dsp::SchmittTrigger lock246ButtonTrigger;

	rack::dsp::SchmittTrigger rotCWTrigger;
	rack::dsp::SchmittTrigger rotCCWTrigger;

	rack::dsp::SchmittTrigger rotCWButtonTrigger;
	rack::dsp::SchmittTrigger rotCCWButtonTrigger;

	rack::dsp::SchmittTrigger scaleCWButtonTrigger;
	rack::dsp::SchmittTrigger scaleCCWButtonTrigger;

	rack::dsp::SchmittTrigger changeBankTrigger;

	rack::dsp::SchmittTrigger prepostTrigger;
	rack::dsp::SchmittTrigger scaleRotTrigger;
	rack::dsp::SchmittTrigger glissTrigger;

	rainbow::Audio audio;

	int frameC = 100000000;
	bool highCPUMode = false;
	bool highCPUModeChanged = true;
	int internalSampleRate = 48000;
	float freqScale = 2.0f;

	void setCPUMode(bool isHigh) {
		if (isHigh) {
			highCPUMode = true;
			internalSampleRate = 96000;
			freqScale = 1.0f;
		} else {
			highCPUMode = false;
			internalSampleRate = 48000;
			freqScale = 2.0f;
		}
		highCPUModeChanged = true;
	}

	json_t *dataToJson() override {

		json_t *rootJ = json_object();

		// highcpu
		json_t *cpuJ = json_integer((int) highCPUMode);
		json_object_set_new(rootJ, "highcpu", cpuJ);

		// gliss
		json_t *glissJ = json_integer((int) io.GLIDE_SWITCH);
		json_object_set_new(rootJ, "gliss", glissJ);

		// prepost
		json_t *prepostJ = json_integer((int) io.PREPOST_SWITCH);
		json_object_set_new(rootJ, "prepost", prepostJ);

		// scale rotation
		json_t *scalerotJ = json_integer((int) io.SCALEROT_SWITCH);
		json_object_set_new(rootJ, "scalerot", scalerotJ);

		// bank
		json_t *bankJ = json_integer((int) currBank);
		json_object_set_new(rootJ, "bank", bankJ);

		// qlocks
		json_t *qlocksJ = json_array();
		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *qlockJ = json_integer((int) io.CHANNEL_Q_ON[i]);
			json_array_append_new(qlocksJ, qlockJ);
		}
		json_object_set_new(rootJ, "qlocks", qlocksJ);

		// locks
		json_t *locksJ = json_array();
		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *lockJ = json_integer((int) io.LOCK_ON[i]);
			json_array_append_new(locksJ, lockJ);
		}
		json_object_set_new(rootJ, "locks", locksJ);

		// engine state
		json_t *note_array	  	= json_array();
		json_t *scale_array		 = json_array();
		json_t *scale_bank_array	= json_array();

		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *noteJ   		= json_integer(state.note[i]);
			json_t *scaleJ	  		= json_integer(state.scale[i]);
			json_t *scale_bankJ		= json_integer(state.scale_bank[i]);

			json_array_append_new(note_array,   	noteJ);
			json_array_append_new(scale_array,	  scaleJ);
			json_array_append_new(scale_bank_array,	scale_bankJ);
		}

		json_object_set_new(rootJ, "note",		note_array);
		json_object_set_new(rootJ, "scale",		scale_array);
		json_object_set_new(rootJ, "scalebank",	scale_bank_array);

		json_t *blockJ = json_string(io.FREQ_BLOCK.to_string().c_str());
		json_object_set_new(rootJ, "freqblock", blockJ);

		json_t *userscale96_array	= json_array();
		for (int i = 0; i < NUM_BANKNOTES; i++) {
			json_t *noteJ   		= json_real(state.userscale96[i]);
			json_array_append_new(userscale96_array,   	noteJ);
		}
		json_object_set_new(rootJ, "userscale",	userscale96_array);

		json_t *userscale48_array	= json_array();
		for (int i = 0; i < NUM_BANKNOTES; i++) {
			json_t *noteJ   		= json_real(state.userscale48[i]);
			json_array_append_new(userscale48_array,   	noteJ);
		}
		json_object_set_new(rootJ, "userscale48",	userscale48_array);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		// gliss
		json_t *cpuJ = json_object_get(rootJ, "highcpu");
		if (cpuJ) {
			setCPUMode(json_integer_value(cpuJ));
		}

		// gliss
		json_t *glissJ = json_object_get(rootJ, "gliss");
		if (glissJ)
			io.GLIDE_SWITCH = json_integer_value(glissJ);

		// prepost
		json_t *prepostJ = json_object_get(rootJ, "prepost");
		if (prepostJ)
			io.PREPOST_SWITCH = json_integer_value(prepostJ);

		// gliss
		json_t *scalerotJ = json_object_get(rootJ, "scalerot");
		if (scalerotJ)
			io.SCALEROT_SWITCH = json_integer_value(scalerotJ);

		// bank
		json_t *bankJ = json_object_get(rootJ, "bank");
		if (bankJ)
			currBank = json_integer_value(bankJ);

		// qlocks
		json_t *qlocksJ = json_object_get(rootJ, "qlocks");
		if (qlocksJ) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *qlockJ = json_array_get(qlocksJ, i);
				if (qlockJ)
					io.CHANNEL_Q_ON[i] = !!json_integer_value(qlockJ);
			}
		}

		// locks
		json_t *locksJ = json_object_get(rootJ, "locks");
		if (locksJ) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *lockJ = json_array_get(locksJ, i);
				if (lockJ)
					io.LOCK_ON[i] = !!json_integer_value(lockJ);
			}
		}

		if (!state.initialised) {
			set_default_param_values();
			return;
		}

		// note
		json_t *note_array = json_object_get(rootJ, "note");
		if (note_array) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *noteJ = json_array_get(note_array, i);
				if (noteJ)
					state.note[i] = json_integer_value(noteJ);
			}
		}

		// scale
		json_t *scale_array = json_object_get(rootJ, "scale");
		if (scale_array) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *scaleJ = json_array_get(scale_array, i);
				if (scaleJ)
					state.scale[i] = json_integer_value(scaleJ);
			}
		}

		// note
		json_t *scale_bank_array = json_object_get(rootJ, "scalebank");
		if (scale_bank_array) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *scale_bankJ = json_array_get(scale_bank_array, i);
				if (scale_bankJ)
					state.scale_bank[i] = json_integer_value(scale_bankJ);
			}
		}

		json_t *blockJ = json_object_get(rootJ, "freqblock");
		if (blockJ)
			io.FREQ_BLOCK = std::bitset<20>(json_string_value(blockJ));

		// userscale 48
		json_t *uscale48_array = json_object_get(rootJ, "userscale48");
		if (uscale48_array) {
			for (int i = 0; i < NUM_BANKNOTES; i++) {
				json_t *noteJ = json_array_get(uscale48_array, i);
				if (noteJ)
					state.userscale48[i] = json_real_value(noteJ);
			}
		}

		// userscale 96
		json_t *uscale96_array = json_object_get(rootJ, "userscale");
		if (uscale96_array) {
			for (int i = 0; i < NUM_BANKNOTES; i++) {
				json_t *noteJ = json_array_get(uscale96_array, i);
				if (noteJ)
					state.userscale96[i] = json_real_value(noteJ);
			}
		}

		load_from_state();

	}

	~Rainbow() {
		delete pMessage;
		delete cMessage;
	}

	Rainbow() : core::PrismModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) { 

		configParam(GLOBAL_Q_PARAM, 0, 4095, 2048, "Global Q");
		configParam(GLOBAL_LEVEL_PARAM, 0, 8191, 4095, "Global Level");
		configParam(SPREAD_PARAM, 0, 4095, 0, "Spread");
		configParam(MORPH_PARAM, 0, 4095, 0, "Morph");
		configParam(SLEW_PARAM, 0, 4095, 0, "Channel slew speed"); // 0% slew
		configSwitch(FILTER_PARAM, 0, 2, 0, "Filter type", {"2-pass", "1-pass", "bpre"});
		configButton(VOCTGLIDE_PARAM, "V/Oct glide");
		configButton(SCALEROT_PARAM, "Scale rotation");
		configButton(PREPOST_PARAM, "Envelope");
		configSwitch(ENV_PARAM, 0, 2, 0, "Envelope", {"fast", "slow", "trigger"});
		configSwitch(NOISE_PARAM, 0, 2, 0, "Noise", {"brown", "pink", "white"});
		configSwitch(OUTCHAN_PARAM, 0, 2, 0, "Output channels", {"mono", "stereo", "6"});

		configParam(COMPRESS_PARAM, 0, 1, 0, "Compress: off/on"); 

		configParam(FREQNUDGE1_PARAM, -4095, 4095, 0, "Freq Nudge odds");
		configParam(FREQNUDGE6_PARAM, -4095, 4095, 0, "Freq Nudge evens");
		configSwitch(MOD135_PARAM, 0, 1, 0, "Mod", {"1", "135"});
		configSwitch(MOD246_PARAM, 0, 1, 0, "Mod", {"6", "246"});

		configParam(BANK_PARAM, 0, 19, 0, "Bank"); 
		configParam(SWITCHBANK_PARAM, 0, 1, 0, "Switch bank"); 

		configParam(ROTCW_PARAM, 0, 1, 0, "Rotate CW/Up"); 
		configParam(ROTCCW_PARAM, 0, 1, 0, "Rotate CCW/Down"); 
		configParam(SCALECW_PARAM, 0, 1, 0, "Scale CW/Up"); 
		configParam(SCALECCW_PARAM, 0, 1, 0, "Scale CCW/Down"); 

		for (int n = 0; n < 6; n++) {
			configParam(CHANNEL_LEVEL_PARAM + n, 0, 4095, 4095, "Channel Level");
			configParam(LEVEL_OUT_PARAM + n, 0, 2, 1, "Channel Level");

			configParam(CHANNEL_Q_PARAM + n, 0, 4095, 2048, "Channel Q");
			configParam(CHANNEL_Q_ON_PARAM + n, 0, 1, 0, "Channel Q activate");

			configParam(LOCKON_PARAM + n, 0, 1, 0, "Lock channel");

			configParam(TRANS_PARAM + n, -12, 12, 0, "Semitone transpose"); 

			vuMeters[n].mode = dsp::VuMeter2::RMS;
			channelClipCnt[n] = 0;

		}

		lightDivider.setDivision(256);

		rotation.configure(&io, &filterbank);
		envelope.configure(&io, &levels);
		ring.configure(&io, &rotation, &envelope, &filterbank, &q);
		filterbank.configure(&io, &rotation, &envelope, &q, &tuning, &levels);
		q.configure(&io);
		tuning.configure(&io, &filterbank);
		levels.configure(&io);
		input.configure(&io, &rotation, &envelope, &filterbank, &tuning, &levels);

		initialise();

		rightExpander.producerMessage = pMessage;
		rightExpander.consumerMessage = cMessage;

		pMessage->updated = false;
		cMessage->updated = false;

		onSampleRateChange();

	}

	void onSampleRateChange() override {
		frameRate = APP->engine->getSampleRate() / 60;
	}

	void onReset() override {
		for (int i = 0 ; i < NUM_CHANNELS; i++) {
			io.LOCK_ON[i] = false;
			io.CHANNEL_Q_ON[i] = false;
		}
		io.FREQ_BLOCK.reset();

		currBank = 0;
		nextBank = 0;

		highCPUMode = false;

		initialise();
	}

	void toggleFreqblock(int id) {
		io.FREQ_BLOCK.flip(id);
	}

	void process(const ProcessArgs &args) override;

};

void Rainbow::process(const ProcessArgs &args) {

	PrismModule::step();

	io.UI_UPDATE = false;
	if (++frameC > frameRate) {
		frameC = 0;
		io.UI_UPDATE = true;
	}

	io.USERSCALE_CHANGED = false;
	if (rightExpander.module) {
		if (rightExpander.module->model == modelRainbowScaleExpander) {
			RainbowScaleExpanderMessage *cM = (RainbowScaleExpanderMessage*)rightExpander.consumerMessage;
			if (cM->updated) {
				for (int i = 0; i < NUM_BANKNOTES; i++) {
					io.USERSCALE96[i] = cM->maxq96[i]; 
					io.USERSCALE48[i] = cM->maxq48[i]; 
				}
				io.USERSCALE_CHANGED = true;
				io.READCOEFFS = true;
			} 
		}
	} 

	io.HICPUMODE = highCPUMode;
	if (highCPUModeChanged) { // Set from widget
		io.READCOEFFS = true;
		highCPUModeChanged = false;
	}

	if (rotCWTrigger.process(inputs[ROTCW_INPUT].getVoltage())) {
		io.ROTUP_TRIGGER = true;
	} else {
		io.ROTUP_TRIGGER = false;
	}

	if (rotCCWTrigger.process(inputs[ROTCCW_INPUT].getVoltage())) {
		io.ROTDOWN_TRIGGER = true;
	} else {
		io.ROTDOWN_TRIGGER = false;
	}

	if (rotCWButtonTrigger.process(params[ROTCW_PARAM].getValue())) {
		io.ROTUP_BUTTON = true;
	} else {
		io.ROTUP_BUTTON = false;
	}

	if (rotCCWButtonTrigger.process(params[ROTCCW_PARAM].getValue())) {
		io.ROTDOWN_BUTTON = true;
	} else {
		io.ROTDOWN_BUTTON = false;
	}

	if (scaleCWButtonTrigger.process(params[SCALECW_PARAM].getValue())) {
		io.SCALEUP_BUTTON = true;
	} else {
		io.SCALEUP_BUTTON = false;
	}

	if (scaleCCWButtonTrigger.process(params[SCALECCW_PARAM].getValue())) {
		io.SCALEDOWN_BUTTON = true;
	} else {
		io.SCALEDOWN_BUTTON = false;
	}

	io.MOD135_SWITCH 		= (Mod135Setting)params[MOD135_PARAM].getValue();
	io.MOD246_SWITCH 		= (Mod246Setting)params[MOD246_PARAM].getValue();

	if (lock135Trigger.process(inputs[LOCK135_INPUT].getVoltage()) ||
		lock135ButtonTrigger.process(params[LOCK135_PARAM].getValue())) {

		io.LOCK_ON[0] = !io.LOCK_ON[0];
		
		if (io.MOD135_SWITCH == Mod_135) {
			io.LOCK_ON[2] = !io.LOCK_ON[2];
			io.LOCK_ON[4] = !io.LOCK_ON[4];
		}
	} 

	if (lock246Trigger.process(inputs[LOCK246_INPUT].getVoltage()) ||
		lock246ButtonTrigger.process(params[LOCK246_PARAM].getValue())) {
		io.LOCK_ON[5] = !io.LOCK_ON[5];
		
		if (io.MOD246_SWITCH == Mod_246) {
			io.LOCK_ON[1] = !io.LOCK_ON[1];
			io.LOCK_ON[3] = !io.LOCK_ON[3];
		}
	} 

	for (int n = 0; n < 6; n++) {
		// Process Locks
		if (lockTriggers[n].process(params[LOCKON_PARAM + n].getValue())) {
			io.LOCK_ON[n] = !io.LOCK_ON[n];
		} 

		// Process QLocks
		if (qlockTriggers[n].process(params[CHANNEL_Q_ON_PARAM + n].getValue())) {
			io.CHANNEL_Q_ON[n] = !io.CHANNEL_Q_ON[n];
		}
	}

	// Handle bank/filter change
	nextBank = params[BANK_PARAM].getValue();
	nextFilter = (FilterSetting)params[FILTER_PARAM].getValue();

	// Handle filter change
	if (nextFilter != currFilter) {
		currFilter = nextFilter;
		if (nextFilter == Bpre && currBank == 19) { 
			// BpRe filters do not support user defined scales, so set bank to Major
			params[BANK_PARAM].setValue(0);
			currBank = 0;
			nextBank = 0;
			io.CHANGED_BANK = true;
			io.NEW_BANK = nextBank;
		}
	}

	// Handle bank switch press
	if (changeBankTrigger.process(params[SWITCHBANK_PARAM].getValue())) {
		if (io.FILTER_SWITCH == Bpre && nextBank == 19) {
			// BpRe filters do not support user defined scales, so prevent bank change to user defined
			io.CHANGED_BANK = false;
			params[BANK_PARAM].setValue(currBank);
		} else {
			io.CHANGED_BANK = true;
			io.NEW_BANK = nextBank;
			currBank = nextBank;
		}
	} else {
		io.CHANGED_BANK = false;
	}

	io.FILTER_SWITCH		= (FilterSetting)params[FILTER_PARAM].getValue();

	int noiseSelected 		= params[NOISE_PARAM].getValue();

	io.MORPH_ADC			= (uint16_t)clamp(params[MORPH_PARAM].getValue() + inputs[MORPH_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);
	io.SPREAD_ADC			= (uint16_t)clamp(params[SPREAD_PARAM].getValue() + inputs[SPREAD_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);

	io.GLOBAL_Q_LEVEL		= (int16_t)clamp(inputs[GLOBAL_Q_INPUT].getVoltage() * 409.5f, -4095.0f, 4095.0f);
	io.GLOBAL_Q_CONTROL		= (int16_t)params[GLOBAL_Q_PARAM].getValue();

	io.GLOBAL_LEVEL_ADC 	= params[GLOBAL_LEVEL_PARAM].getValue() / 4095.0f;
	io.GLOBAL_LEVEL_CV		= inputs[GLOBAL_LEVEL_INPUT].getVoltage() / 5.0f;

	for (int n = 0; n < 6; n++) {

		if (!inputs[MONO_LEVEL_INPUT + n].isConnected() && !inputs[POLY_LEVEL_INPUT].isConnected()) { 
			io.LEVEL_CV[n] = 1.0f;
		 } else {
			io.LEVEL_CV[n] = clamp((inputs[MONO_LEVEL_INPUT + n].getVoltage() + inputs[POLY_LEVEL_INPUT].getVoltage(n) + 5.0f) / 10.0f, 0.0f, 1.0f);
		 }

		io.LEVEL_ADC[n] 		= clamp(params[CHANNEL_LEVEL_PARAM + n].getValue() / 4095.0f, 0.0f, 1.0f);

		io.CHANNEL_Q_LEVEL[n] 	= (int16_t)clamp((inputs[MONO_Q_INPUT + n].getVoltage() + inputs[POLY_Q_INPUT].getVoltage(n))  * 409.5f, -4095.0f, 4095.0f);
		io.CHANNEL_Q_CONTROL[n]	= (int16_t)params[CHANNEL_Q_PARAM + n].getValue();

		io.TRANS_DIAL[n]		= params[TRANS_PARAM + n].getValue();
	}

	io.FREQNUDGE1_ADC		= (int16_t)params[FREQNUDGE1_PARAM].getValue();
	io.FREQNUDGE6_ADC		= (int16_t)params[FREQNUDGE6_PARAM].getValue();
	io.SCALE_ADC			= (uint16_t)clamp(inputs[SCALE_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);

	io.ROTCV_ADC			= (uint16_t)clamp(inputs[ROTATECV_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);

	io.FREQCV1_CHAN		= inputs[FREQCV1_INPUT].getChannels();
	io.FREQCV6_CHAN		= inputs[FREQCV6_INPUT].getChannels();
	for (int i = 0; i < 3; i++) {
		io.FREQCV1_CV[i] = clamp(inputs[FREQCV1_INPUT].getVoltage(i) * 0.5f, -5.0f, 5.0f); 
		io.FREQCV6_CV[i] = clamp(inputs[FREQCV6_INPUT].getVoltage(i) * 0.5f, -5.0f, 5.0f); 
	}

	io.SLEW_ADC		= (uint16_t)params[SLEW_PARAM].getValue();
	io.ENV_SWITCH	= (EnvelopeMode)params[ENV_PARAM].getValue();

	if (glissTrigger.process(params[VOCTGLIDE_PARAM].getValue())) {
		io.GLIDE_SWITCH = !io.GLIDE_SWITCH;
	} 

	if (prepostTrigger.process(params[PREPOST_PARAM].getValue())) {
		io.PREPOST_SWITCH = !io.PREPOST_SWITCH;
	} 

	if (scaleRotTrigger.process(params[SCALEROT_PARAM].getValue())) {
		io.SCALEROT_SWITCH = !io.SCALEROT_SWITCH;
	} 

	prepare();

	audio.inputChannels = std::min(inputs[POLY_IN_INPUT].getChannels(), 6);
	audio.outputChannels = params[OUTCHAN_PARAM].getValue(); 
	audio.noiseSelected = noiseSelected;
	audio.sampleRate = args.sampleRate;
	audio.internalSampleRate = internalSampleRate;
	audio.outputScale = freqScale;

	switch(audio.outputChannels) {
		case 0:
			audio.ChannelProcess1(io, inputs[POLY_IN_INPUT], outputs[POLY_OUT_OUTPUT], filterbank);
			break;
		case 1:
			audio.ChannelProcess2(io, inputs[POLY_IN_INPUT], outputs[POLY_OUT_OUTPUT], filterbank);
			break;
		case 2:
			audio.ChannelProcess6(io, inputs[POLY_IN_INPUT], outputs[POLY_OUT_OUTPUT], filterbank);
			break;
		default:
			audio.ChannelProcess1(io, inputs[POLY_IN_INPUT], outputs[POLY_OUT_OUTPUT], filterbank);
	}

	// Populate poly outputs
	outputs[POLY_VOCT_OUTPUT].setChannels(6);
	outputs[POLY_ENV_OUTPUT].setChannels(12);
	for (int n = 0; n < 6; n++) {
		outputs[POLY_ENV_OUTPUT].setVoltage(clamp(io.env_out[n] * 100.0f, 0.0f, 10.0f), n);
		outputs[POLY_ENV_OUTPUT].setVoltage(io.OUTLEVEL[n] * 10.0f, n + 6);
		outputs[POLY_VOCT_OUTPUT].setVoltage(io.voct_out[n], n);
		outputs[MONO_ENV_OUTPUT + n].setVoltage(clamp(io.env_out[n] * 100.0f, 0.0f, 10.0f));
		outputs[MONO_VOCT_OUTPUT + n].setVoltage(io.voct_out[n]);

		params[Rainbow::LEVEL_OUT_PARAM + n].setValue(io.OUTLEVEL[n]);
	}

	for (int n = 0; n < 6; n++) {
		vuMeters[n].process(args.sampleTime, io.channelLevel[n]);
	}

	if (io.UI_UPDATE) {

		// Set VCV LEDs
		for (int n = 0; n < 6; n++) {
			io.LOCK_ON[n] ? lights[LOCK_LIGHT + n].setBrightness(1.0f) : lights[LOCK_LIGHT + n].setBrightness(0.0f); 
			io.CHANNEL_Q_ON[n] ? lights[QLOCK_LIGHT + n].setBrightness(1.0f) : lights[QLOCK_LIGHT + n].setBrightness(0.0f); 
		}

		io.INPUT_CLIP ? lights[CLIP_LIGHT].setBrightness(1.0f) : lights[CLIP_LIGHT].setBrightness(0.0f); 

		inputs[POLY_IN_INPUT].getChannels() ? lights[NOISE_LIGHT].setBrightness(0.0f) : lights[NOISE_LIGHT].setBrightness(1.0f); 
		io.GLIDE_SWITCH ? lights[VOCTGLIDE_LIGHT].setBrightness(1.0f) : lights[VOCTGLIDE_LIGHT].setBrightness(0.0f); 
		io.PREPOST_SWITCH ? lights[PREPOST_LIGHT].setBrightness(0.0f) : lights[PREPOST_LIGHT].setBrightness(1.0f); // Light on if PRE (inverted)
		io.SCALEROT_SWITCH ? lights[SCALEROT_LIGHT].setBrightness(1.0f) : lights[SCALEROT_LIGHT].setBrightness(0.0f);

		io.FREQCV1_CHAN > 1 ? lights[POLYCV1IN_LIGHT].setBrightness(1.0f) : lights[POLYCV1IN_LIGHT].setBrightness(0.0f); 
		io.FREQCV6_CHAN > 1 ? lights[POLYCV6IN_LIGHT].setBrightness(1.0f) : lights[POLYCV6IN_LIGHT].setBrightness(0.0f); 

		highCPUMode ? lights[CPUMODE_LIGHT].setBrightness(1.0f) : lights[CPUMODE_LIGHT].setBrightness(0.0f); 

		switch(audio.inputChannels) {
			case 0:
					lights[MONOIN_LIGHT].setBrightness(0.0f);
					lights[OEIN_LIGHT].setBrightness(0.0f);
					lights[OEIN_LIGHT + 1].setBrightness(0.0f);
					lights[POLYIN_LIGHT].setBrightness(0.0f);
					break;
			case 1:
					lights[MONOIN_LIGHT].setBrightness(1.0f);
					lights[OEIN_LIGHT].setBrightness(0.0f);
					lights[OEIN_LIGHT + 1].setBrightness(0.0f);
					lights[POLYIN_LIGHT].setBrightness(0.0f);
					break;
			case 2:
					lights[MONOIN_LIGHT].setBrightness(0.0f);
					lights[OEIN_LIGHT].setBrightness(0.0f);
					lights[OEIN_LIGHT + 1].setBrightness(1.0f);
					lights[POLYIN_LIGHT].setBrightness(0.0f);
					break;
			case 3:
					lights[MONOIN_LIGHT].setBrightness(0.0f);
					lights[OEIN_LIGHT].setBrightness(1.0f);
					lights[OEIN_LIGHT + 1].setBrightness(0.0f);
					lights[POLYIN_LIGHT].setBrightness(0.0f);
					break;
			default:
					lights[MONOIN_LIGHT].setBrightness(0.0f);
					lights[OEIN_LIGHT].setBrightness(0.0f);
					lights[OEIN_LIGHT + 1].setBrightness(0.0f);
					lights[POLYIN_LIGHT].setBrightness(1.0f);
					break;
		}

		for (int i = 0; i < NUM_FILTS; i++) {
			if (io.FREQ_BLOCK[i]) {
				ringLEDs[i]->color 			= nvgRGBf(0.0f, 0.0f, 0.0f);
				ringLEDs[i]->colorBorder 	= blockedBorder;
			} else {
				ringLEDs[i]->color = nvgRGBf(
					io.ring[i][0], 
					io.ring[i][1],
					io.ring[i][2]);
				ringLEDs[i]->colorBorder = defaultBorder;
			}
		}

		for (int i = 0; i < NUM_SCALES; i++) {
			scaleLEDs[i]->color = nvgRGBf(
				io.scale[i][0], 
				io.scale[i][1],
				io.scale[i][2]);
			scaleLEDs[i]->colorBorder = defaultBorder;
		}

		bool procVu = lightDivider.process();
		for (int i = 0; i < NUM_CHANNELS; i++) {

			if (procVu) {
				vuMeters[i].getBrightness(clipLimit, clipLimit) == 1.0f ? channelClipCnt[i]++ : channelClipCnt[i] = 0;
			}

			if (channelClipCnt[i] & 32) {
				envelopeLEDs[i]->color = nvgRGBf(0.0f, 0.0f, 0.0f);
				envelopeLEDs[i]->colorBorder = defaultBorder;
			} else {
				envelopeLEDs[i]->color = nvgRGBf(
					io.envelope_leds[i][0], 
					io.envelope_leds[i][1],
					io.envelope_leds[i][2]);
				envelopeLEDs[i]->colorBorder = defaultBorder;
			}

			qLEDs[i]->color = nvgRGBf(
				io.q_leds[i][0], 
				io.q_leds[i][1],
				io.q_leds[i][2]);
			qLEDs[i]->colorBorder = defaultBorder;

			tuningLEDs[i]->color = nvgHSL(
				io.tuning_out_leds[i][0], 
				io.tuning_out_leds[i][1],
				io.tuning_out_leds[i][2]);
			tuningLEDs[i]->colorBorder = defaultBorder;

		}
	}
}

void Rainbow::initialise(void) {

	set_default_param_values();
	
	filterbank.set_default_user_scalebank();

	rotation.spread = (io.SPREAD_ADC >> 8) + 1;
	rotation.update_spread(1);

	tuning.initialise();

	envelope.initialise();

} 

void Rainbow::prepare(void) {

	input.param_read_switches();

	tuning.update();

	ring.update_led_ring();
	
	rotation.update_motion();

	envelope.update();

	int32_t t_spread = input.read_spread();
	if (t_spread != -1) {
		rotation.update_spread(t_spread);
	}

	filterbank.process_bank_change();

	filterbank.process_user_scale_change();

	if (io.ROTUP_TRIGGER || io.ROTUP_BUTTON) {
		rotation.rotate_up();
	}

	if (io.ROTDOWN_TRIGGER || io.ROTDOWN_BUTTON) {
		rotation.rotate_down();
	}

	if (io.SCALEUP_BUTTON) {
		rotation.change_scale_up();
	}

	if (io.SCALEDOWN_BUTTON) {
		rotation.change_scale_down();
	}

	input.process_rotateCV();
	input.process_scaleCV();

	levels.update();

	populate_state();

}

void Rainbow::set_default_param_values(void) {
	
	//Set default parameter values
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		filterbank.note[i]  				= i + 8;
		filterbank.scale[i] 				= 0;
		rotation.motion_fadeto_scale[i]	= filterbank.scale[i];
		rotation.motion_scale_dest[i]		= filterbank.scale[i];
		filterbank.scale_bank[i] 			= 0;
		rotation.motion_spread_dir[i]		= 0;
		rotation.motion_spread_dest[i]		= filterbank.note[i];
		rotation.motion_fadeto_note[i]		= filterbank.note[i];

		rotation.motion_morphpos[i]			= 0.0f;
		tuning.freq_shift[i]	 			= 0.0f;
		rotation.motion_scalecv_overage[i]	= 0;
	}

	rotation.motion_notejump	= 0;
	rotation.motion_rotate		= 0;

	filterbank.filter_type = MAXQ;
	filterbank.filter_mode = TWOPASS;

	state.initialised = true;

}

void Rainbow::load_from_state(void) {

	if(state.initialised) {

		//Set default parameter values
		for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
			filterbank.note[i]					= state.note[i];
			filterbank.scale[i]					= state.scale[i];
			rotation.motion_fadeto_scale[i]		= filterbank.scale[i];
			rotation.motion_scale_dest[i]		= filterbank.scale[i];
			filterbank.scale_bank[i]			= state.scale_bank[i];
			rotation.motion_spread_dir[i]		= 0;
			rotation.motion_spread_dest[i]		= filterbank.note[i];
			rotation.motion_fadeto_note[i]		= filterbank.note[i];

			rotation.motion_morphpos[i]			= 0.0f;
			tuning.freq_shift[i]				= 0.0f;
			rotation.motion_scalecv_overage[i] 	= 0;
		}

		for (int i = 0; i < NUM_BANKNOTES; i++) {
			filterbank.userscale_bank96[i] = state.userscale96[i];
			filterbank.userscale_bank48[i] = state.userscale48[i];
		}

		rotation.motion_notejump	= 0;
		rotation.motion_rotate		= 0;

		state.initialised = true;

	}

}

void Rainbow::populate_state(void) {

	if(state.initialised) {
		for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
			state.note[i]		= filterbank.note[i];
			state.scale[i]		= filterbank.scale[i];
			state.scale_bank[i]	= filterbank.scale_bank[i];
		}
	}

	for (int i = 0; i < NUM_BANKNOTES; i++) {
		state.userscale96[i] = filterbank.userscale_bank96[i]; 
		state.userscale48[i] = filterbank.userscale_bank48[i]; 
	}

}


void LED::onButton(const event::Button &e) {
	Widget::onButton(e);
	if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
		if (module) {
			module->toggleFreqblock(id);
		}
	} 
}

struct BankWidget : Widget {

	std::string fontPath;
	Rainbow *module = NULL;
	ScaleSet scales;
	NVGcolor colors[NUM_SCALEBANKS] = {

		// Shades of Blue
		nvgRGBf( 255.0f/255.0f,		070.0f/255.0f,	255.0f/255.0f	),
		nvgRGBf( 250.0f/255.0f,		080.0f/255.0f,	250.0f/255.0f	),
		nvgRGBf( 245.0f/255.0f,		090.0f/255.0f,	245.0f/255.0f	),
		nvgRGBf( 240.0f/255.0f,		100.0f/255.0f,	240.0f/255.0f	),
		nvgRGBf( 235.0f/255.0f,		110.0f/255.0f,	235.0f/255.0f	),
		nvgRGBf( 230.0f/255.0f,		120.0f/255.0f,	230.0f/255.0f	),
						
		// Shades of Cyan
		nvgRGBf( 150.0f/255.0f,		255.0f/255.0f,	255.0f/255.0f	),
		nvgRGBf( 130.0f/255.0f,		245.0f/255.0f,	245.0f/255.0f	),
		nvgRGBf( 120.0f/255.0f,		235.0f/255.0f,	235.0f/255.0f	),

		// Shades of Yellow
		nvgRGBf( 255.0f/255.0f,		255.0f/255.0f,	150.0f/255.0f	),
		nvgRGBf( 255.0f/255.0f,		245.0f/255.0f,	130.0f/255.0f	),
		nvgRGBf( 255.0f/255.0f,		235.0f/255.0f,	120.0f/255.0f	),
		nvgRGBf( 255.0f/255.0f,		225.0f/255.0f,	110.0f/255.0f	),

		// Shades of Green	
		nvgRGBf( 588.0f/1023.0f,	954.0f/1023.0f,	199.0f/1023.0f	),
		nvgRGBf( 274.0f/1023.0f,	944.0f/1023.0f,	67.0f/1023.0f	),
		nvgRGBf( 83.0f/1023.0f,		934.0f/1023.0f,	1.0f/1023.0f	),
		nvgRGBf( 1.0f/1023.0f,		924.0f/1023.0f,	1.0f/1023.0f	),
		nvgRGBf( 100.0f/1023.0f,	824.0f/1023.0f,	9.0f/1023.0f	),
		nvgRGBf( 100.0f/1023.0f,	724.0f/1023.0f,	4.0f/1023.0f	),

		nvgRGBf( 900.0f/1023.0f,	900.0f/1023.0f,	900.0f/1023.0f	)

	};

	BankWidget() {
		fontPath = asset::plugin(pluginInstance, "res/RobotoCondensed-Regular.ttf");
	}

	// void draw(const DrawArgs& ctx) override {
	void drawLayer(const DrawArgs& ctx, int layer) override {
		if (layer != 1) return;
		if (module == NULL) return;
		
		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
		if (font) {
			nvgGlobalTint(ctx.vg, color::WHITE);
			nvgFontSize(ctx.vg, 12.0f);
			nvgFontFaceId(ctx.vg, font->handle);

			char text[128];

			if (module->currBank == module->nextBank) {
				nvgFillColor(ctx.vg, colors[module->currBank]);
				snprintf(text, sizeof(text), "%s", scales.presets[module->currBank]->name.c_str());
			} else {
				nvgFillColor(ctx.vg, colors[module->nextBank]);
				snprintf(text, sizeof(text), "%s*", scales.presets[module->nextBank]->name.c_str());
			}

			nvgText(ctx.vg, 5, 13, text, NULL);
		}
		Widget::drawLayer(ctx, layer);
	}

};

struct RainbowWidget : ModuleWidget {
	
	RainbowWidget(Rainbow *module) {

		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/prism_Rainbow.svg")));

		addParam(createParamCentered<gui::PrismButton>(Vec(119.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCKON_PARAM+0));
		addParam(createParamCentered<gui::PrismButton>(Vec(159.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCKON_PARAM+1));
		addParam(createParamCentered<gui::PrismButton>(Vec(199.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCKON_PARAM+2));
		addParam(createParamCentered<gui::PrismButton>(Vec(239.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCKON_PARAM+3));
		addParam(createParamCentered<gui::PrismButton>(Vec(279.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCKON_PARAM+4));
		addParam(createParamCentered<gui::PrismButton>(Vec(319.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCKON_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(75.000 + 11.0, 380.0f - 126.000f - 11.0), module, Rainbow::SLEW_PARAM));
		addParam(createParam<gui::PrismSSwitch3>(Vec(79.5f, 380.0f - 272.504f - 35.0f), module, Rainbow::NOISE_PARAM));
		addParam(createParamCentered<gui::PrismButton>(Vec(479.000 + 7.000, 380.0f - 187.000 - 7.000), module, Rainbow::SCALEROT_PARAM)); 
		addParam(createParamCentered<gui::PrismButton>(Vec(423.000 + 7.000, 380.0f - 243.000 - 7.000), module, Rainbow::ROTCCW_PARAM));
		addParam(createParamCentered<gui::PrismButton>(Vec(535.000 + 7.000, 380.0f - 243.000 - 7.000), module, Rainbow::ROTCW_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(435.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::FREQNUDGE1_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(435.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::FREQNUDGE6_PARAM));
		addParam(createParam<gui::PrismLEDSlider>(Vec(119.0f + 2.5, 380.0f - 155.0f - 77.0f - 0.5), module, Rainbow::CHANNEL_LEVEL_PARAM+0));
		addParam(createParam<gui::PrismLEDSlider>(Vec(159.0f + 2.5, 380.0f - 155.0f - 77.0f - 0.5), module, Rainbow::CHANNEL_LEVEL_PARAM+1));
		addParam(createParam<gui::PrismLEDSlider>(Vec(199.0f + 2.5, 380.0f - 155.0f - 77.0f - 0.5), module, Rainbow::CHANNEL_LEVEL_PARAM+2));
		addParam(createParam<gui::PrismLEDSlider>(Vec(239.0f + 2.5, 380.0f - 155.0f - 77.0f - 0.5), module, Rainbow::CHANNEL_LEVEL_PARAM+3));
		addParam(createParam<gui::PrismLEDSlider>(Vec(279.0f + 2.5, 380.0f - 155.0f - 77.0f - 0.5), module, Rainbow::CHANNEL_LEVEL_PARAM+4));
		addParam(createParam<gui::PrismLEDSlider>(Vec(319.0f + 2.5, 380.0f - 155.0f - 77.0f - 0.5), module, Rainbow::CHANNEL_LEVEL_PARAM+5));
		addParam(createParam<gui::PrismSSwitch3>(Vec(79.5f, 380.0f - 205.5f - 33.0f), module, Rainbow::ENV_PARAM));
		addParam(createParamCentered<gui::PrismButton>(Vec(79.000 + 7.000, 380.0f - 187.000 - 7.000), module, Rainbow::PREPOST_PARAM)); 
		addParam(createParamCentered<gui::PrismButton>(Vec(79.000 + 7.000, 380.0f - 322.000 - 7.000), module, Rainbow::VOCTGLIDE_PARAM)); 

		addParam(createParamCentered<gui::PrismButton>(Vec(423.000 + 7.000, 380.0f - 131.000 - 7.000), module, Rainbow::SCALECCW_PARAM));
		addParam(createParamCentered<gui::PrismButton>(Vec(535.000 + 7.000, 380.0f - 131.000 - 7.000), module, Rainbow::SCALECW_PARAM));
		addParam(createParamCentered<gui::PrismButton>(Vec(119.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::CHANNEL_Q_ON_PARAM+0));
		addParam(createParamCentered<gui::PrismButton>(Vec(159.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::CHANNEL_Q_ON_PARAM+1));
		addParam(createParamCentered<gui::PrismButton>(Vec(199.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::CHANNEL_Q_ON_PARAM+2));
		addParam(createParamCentered<gui::PrismButton>(Vec(239.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::CHANNEL_Q_ON_PARAM+3));
		addParam(createParamCentered<gui::PrismButton>(Vec(279.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::CHANNEL_Q_ON_PARAM+4));
		addParam(createParamCentered<gui::PrismButton>(Vec(319.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::CHANNEL_Q_ON_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(115.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::CHANNEL_Q_PARAM+0));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(155.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::CHANNEL_Q_PARAM+1));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(195.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::CHANNEL_Q_PARAM+2));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(235.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::CHANNEL_Q_PARAM+3));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(275.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::CHANNEL_Q_PARAM+4));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(315.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::CHANNEL_Q_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobSnap>(Vec(395.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::BANK_PARAM));
		addParam(createParamCentered<gui::PrismButton>(Vec(439.000 + 7.000, 380.0f - 322.000 - 7.000), module, Rainbow::SWITCHBANK_PARAM));
		addParam(createParam<gui::PrismSSwitchR>(Vec(399.500f, 380.0f - 55.0f - 24.0f), module, Rainbow::MOD135_PARAM));
		addParam(createParam<gui::PrismSSwitchR>(Vec(399.500f, 380.0f - 25.0f - 24.0f), module, Rainbow::MOD246_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(Vec(115.000 + 11.0, 380.0f - 288.000 - 11.0), module, Rainbow::TRANS_PARAM+0));
		addParam(createParamCentered<gui::PrismKnobSnap>(Vec(155.000 + 11.0, 380.0f - 288.000 - 11.0), module, Rainbow::TRANS_PARAM+1));
		addParam(createParamCentered<gui::PrismKnobSnap>(Vec(195.000 + 11.0, 380.0f - 288.000 - 11.0), module, Rainbow::TRANS_PARAM+2));
		addParam(createParamCentered<gui::PrismKnobSnap>(Vec(235.000 + 11.0, 380.0f - 288.000 - 11.0), module, Rainbow::TRANS_PARAM+3));
		addParam(createParamCentered<gui::PrismKnobSnap>(Vec(275.000 + 11.0, 380.0f - 288.000 - 11.0), module, Rainbow::TRANS_PARAM+4));
		addParam(createParamCentered<gui::PrismKnobSnap>(Vec(315.000 + 11.0, 380.0f - 288.000 - 11.0), module, Rainbow::TRANS_PARAM+5));
		addParam(createParam<gui::PrismSSwitch3R>(Vec(39.500f, 380.0f - 272.500f - 35.000f), module, Rainbow::OUTCHAN_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(515.000 + 11.0, 380.0f - 288.000 - 11.0), module, Rainbow::MORPH_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(Vec(435.000 + 11.0, 380.0f - 288.000 - 11.0), module, Rainbow::SPREAD_PARAM));
		addParam(createParamCentered<gui::PrismLargeKnobSnap>(Vec(29.000 + 17.0, 380.0f - 80.000 - 17.000), module, Rainbow::GLOBAL_Q_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(Vec(75.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::FILTER_PARAM));
		addParam(createParamCentered<gui::PrismLargeKnobSnap>(Vec(29.000 + 17.0, 380.0f - 177.000 - 17.000), module, Rainbow::GLOBAL_LEVEL_PARAM));

		addParam(createParam<gui::PrismLEDIndicator>(Vec(133.0f, 380.0f - 224.0f), module, Rainbow::LEVEL_OUT_PARAM+0));
		addParam(createParam<gui::PrismLEDIndicator>(Vec(173.0f, 380.0f - 224.0f), module, Rainbow::LEVEL_OUT_PARAM+1));
		addParam(createParam<gui::PrismLEDIndicator>(Vec(213.0f, 380.0f - 224.0f), module, Rainbow::LEVEL_OUT_PARAM+2));
		addParam(createParam<gui::PrismLEDIndicator>(Vec(253.0f, 380.0f - 224.0f), module, Rainbow::LEVEL_OUT_PARAM+3));
		addParam(createParam<gui::PrismLEDIndicator>(Vec(293.0f, 380.0f - 224.0f), module, Rainbow::LEVEL_OUT_PARAM+4));
		addParam(createParam<gui::PrismLEDIndicator>(Vec(333.0f, 380.0f - 224.0f), module, Rainbow::LEVEL_OUT_PARAM+5));

		addParam(createParam<gui::PrismButton>(Vec(559.000, 380.0f - 60.000 - 14.000), module, Rainbow::LOCK135_PARAM));
		addParam(createParam<gui::PrismButton>(Vec(559.000, 380.0f - 30.000 - 14.000), module, Rainbow::LOCK246_PARAM));

		addInput(createInputCentered<gui::PrismPort>(Vec(475.000 + 11.0, 380.0f - 263.000 - 11.0), module, Rainbow::ROTATECV_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(395.000 + 11.0, 380.0f - 183.500 - 11.0), module, Rainbow::ROTCCW_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(555.000 + 11.0, 380.0f - 183.500 - 11.0), module, Rainbow::ROTCW_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(475.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::FREQCV1_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(475.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::FREQCV6_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(475.500 + 11.0, 380.0f - 103.000 - 11.0), module, Rainbow::SCALE_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(515.000 + 11.0, 380.0f - 56.000 - 11.0), module, Rainbow::LOCK135_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(515.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::LOCK246_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(35.000 + 11.0, 380.0f - 240.000 - 11.0), module, Rainbow::POLY_IN_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(555.000 + 11.0, 380.0f - 263.000 - 11.0), module, Rainbow::MORPH_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(395.000 + 11.0, 380.0f - 263.000 - 11.0), module, Rainbow::SPREAD_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(35.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::GLOBAL_Q_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(35.000 + 11.0, 380.0f - 126.000 - 11.0), module, Rainbow::GLOBAL_LEVEL_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(355.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::POLY_Q_INPUT));
		addInput(createInputCentered<gui::PrismPort>(Vec(355.000 + 11.0, 380.0f - 126.000 - 11.0), module, Rainbow::POLY_LEVEL_INPUT));

		addInput(createInputCentered<gui::PrismPort>(Vec(115.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::MONO_Q_INPUT+0));
		addInput(createInputCentered<gui::PrismPort>(Vec(155.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::MONO_Q_INPUT+1));
		addInput(createInputCentered<gui::PrismPort>(Vec(195.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::MONO_Q_INPUT+2));
		addInput(createInputCentered<gui::PrismPort>(Vec(235.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::MONO_Q_INPUT+3));
		addInput(createInputCentered<gui::PrismPort>(Vec(275.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::MONO_Q_INPUT+4));
		addInput(createInputCentered<gui::PrismPort>(Vec(315.000 + 11.0, 380.0f - 26.000 - 11.0), module, Rainbow::MONO_Q_INPUT+5));

		addInput(createInputCentered<gui::PrismPort>(Vec(115.000 + 11.0, 380.0f - 126.000 - 11.0), module, Rainbow::MONO_LEVEL_INPUT+0));
		addInput(createInputCentered<gui::PrismPort>(Vec(155.000 + 11.0, 380.0f - 126.000 - 11.0), module, Rainbow::MONO_LEVEL_INPUT+1));
		addInput(createInputCentered<gui::PrismPort>(Vec(195.000 + 11.0, 380.0f - 126.000 - 11.0), module, Rainbow::MONO_LEVEL_INPUT+2));
		addInput(createInputCentered<gui::PrismPort>(Vec(235.000 + 11.0, 380.0f - 126.000 - 11.0), module, Rainbow::MONO_LEVEL_INPUT+3));
		addInput(createInputCentered<gui::PrismPort>(Vec(275.000 + 11.0, 380.0f - 126.000 - 11.0), module, Rainbow::MONO_LEVEL_INPUT+4));
		addInput(createInputCentered<gui::PrismPort>(Vec(315.000 + 11.0, 380.0f - 126.000 - 11.0), module, Rainbow::MONO_LEVEL_INPUT+5));

		addOutput(createOutputCentered<gui::PrismPort>(Vec(35.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::POLY_OUT_OUTPUT));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(355.000 + 11.0, 380.0f - 240.000 - 11.0), module, Rainbow::POLY_ENV_OUTPUT));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(355.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::POLY_VOCT_OUTPUT));

		addOutput(createOutputCentered<gui::PrismPort>(Vec(115.000 + 11.0, 380.0f - 240.000 - 11.0), module, Rainbow::MONO_ENV_OUTPUT+0));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(155.000 + 11.0, 380.0f - 240.000 - 11.0), module, Rainbow::MONO_ENV_OUTPUT+1));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(195.000 + 11.0, 380.0f - 240.000 - 11.0), module, Rainbow::MONO_ENV_OUTPUT+2));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(235.000 + 11.0, 380.0f - 240.000 - 11.0), module, Rainbow::MONO_ENV_OUTPUT+3));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(275.000 + 11.0, 380.0f - 240.000 - 11.0), module, Rainbow::MONO_ENV_OUTPUT+4));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(315.000 + 11.0, 380.0f - 240.000 - 11.0), module, Rainbow::MONO_ENV_OUTPUT+5));

		addOutput(createOutputCentered<gui::PrismPort>(Vec(115.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::MONO_VOCT_OUTPUT+0));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(155.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::MONO_VOCT_OUTPUT+1));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(195.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::MONO_VOCT_OUTPUT+2));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(235.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::MONO_VOCT_OUTPUT+3));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(275.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::MONO_VOCT_OUTPUT+4));
		addOutput(createOutputCentered<gui::PrismPort>(Vec(315.000 + 11.0, 380.0f - 318.000 - 11.0), module, Rainbow::MONO_VOCT_OUTPUT+5));

		addChild(createLightCentered<MediumLight<RedLight>>(Vec(119.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCK_LIGHT+0));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(159.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCK_LIGHT+1));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(199.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCK_LIGHT+2));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(239.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCK_LIGHT+3));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(279.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCK_LIGHT+4));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(319.0f + 7.000, 380.0f - 352.000 - 7.000), module, Rainbow::LOCK_LIGHT+5));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(119.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::QLOCK_LIGHT+0));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(159.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::QLOCK_LIGHT+1));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(199.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::QLOCK_LIGHT+2));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(239.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::QLOCK_LIGHT+3));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(279.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::QLOCK_LIGHT+4));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(319.0f + 7.000, 380.0f - 90.000 - 7.000), module, Rainbow::QLOCK_LIGHT+5));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(17.500 + 4.500, 380.0f - 261.500 - 4.500), module, Rainbow::CLIP_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(81.500 + 4.500, 380.0f - 309.509 - 4.500), module, Rainbow::NOISE_LIGHT));

		addChild(createLightCentered<MediumLight<RedLight>>(Vec(479.000 + 7.000, 380.0f - 187.000 -7.000), module, Rainbow::SCALEROT_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(79.000 + 7.000, 380.0f - 187.000 - 7.000), module, Rainbow::PREPOST_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(79.000 + 7.000, 380.0f - 322.000 - 7.000), module, Rainbow::VOCTGLIDE_LIGHT));

		addChild(createLightCentered<TinyLight<RedLight>>(Vec((256.5 + 5.0) + 6 * 40.0, 380.0 - 77.500 - 4.5), module, Rainbow::POLYCV1IN_LIGHT));
		addChild(createLightCentered<TinyLight<RedLight>>(Vec((256.5 + 5.0) + 6 * 40.0, 380.0 - 77.500 - 4.5 + 30.0), module, Rainbow::POLYCV6IN_LIGHT));

		addChild(createLightCentered<SmallLight<RedLight>>(Vec(5.500f, 380.0f - 272.500f - 30.500f), module, Rainbow::MONOIN_LIGHT));
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(5.500f, 380.0f - 272.500f - 18.400f), module, Rainbow::OEIN_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(Vec(5.500f, 380.0f - 272.500f - 6.300f), module, Rainbow::POLYIN_LIGHT));

		addChild(createLightCentered<SmallLight<RedLight>>(Vec(5.500f, 5.500f), module, Rainbow::CPUMODE_LIGHT));

		if(module) {

			BankWidget *bankW = new BankWidget();
			bankW->module = module;
			bankW->box.pos = Vec(474.962f, 380.0 - 320.162 - 17.708);
			bankW->box.size = Vec(80.0, 20.0f);
			addChild(bankW);

			float XStartL = 106.5;
			float XStartR = 256.5 + 2.0;
			float xDelta = 40.0;
			float yVoct = 380.0 - 339.500 - 4.5;
			float yEnv = 380.0 - 261.500 - 4.5;
			float yQ = 380.0 - 77.500 - 4.5;

			for (int i = 0; i < 3; i++) {
				module->qLEDs[i] = new LED(i, XStartL + i * xDelta, yQ);
				module->qLEDs[i]->module = NULL;
				addChild(module->qLEDs[i]);

				module->envelopeLEDs[i] = new LED(i, XStartL + i * xDelta, yEnv);
				module->envelopeLEDs[i]->module = NULL;
				addChild(module->envelopeLEDs[i]);

				module->tuningLEDs[i] = new LED(i, XStartL + i * xDelta, yVoct);
				module->tuningLEDs[i]->module = NULL;
				addChild(module->tuningLEDs[i]);
			}

			for (int i = 3; i < 6; i++) {
				module->qLEDs[i] = new LED(i, XStartR + (i - 3) * xDelta, yQ);
				module->qLEDs[i]->module = NULL;
				addChild(module->qLEDs[i]);

				module->envelopeLEDs[i] = new LED(i, XStartR + (i - 3) * xDelta, yEnv);
				module->envelopeLEDs[i]->module = NULL;
				addChild(module->envelopeLEDs[i]);

				module->tuningLEDs[i] = new LED(i, XStartR + (i - 3) * xDelta, yVoct);
				module->tuningLEDs[i]->module = NULL;
				addChild(module->tuningLEDs[i]);
			}

		}

		if (module) {
			Vec ringBox(Vec(429.258, 137.198 - 2.9));
			float ringDiv = (core::PI * 2.0f) / NUM_FILTS;

			for (int i = 0; i < NUM_FILTS; i++) {
		
				float xPos  = sin(core::PI - ringDiv * i) * 50.0f;
				float yPos  = cos(core::PI - ringDiv * i) * 50.0f;

				module->ringLEDs[i] = new LED(i, ringBox.x + 50 + xPos, ringBox.y + 50.0f + yPos);
				module->ringLEDs[i]->module = module;
				addChild(module->ringLEDs[i]);
			}

			float scaleDiv = (core::PI * 2.0f) / NUM_SCALES;

			for (int i = 0; i < NUM_SCALES; i++) {
		
				float xPos  = sin(core::PI - scaleDiv * i) * 30.0f;
				float yPos  = cos(core::PI - scaleDiv * i) * 30.0f;

				module->scaleLEDs[i] = new LED(i, ringBox.x + 50.0f + xPos, ringBox.y + 50.0f + yPos);
				module->scaleLEDs[i]->module = NULL;
				addChild(module->scaleLEDs[i]);
			}
		}
	}

	void appendContextMenu(Menu *menu) override {

		Rainbow *rainbow = dynamic_cast<Rainbow*>(module);
		assert(rainbow);

		struct CPUItem : MenuItem {
			Rainbow *module;
			bool mode;
			void onAction(const rack::event::Action &e) override {
				module->setCPUMode(mode);
			}
		};

		struct CPUMenu : MenuItem {
			Rainbow *module;
			Menu *createChildMenu() override {
				Menu *menu = new Menu;
				std::vector<bool> modes = {true, false};
				std::vector<std::string> names = {"High CPU Mode (96Khz)", "Low CPU Mode (48KHz)"};

				for (size_t i = 0; i < modes.size(); i++) {
					CPUItem *item = createMenuItem<CPUItem>(names[i], CHECKMARK(module->highCPUMode == modes[i]));
					item->module = module;
					item->mode = modes[i];
					menu->addChild(item);
				}
				return menu;
			}
		};

		menu->addChild(construct<MenuLabel>());
		CPUMenu *item = createMenuItem<CPUMenu>("CPU Mode");
		item->module = rainbow;
		menu->addChild(item);

     }

};

Model *modelRainbow = createModel<Rainbow, RainbowWidget>("Rainbow");
