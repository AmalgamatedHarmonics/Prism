#include <bitset>

#include "common.hpp"

#include "plugin.hpp"
#include "Common.hpp"
#include "Rainbow.hpp"

#include "dsp/noise.hpp"

using namespace prism;

struct frame {
	int16_t l;
	int16_t r;
};

struct Rainbow;

struct LED : Widget {

	NVGcolor color;
	NVGcolor colorBorder;

	Rainbow *module = NULL;

	int id;

	float ledRadius = 5.0f;
	float ledStrokeWidth = 1.5f;
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

	void draw(const DrawArgs &args) override {
		nvgFillColor(args.vg, color);
		nvgStrokeColor(args.vg, colorBorder);
		nvgStrokeWidth(args.vg, ledStrokeWidth);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0.0f, 0.0f);
		nvgCircle(args.vg, xCenter, yCenter, ledRadius);
		nvgFill(args.vg);
		nvgStroke(args.vg);
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
		SLEWON_PARAM,
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
		COMPRESS_PARAM,
		ENUMS(LEVEL_OUT_PARAM,6),
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
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_LEFT_OUTPUT,
		OUT_RIGHT_OUTPUT,
		POLY_ENV_OUTPUT,
		POLY_VOCT_OUTPUT,
		POLY_DEBUG_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLIP_LIGHT,
		ENUMS(LOCK_LIGHT,6),
		ENUMS(QLOCK_LIGHT,6),
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

	NVGcolor defaultBorder = nvgRGB(50, 150, 50);
	NVGcolor blockedBorder = nvgRGB(255, 0, 0);

    rainbow::Controller main;

	int currBank = 0; // TODO Move to State
	int nextBank = 0;

	int32_t out[BUFFER_SIZE] = {};

	const float MIN_12BIT = -16777216.0f;
	const float MAX_12BIT = 16777215.0f;

	dsp::SampleRateConverter<2> twoInputSrc;
	dsp::SampleRateConverter<3> threeInputSrc;
	dsp::SampleRateConverter<6> sixInputSrc;

	dsp::SampleRateConverter<2> outputSrc;

	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> twoInputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<3>, 256> threeInputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<6>, 256> sixInputBuffer;

	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;

	bogaudio::dsp::PinkNoiseGenerator pink;
	bogaudio::dsp::RedNoiseGenerator brown;
	bogaudio::dsp::WhiteNoiseGenerator white;

	rack::dsp::SchmittTrigger lockTriggers[6];
	rack::dsp::SchmittTrigger qlockTriggers[6];
	rack::dsp::SchmittTrigger lock135Trigger;
	rack::dsp::SchmittTrigger lock246Trigger;

	rack::dsp::SchmittTrigger rotCWTrigger;
	rack::dsp::SchmittTrigger rotCCWTrigger;

	rack::dsp::SchmittTrigger rotCWButtonTrigger;
	rack::dsp::SchmittTrigger rotCCWButtonTrigger;

	rack::dsp::SchmittTrigger scaleCWButtonTrigger;
	rack::dsp::SchmittTrigger scaleCCWButtonTrigger;

	rack::dsp::SchmittTrigger changeBankTrigger;

	json_t *dataToJson() override {

        json_t *rootJ = json_object();

		// bank
		json_t *bankJ = json_integer((int) currBank);
		json_object_set_new(rootJ, "bank", bankJ);

		// qlocks
		json_t *qlocksJ = json_array();
		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *qlockJ = json_integer((int) main.io->CHANNEL_Q_ON[i]);
			json_array_append_new(qlocksJ, qlockJ);
		}
		json_object_set_new(rootJ, "qlocks", qlocksJ);

		// locks
		json_t *locksJ = json_array();
		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *lockJ = json_integer((int) main.io->LOCK_ON[i]);
			json_array_append_new(locksJ, lockJ);
		}
		json_object_set_new(rootJ, "locks", locksJ);

		// engine state
		json_t *note_array      	= json_array();
		json_t *scale_array         = json_array();
		json_t *scale_bank_array	= json_array();

		for (int i = 0; i < NUM_CHANNELS; i++) {
			json_t *noteJ   		= json_integer(main.state->note[i]);
			json_t *scaleJ      	= json_integer(main.state->scale[i]);
			json_t *scale_bankJ    	= json_integer(main.state->scale_bank[i]);

			json_array_append_new(note_array,   	noteJ);
			json_array_append_new(scale_array,      scaleJ);
			json_array_append_new(scale_bank_array,	scale_bankJ);
		}

		json_object_set_new(rootJ, "note",		note_array);
		json_object_set_new(rootJ, "scale",		scale_array);
		json_object_set_new(rootJ, "scalebank",	scale_bank_array);

		json_t *blockJ = json_string(main.io->FREQ_BLOCK.to_string().c_str());
		json_object_set_new(rootJ, "freqblock", blockJ);

        return rootJ;
    }

	void dataFromJson(json_t *rootJ) override {

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
					main.io->CHANNEL_Q_ON[i] = !!json_integer_value(qlockJ);
			}
		}

		// locks
		json_t *locksJ = json_object_get(rootJ, "locks");
		if (locksJ) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *lockJ = json_array_get(locksJ, i);
				if (lockJ)
					main.io->LOCK_ON[i] = !!json_integer_value(lockJ);
			}
		}

		if (!main.state->initialised) {
			main.set_default_param_values();
			return;
		}

		// note
		json_t *note_array = json_object_get(rootJ, "note");
		if (note_array) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *noteJ = json_array_get(note_array, i);
				if (noteJ)
					main.state->note[i] = json_integer_value(noteJ);
			}
		}

		// scale
		json_t *scale_array = json_object_get(rootJ, "scale");
		if (scale_array) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *scaleJ = json_array_get(scale_array, i);
				if (scaleJ)
					main.state->scale[i] = json_integer_value(scaleJ);
			}
		}

		// note
		json_t *scale_bank_array = json_object_get(rootJ, "scalebank");
		if (scale_bank_array) {
			for (int i = 0; i < NUM_CHANNELS; i++) {
				json_t *scale_bankJ = json_array_get(scale_bank_array, i);
				if (scale_bankJ)
					main.state->scale_bank[i] = json_integer_value(scale_bankJ);
			}
		}

		json_t *blockJ = json_object_get(rootJ, "freqblock");
		if (blockJ)
			main.io->FREQ_BLOCK = std::bitset<20>(json_string_value(blockJ));

		main.load_from_state();

	}

	void TwoChannelProcess(int inputChannels, int noiseSelected, float sampleRate);
	void ThreeChannelProcess(float sampleRate);
	void SixChannelProcess(float sampleRate);

	Rainbow() : core::PrismModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) { 

		configParam(GLOBAL_Q_PARAM, 0, 4095, 2048, "Global Q");
		configParam(GLOBAL_LEVEL_PARAM, 0, 4095, 4095, "Global Level");
		configParam(SPREAD_PARAM, 0, 4095, 0, "Spread");
		configParam(MORPH_PARAM, 0, 4095, 0, "Morph");

		configParam(SLEW_PARAM, 0, 4095, 0, "Channel slew speed"); // 0% slew
		configParam(FILTER_PARAM, 0, 2, 0, "Filter type: 2-pass, 1-pass, bpre"); // two/one/bpre
		configParam(VOCTGLIDE_PARAM, 0, 1, 0, "V/Oct glide on/off"); // on/off
		configParam(SCALEROT_PARAM, 0, 1, 0, "Scale rotation on/off"); // on/off
		configParam(PREPOST_PARAM, 0, 1, 0, "Envelope: pre/post"); //pre/post
		configParam(ENV_PARAM, 0, 2, 0, "Envelope: fast/slow/trigger"); // fast/slow/trigger
		configParam(NOISE_PARAM, 0, 2, 0, "Noise: brown/pink/white"); // brown/pink/white

        configParam(COMPRESS_PARAM, 0, 1, 0, "Compress: off/on"); 

	    configParam(FREQNUDGE1_PARAM, 0, 4095, 0, "Freq Nudge odds");
    	configParam(FREQNUDGE6_PARAM, 0, 4095, 0, "Freq Nudge evens");
		configParam(MOD135_PARAM, 0, 1, 0, "Mod 1/135"); // 1/135
		configParam(MOD246_PARAM, 0, 1, 0, "Mod 2/246"); // 6/246

		configParam(BANK_PARAM, 0, 18, 0, "Bank"); 
		configParam(SWITCHBANK_PARAM, 0, 1, 0, "Switch bank"); 

		configParam(ROTCW_PARAM, 0, 1, 0, "Rotate CW/Up"); 
		configParam(ROTCCW_PARAM, 0, 1, 0, "Rotate CCW/Down"); 
		configParam(SCALECW_PARAM, 0, 1, 0, "Scale CW/Up"); 
		configParam(SCALECCW_PARAM, 0, 1, 0, "Scale CCW/Down"); 

		for (int n = 0; n < 6; n++) {
			configParam(CHANNEL_LEVEL_PARAM + n, 0, 4095, 4095, "Channel Level");
			configParam(LEVEL_OUT_PARAM + n, 0, 1, 1, "Channel Level");

			configParam(CHANNEL_Q_PARAM + n, 0, 4095, 2048, "Channel Q");
			configParam(CHANNEL_Q_ON_PARAM + n, 0, 1, 0, "Channel Q activate");

			configParam(LOCKON_PARAM + n, 0, 1, 0, "Lock channel");

			configParam(TRANS_PARAM + n, -6, 6, 0, "Semitone transpose"); 

			vuMeters[n].mode = dsp::VuMeter2::RMS;
			channelClipCnt[n] = 0;

		}

		lightDivider.setDivision(256);

	    main.initialise();

	}

	void onReset() override {
		for (int i = 0 ; i < NUM_CHANNELS; i++) {
			main.io->LOCK_ON[i] = false;
			main.io->CHANNEL_Q_ON[i] = false;
		}
		main.io->FREQ_BLOCK.reset();

		currBank = 0;
		nextBank = 0;

		main.initialise();
	}

	void process(const ProcessArgs &args) override;

	void toggleFreqblock(int id) {
		main.io->FREQ_BLOCK.flip(id);
	}

};

void Rainbow::TwoChannelProcess(int inputChannels, int noiseSelected, float sampleRate) {
		// Get input
	dsp::Frame<2> twoInputFrame = {};
	if (!twoInputBuffer.full()) {
		if (inputChannels == 0) {
			float nO;
			float nE;
			switch (noiseSelected) {
				case 0:
					nO = brown.next() * 10.0f - 5.0f;
					nE = brown.next() * 10.0f - 5.0f;
					break;
				case 1:
					nO = pink.next() * 10.0f - 5.0f;
					nE = pink.next() * 10.0f - 5.0f;
					break;
				case 2:
					nO = white.next() * 10.0f - 5.0f;
					nE = white.next() * 10.0f - 5.0f;
					break;
				default:
					nO = pink.next() * 10.0f - 5.0f;
					nE = pink.next() * 10.0f - 5.0f;
			}
			twoInputFrame.samples[0] = nO / 5.0f;
			twoInputFrame.samples[1] = nE / 5.0f;
		} else if (inputChannels == 1) {
			twoInputFrame.samples[0] = inputs[POLY_IN_INPUT].getVoltage(0) / 5.0f;
			twoInputFrame.samples[1] = inputs[POLY_IN_INPUT].getVoltage(0) / 5.0f;
		} else {
			twoInputFrame.samples[0] = inputs[POLY_IN_INPUT].getVoltage(0) / 5.0f;
			twoInputFrame.samples[1] = inputs[POLY_IN_INPUT].getVoltage(1) / 5.0f;
		} 
		
		twoInputBuffer.push(twoInputFrame);		
	}

	// Process buffer
	if (outputBuffer.empty()) {

		{
			twoInputSrc.setRates(sampleRate, 96000);
			dsp::Frame<2> twoInputFrames[NUM_SAMPLES];
			int inLen = twoInputBuffer.size();
			int outLen = NUM_SAMPLES;
			twoInputSrc.process(twoInputBuffer.startData(), &inLen, twoInputFrames, &outLen);
			twoInputBuffer.startIncr(inLen);

			for (int i = 0; i < NUM_SAMPLES; i++) {
				int32_t odd = (int32_t)clamp(twoInputFrames[i].samples[0] * MAX_12BIT, MIN_12BIT, MAX_12BIT);
				int32_t even = (int32_t)clamp(twoInputFrames[i].samples[1] * MAX_12BIT, MIN_12BIT, MAX_12BIT);
				main.io->in[0][i] = odd;
				main.io->in[1][i] = even;
				main.io->in[2][i] = odd;
				main.io->in[3][i] = even;
				main.io->in[4][i] = odd;
				main.io->in[5][i] = even;
			}
		}

		main.process_audio();

		// Convert output buffer
		{
			dsp::Frame<2> outputFrames[NUM_SAMPLES];
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames[i].samples[0] = out[i * 2] / MAX_12BIT;
				outputFrames[i].samples[1] = out[i * 2 + 1] / MAX_12BIT;
			}

			outputSrc.setRates(96000, sampleRate);
			int inLen = NUM_SAMPLES;
			int outLen = outputBuffer.capacity();
			outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}
	}
}

void Rainbow::ThreeChannelProcess(float sampleRate) {
		// Get input
	dsp::Frame<3> threeInputFrame = {};
	if (!threeInputBuffer.full()) {
		for (int chan = 0; chan < 3; chan++) {
			threeInputFrame.samples[chan] = inputs[POLY_IN_INPUT].getVoltage(chan) / 5.0f;
		}
		threeInputBuffer.push(threeInputFrame);		
	}

	// Process buffer
	if (outputBuffer.empty()) {

		{
			threeInputSrc.setRates(sampleRate, 96000);
			dsp::Frame<3> threeInputFrames[NUM_SAMPLES];
			int inLen = threeInputBuffer.size();
			int outLen = NUM_SAMPLES;
			threeInputSrc.process(threeInputBuffer.startData(), &inLen, threeInputFrames, &outLen);
			threeInputBuffer.startIncr(inLen);

			for (int i = 0; i < NUM_SAMPLES; i++) {
				int32_t i0 = (int32_t)clamp(threeInputFrames[i].samples[0] * MAX_12BIT, MIN_12BIT, MAX_12BIT);
				int32_t i1 = (int32_t)clamp(threeInputFrames[i].samples[1] * MAX_12BIT, MIN_12BIT, MAX_12BIT);
				int32_t i2 = (int32_t)clamp(threeInputFrames[i].samples[2] * MAX_12BIT, MIN_12BIT, MAX_12BIT);
				main.io->in[0][i] = i0;
				main.io->in[1][i] = i0;
				main.io->in[2][i] = i1;
				main.io->in[3][i] = i1;
				main.io->in[4][i] = i2;
				main.io->in[5][i] = i2;
			}
		}

		main.process_audio();

		// Convert output buffer
		{
			dsp::Frame<2> outputFrames[NUM_SAMPLES];
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames[i].samples[0] = out[i * 2] / MAX_12BIT;
				outputFrames[i].samples[1] = out[i * 2 + 1] / MAX_12BIT;
			}

			outputSrc.setRates(96000, sampleRate);
			int inLen = NUM_SAMPLES;
			int outLen = outputBuffer.capacity();
			outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}
	}
}

void Rainbow::SixChannelProcess(float sampleRate) {
	// Get input
	dsp::Frame<NUM_CHANNELS> sixInputFrame = {};
	if (!sixInputBuffer.full()) {
		for (int chan = 0; chan < NUM_CHANNELS; chan++) {
			sixInputFrame.samples[chan] = inputs[POLY_IN_INPUT].getVoltage(chan) / 5.0f;
		}
		sixInputBuffer.push(sixInputFrame);		
	}

	// Process buffer
	if (outputBuffer.empty()) {

		{
			sixInputSrc.setRates(sampleRate, 96000);
			dsp::Frame<NUM_CHANNELS> sixInputFrames[NUM_SAMPLES];
			int inLen = sixInputBuffer.size();
			int outLen = NUM_SAMPLES;
			sixInputSrc.process(sixInputBuffer.startData(), &inLen, sixInputFrames, &outLen);
			sixInputBuffer.startIncr(inLen);

			for (int i = 0; i < NUM_CHANNELS; i++) {
				for (int j = 0; j < NUM_SAMPLES; j++) {
					main.io->in[i][j] = (int32_t)clamp(sixInputFrames[j].samples[i] * MAX_12BIT, MIN_12BIT, MAX_12BIT);
				}
			}
		}

		main.process_audio();

		// Convert output buffer
		{
			dsp::Frame<2> outputFrames[NUM_SAMPLES];
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames[i].samples[0] = out[i * 2] / MAX_12BIT;
				outputFrames[i].samples[1] = out[i * 2 + 1] / MAX_12BIT;
			}

			outputSrc.setRates(96000, sampleRate);
			int inLen = NUM_SAMPLES;
			int outLen = outputBuffer.capacity();
			outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}
	}
}

void Rainbow::process(const ProcessArgs &args) {
	
	PrismModule::step();

	if (rotCWTrigger.process(inputs[ROTCW_INPUT].getVoltage())) {
		main.io->ROTUP_TRIGGER = true;
	} else {
		main.io->ROTUP_TRIGGER = false;
	}

	if (rotCCWTrigger.process(inputs[ROTCCW_INPUT].getVoltage())) {
		main.io->ROTDOWN_TRIGGER = true;
	} else {
		main.io->ROTDOWN_TRIGGER = false;
	}

	if (rotCWButtonTrigger.process(params[ROTCW_PARAM].getValue())) {
    	main.io->ROTUP_BUTTON = true;
	} else {
    	main.io->ROTUP_BUTTON = false;
	}

	if (rotCCWButtonTrigger.process(params[ROTCCW_PARAM].getValue())) {
    	main.io->ROTDOWN_BUTTON = true;
	} else {
    	main.io->ROTDOWN_BUTTON = false;
	}

	if (scaleCWButtonTrigger.process(params[SCALECW_PARAM].getValue())) {
    	main.io->SCALEUP_BUTTON = true;
	} else {
    	main.io->SCALEUP_BUTTON = false;
	}

	if (scaleCCWButtonTrigger.process(params[SCALECCW_PARAM].getValue())) {
    	main.io->SCALEDOWN_BUTTON = true;
	} else {
    	main.io->SCALEDOWN_BUTTON = false;
	}

	main.io->MOD135_SWITCH 		= (Mod135Setting)params[MOD135_PARAM].getValue();
	main.io->MOD246_SWITCH 		= (Mod246Setting)params[MOD246_PARAM].getValue();

	if (lock135Trigger.process(inputs[LOCK135_INPUT].getVoltage())) {
		main.io->LOCK_ON[0] = !main.io->LOCK_ON[0];
		
		if (main.io->MOD135_SWITCH == Mod_135) {
			main.io->LOCK_ON[2] = !main.io->LOCK_ON[2];
			main.io->LOCK_ON[4] = !main.io->LOCK_ON[4];
		}
	} 

	if (lock246Trigger.process(inputs[LOCK246_INPUT].getVoltage())) {
		main.io->LOCK_ON[5] = !main.io->LOCK_ON[5];
		
		if (main.io->MOD246_SWITCH == Mod_246) {
			main.io->LOCK_ON[1] = !main.io->LOCK_ON[1];
			main.io->LOCK_ON[3] = !main.io->LOCK_ON[3];
		}
	} 

	for (int n = 0; n < 6; n++) {

		// Process Locks
		if (lockTriggers[n].process(params[LOCKON_PARAM + n].getValue())) {
			main.io->LOCK_ON[n] = !main.io->LOCK_ON[n];
		} 

		// Process QLocks
		if (qlockTriggers[n].process(params[CHANNEL_Q_ON_PARAM + n].getValue())) {
			main.io->CHANNEL_Q_ON[n] = !main.io->CHANNEL_Q_ON[n];
		}

	}

	nextBank = params[BANK_PARAM].getValue();

	if (changeBankTrigger.process(params[SWITCHBANK_PARAM].getValue())) {
    	main.io->CHANGED_BANK = true;
    	main.io->NEW_BANK = nextBank;
		currBank = nextBank;
	} else {
    	main.io->CHANGED_BANK = false;
	}

	int noiseSelected = params[NOISE_PARAM].getValue();

	main.io->MORPH_ADC 			= (uint16_t)clamp(params[MORPH_PARAM].getValue() + inputs[MORPH_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);
	main.io->SPREAD_ADC 		= (uint16_t)clamp(params[SPREAD_PARAM].getValue() + inputs[SPREAD_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);

	main.io->GlobalQLevel 			= (int16_t)clamp(inputs[GLOBAL_Q_INPUT].getVoltage() * 409.5f, -4095.0f, 4095.0f);
	main.io->GlobalQControl 		= (int16_t)params[GLOBAL_Q_PARAM].getValue();

	bool haveGlobalLevelCV		= inputs[GLOBAL_LEVEL_INPUT].isConnected();
	bool haveChannelLevelCV		= inputs[POLY_LEVEL_INPUT].isConnected();

	float globalLevelCV 		= haveGlobalLevelCV ?
		clamp(inputs[GLOBAL_LEVEL_INPUT].getVoltage() / 5.0f, -1.0f, 1.0f) :
		1.0f;

	float globalLevelControl 	= params[GLOBAL_LEVEL_PARAM].getValue() / 4095.0f;

	for (int n = 0; n < 6; n++) {

		main.io->ChannelQLevel[n] 	= (int16_t)clamp(inputs[POLY_Q_INPUT].getVoltage() * 409.5f, -4095.0, 4095.0f);
		main.io->ChannelQControl[n] = (int16_t)params[CHANNEL_Q_PARAM + n].getValue();

		float channelLevelControl 	= params[CHANNEL_LEVEL_PARAM + n].getValue() / 4095.0f;
		main.io->LEVEL[n] 			= globalLevelControl * channelLevelControl;

		float channelLevelCV 		= haveChannelLevelCV ?
			clamp(inputs[POLY_LEVEL_INPUT].getVoltage(n) / 5.0f, -1.0f, 1.0f) :
			1.0f;

		if (haveGlobalLevelCV || haveChannelLevelCV) {
			main.io->LEVEL[n] = main.io->LEVEL[n] + globalLevelCV * channelLevelCV;
		}

		main.io->TRANS_DIAL[n] 		= params[TRANS_PARAM + n].getValue();
	}

	main.io->FREQNUDGE1_ADC 	= (uint16_t)params[FREQNUDGE1_PARAM].getValue();
	main.io->FREQNUDGE6_ADC 	= (uint16_t)params[FREQNUDGE6_PARAM].getValue();
	main.io->SCALE_ADC 			= (uint16_t)clamp(inputs[SCALE_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);

	main.io->ROTCV_ADC 			= (uint16_t)clamp(inputs[ROTATECV_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);
	main.io->FREQCV1_ADC 		= (uint16_t)clamp(inputs[FREQCV1_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);
	main.io->FREQCV6_ADC 		= (uint16_t)clamp(inputs[FREQCV6_INPUT].getVoltage() * 409.5f, 0.0f, 4095.0f);

	main.io->SLEW_ADC			= (uint16_t)params[SLEW_PARAM].getValue();							

	main.io->FILTER_SWITCH 		= (FilterSetting)params[FILTER_PARAM].getValue();
	main.io->SCALEROT_SWITCH 	= (ScaleRotationSetting)params[SCALEROT_PARAM].getValue();
	main.io->PREPOST_SWITCH 	= (PrePostSetting)params[PREPOST_PARAM].getValue();
	main.io->ENV_SWITCH 		= (EnvelopeMode)params[ENV_PARAM].getValue();
	main.io->GLIDE_SWITCH		= (GlideSetting)params[VOCTGLIDE_PARAM].getValue();

	main.io->out 				= out;
	
	main.prepare();

	int inputChannels = inputs[POLY_IN_INPUT].getChannels();

	// 0 inputs -> 2 x noise on O/E channels
	// 1 inputs -> 1 x input on all channels
	// 2 inputs -> 2 x input on O/E channels
	// 3+ input -> 1 x inputs per channel

	switch(inputChannels) {
		case 0:
		case 1:
		case 2:
			TwoChannelProcess(inputChannels, noiseSelected, args.sampleRate);
			break;
		case 3:
			ThreeChannelProcess(args.sampleRate);
			break;
		default:
			SixChannelProcess(args.sampleRate);
	}

	// Set output
	dsp::Frame<2> outputFrame = {};
	if (!outputBuffer.empty()) {
		outputFrame = outputBuffer.shift();

		if (outputs[OUT_RIGHT_OUTPUT].isConnected()) {
			outputs[OUT_LEFT_OUTPUT].setVoltage(outputFrame.samples[1] * 5.0f); // TODO THis seems to be reversed for some reason
			outputs[OUT_RIGHT_OUTPUT].setVoltage(outputFrame.samples[0] * 5.0f);
		} else {
			outputs[OUT_LEFT_OUTPUT].setVoltage((outputFrame.samples[0] + outputFrame.samples[1]) * 2.5f);
		}
	}

	// Populate poly outputs
	outputs[POLY_VOCT_OUTPUT].setChannels(6);
	outputs[POLY_ENV_OUTPUT].setChannels(6);
	for (int n = 0; n < 6; n++) {
		outputs[POLY_ENV_OUTPUT].setVoltage(main.io->env_out[n] * 10.0f, n);
		outputs[POLY_VOCT_OUTPUT].setVoltage(main.io->voct_out[n], n);
	}

	for (int n = 0; n < 6; n++) {
		vuMeters[n].process(args.sampleTime, main.io->channelLevel[n]);
	}

	// outputs[POLY_DEBUG_OUTPUT].setChannels(16);
	// for (int n = 0; n < 16; n++) {
	// 	outputs[POLY_DEBUG_OUTPUT].setVoltage(main.io->DEBUG[n], n);
	// }

	// Set VCV LEDs
	for (int n = 0; n < 6; n++) {
		main.io->LOCK_ON[n] ? lights[LOCK_LIGHT + n].setBrightness(1.0f) : lights[LOCK_LIGHT + n].setBrightness(0.0f); 
		main.io->CHANNEL_Q_ON[n] ? lights[QLOCK_LIGHT + n].setBrightness(1.0f) : lights[QLOCK_LIGHT + n].setBrightness(0.0f); 
	}

	main.io->INPUT_CLIP ? lights[CLIP_LIGHT].setBrightness(1.0f) : lights[CLIP_LIGHT].setBrightness(0.0f); 

	for (int i = 0; i < NUM_FILTS; i++) {
		if (main.io->FREQ_BLOCK[i]) {
			ringLEDs[i]->color 			= nvgRGBf(0.0f, 0.0f, 0.0f);
			ringLEDs[i]->colorBorder 	= blockedBorder;
		} else {
			ringLEDs[i]->color = nvgRGBf(
				main.io->ring[i][0], 
				main.io->ring[i][1],
				main.io->ring[i][2]);
			ringLEDs[i]->colorBorder = defaultBorder;			
		}
	}

	for (int i = 0; i < NUM_SCALES; i++) {
		scaleLEDs[i]->color = nvgRGBf(
			main.io->scale[i][0], 
			main.io->scale[i][1],
			main.io->scale[i][2]);
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
				main.io->envelope_leds[i][0], 
				main.io->envelope_leds[i][1],
				main.io->envelope_leds[i][2]);
			envelopeLEDs[i]->colorBorder = defaultBorder;		
		}

		qLEDs[i]->color = nvgRGBf(
			main.io->q_leds[i][0], 
			main.io->q_leds[i][1],
			main.io->q_leds[i][2]);
		qLEDs[i]->colorBorder = defaultBorder;		

		tuningLEDs[i]->color = nvgRGBf(
			main.io->tuning_out_leds[i][0], 
			main.io->tuning_out_leds[i][1],
			main.io->tuning_out_leds[i][2]);
		tuningLEDs[i]->colorBorder = defaultBorder;		

		params[Rainbow::LEVEL_OUT_PARAM+i].setValue(main.io->OUTLEVEL[i]);

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

	std::shared_ptr<Font> font;

	BankWidget() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/BarlowCondensed-Bold.ttf"));
	}

	Rainbow *module = NULL;

	std::string banks[NUM_SCALEBANKS] = {"MAJOR (ET)", "MINOR (ET)", "INTERVALS (ET)", "TRIADS (ET)", "CHROMATIC (ET)", "WHOLE STEP (ET)", 
		"INTERVALS (JT)", "TRIADS (JT)", "WHOLE STEP (JT)", 
		"INDIAN PENTATONIC", "INDIAN SHRUTIS", "MESOPOTAMIAN", "GAMELAN PELOG",
		"ALPHA 1", "ALPHA 2", "GAMMA", "17 NOTE/OCT", "BOHLEN PIERCE", "296 EQ",
		"User Scale (null)"
		};

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

		nvgRGBf( 700.0f/1023.0f		, 700.0f/1023.0f	, 700.0f/1023.0f)

	};

	void draw(const DrawArgs &ctx) override {

		if (module == NULL) {
			return;
	    }

		nvgFontSize(ctx.vg, 17.0f);
		nvgFontFaceId(ctx.vg, font->handle);
		// nvgTextLetterSpacing(ctx.vg, -1);

		char text[128];

		if (module->currBank == module->nextBank) {
			nvgFillColor(ctx.vg, colors[module->currBank]);
			snprintf(text, sizeof(text), "%s", banks[module->currBank].c_str());
			nvgText(ctx.vg, 0, box.pos.y, text, NULL);
		} else {
			nvgFillColor(ctx.vg, colors[module->nextBank]);
			snprintf(text, sizeof(text), "%s*", banks[module->nextBank].c_str());
			nvgText(ctx.vg, 0, box.pos.y, text, NULL);
		}

	}

};

struct RainbowWidget : ModuleWidget {
	
	RainbowWidget(Rainbow *module) {

		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Rainbow.svg")));

		// addOutput(createOutput<PJ301MPort>(mm2px(Vec(5,5)), module, Rainbow::POLY_DEBUG_OUTPUT));

		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(116.911f, 15.686f)), module, Rainbow::LOCKON_PARAM+0));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(128.057f, 15.686f)), module, Rainbow::LOCKON_PARAM+1));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(139.202f, 15.686f)), module, Rainbow::LOCKON_PARAM+2));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(150.348f, 15.686f)), module, Rainbow::LOCKON_PARAM+3));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(161.494f, 15.686f)), module, Rainbow::LOCKON_PARAM+4));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(172.64f, 15.686f)), module, Rainbow::LOCKON_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(25.752f, 16.594f)), module, Rainbow::SLEW_PARAM));
		addParam(createParam<gui::PrismSSwitch3>(mm2px(Vec(49.765f, 9.144f)), module, Rainbow::NOISE_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(69.517f, 9.144f)), module, Rainbow::SCALEROT_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(229.051f, 21.33f)), module, Rainbow::ROTCCW_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(266.468f, 21.33f)), module, Rainbow::ROTCW_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(100.89f, 35.8f)), module, Rainbow::FREQNUDGE1_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(188.66f, 35.8f)), module, Rainbow::FREQNUDGE6_PARAM));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(113.461f, 25.372f)), module, Rainbow::CHANNEL_LEVEL_PARAM+0));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(124.607f, 25.372f)), module, Rainbow::CHANNEL_LEVEL_PARAM+1));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(135.752f, 25.372f)), module, Rainbow::CHANNEL_LEVEL_PARAM+2));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(146.898f, 25.372f)), module, Rainbow::CHANNEL_LEVEL_PARAM+3));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(158.044f, 25.372f)), module, Rainbow::CHANNEL_LEVEL_PARAM+4));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(169.19f, 25.372f)), module, Rainbow::CHANNEL_LEVEL_PARAM+5));

		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(118.761f, 25.372f)), module, Rainbow::LEVEL_OUT_PARAM+0));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(129.907f, 25.372f)), module, Rainbow::LEVEL_OUT_PARAM+1));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(141.052f, 25.372f)), module, Rainbow::LEVEL_OUT_PARAM+2));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(152.198f, 25.372f)), module, Rainbow::LEVEL_OUT_PARAM+3));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(163.344f, 25.372f)), module, Rainbow::LEVEL_OUT_PARAM+4));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(174.49f, 25.372f)), module, Rainbow::LEVEL_OUT_PARAM+5));

		addParam(createParam<gui::PrismSSwitch3>(mm2px(Vec(10.261f, 40.692f)), module, Rainbow::ENV_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(30.013f, 40.692f)), module, Rainbow::PREPOST_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(49.765f, 40.692f)), module, Rainbow::VOCTGLIDE_PARAM));

		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(229.051f, 58.747f)), module, Rainbow::SCALECCW_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(266.468f, 58.747f)), module, Rainbow::SCALECW_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(116.911f, 68.355f)), module, Rainbow::CHANNEL_Q_ON_PARAM+0));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(128.057f, 68.355f)), module, Rainbow::CHANNEL_Q_ON_PARAM+1));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(139.202f, 68.355f)), module, Rainbow::CHANNEL_Q_ON_PARAM+2));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(150.348f, 68.355f)), module, Rainbow::CHANNEL_Q_ON_PARAM+3));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(161.494f, 68.355f)), module, Rainbow::CHANNEL_Q_ON_PARAM+4));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(172.64f, 68.355f)), module, Rainbow::CHANNEL_Q_ON_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(116.911f, 82.174f)), module, Rainbow::CHANNEL_Q_PARAM+0));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(128.057f, 82.174f)), module, Rainbow::CHANNEL_Q_PARAM+1));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(139.202f, 82.174f)), module, Rainbow::CHANNEL_Q_PARAM+2));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(150.348f, 82.174f)), module, Rainbow::CHANNEL_Q_PARAM+3));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(161.494f, 82.174f)), module, Rainbow::CHANNEL_Q_PARAM+4));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(172.64f, 82.174f)), module, Rainbow::CHANNEL_Q_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(269.073f, 87.016f)), module, Rainbow::BANK_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(282.213f, 87.016f)), module, Rainbow::SWITCHBANK_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(98.44f, 86.261f)), module, Rainbow::MOD135_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(186.21f, 86.261f)), module, Rainbow::MOD246_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(116.911f, 98.611f)), module, Rainbow::TRANS_PARAM+0));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(128.057f, 98.611f)), module, Rainbow::TRANS_PARAM+1));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(139.202f, 98.611f)), module, Rainbow::TRANS_PARAM+2));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(150.348f, 98.611f)), module, Rainbow::TRANS_PARAM+3));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(161.494f, 98.611f)), module, Rainbow::TRANS_PARAM+4));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(172.64f, 98.611f)), module, Rainbow::TRANS_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(231.28f, 106.866f)), module, Rainbow::MORPH_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(263.144f, 106.866f)), module, Rainbow::SPREAD_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(100.89f, 118.183f)), module, Rainbow::GLOBAL_Q_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(144.775f, 118.183f)), module, Rainbow::FILTER_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(188.66f, 118.183f)), module, Rainbow::GLOBAL_LEVEL_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(247.76f, 13.58f)), module, Rainbow::ROTATECV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(221.301f, 40.038f)), module, Rainbow::ROTCCW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(274.218f, 40.038f)), module, Rainbow::ROTCW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(100.89f, 53.655f)), module, Rainbow::FREQCV1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(188.66f, 53.655f)), module, Rainbow::FREQCV6_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(247.76f, 66.497f)), module, Rainbow::SCALE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(100.89f, 72.407f)), module, Rainbow::LOCK135_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(188.66f, 72.407f)), module, Rainbow::LOCK246_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.899f, 76.69f)), module, Rainbow::POLY_IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(220.022f, 118.124f)), module, Rainbow::MORPH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(274.402f, 118.124f)), module, Rainbow::SPREAD_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(86.52f, 118.182f)), module, Rainbow::GLOBAL_Q_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(202.983f, 118.182f)), module, Rainbow::GLOBAL_LEVEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(117.04f, 118.183f)), module, Rainbow::POLY_Q_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(172.511f, 118.183f)), module, Rainbow::POLY_LEVEL_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.899f, 97.113f)), module, Rainbow::OUT_LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(54.778f, 97.321f)), module, Rainbow::OUT_RIGHT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.899f, 119.686f)), module, Rainbow::POLY_ENV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(54.778f, 119.686f)), module, Rainbow::POLY_VOCT_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(116.911f, 15.686f)), module, Rainbow::LOCK_LIGHT+0));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(128.057f, 15.686f)), module, Rainbow::LOCK_LIGHT+1));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(139.202f, 15.686f)), module, Rainbow::LOCK_LIGHT+2));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(150.348f, 15.686f)), module, Rainbow::LOCK_LIGHT+3));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(161.494f, 15.686f)), module, Rainbow::LOCK_LIGHT+4));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(172.64f, 15.686f)), module, Rainbow::LOCK_LIGHT+5));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(116.911f, 68.355f)), module, Rainbow::QLOCK_LIGHT+0));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(128.057f, 68.355f)), module, Rainbow::QLOCK_LIGHT+1));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(139.202f, 68.355f)), module, Rainbow::QLOCK_LIGHT+2));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(150.348f, 68.355f)), module, Rainbow::QLOCK_LIGHT+3));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(161.494f, 68.355f)), module, Rainbow::QLOCK_LIGHT+4));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(172.64f, 68.355f)), module, Rainbow::QLOCK_LIGHT+5));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(40.366f, 72.769f)), module, Rainbow::CLIP_LIGHT));

		if(module) {

			BankWidget *bankW = new BankWidget();
			bankW->module = module;
			bankW->box.pos = mm2px(Vec(224.0f, 44.4f));
			bankW->box.size = Vec(80.0, 20.0f);
			addChild(bankW);

			Vec levelStrip(mm2px(Vec(113.3f, 21.5f)));
			Vec channelStrip(mm2px(Vec(113.3f, 54.0f)));

			for (int i = 0; i < NUM_CHANNELS; i++) {
				module->qLEDs[i] = new LED(i, channelStrip.x + i * 32.5f, channelStrip.y + 11.0f);
				module->qLEDs[i]->module = NULL;
				addChild(module->qLEDs[i]);

				module->envelopeLEDs[i] = new LED(i, channelStrip.x + i * 32.5f + 12.0f, channelStrip.y + 11.0f);
				module->envelopeLEDs[i]->module = NULL;
				addChild(module->envelopeLEDs[i]);

				module->tuningLEDs[i] = new LED(i, channelStrip.x + i * 32.5f + 6.0f, channelStrip.y + 0.0f);
				module->tuningLEDs[i]->module = NULL;
				addChild(module->tuningLEDs[i]);
			}
		}

		if (module) {
			Vec ringBox(mm2px(Vec(227.794f, 22.6f)));
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
};

Model *modelRainbow = createModel<Rainbow, RainbowWidget>("Rainbow");
