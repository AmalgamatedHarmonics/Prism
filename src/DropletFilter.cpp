/*
 * filter.c - DSP bandpass resonant filter
 *
 * Author: Dan Green (danngreen1@gmail.com), Hugo Paris (hugoplho@gmail.com)
 * Algorithm based on work by Max Matthews and Julius O. Smith III, "Methods for Synthesizing Very High Q Parametrically Well Behaved Two Pole Filters", as published here: https://ccrma.stanford.edu/~jos/smac03maxjos/smac03maxjos.pdf
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#include <iostream>

#include "Common.hpp"
#include "Droplet.hpp"

using namespace droplet;

extern float exp_4096[4096];
extern float log_4096[4096];
extern uint32_t twopass_calibration[3380];

void Filter::configure(IO *_io) {
	io			= _io;
}

void Filter::initialise() {
	io->env_out = 0.0f;
}

void Filter::reset() {

	float *ff = (float *)buf;
	*(ff)	= 0.0f;
	*(ff+1)	= 0.0f;
	*(ff+2)	= 0.0f;

	if (filter_mode == TwoPass) {
		float *ffa = (float *)buf_a;
			*(ffa)        = 0.0f;
			*(ffa+1)      = 0.0f;
			*(ffa+2)      = 0.0f;
	}

}

void Filter::update_q(void) {

 	if (q_update_ctr++ > Q_UPDATE_RATE) { 
		q_update_ctr = 0;

		float lpf = Q_LPF;

		//Check jack + LPF
		int32_t qg = io->Q_LEVEL + io->Q_CONTROL;
		if (qg < 0) {
			qg = 0;
		}
		if (qg > 4095) {
			qg = 4095;
		}
		
		global_lpf *= lpf;
		global_lpf += (1.0f - lpf) * qg;

		prev_qval = qval_goal;
		qval_goal = global_lpf;

 	}
 	
 	// SMOOTH OUT DATA BETWEEN ADC READS
	qval = (uint32_t)(prev_qval + (q_update_ctr * (qval_goal - prev_qval) / 51.0f)); // Q_UPDATE_RATE + 1

}

void Filter::update_env(void) {

	filter_mode = io->FILTER_SWITCH;

	switch (io->ENV_SWITCH) {
		case Fast:
			env_track_mode = Fast;
			envspeed_attack = 0.9990f;
			envspeed_decay  = 0.9991f;
			break;
		case Slow:
			env_track_mode = Slow;
			envspeed_attack = 0.9995f;
			envspeed_decay  = 0.9999f;
			break;
		case Trigger:
			env_track_mode = Trigger;
			envspeed_attack = 0.0f;
			envspeed_decay  = 0.0f;
			break;
	}

	if (env_update_ctr++ > ENV_UPDATE_RATE) {
		env_update_ctr = 0;

		if (env_track_mode == Slow || env_track_mode == Fast) {

			//Apply LPF
			if(envelope < envout_preload) {
				envelope *= envspeed_attack;
				envelope += (1.0f - envspeed_attack) * envout_preload;
			} else {
				envelope *= envspeed_decay;
				envelope += (1.0f - envspeed_decay) * envout_preload;
			}

			io->env_out = envelope / ENV_SCALE;

			if (io->env_out > 1.0f) {
				io->env_out = 1.0f;
			}

		} else { //trigger mode

			if (stored_trigger_level < 0.002f) { // There is a minimum trigger level
				envout_preload *= 0.5f;
			} else {
				envout_preload *= stored_trigger_level;
			}

			if (env_trigout) { // Have bee triggered so ignore the input signal
				env_trigout--;
			} else {
				if (((uint32_t)envout_preload) > 1000000) { 
					env_low_ctr = 0;
					env_trigout = 40; // about 40 clicks * 50 wait cycles = 1 every 2000 cycles or 22ms
					io->env_out = 1.0f;
				} else {
					if (++env_low_ctr >= 40) { 
						io->env_out = 0.0f;
					}
				}
			}
		}
	}
}


void Filter::filter() {

	float f_blended;

	// UPDATE QVAL
	update_q();
	update_env();

	// Reset filter_out
	for (uint8_t sample = 0; sample < NUM_SAMPLES; sample++) {
		filter_out[sample] = 0.0f;
	}

	if (filter_mode == TwoPass) {
		twopass();
	} else {
		onepass();
	}

	for (int i = 0; i < NUM_SAMPLES; i++) {
		io->out[i] = filter_out[i];
	}

	f_blended = filter_out[0];

	if (f_blended > 0.0f) { // Envelope does not take into account channel level
		envout_preload = f_blended;
	} else {
		envout_preload = -1.0f * f_blended;
	}

}

void Filter::onepass() {

	float c0, c1, c2;
	float iir;

	c0 = 1.0f - exp_4096[(uint32_t)(qval / 1.4f) + 200] / 10.0; //exp[200...3125]

	// FREQ: c1 = 2 * pi * freq / samplerate
	
	c1 = dsp::FREQ_C4 * cCoeff;
	if (c1 > 1.30899581f) {
		c1 = 1.30899581f; //hard limit at 20k
	}

	// AMPLITUDE: Boost high freqs and boost low resonance
	c2  = (0.003f * c1) - (0.1f * c0) + 0.102f;
	c2 *= ((4096.0f - qval) / 1024.0f) + 1.04f;

	for (int i = 0; i < NUM_SAMPLES; i++) {
		buf[2] = (c0 * buf[1] + c1 * buf[0]) - c2 * io->in[i];
		iir = buf[0] - (c1 * buf[2]);
		buf[0] = iir;

		buf[1] = buf[2];
		filter_out[i] = buf[1];
	}

}

void Filter::twopass() {

	float filter_out_a[NUM_SAMPLES];	// first filter out for two-pass
	float filter_out_b[NUM_SAMPLES];	// second filter out for two-pass

	float c0, c1, c2;
	float c0_a, c2_a;

	float pos_in_cf;	// % of Qknob position within crossfade region
	float ratio_a;
	float ratio_b;		// two-pass filter crossfade ratios

	qc = qval;

	// QVAL ADJUSTMENTS
	// first filter max Q at noon on Q knob
	qval_a = qc * 2.0f;
	if (qval_a > 4095.0f) {
		qval_a = 4095.0f;
	}

	// limit q knob range on second filter
	if (qc < 3900.0f) {
		qval_b = 1000.0f;
	} else if (qc >= 3900.0f) {
		qval_b = 1000.0f + (qc - 3900.0f) * 15.0f;
	} // 1000 to 3925
	
	// Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
	c0_a = 1.0f - exp_4096[(uint32_t)(qval_a / 1.4f) + 200] / 10.0f; //exp[200...3125]
	c0   = 1.0f - exp_4096[(uint32_t)(qval_b / 1.4f) + 200] / 10.0f; //exp[200...3125]

	// FREQ: c1 = 2 * pi * freq / samplerate
	c1 = (dsp::FREQ_C4 * 2.0f * prism::core::PI) / 96000;
	if (c1 > 1.30899581f) {
		c1 = 1.30899581f; //hard limit at 20k
	}

	// CROSSFADE between the two filters
	if (qc < CROSSFADE_MIN) {
		ratio_a = 1.0f;
	} else if (qc > CROSSFADE_MAX) {
		ratio_a = 0.0f;
	} else {
		pos_in_cf = (qc - CROSSFADE_MIN) / CROSSFADE_WIDTH;
		ratio_a   = 1.0f - pos_in_cf;
	}

	ratio_b = (1.0f - ratio_a);
	ratio_b *= 43801543.68f / twopass_calibration[(uint32_t)(qval_b - 900)]; 
	
	// AMPLITUDE: Boost high freqs and boost low resonance
	c2_a  = (0.003f * c1) - (0.1f * c0_a) + 0.102f;
	c2	= (0.003f * c1) - (0.1f * c0)   + 0.102f;
	c2 *= ratio_b;

	for (int sample_index = 0; sample_index < NUM_SAMPLES; sample_index++) {

		// FIRST PASS (_a)
		buf_a[2] = (c0_a * buf_a[1] + c1 * buf_a[0]) - c2_a * io->in[sample_index];
		buf_a[0] = buf_a[0] - (c1 * buf_a[2]);

		buf_a[1] = buf_a[2];
		filter_out_a[sample_index] =  buf_a[1];

		// SECOND PASS (_b)
		buf[2] = (c0 * buf[1] + c1 * buf[0]) - c2 * (filter_out_a[sample_index]);
		buf[0] = buf[0] - (c1 * buf[2]);

		buf[1] = buf[2];
		filter_out_b[sample_index] = buf[1];

		filter_out[sample_index] = (ratio_a * filter_out_a[sample_index]) - filter_out_b[sample_index]; // output of filter two needs to be inverted to avoid phase cancellation
	
	}

}

