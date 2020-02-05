#pragma once

#include <cmath>
#include <iostream>
#include <vector>
#include <inttypes.h>

#include "plugin.hpp"

#include "dsp/noise.hpp"

// Audio buffer sizing
#define NUM_SAMPLES 32

#define TWELFTHROOTTWO 1.05946309436
#define ROOT 13.75
#define SLIDEREDITNOTE_LPF 0.980
#define CENTER_PLATEAU 80

enum FilterSetting {
	TwoPass = 0,
	OnePass
};

enum EnvelopeMode {
	Fast = 0,
	Slow,
	Trigger
};

uint32_t diff(uint32_t a, uint32_t b);

namespace droplet {

struct Audio;
struct Filter;
struct IO;

struct Audio {

	const float MIN_12BIT = -16777216.0f;
	const float MAX_12BIT = 16777215.0f;

	const int	MAX_SAMPLE_RATE	= 96000;
	int			SAMPLE_RATE		= 32000;
	float		SAMPLE_C		= (float)MAX_SAMPLE_RATE / (float)SAMPLE_RATE;

	int noiseSelected;
	int sampleRate;

	bogaudio::dsp::PinkNoiseGenerator pink;
	bogaudio::dsp::RedNoiseGenerator brown;
	bogaudio::dsp::WhiteNoiseGenerator white;

	dsp::SampleRateConverter<1> inputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> inputBuffer;

	dsp::SampleRateConverter<1> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> outputBuffer;

	void ChannelProcess(droplet::IO &io, rack::engine::Input &input, rack::engine::Output &output, droplet::Filter &filter);

};

struct Filter {

	IO *			io;

	// Q
	uint32_t	qval;
	float		qval_goal 	= 0.0f;
	float		prev_qval 	= 0.0f;
	float		global_lpf	= 0.0f;

	uint32_t 	q_update_ctr	= UINT32_MAX; // Initialise to always fire on first pass 
   	uint32_t	Q_UPDATE_RATE	= 50; 
	uint32_t	QPOT_MIN_CHANGE	= 100;
	float		Q_LPF			= 0.90f; // UPDATE - does this need to be some fraction of 0.95@96Khz

	// Envelope
	const float ENV_SCALE = 4.0e+7;

	float envout_preload;

	float stored_trigger_level = 0;
	float envelope;
	uint32_t env_trigout;
	uint32_t env_low_ctr;

	uint32_t env_update_ctr = UINT32_MAX;
	uint32_t ENV_UPDATE_RATE = 50;

	bool			env_prepost_mode; // false = pre
	EnvelopeMode	env_track_mode;
	float			envspeed_attack;
	float			envspeed_decay;

	// Filter
	FilterSetting 	filter_mode 	= TwoPass;
	float 			SAMPLE_RATE		= 32000.0f;
	float 			factor			= 10.0 * (SAMPLE_RATE / 96000.0);
	float 			cCoeff 			= 2.0 * 3.14159265358979323846 / SAMPLE_RATE; 

	void configure(IO *_io);

	float CROSSFADE_POINT 	= 4095.0f * 2.0f / 3.0f;
	float CROSSFADE_WIDTH 	= 1800.0f;
	float CROSSFADE_MIN 	= CROSSFADE_POINT - CROSSFADE_WIDTH / 2.0f;
	float CROSSFADE_MAX 	= CROSSFADE_POINT + CROSSFADE_WIDTH / 2.0f;

	// filter output
	float filter_out[NUM_SAMPLES];

	// filter buffer
	float buf[3]; 

	// buffer for first filter of two-pass
	float buf_a[3]; 

   	// Filter parameters
	float qval_b	= 0.0f;	
	float qval_a	= 0.0f; 	
	float qc		= 0.0f;

	void update_env();
	void update_q();

	void initialise();
	void reset();
	void filter();
	void onepass();
	void twopass();

};

struct IO {

	int16_t					Q_LEVEL;
	int16_t					Q_CONTROL;
	FilterSetting			FILTER_SWITCH;
	EnvelopeMode			ENV_SWITCH;
	float					FREQ;

	// Audio
	int32_t					in[NUM_SAMPLES] = {}; 
	int32_t					out[NUM_SAMPLES] = {}; 

	// OUTPUTS
	float					env_out;
 	float					DEBUG[16];

};

}