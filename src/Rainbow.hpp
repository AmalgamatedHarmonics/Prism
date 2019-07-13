#pragma once

#include <bitset>
#include <cmath>
#include <iostream>
#include <inttypes.h>

//Number of components
#define NUM_FILTS 20
#define NUM_CHANNELS 6
#define NUM_SCALES 11
#define NUM_SCALEBANKS 20

// Audio buffer sizing
#define BUFFER_SIZE 8
#define NUM_SAMPLES 4

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

enum SlewSetting {
	SlewOff = 0,
	SlewMorph,
	SlewControl
};

enum ScaleRotationSetting {
	RotateOff = 0,
	RotateOn
};

enum VOctTrackSetting {
	VOctTrackOff = 0,
	VOctTrackOn
};

enum PrePostSetting {
	Pre = 0,
	Post
};

enum EnvelopeMode {
	Fast = 0,
	Slow,
	Trigger
};

enum GlideSetting {
	GlideOff = 0,
	GlideOn,
};

enum Compressor {
	CompressorOff = 0,
	CompressorOn = 0
};


// Utility functions
void *memset(void *s, int c, size_t n);

void *memcpy(void *dest, const void *src, size_t n);

uint32_t diff(uint32_t a, uint32_t b);

namespace rainbow {

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
struct Limiter;
struct State;

struct Envelope {

    Levels *    levels;
    IO *        io;

    float MIN_VOCT = -10.0f/3.0f;
    float MAX_VOCT = 4.75f;

    float envout_preload[NUM_CHANNELS];
    float envout_preload_voct[NUM_CHANNELS];

    // Private
    float stored_trigger_level[NUM_CHANNELS] = {0, 0, 0, 0, 0, 0};
	float envelope[NUM_CHANNELS];
   	uint32_t env_trigout[NUM_CHANNELS];
	uint32_t env_low_ctr[NUM_CHANNELS];

    uint32_t env_update_ctr = UINT32_MAX;
    uint32_t ENV_UPDATE_RATE = 50;

    PrePostSetting  env_prepost_mode;
    EnvOutModes     env_track_mode;
    float           envspeed_attack;
    float           envspeed_decay;

    void configure(Levels *_levels, IO *_io);

    void initialise(void);
    void update();
    float freqCoeftoVOct(uint32_t k, float v);

};

struct Filter {

    Rotation *      rotation;
    Envelope *      envelope;
    Q *             q;
    Tuning *        tuning;
    IO *            io;
    Levels *        levels;

    //Filters
    uint8_t note[NUM_CHANNELS];
    uint8_t scale[NUM_CHANNELS];
    uint8_t scale_bank[NUM_CHANNELS];

    int32_t	left_buffer[NUM_SAMPLES];
    int32_t	right_buffer[NUM_SAMPLES];
    int32_t	filtered_buffer[NUM_SAMPLES];
    int32_t	filtered_bufferR[NUM_SAMPLES];

    // filter coefficients
    float *c_hiq[NUM_CHANNELS];
    float *c_loq[NUM_CHANNELS];

    // filter buffer
    float buf[NUM_CHANNELS][NUM_SCALES][NUM_FILTS][3]; 

    // buffer for first filter of two-pass
    float buf_a[NUM_CHANNELS][NUM_SCALES][NUM_FILTS][3]; 

    float filter_out[NUM_FILTS][NUM_SAMPLES];

   	// Filter parameters
	float qval_b[NUM_CHANNELS]   = {0, 0, 0, 0, 0, 0};	
	float qval_a[NUM_CHANNELS]   = {0, 0, 0, 0, 0, 0};	
	float qc[NUM_CHANNELS]   	= {0, 0, 0, 0, 0, 0};

	uint8_t old_scale_bank[NUM_CHANNELS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    float CROSSFADE_POINT = 4095.0f * 2.0f / 3.0f;
    float CROSSFADE_WIDTH = 1800.0f;
    float CF_MIN = CROSSFADE_POINT - CROSSFADE_WIDTH / 2.0f;
    float CF_MAX = CROSSFADE_POINT + CROSSFADE_WIDTH / 2.0f;
    uint32_t SLIDER_CLIP_LEVEL = 0x50000000;
    int32_t INPUT_LED_CLIP_LEVEL = 0x58000000;

    FilterTypes filter_type = MAXQ;
    FilterModes filter_mode = TWOPASS;
    FilterTypes new_filter_type;

    Limiter *compressor;

    bool filter_type_changed = false;

    float user_scalebank[231];
    float DEFAULT_user_scalebank[21] = {
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

    void configure(Rotation *_rotation, Envelope *_envelope, Q *_q, Tuning *_tuning, IO *_io, Levels *_levels);

    void process_scale_bank(void);

    void process_bank_change(void);
    void filter_twopass(void);
    void filter_onepass(void);
    void filter_bpre(void);

    void change_filter_type(FilterTypes newtype);
    void process_audio_block(int32_t *src, int32_t *dst);
    void set_default_user_scalebank();

    void update_slider_leds(void);

    void audio_split(uint16_t sz, int32_t *src, int32_t *ldst, int32_t *rdst);
    void audio_merge(uint16_t sz, int32_t *lsrc, int32_t *rsrc, int32_t *dst);

};

struct IO {

    uint16_t MORPH_ADC;
    uint16_t QPOT_ADC;
    uint16_t QVAL_ADC;

    uint16_t SLIDER_ADC[6];
    uint16_t LEVEL_ADC[6];

    uint16_t FREQNUDGE1_ADC;
    uint16_t FREQNUDGE6_ADC;

    uint16_t SLEW_ADC;
    uint16_t SCALE_ADC;
    uint16_t SPREAD_ADC;
    uint16_t ROTCV_ADC;
    uint16_t FREQCV1_ADC;
    uint16_t FREQCV6_ADC;

    FilterSetting           FILTER_SWITCH;
    Mod135Setting           MOD135_SWITCH;
    Mod246Setting           MOD246_SWITCH;
    SlewSetting             SLEW_SWITCH;
    ScaleRotationSetting    SCALEROT_SWITCH;
    PrePostSetting          PREPOST_SWITCH;
    GlideSetting            GLIDE_SWITCH;
    EnvelopeMode            ENV_SWITCH;

    uint16_t            CHANNEL_Q_ADC[6];
    bool                CHANNEL_Q_ON[6];
    bool                LOCK_ON[6];
    int8_t              TRANS_DIAL[6];

    // CV Rotate
    bool ROTUP_TRIGGER;
    bool ROTDOWN_TRIGGER;

    // Button Rotate
    bool ROTUP_BUTTON;
    bool ROTDOWN_BUTTON;

    // Button scale
    bool SCALEUP_BUTTON;
    bool SCALEDOWN_BUTTON;

    // Bank select
    bool        CHANGED_BANK;
    uint8_t     NEW_BANK;

    //FREQ BLOCKS
    std::bitset<20> FREQ_BLOCK;

    // Audio
    int32_t     *in;

    // OUTPUTS
    float env_out[NUM_CHANNELS];
    float voct_out[NUM_CHANNELS];

    Compressor     COMPRESS;

    // LEDS
    bool    CLIP_ODD;
    bool    CLIP_EVEN;

   	float ring[20][3];
   	float scale[11][3];

    float envelope_leds[NUM_CHANNELS][3];
    float level_leds[NUM_CHANNELS][3];
    float tuning_out_leds[NUM_CHANNELS][3];

    float channelLevel[NUM_CHANNELS]; // 0.0 - 1+, 1 = Clipping

    bool    FORCE_RING_UPDATE = true; // TODO force update first time through

    int32_t     *out;

    float DEBUG[16];

};

struct LEDRing {

    Rotation *      rotation;
    Envelope *      envelope;
    IO *            io;
    Filter *        filter;
    Tuning *        tuning;
    Levels *        levels;

    float sqrt2over2 = sqrt(2.0f) / 2.0f;
    float sqrt2 = sqrt(2.0f);
    float maxNudge = 1.0f + 4095.0f / 55000.0f;

    // Counters
	uint8_t filter_flash_ctr = 0;
	uint32_t led_ring_update_ctr = UINT32_MAX; // Initialise to always fire on first pass 
    uint8_t flash_ctr = 0;
	uint8_t elacs_ctr[NUM_SCALES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    float channel_led_colors[6][3] = {
        {255.0f/255.0f,     100.0f/255.0f,  100.0f/255.0f}, // Red
        {255.0f/255.0f,     255.0f/255.0f,  100.0f/255.0f}, // Yellow
        {100.0f/255.0f,     255.0f/255.0f,  100.0f/255.0f}, // Green
        {100.0f/255.0f,     255.0f/255.0f,  255.0f/255.0f}, // Cyan      
        {100.0f/255.0f,     100.0f/255.0f,  255.0f/255.0f}, // Blue
        {255.0f/255.0f,     100.0f/255.0f,  255.0f/255.0f}, // Magenta
        };

    void configure(Rotation *_rotation, Envelope *_envelope, IO *_io, Filter *_filter, Tuning *_tuning, Levels *_levels);

    void display_filter_rotation();
    void display_scale();
    void update_led_ring();
    void calculate_envout_leds();

};

struct Inputs {

    Rotation *      rotation;
    Envelope *      envelope;
    IO *            io;
    Filter *        filter;
    Tuning *        tuning;
    Levels *        levels;

    float SCALECV_LPF = 0.99f;
    uint32_t SPREAD_ADC_HYSTERESIS = 75;

	SlewSetting lastSlewSetting = SlewOff;

   	uint16_t old_rotcv_adc = 0;
	int8_t rot_offset = 0;
	int8_t old_rot_offset = 0;

	int32_t t_scalecv = 0;
	int32_t t_old_scalecv = 0;
	float lpf_buf;

    void configure(Rotation *_rotation, Envelope *_envelope, IO *_io, Filter *_filter, Tuning *_tuning, Levels *_levels);

    void param_read_switches(void);
    int8_t read_spread(void);
    void process_rotateCV(void);
    void process_scaleCV(void);

};


struct LPF {

	//Value outputs:
	uint16_t			raw_val;
	float				lpf_val;
	int16_t				bracketed_val;

	//Settings (input)
	uint16_t			iir_lpf_size;	//size of iir average. 0 = disabled. if fir_lpf_size > 0, then iir average is disabled.
	uint16_t			fir_lpf_size; 	//size of moving average (number of samples to average). 0 = disabled.
	int16_t				bracket_size;	//size of bracket (ignore changes when old_val-bracket_size < new_val < old_val+bracket_size)
	AnalogPolarity	    polarity;		//AP_UNIPOLAR or AP_BIPOLAR

	//Filter window buffer and index
	int32_t 			fir_lpf[MAX_FIR_LPF_SIZE];
	uint32_t 			fir_lpf_i;

    void setup_fir_filter();
    void apply_fir_lpf();
    void apply_bracket();

};

struct Controller {
    
    Rotation *      rotation;
    Envelope *      envelope;
    LEDRing *       ring;
    Filter *        filter;
    IO *            io;
    Q *             q;
    Tuning *        tuning;
    Levels *        levels;
    Inputs *        input;  
    State *         state;

    Controller();  
    void set_default_param_values(void);
    void load_from_state(void);
    void populate_state(void);

    void initialise(void);
    void prepare(void);
    void process_audio(void);

};

struct Rotation {

    Filter *        filter;
    IO *            io;

    uint16_t rotate_to_next_scale;

    int8_t motion_fadeto_note[NUM_CHANNELS];
    int8_t motion_fadeto_scale[NUM_CHANNELS];

    int8_t motion_rotate;
    int8_t motion_spread_dest[NUM_CHANNELS];
    int8_t motion_spread_dir[NUM_CHANNELS];

    int8_t motion_notejump;
    int8_t motion_scale_dest[NUM_CHANNELS];
    int8_t motion_scalecv_overage[NUM_CHANNELS] = {0, 0, 0, 0, 0, 0};

    float motion_morphpos[NUM_CHANNELS] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    float f_morph = 0.0;

    int8_t spread = 0;	
    int8_t old_spread = 1;

    uint32_t rot_update_ctr = UINT32_MAX;
    uint32_t ROT_UPDATE_RATE = 50;

    uint8_t scale_bank_defaultscale[NUM_SCALEBANKS] = {4, 4, 6, 5, 9, 5, 5};

    void configure(Filter *_filter, IO *_io);

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

    IO *    io;

    //Q POT AND CV
    uint32_t    qval[NUM_CHANNELS];
    float       qval_goal[NUM_CHANNELS] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	float       prev_qval[NUM_CHANNELS] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    float       qcv_lpf;
	float       qpot_lpf;
	float       qlockpot_lpf[NUM_CHANNELS] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

	uint32_t q_update_ctr = UINT32_MAX; // Initialise to always fire on first pass 
   	uint32_t Q_UPDATE_RATE = 15; 

    uint32_t QPOT_MIN_CHANGE = 100;
    float QPOT_LPF = 0.95f;
    float QCV_LPF = 0.95f;

    void configure(IO *_io);
    void update(void);

};

struct Tuning {

    Filter *        filter;
    IO *            io;

    //FREQ NUDGE/LOCK JACKS
    float freq_nudge[NUM_CHANNELS] = {1.0, 1.0};
    float coarse_adj_led[NUM_CHANNELS];
    float coarse_adj[NUM_CHANNELS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    float freq_shift[NUM_CHANNELS];

    uint32_t tuning_update_ctr = UINT32_MAX;
    uint32_t TUNING_UPDATE_RATE = 50;

    float FREQNUDGE_LPF = 0.995f;

    uint16_t mod_mode_135;
    uint16_t mod_mode_246;

    float t_fo;
    float t_fe;						// buffers for freq nudge knob readouts 
    float f_nudge_odds = 1;
    float f_nudge_evens = 1;
    float f_shift_odds = 1;
    float f_shift_evens = 1;

    LPF freq_jack_conditioning[2];     //LPF and bracketing for freq jacks

    void configure(Filter * _filter, IO *_io);

    void initialise(void);
    void update(void);

};

struct Levels {

    IO *	io;

    //CHANNEL LEVELS/SLEW
    float channel_level[NUM_CHANNELS] = {0, 0, 0, 0, 0, 0};

    float CHANNEL_LEVEL_MIN_LPF = 0.75f;
    float channel_level_lpf = CHANNEL_LEVEL_MIN_LPF;

    // Private
	uint32_t level_update_ctr = UINT32_MAX; // Initialise to always fire on first pass
	uint32_t LEVEL_UPDATE_RATE = 50; 
    uint32_t SLIDER_CHANGE_MIN = 20;
    float SLIDER_LPF_MIN = 0.007f;
    float LEVEL_RATE = 50.0f;

	float prev_level[NUM_CHANNELS]  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	float level_goal[NUM_CHANNELS]  = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	float level_inc[NUM_CHANNELS]	= {0, 0, 0, 0, 0, 0};

    void configure(IO *_io);

    void update(void);

};

struct Limiter {

    float max_sampleval;
    float threshold_compiled;
    float threshold_percent;
    int32_t threshold_value;

    int32_t limit(int32_t val);
    void initialise(uint32_t max_sampleval_in, float threshold_percent_in);

};

struct State {

    bool initialised = false;

    uint8_t note[NUM_CHANNELS];
    uint8_t scale[NUM_CHANNELS];
    uint8_t scale_bank[NUM_CHANNELS];

	FilterTypes filter_type;
	FilterModes filter_mode;

};

}