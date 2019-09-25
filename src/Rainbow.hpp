#pragma once

#include <bitset>
#include <cmath>
#include <iostream>
#include <vector>
#include <inttypes.h>

#include "plugin.hpp"

#include "dsp/noise.hpp"
#include "scales/Scales.hpp"

//Number of components
#define NUM_FILTS 20
#define NUM_CHANNELS 6
#define NUM_SCALES 11
#define NUM_SCALEBANKS 20

// Number of notes to completely define a scale
#define NUM_SCALENOTES 21
#define NUM_BANKNOTES 231

// Audio buffer sizing
#define NUM_SAMPLES 32

// LPF
#define MAX_FIR_LPF_SIZE 40

// User scales - come back to this later
#define COEF_COEF (2.0 * 3.14159265358979323846 / 96000.0)
#define TWELFTHROOTTWO 1.05946309436
#define ROOT 13.75
#define SLIDEREDITNOTE_LPF 0.980
#define CENTER_PLATEAU 80

enum FilterModes {
	TWOPASS = 2,
	ONEPASS = 3
};

enum FilterTypes {
	BPRE,
	MAXQ
};

enum EnvOutModes {
	ENV_SLOW,
	ENV_FAST,
	ENV_TRIG
};

enum AnalogPolarity {
	AP_UNIPOLAR,
	AP_BIPOLAR
};

enum FilterSetting {
	TwoPass = 0,
	OnePass,
	Bpre
};

enum Mod135Setting {
	Mod_1 = 0,
	Mod_135
};

enum Mod246Setting {
	Mod_6 = 0,
	Mod_246
};

enum VOctTrackSetting {
	VOctTrackOff = 0,
	VOctTrackOn
};

enum EnvelopeMode {
	Fast = 0,
	Slow,
	Trigger
};

uint32_t diff(uint32_t a, uint32_t b);

struct RainbowScaleExpanderMessage {
	float maxq48[NUM_BANKNOTES];
	float maxq96[NUM_BANKNOTES];
	bool updated;
};

namespace rainbow {

struct Audio;
struct Envelope;
struct Filter;
struct IO;
struct Inputs;
struct LEDRing;
struct LPF;
struct Controller;
struct Rotation;
struct Q;
struct Tuning;
struct Levels;
struct State;

struct Audio {

	const float MIN_12BIT = -16777216.0f;
	const float MAX_12BIT = 16777215.0f;

	int inputChannels;
	int outputChannels;
	int noiseSelected;
	int sampleRate;
	int internalSampleRate = 48000;
	float outputScale = 2.0f;

	bogaudio::dsp::PinkNoiseGenerator pink;
	bogaudio::dsp::RedNoiseGenerator brown;
	bogaudio::dsp::WhiteNoiseGenerator white;

	dsp::SampleRateConverter<1> nInputSrc[6] = {};
	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> nInputBuffer[6] = {};
	dsp::Frame<1> nInputFrame[6] = {};
	dsp::Frame<1> nInputFrames[6][NUM_SAMPLES] = {};

	dsp::SampleRateConverter<6> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<6>, 256> outputBuffer;
	dsp::Frame<6> outputFrame = {};
	dsp::Frame<6> outputFrames[NUM_SAMPLES] = {};

	dsp::SampleRateConverter<1> outputSrc1;
	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> outputBuffer1;
	dsp::Frame<1> outputFrame1 = {};
	dsp::Frame<1> outputFrames1[NUM_SAMPLES] = {};

	dsp::SampleRateConverter<2> outputSrc2;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer2;
	dsp::Frame<2> outputFrame2 = {};
	dsp::Frame<2> outputFrames2[NUM_SAMPLES] = {};

	dsp::SampleRateConverter<6> outputSrc6;
	dsp::DoubleRingBuffer<dsp::Frame<6>, 256> outputBuffer6;
	dsp::Frame<6> outputFrame6 = {};
	dsp::Frame<6> outputFrames6[NUM_SAMPLES] = {};


   	float generateNoise();
	void ChannelProcess1(rainbow::Controller &main, rack::engine::Input &input, rack::engine::Output &output);
	void ChannelProcess2(rainbow::Controller &main, rack::engine::Input &input, rack::engine::Output &output);
	void ChannelProcess6(rainbow::Controller &main, rack::engine::Input &input, rack::engine::Output &output);

};

struct Envelope {

	Levels *	levels;
	IO *		io;

	const float MIN_VOCT = -10.0f/3.0f;
	const float MAX_VOCT = 4.75f;
	const float VOCT_RANGE = -MIN_VOCT + MAX_VOCT;
	const float ENV_SCALE = 4.0e+7;

	float envout_preload[NUM_CHANNELS];
	float envout_preload_voct[NUM_CHANNELS];

	// Private
	float stored_trigger_level[NUM_CHANNELS] = {0, 0, 0, 0, 0, 0};
	float envelope[NUM_CHANNELS];
	uint32_t env_trigout[NUM_CHANNELS];
	uint32_t env_low_ctr[NUM_CHANNELS];

	uint32_t env_update_ctr = UINT32_MAX;
	uint32_t ENV_UPDATE_RATE = 50;

	bool			env_prepost_mode; // false = pre
	EnvOutModes		env_track_mode;
	float			envspeed_attack;
	float			envspeed_decay;

	void configure(IO *_io, Levels *_levels);

	void initialise(void);
	void update();
	float freqCoeftoVOct(float k);

};

struct Filter {

	Rotation *		rotation;
	Envelope *		envelope;
	Q *				q;
	Tuning *		tuning;
	IO *			io;
	Levels *		levels;

	ScaleSet scales;

	//Filters
	uint8_t note[NUM_CHANNELS];
	uint8_t scale[NUM_CHANNELS];
	uint8_t scale_bank[NUM_CHANNELS];

	// filter coefficients
	float *c_hiq[NUM_CHANNELS];
	float *c_loq[NUM_CHANNELS];

	float *bpretuning[NUM_CHANNELS];

	// filter buffer
	float buf[NUM_CHANNELS][NUM_SCALES][NUM_FILTS][3]; 

	// buffer for first filter of two-pass
	float buf_a[NUM_CHANNELS][NUM_SCALES][NUM_FILTS][3]; 

	float filter_out[NUM_FILTS][NUM_SAMPLES];

   	// Filter parameters
	float qval_b[NUM_CHANNELS]   = {0, 0, 0, 0, 0, 0};	
	float qval_a[NUM_CHANNELS]   = {0, 0, 0, 0, 0, 0};	
	float qc[NUM_CHANNELS]   	 = {0, 0, 0, 0, 0, 0};

	uint8_t old_scale_bank[NUM_CHANNELS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	float CROSSFADE_POINT = 4095.0f * 2.0f / 3.0f;
	float CROSSFADE_WIDTH = 1800.0f;
	float CROSSFADE_MIN = CROSSFADE_POINT - CROSSFADE_WIDTH / 2.0f;
	float CROSSFADE_MAX = CROSSFADE_POINT + CROSSFADE_WIDTH / 2.0f;
	int32_t INPUT_LED_CLIP_LEVEL = 0xFFFFFF;
	uint32_t CLIP_LEVEL = 0x04C00000;

	FilterTypes filter_type = MAXQ;
	FilterModes filter_mode = TWOPASS;
	FilterTypes new_filter_type;

	bool filter_type_changed = false;

	float userscale_bank96[231];
	float userscale_bank48[231];

	void configure(IO *_io, Rotation *_rotation, Envelope *_envelope, Q *_q, Tuning *_tuning, Levels *_levels);

	void process_scale_bank(void);

	void process_bank_change(void);
	void process_user_scale_change(void);

	void filter_twopass();
	void filter_onepass();
	void filter_bpre();

	void change_filter_type(FilterTypes newtype);
	void process_audio_block();
	void set_default_user_scalebank();

	void update_slider_leds(void);

};

struct IO {

	bool					UI_UPDATE;
	bool					HICPUMODE;
	bool					READCOEFFS = true;

	uint16_t				MORPH_ADC;

	int16_t					GLOBAL_Q_LEVEL;
	int16_t					GLOBAL_Q_CONTROL;
	int16_t					CHANNEL_Q_LEVEL[NUM_CHANNELS];
	int16_t					CHANNEL_Q_CONTROL[NUM_CHANNELS];

	float					GLOBAL_LEVEL_ADC;
	float					GLOBAL_LEVEL_CV;
	float					LEVEL_ADC[NUM_CHANNELS];
	float					LEVEL_CV[NUM_CHANNELS];

	int16_t					FREQNUDGE1_ADC;
	int16_t					FREQNUDGE6_ADC;

	uint16_t				SLEW_ADC;
	uint16_t				SCALE_ADC;
	uint16_t				SPREAD_ADC;
	uint16_t				ROTCV_ADC;

	float					FREQCV1_CV[3];
	int						FREQCV1_CHAN;

	float					FREQCV6_CV[3];
	int						FREQCV6_CHAN;

	FilterSetting			FILTER_SWITCH;
	Mod135Setting			MOD135_SWITCH;
	Mod246Setting			MOD246_SWITCH;
	bool					SCALEROT_SWITCH;
	bool					PREPOST_SWITCH;
	bool					GLIDE_SWITCH;
	EnvelopeMode			ENV_SWITCH;

	bool					CHANNEL_Q_ON[NUM_CHANNELS];
	bool					LOCK_ON[NUM_CHANNELS];
	int8_t					TRANS_DIAL[NUM_CHANNELS];

	// CV Rotate
	bool					ROTUP_TRIGGER;
	bool					ROTDOWN_TRIGGER;

	// Button Rotate
	bool					ROTUP_BUTTON;
	bool					ROTDOWN_BUTTON;

	// Button scale
	bool					SCALEUP_BUTTON;
	bool					SCALEDOWN_BUTTON;

	// Bank select
	bool					CHANGED_BANK;
	uint8_t					NEW_BANK;
	float					USERSCALE96[NUM_BANKNOTES];
	float					USERSCALE48[NUM_BANKNOTES];
	bool					USERSCALE_CHANGED = false;

	//FREQ BLOCKS
	std::bitset<20>			FREQ_BLOCK;

	// Audio
	int32_t					in[NUM_CHANNELS][NUM_SAMPLES] = {}; 
	int32_t					out[NUM_CHANNELS][NUM_SAMPLES] = {}; 

	// OUTPUTS
	float					env_out[NUM_CHANNELS];
	float					voct_out[NUM_CHANNELS];
	float					OUTLEVEL[NUM_SCALES];

	// LEDS
	bool					INPUT_CLIP;
	
	float					ring[NUM_FILTS][3];
	float					scale[NUM_SCALES][3];

	float					envelope_leds[NUM_CHANNELS][3];
	float					q_leds[NUM_CHANNELS][3];
	float					tuning_out_leds[NUM_CHANNELS][3];

	float					channelLevel[NUM_CHANNELS]; // 0.0 - 1+, 1 = Clipping

	bool					FORCE_RING_UPDATE = true;
 
	float					DEBUG[16];

};

struct LEDRing {

	Rotation *		rotation;
	Envelope *		envelope;
	IO *			io;
	Filter *		filter;
	Q *				q;


	const float sqrt2over2			= sqrt(2.0f) / 2.0f;
	const float sqrt2				= sqrt(2.0f);
	const float maxNudge			= 1.0f + 4095.0f / 55000.0f;
	const float hslRange 			= 2.0f/3.0f;

	// Counters
	uint8_t filter_flash_ctr		= 0;
	uint32_t led_ring_update_ctr	= UINT32_MAX; // Initialise to always fire on first pass 
	uint8_t flash_ctr				= 0;
	uint8_t elacs_ctr[NUM_SCALES]	= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	
	float channel_led_colors[NUM_CHANNELS][3] = {
		{255.0f/255.0f,	 100.0f/255.0f,  100.0f/255.0f}, // Red
		{255.0f/255.0f,	 255.0f/255.0f,  100.0f/255.0f}, // Yellow
		{100.0f/255.0f,	 255.0f/255.0f,  100.0f/255.0f}, // Green
		{100.0f/255.0f,	 255.0f/255.0f,  255.0f/255.0f}, // Cyan	  
		{100.0f/255.0f,	 100.0f/255.0f,  255.0f/255.0f}, // Blue
		{255.0f/255.0f,	 100.0f/255.0f,  255.0f/255.0f}, // Magenta
		};

	void configure(IO *_io, Rotation *_rotation, Envelope *_envelope, Filter *_filter, Q *_q);

	void display_filter_rotation();
	void display_scale();
	void update_led_ring();
	void calculate_envout_leds();

};

struct Inputs {

	Rotation *		rotation;
	Envelope *		envelope;
	IO *			io;
	Filter *		filter;
	Tuning *		tuning;
	Levels *		levels;

	float SCALECV_LPF				= 0.99f;
	uint32_t SPREAD_ADC_HYSTERESIS	= 75;

   	uint16_t old_rotcv_adc			= 0;
	int8_t rot_offset				= 0;
	int8_t old_rot_offset			= 0;

	int32_t t_scalecv				= 0;
	int32_t t_old_scalecv			= 0;
	float lpf_buf;

	FilterSetting oldFilter;

	void configure(IO *_io, Rotation *_rotation, Envelope *_envelope, Filter *_filter, Tuning *_tuning, Levels *_levels);

	void param_read_switches(void);
	int8_t read_spread(void);
	void process_rotateCV(void);
	void process_scaleCV(void);

};


struct LPF {

	//Value outputs:
	float				raw_val;
	float				lpf_val;
	float				bracketed_val;

	//Settings (input)
	uint16_t			iir_lpf_size;	//size of iir average. 0 = disabled. if fir_lpf_size > 0, then iir average is disabled.
	uint16_t			fir_lpf_size;	//size of moving average (number of samples to average). 0 = disabled.
	float				bracket_size;	//size of bracket (ignore changes when old_val-bracket_size < new_val < old_val+bracket_size)
	AnalogPolarity		polarity;		//AP_UNIPOLAR or AP_BIPOLAR

	//Filter window buffer and index
	float	 			fir_lpf[MAX_FIR_LPF_SIZE];
	uint32_t 			fir_lpf_i;

	void setup_fir_filter();
	void apply_fir_lpf();
	void apply_bracket();

};

struct Controller {
	
	Rotation *		rotation;
	Envelope *		envelope;
	LEDRing *		ring;
	Filter *		filter;
	IO *			io;
	Q *				q;
	Tuning *		tuning;
	Levels *		levels;
	Inputs *		input;  
	State *			state;

	Controller();  
	void set_default_param_values(void);
	void load_from_state(void);
	void populate_state(void);

	void initialise(void);
	void prepare(void);
	void process_audio(void);

};

struct Rotation {

	Filter *		filter;
	IO *			io;

	uint16_t rotate_to_next_scale;

	int8_t motion_fadeto_note[NUM_CHANNELS];
	int8_t motion_fadeto_scale[NUM_CHANNELS];

	int32_t motion_rotate;
	int8_t motion_spread_dest[NUM_CHANNELS];
	int8_t motion_spread_dir[NUM_CHANNELS];

	int8_t motion_notejump;
	int8_t motion_scale_dest[NUM_CHANNELS];
	int8_t motion_scalecv_overage[NUM_CHANNELS]		= {0, 0, 0, 0, 0, 0};

	float motion_morphpos[NUM_CHANNELS]				= {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

	float f_morph									= 0.0;

	int8_t spread									= 0;	
	int8_t old_spread								= 1;

	uint32_t rot_update_ctr							= UINT32_MAX;
	uint32_t ROT_UPDATE_RATE						= 50;

	uint8_t scale_bank_defaultscale[NUM_SCALEBANKS]	= {4, 4, 6, 5, 9, 5, 5};

	void configure(IO *_io, Filter *_filter);

	void update_spread(int8_t t_spread);
	void update_morph(void);
	void update_motion(void);

	void rotate_down(void);
	void rotate_up(void);

	void change_scale_up(void);
	void change_scale_down(void);
	void jump_scale_with_cv(int8_t shift_amt);

	bool is_morphing(void);
	bool is_spreading(void);

	void jump_rotate_with_cv(int8_t shift_amt);

};

struct Q {

	IO *	io;

	//Q POT AND CV
	uint32_t	qval[NUM_CHANNELS];
	float		qval_goal[NUM_CHANNELS]		= {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	float		prev_qval[NUM_CHANNELS]		= {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	
	float		global_lpf;
	float		qlockpot_lpf[NUM_CHANNELS]	= {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

	uint32_t q_update_ctr					= UINT32_MAX; // Initialise to always fire on first pass 
   	uint32_t Q_UPDATE_RATE					= 50; 

	uint32_t QPOT_MIN_CHANGE				= 100;
	float Q_LPF_96							= 0.95f;
	float Q_LPF_48							= 0.90f;

	void configure(IO *_io);
	void update(void);

};

struct Tuning {

	Filter *		filter;
	IO *			io;

	//FREQ NUDGE/LOCK JACKS
	float freq_nudge[NUM_CHANNELS]	= {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
	float coarse_adj_led[NUM_CHANNELS];
	float coarse_adj[NUM_CHANNELS]	= {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
	float freq_shift[NUM_CHANNELS];

	float twelveroottwo[25];

	uint32_t tuning_update_ctr		= UINT32_MAX;
	uint32_t TUNING_UPDATE_RATE		= 50;

	float FREQNUDGE_LPF				= 0.995f;

	uint16_t mod_mode_135;
	uint16_t mod_mode_246;

	float t_fo;
	float t_fe; // buffers for freq nudge knob readouts 
	float f_nudge_odds 				= 1;
	float f_nudge_evens 			= 1;

	LPF freq_jack_conditioning[2];	//LPF and bracketing for freq jacks

	void configure(IO *_io, Filter * _filter);

	void initialise(void);
	void update(void);

};

struct Levels {

	IO *	io;

	//CHANNEL LEVELS/SLEW
	float channel_level[NUM_CHANNELS] = {0, 0, 0, 0, 0, 0};

	float CHANNEL_LEVEL_MIN_LPF			= 0.75f;
	float channel_level_lpf				= CHANNEL_LEVEL_MIN_LPF;

	float global_cv_lpf;
	float level_cv_lpf[6];

	// Private
	uint32_t level_update_ctr			= UINT32_MAX; // Initialise to always fire on first pass
	uint32_t LEVEL_UPDATE_RATE			= 50; 
	uint32_t SLIDER_CHANGE_MIN			= 20;
	float SLIDER_LPF_MIN				= 0.007f;
	float LEVEL_RATE					= 50.0f;

	float prev_level[NUM_CHANNELS]		= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	float level_goal[NUM_CHANNELS]		= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	float level_inc[NUM_CHANNELS]		= {0, 0, 0, 0, 0, 0};

	void configure(IO *_io);

	void update(void);

};

struct State {

	bool initialised = false;

	uint8_t note[NUM_CHANNELS];
	uint8_t scale[NUM_CHANNELS];
	uint8_t scale_bank[NUM_CHANNELS];
	float userscale96[NUM_BANKNOTES];
	float userscale48[NUM_BANKNOTES];

	FilterTypes filter_type;
	FilterModes filter_mode;

};

}