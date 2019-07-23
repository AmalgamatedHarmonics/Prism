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

	float ledRadius = 5;
	float ledStrokeWidth = 1.5;
	float xCenter;
	float yCenter;

	LED(int i, float xPos, float yPos) {
		id = i;
		box.pos.x = xPos;
		box.pos.y = yPos;
		box.size.x = ledRadius * 2 + ledStrokeWidth * 2;
		box.size.y = ledRadius * 2 + ledStrokeWidth * 2;
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
		nvgMoveTo(args.vg, 0, 0);
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
		ODD_ATTN_PARAM,
		EVEN_ATTN_PARAM,
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
		IN_ODD_INPUT,
		IN_EVEN_INPUT,
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
		CLIP_ODD_LIGHT,
		CLIP_EVEN_LIGHT,
		ENUMS(LOCK_LIGHT,6),
		ENUMS(QLOCK_LIGHT,6),
		NUM_LIGHTS
	};

	LED *ringLEDs[NUM_FILTS] = {};
	LED *scaleLEDs[NUM_SCALES] = {};
	LED *envelopeLEDs[NUM_CHANNELS] = {};
	LED *qLEDs[NUM_CHANNELS] = {};
	LED *tuningLEDs[NUM_CHANNELS] = {};
	LED *outputLEDs[NUM_CHANNELS] = {};

	dsp::VuMeter2 vuMeters[6];
	dsp::ClockDivider lightDivider;

	NVGcolor defaultBorder = nvgRGB(50, 150, 50);
	NVGcolor blockedBorder = nvgRGB(255, 0, 0);

    rainbow::Controller main;

	int currBank = 0; // TODO Move to State
	int nextBank = 0;

	int32_t in[BUFFER_SIZE] = {};  	// 16 stereo sample pairs
	int32_t out[BUFFER_SIZE] = {};

	dsp::SampleRateConverter<2> inputSrc;
	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> inputBuffer;
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

	Rainbow() : core::PrismModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) { 

		configParam(GLOBAL_Q_PARAM, 0, 4095, 2048, "Global Q");
		configParam(GLOBAL_LEVEL_PARAM, 0, 4095, 4095, "Global Level");
		configParam(SPREAD_PARAM, 0, 4095, 0, "Spread");
		configParam(MORPH_PARAM, 0, 4095, 0, "Morph");

		configParam(SLEW_PARAM, 0, 4095, 2048, "Channel slew speed"); // 50% slew
		configParam(SLEWON_PARAM, 0, 2, 0, "Channel slew: on/from morph/from pot"); // off/morph/control
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

		configParam(ODD_ATTN_PARAM, 0.0f, 1.0f, 1.0f, "Odd input attenuation");
		configParam(EVEN_ATTN_PARAM, 0.0f, 1.0f, 1.0f, "Even input attenuation");

		for (int n = 0; n < 6; n++) {
			configParam(CHANNEL_LEVEL_PARAM + n, 0, 4095, 4095, "Channel Level");
			configParam(LEVEL_OUT_PARAM + n, 0, 1, 1, "Channel Level");

			configParam(CHANNEL_Q_PARAM + n, 0, 4095, 2048, "Channel Q");
			configParam(CHANNEL_Q_ON_PARAM + n, 0, 1, 0, "Channel Q activate");

			configParam(LOCKON_PARAM + n, 0, 1, 0, "Lock channel");

			configParam(TRANS_PARAM + n, -6, 6, 0, "Semitone transpose"); 

			vuMeters[n].mode = dsp::VuMeter2::RMS;

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

void Rainbow::process(const ProcessArgs &args) {
	
	PrismModule::step();

	// float inMin = INT32_MIN;
	// float inMax = INT32_MAX;
	// float outMax = INT32_MAX;
	float inMin = -16777216.0f;
	float inMax = 16777215.0f;
	float outMax = 16777215.0f;

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

    // main.io->COMPRESS			= (Compressor)params[COMPRESS_PARAM].getValue();
    main.io->COMPRESS			= CompressorOff;

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

	main.io->MORPH_ADC 			= (uint16_t)clamp(params[MORPH_PARAM].getValue() + inputs[MORPH_INPUT].getVoltage() * 409.5f, 0.0, 4095.0f);
	main.io->SPREAD_ADC 		= (uint16_t)clamp(params[SPREAD_PARAM].getValue() + inputs[SPREAD_INPUT].getVoltage() * 409.5f, 0.0, 4095.0f);

	main.io->GlobalQLevel 			= (int16_t)clamp(inputs[GLOBAL_Q_INPUT].getVoltage() * 409.5f, -4095.0, 4095.0f);
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
	main.io->SCALE_ADC 			= (uint16_t)clamp(inputs[SCALE_INPUT].getVoltage() * 409.5f, 0.0, 4095.0f);

	main.io->ROTCV_ADC 			= (uint16_t)clamp(inputs[ROTATECV_INPUT].getVoltage() * 409.5f, 0.0, 4095.0f);
	main.io->FREQCV1_ADC 		= (uint16_t)clamp(inputs[FREQCV1_INPUT].getVoltage() * 409.5f, 0.0, 4095.0f);
	main.io->FREQCV6_ADC 		= (uint16_t)clamp(inputs[FREQCV6_INPUT].getVoltage() * 409.5f, 0.0, 4095.0f);

	main.io->SLEW_ADC			= (uint16_t)params[SLEW_PARAM].getValue();							
	main.io->SLEW_SWITCH 		= (SlewSetting)params[SLEWON_PARAM].getValue();

	main.io->FILTER_SWITCH 		= (FilterSetting)params[FILTER_PARAM].getValue();
	main.io->SCALEROT_SWITCH 	= (ScaleRotationSetting)params[SCALEROT_PARAM].getValue();
	main.io->PREPOST_SWITCH 	= (PrePostSetting)params[PREPOST_PARAM].getValue();
	main.io->ENV_SWITCH 		= (EnvelopeMode)params[ENV_PARAM].getValue();
	main.io->GLIDE_SWITCH		= (GlideSetting)params[VOCTGLIDE_PARAM].getValue();

	main.io->in 				= in;
	main.io->out 				= out;
	
	main.prepare();

	// Get input
	dsp::Frame<2> inputFrame = {};
	if (!inputBuffer.full()) {

		if (inputs[IN_ODD_INPUT].isConnected()) {
			inputFrame.samples[0] = inputs[IN_ODD_INPUT].getVoltage() * params[ODD_ATTN_PARAM].getValue() / 5.0;

			if (inputs[IN_EVEN_INPUT].isConnected()) {
				inputFrame.samples[1] = inputs[IN_EVEN_INPUT].getVoltage() * params[EVEN_ATTN_PARAM].getValue() / 5.0;
			} else {
				inputFrame.samples[1] = inputs[IN_ODD_INPUT].getVoltage() * params[ODD_ATTN_PARAM].getValue() / 5.0;
			}

		} else {
			float nO;
			switch (noiseSelected) {
				case 0:
					nO = brown.next() * 10.0f - 5.0f;
					break;
				case 1:
					nO = pink.next() * 10.0f - 5.0f;
					break;
				case 2:
					nO = white.next() * 10.0f - 5.0f;
					break;
				default:
					nO = pink.next() * 10.0f - 5.0f;
			}
			inputFrame.samples[0] = nO * params[ODD_ATTN_PARAM].getValue() / 5.0;

			if (inputs[IN_EVEN_INPUT].isConnected()) {
				inputFrame.samples[1] = inputs[IN_EVEN_INPUT].getVoltage() * params[EVEN_ATTN_PARAM].getValue() / 5.0;
			} else {
				float nE;
				switch (noiseSelected) {
					case 0:
						nE = brown.next() * 10.0f - 5.0f;
						break;
					case 1:
						nE = pink.next() * 10.0f - 5.0f;
						break;
					case 2:
						nE = white.next() * 10.0f - 5.0f;
						break;
					default:
						nE = pink.next() * 10.0f - 5.0f;
				}
				inputFrame.samples[1] = nE * params[EVEN_ATTN_PARAM].getValue() / 5.0;
			}

		}

		inputBuffer.push(inputFrame);
		
	}

	// Process buffer
	if (outputBuffer.empty()) {

		{
			inputSrc.setRates(args.sampleRate, 96000);
			dsp::Frame<2> inputFrames[NUM_SAMPLES];
			int inLen = inputBuffer.size();
			int outLen = NUM_SAMPLES;
			inputSrc.process(inputBuffer.startData(), &inLen, inputFrames, &outLen);
			inputBuffer.startIncr(inLen);

			for (int i = 0; i < NUM_SAMPLES; i++) {
				in[i * 2] 		= (int32_t)clamp(inputFrames[i].samples[0] * inMax, inMin, inMax);
				in[i * 2 + 1] 	= (int32_t)clamp(inputFrames[i].samples[1] * inMax, inMin, inMax);
			}
		}

		main.process_audio();

		// Convert output buffer
		{
			dsp::Frame<2> outputFrames[NUM_SAMPLES];
			for (int i = 0; i < NUM_SAMPLES; i++) {
				outputFrames[i].samples[0] = out[i * 2] / outMax;
				outputFrames[i].samples[1] = out[i * 2 + 1] / outMax;
			}

			outputSrc.setRates(96000, args.sampleRate);
			int inLen = NUM_SAMPLES;
			int outLen = outputBuffer.capacity();
			outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}
	}

	// Set output
	dsp::Frame<2> outputFrame = {};
	if (!outputBuffer.empty()) {
		outputFrame = outputBuffer.shift();

		if (outputs[OUT_RIGHT_OUTPUT].isConnected()) {
			outputs[OUT_LEFT_OUTPUT].setVoltage(outputFrame.samples[1] * 5.0f); // TODO THis seems to be reversed for some reason
			outputs[OUT_RIGHT_OUTPUT].setVoltage(outputFrame.samples[0] * 5.0f);
		} else {
			outputs[OUT_LEFT_OUTPUT].setVoltage((outputFrame.samples[0] + outputFrame.samples[1]) * 2.5);
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
		main.io->LOCK_ON[n] ? lights[LOCK_LIGHT + n].setBrightness(1.0) : lights[LOCK_LIGHT + n].setBrightness(0.0); 
		main.io->CHANNEL_Q_ON[n] ? lights[QLOCK_LIGHT + n].setBrightness(1.0) : lights[QLOCK_LIGHT + n].setBrightness(0.0); 
	}

	main.io->CLIP_ODD ? lights[CLIP_ODD_LIGHT].setBrightness(1.0) : lights[CLIP_ODD_LIGHT].setBrightness(0.0); 

	if (inputs[IN_EVEN_INPUT].isConnected()) { // Only process light if even input is connected
		main.io->CLIP_EVEN ? lights[CLIP_EVEN_LIGHT].setBrightness(1.0) : lights[CLIP_EVEN_LIGHT].setBrightness(0.0); 
	} else {
		lights[CLIP_EVEN_LIGHT].setBrightness(0.0); 
	}

	for (int i = 0; i < NUM_FILTS; i++) {
		if (main.io->FREQ_BLOCK[i]) {
			ringLEDs[i]->color 			= nvgRGBf(0, 0, 0);
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

	for (int i = 0; i < NUM_CHANNELS; i++) {
		envelopeLEDs[i]->color = nvgRGBf(
			main.io->envelope_leds[i][0], 
			main.io->envelope_leds[i][1],
			main.io->envelope_leds[i][2]);
		envelopeLEDs[i]->colorBorder = defaultBorder;		

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

		if (lightDivider.process()) {
			for (int i = 0; i < 6; i++) {
				float b = vuMeters[i].getBrightness(-6.f, 0.f) * 2.0f;
				outputLEDs[i]->color = nvgRGBf(b, 1 - b, 0);
				outputLEDs[i]->colorBorder = defaultBorder;
			}
		}

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

		nvgFontSize(ctx.vg, 17);
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

		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(116.911, 15.686)), module, Rainbow::LOCKON_PARAM+0));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(128.057, 15.686)), module, Rainbow::LOCKON_PARAM+1));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(139.202, 15.686)), module, Rainbow::LOCKON_PARAM+2));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(150.348, 15.686)), module, Rainbow::LOCKON_PARAM+3));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(161.494, 15.686)), module, Rainbow::LOCKON_PARAM+4));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(172.64, 15.686)), module, Rainbow::LOCKON_PARAM+5));
		addParam(createParam<gui::PrismSSwitch3>(mm2px(Vec(10.261, 9.144)), module, Rainbow::SLEWON_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(25.752, 16.594)), module, Rainbow::SLEW_PARAM));
		addParam(createParam<gui::PrismSSwitch3>(mm2px(Vec(49.765, 9.144)), module, Rainbow::NOISE_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(69.517, 9.144)), module, Rainbow::SCALEROT_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(229.051, 21.33)), module, Rainbow::ROTCCW_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(266.468, 21.33)), module, Rainbow::ROTCW_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(100.89, 35.8)), module, Rainbow::FREQNUDGE1_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(188.66, 35.8)), module, Rainbow::FREQNUDGE6_PARAM));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(113.461, 25.372)), module, Rainbow::CHANNEL_LEVEL_PARAM+0));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(124.607, 25.372)), module, Rainbow::CHANNEL_LEVEL_PARAM+1));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(135.752, 25.372)), module, Rainbow::CHANNEL_LEVEL_PARAM+2));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(146.898, 25.372)), module, Rainbow::CHANNEL_LEVEL_PARAM+3));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(158.044, 25.372)), module, Rainbow::CHANNEL_LEVEL_PARAM+4));
		addParam(createParam<gui::PrismLEDSlider>(mm2px(Vec(169.19, 25.372)), module, Rainbow::CHANNEL_LEVEL_PARAM+5));

		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(118.761, 25.372)), module, Rainbow::LEVEL_OUT_PARAM+0));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(129.907, 25.372)), module, Rainbow::LEVEL_OUT_PARAM+1));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(141.052, 25.372)), module, Rainbow::LEVEL_OUT_PARAM+2));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(152.198, 25.372)), module, Rainbow::LEVEL_OUT_PARAM+3));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(163.344, 25.372)), module, Rainbow::LEVEL_OUT_PARAM+4));
		addParam(createParam<gui::PrismLEDIndicator>(mm2px(Vec(174.49, 25.372)), module, Rainbow::LEVEL_OUT_PARAM+5));

		addParam(createParam<gui::PrismSSwitch3>(mm2px(Vec(10.261, 40.692)), module, Rainbow::ENV_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(30.013, 40.692)), module, Rainbow::PREPOST_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(49.765, 40.692)), module, Rainbow::VOCTGLIDE_PARAM));
		// addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(69.517, 40.692)), module, Rainbow::COMPRESS_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(229.051, 58.747)), module, Rainbow::SCALECCW_PARAM));
		addParam(createParamCentered<gui::PrismLargeButton>(mm2px(Vec(266.468, 58.747)), module, Rainbow::SCALECW_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(116.911, 68.355)), module, Rainbow::CHANNEL_Q_ON_PARAM+0));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(128.057, 68.355)), module, Rainbow::CHANNEL_Q_ON_PARAM+1));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(139.202, 68.355)), module, Rainbow::CHANNEL_Q_ON_PARAM+2));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(150.348, 68.355)), module, Rainbow::CHANNEL_Q_ON_PARAM+3));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(161.494, 68.355)), module, Rainbow::CHANNEL_Q_ON_PARAM+4));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(172.64, 68.355)), module, Rainbow::CHANNEL_Q_ON_PARAM+5));
		addParam(createParamCentered<gui::PrismTrimpotNoSnap>(mm2px(Vec(40.366, 80.3)), module, Rainbow::ODD_ATTN_PARAM));
		addParam(createParamCentered<gui::PrismTrimpotNoSnap>(mm2px(Vec(65.245, 80.3)), module, Rainbow::EVEN_ATTN_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(116.911, 82.174)), module, Rainbow::CHANNEL_Q_PARAM+0));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(128.057, 82.174)), module, Rainbow::CHANNEL_Q_PARAM+1));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(139.202, 82.174)), module, Rainbow::CHANNEL_Q_PARAM+2));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(150.348, 82.174)), module, Rainbow::CHANNEL_Q_PARAM+3));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(161.494, 82.174)), module, Rainbow::CHANNEL_Q_PARAM+4));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(172.64, 82.174)), module, Rainbow::CHANNEL_Q_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(269.073, 87.016)), module, Rainbow::BANK_PARAM));
		addParam(createParamCentered<gui::PrismButton>(mm2px(Vec(282.213, 87.016)), module, Rainbow::SWITCHBANK_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(98.44, 86.261)), module, Rainbow::MOD135_PARAM));
		addParam(createParam<gui::PrismSSwitch>(mm2px(Vec(186.21, 86.261)), module, Rainbow::MOD246_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(116.911, 98.611)), module, Rainbow::TRANS_PARAM+0));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(128.057, 98.611)), module, Rainbow::TRANS_PARAM+1));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(139.202, 98.611)), module, Rainbow::TRANS_PARAM+2));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(150.348, 98.611)), module, Rainbow::TRANS_PARAM+3));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(161.494, 98.611)), module, Rainbow::TRANS_PARAM+4));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(172.64, 98.611)), module, Rainbow::TRANS_PARAM+5));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(231.28, 106.866)), module, Rainbow::MORPH_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(263.144, 106.866)), module, Rainbow::SPREAD_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(100.89, 118.183)), module, Rainbow::GLOBAL_Q_PARAM));
		addParam(createParamCentered<gui::PrismKnobSnap>(mm2px(Vec(144.775, 118.183)), module, Rainbow::FILTER_PARAM));
		addParam(createParamCentered<gui::PrismKnobNoSnap>(mm2px(Vec(188.66, 118.183)), module, Rainbow::GLOBAL_LEVEL_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(247.76, 13.58)), module, Rainbow::ROTATECV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(221.301, 40.038)), module, Rainbow::ROTCCW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(274.218, 40.038)), module, Rainbow::ROTCW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(100.89, 53.655)), module, Rainbow::FREQCV1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(188.66, 53.655)), module, Rainbow::FREQCV6_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(247.76, 66.497)), module, Rainbow::SCALE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(100.89, 72.407)), module, Rainbow::LOCK135_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(188.66, 72.407)), module, Rainbow::LOCK246_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.899, 76.69)), module, Rainbow::IN_ODD_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(54.778, 76.69)), module, Rainbow::IN_EVEN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(220.022, 118.124)), module, Rainbow::MORPH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(274.402, 118.124)), module, Rainbow::SPREAD_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(86.52, 118.182)), module, Rainbow::GLOBAL_Q_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(202.983, 118.182)), module, Rainbow::GLOBAL_LEVEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(117.04, 118.183)), module, Rainbow::POLY_Q_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(172.511, 118.183)), module, Rainbow::POLY_LEVEL_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.899, 97.113)), module, Rainbow::OUT_LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(54.778, 97.321)), module, Rainbow::OUT_RIGHT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.899, 119.686)), module, Rainbow::POLY_ENV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(54.778, 119.686)), module, Rainbow::POLY_VOCT_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(116.911, 15.686)), module, Rainbow::LOCK_LIGHT+0));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(128.057, 15.686)), module, Rainbow::LOCK_LIGHT+1));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(139.202, 15.686)), module, Rainbow::LOCK_LIGHT+2));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(150.348, 15.686)), module, Rainbow::LOCK_LIGHT+3));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(161.494, 15.686)), module, Rainbow::LOCK_LIGHT+4));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(172.64, 15.686)), module, Rainbow::LOCK_LIGHT+5));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(116.911, 68.355)), module, Rainbow::QLOCK_LIGHT+0));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(128.057, 68.355)), module, Rainbow::QLOCK_LIGHT+1));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(139.202, 68.355)), module, Rainbow::QLOCK_LIGHT+2));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(150.348, 68.355)), module, Rainbow::QLOCK_LIGHT+3));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(161.494, 68.355)), module, Rainbow::QLOCK_LIGHT+4));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(172.64, 68.355)), module, Rainbow::QLOCK_LIGHT+5));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(40.366, 72.769)), module, Rainbow::CLIP_ODD_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(65.245, 72.769)), module, Rainbow::CLIP_EVEN_LIGHT));

		if(module) {

			BankWidget *bankW = new BankWidget();
			bankW->module = module;
			bankW->box.pos = mm2px(Vec(224.0, 44.4));
			bankW->box.size = Vec(80, 20);
			addChild(bankW);

			Vec levelStrip(mm2px(Vec(113.3, 21.5)));
			Vec channelStrip(mm2px(Vec(113.3, 54.0)));

			for (int i = 0; i < NUM_CHANNELS; i++) {
				module->outputLEDs[i] = new LED(i, levelStrip.x + i * 32.5 + 6, levelStrip.y);
				module->outputLEDs[i]->module = NULL;
				addChild(module->outputLEDs[i]);

				module->qLEDs[i] = new LED(i, channelStrip.x + i * 32.5, channelStrip.y + 11);
				module->qLEDs[i]->module = NULL;
				addChild(module->qLEDs[i]);

				module->envelopeLEDs[i] = new LED(i, channelStrip.x + i * 32.5 + 12, channelStrip.y + 11);
				module->envelopeLEDs[i]->module = NULL;
				addChild(module->envelopeLEDs[i]);

				module->tuningLEDs[i] = new LED(i, channelStrip.x + i * 32.5 + 6, channelStrip.y + 0);
				module->tuningLEDs[i]->module = NULL;
				addChild(module->tuningLEDs[i]);
			}
		}

		if (module) {
			Vec ringBox(mm2px(Vec(227.794, 22.6)));
			float ringDiv = (core::PI * 2) / NUM_FILTS;

			for (int i = 0; i < NUM_FILTS; i++) {
		
				float xPos  = sin(core::PI - ringDiv * i) * 50.0f;
				float yPos  = cos(core::PI - ringDiv * i) * 50.0f;

				module->ringLEDs[i] = new LED(i, ringBox.x + 50 + xPos, ringBox.y + 50 + yPos);
				module->ringLEDs[i]->module = module;
				addChild(module->ringLEDs[i]);
			}

			float scaleDiv = (core::PI * 2) / NUM_SCALES;

			for (int i = 0; i < NUM_SCALES; i++) {
		
				float xPos  = sin(core::PI - scaleDiv * i) * 30.0;
				float yPos  = cos(core::PI - scaleDiv * i) * 30.0f;

				module->scaleLEDs[i] = new LED(i, ringBox.x + 50 + xPos, ringBox.y + 50 + yPos);
				module->scaleLEDs[i]->module = NULL;
				addChild(module->scaleLEDs[i]);
			}
		}
	}
};

Model *modelRainbow = createModel<Rainbow, RainbowWidget>("Rainbow");
