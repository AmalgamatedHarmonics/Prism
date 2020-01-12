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

#include "Rainbow.hpp"
#include "scales/Scales.hpp"

using namespace rainbow;

extern float exp_4096[4096];
extern float log_4096[4096];
extern uint32_t twopass_calibration[3380];

// CALCULATE FILTER OUTPUTS
//filter_out[0-5] are the note[]/scale[]/scale_bank[] filters.
//filter_out[6-11] are the morph destination values
//filter_out[channel1-6][buffer_sample]
void Filter::filter_twopass(FilterBank *fb, float **filter_out) { 

	float filter_out_a[NUM_FILTS][NUM_SAMPLES];	// first filter out for two-pass
	float filter_out_b[NUM_FILTS][NUM_SAMPLES];	// second filter out for two-pass

	uint8_t filter_num;
	uint8_t channel_num;
	uint8_t scale_num;

	float c0, c1, c2;
	float c0_a, c2_a;

	float pos_in_cf;	// % of Qknob position within crossfade region
	float ratio_a;
	float ratio_b;		// two-pass filter crossfade ratios

	float destvoct[6];

	int32_t *ptmp_i32;

	fb->io->INPUT_CLIP = false;

	for (channel_num = 0; channel_num < NUM_CHANNELS; channel_num++) {
		filter_num = fb->note[channel_num];
		scale_num  = fb->scale[channel_num];

		qc[channel_num] = fb->q->qval[channel_num];

		// QVAL ADJUSTMENTS
		// first filter max Q at noon on Q knob
		qval_a[channel_num]	= qc[channel_num] * 2.0f;
		if (qval_a[channel_num] > 4095.0f) {
			qval_a[channel_num] = 4095.0f;
		}

		// limit q knob range on second filter
		if (qc[channel_num] < 3900.0f) {
			qval_b[channel_num] = 1000.0f;
		} else if (qc[channel_num] >= 3900.0f) {
			qval_b[channel_num] = 1000.0f + (qc[channel_num] - 3900.0f) * 15.0f;
		} // 1000 to 3925
		
		// Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
		if (fb->io->HICPUMODE) {
			c0_a = 1.0f - exp_4096[(uint32_t)(qval_a[channel_num] / 1.4f) + 200] / 10.0f; //exp[200...3125]
			c0   = 1.0f - exp_4096[(uint32_t)(qval_b[channel_num] / 1.4f) + 200] / 10.0f; //exp[200...3125]
		} else {
			c0_a = 1.0f - exp_4096[(uint32_t)(qval_a[channel_num] / 1.4f) + 200] / 5.0f; //exp[200...3125]
			c0   = 1.0f - exp_4096[(uint32_t)(qval_b[channel_num] / 1.4f) + 200] / 5.0f; //exp[200...3125]
		}

		// FREQ: c1 = 2 * pi * freq / samplerate
		c1 = *(fb->c_hiq[channel_num] + (scale_num * NUM_SCALENOTES) + filter_num);
		c1 *= fb->tuning->freq_nudge[channel_num] * fb->tuning->freq_shift[channel_num];

		if (fb->io->HICPUMODE) {
			if (c1 > 1.30899581f) {
				c1 = 1.30899581f; //hard limit at 20k
			}
		} else {
			if (c1 > 1.9f) {
				c1 = 1.9f; //hard limit at 20k
			}
		}

		// CROSSFADE between the two filters
		if (qc[channel_num] < CROSSFADE_MIN) {
			ratio_a = 1.0f;
		} else if (qc[channel_num] > CROSSFADE_MAX) {
			ratio_a = 0.0f;
		} else {
			pos_in_cf = (qc[channel_num] - CROSSFADE_MIN) / CROSSFADE_WIDTH;
			ratio_a   = 1.0f - pos_in_cf;
		}

		ratio_b = (1.0f - ratio_a);
		ratio_b *= 43801543.68f / twopass_calibration[(uint32_t)(qval_b[channel_num] - 900)]; 
		// FIXME: 43801543.68f gain could be directly printed into calibration vector
		
		// AMPLITUDE: Boost high freqs and boost low resonance
		c2_a  = (0.003f * c1) - (0.1f * c0_a) + 0.102f;
		c2	= (0.003f * c1) - (0.1f * c0)   + 0.102f;
		c2 *= ratio_b;

		ptmp_i32 = fb->io->in[channel_num];

		int j = channel_num;
		for (int i = 0; i < NUM_SAMPLES; i++) {

			if (*ptmp_i32 >= INPUT_LED_CLIP_LEVEL) {
				fb->io->INPUT_CLIP = true;
			} 

			// FIRST PASS (_a)
			buf_a[channel_num][scale_num][filter_num][2] = (c0_a * buf_a[channel_num][scale_num][filter_num][1] + c1 * buf_a[channel_num][scale_num][filter_num][0]) - c2_a * (*ptmp_i32++);
			buf_a[channel_num][scale_num][filter_num][0] = buf_a[channel_num][scale_num][filter_num][0] - (c1 * buf_a[channel_num][scale_num][filter_num][2]);

			buf_a[channel_num][scale_num][filter_num][1] = buf_a[channel_num][scale_num][filter_num][2];
			filter_out_a[j][i] =  buf_a[channel_num][scale_num][filter_num][1];

			// SECOND PASS (_b)
			buf[channel_num][scale_num][filter_num][2] = (c0 * buf[channel_num][scale_num][filter_num][1] + c1 * buf[channel_num][scale_num][filter_num][0]) - c2 * (filter_out_a[j][i]);
			buf[channel_num][scale_num][filter_num][0] = buf[channel_num][scale_num][filter_num][0] - (c1 * buf[channel_num][scale_num][filter_num][2]);

			buf[channel_num][scale_num][filter_num][1] = buf[channel_num][scale_num][filter_num][2];
			filter_out_b[j][i] = buf[channel_num][scale_num][filter_num][1];

			filter_out[j][i] = (ratio_a * filter_out_a[j][i]) - filter_out_b[j][i]; // output of filter two needs to be inverted to avoid phase cancellation
		
		}

		// Set VOCT output
		fb->envelope->envout_preload_voct[channel_num] = c1;

		// Calculate the morph destination filter:
		// Calcuate c1 and c2, which must be updated since the freq changed, and then calculate an entire filter for each channel that's morphing
		// (Although it makes for poor readability to duplicate the inner loop section above, we save critical CPU time to do it this way)
		if (fb->rotation->motion_morphpos[channel_num] > 0.0) {

			filter_num = fb->rotation->motion_fadeto_note[channel_num];
			scale_num  = fb->rotation->motion_fadeto_scale[channel_num];

			//FREQ: c1 = 2 * pi * freq / samplerate
			c1 = *(fb->c_hiq[channel_num] + (scale_num * NUM_SCALENOTES) + filter_num);
			c1 *= fb->tuning->freq_nudge[channel_num] * fb->tuning->freq_shift[channel_num];

			if (fb->io->HICPUMODE) {
				if (c1 > 1.30899581f) {
					c1 = 1.30899581f; //hard limit at 20k
				}
			} else {
				if (c1 > 1.9f) {
					c1 = 1.9f; //hard limit at 20k
				}
			}

			destvoct[channel_num] = c1;

			//AMPLITUDE: Boost high freqs and boost low resonance
			c2_a  = (0.003f * c1) - (0.1f * c0_a) + 0.102f;
			c2	= (0.003f * c1) - (0.1f * c0)   + 0.102f;
			c2 	*= ratio_b;

			ptmp_i32 = fb->io->in[channel_num];

			j = channel_num + 6;
			for (int i = 0; i < NUM_SAMPLES; i++) {
				// FIRST PASS (_a)
				buf_a[channel_num][scale_num][filter_num][2] = (c0_a * buf_a[channel_num][scale_num][filter_num][1] + c1 * buf_a[channel_num][scale_num][filter_num][0]) - c2_a * (*ptmp_i32++);
				buf_a[channel_num][scale_num][filter_num][0] = buf_a[channel_num][scale_num][filter_num][0] - (c1 * buf_a[channel_num][scale_num][filter_num][2]);

				buf_a[channel_num][scale_num][filter_num][1] = buf_a[channel_num][scale_num][filter_num][2];
				filter_out_a[j][i] =  buf_a[channel_num][scale_num][filter_num][1];

				// SECOND PASS (_b)
				buf[channel_num][scale_num][filter_num][2] = (c0 * buf[channel_num][scale_num][filter_num][1] + c1 * buf[channel_num][scale_num][filter_num][0]) - c2  * (filter_out_a[j][i]);
				buf[channel_num][scale_num][filter_num][0] = buf[channel_num][scale_num][filter_num][0] - (c1 * buf[channel_num][scale_num][filter_num][2]);

				buf[channel_num][scale_num][filter_num][1] = buf[channel_num][scale_num][filter_num][2];
				filter_out_b[j][i] = buf[channel_num][scale_num][filter_num][1];

				filter_out[j][i] = (ratio_a * filter_out_a[j][i]) - filter_out_b[j][i]; // output of filter two needs to be inverted to avoid phase cancellation
			}

			// VOCT output with glissando
			if (fb->io->GLIDE_SWITCH) {
				fb->envelope->envout_preload_voct[channel_num] = 
					(fb->envelope->envout_preload_voct[channel_num] * (1.0f - fb->rotation->motion_morphpos[channel_num])) + 
					(destvoct[channel_num] * fb->rotation->motion_morphpos[channel_num]);
			}
		}
	}
}

//Calculate filter_out[]
//filter_out[0-5] are the note[]/scale[]/scale_bank[] filters. filter_out[6-11] are the morph destination values
void Filter::filter_onepass(FilterBank *fb, float **filter_out) { 

	uint8_t filter_num;
	uint8_t channel_num;
	uint8_t scale_num;
	uint8_t nudge_filter_num;

	float c0, c1, c2;
	float tmp;
	float iir;

	float destvoct[6];

	fb->io->INPUT_CLIP = false;

	for (int j = 0; j < NUM_CHANNELS * 2; j++) {

		if (j < NUM_CHANNELS) {
			channel_num = j;
		} else {
			channel_num = j - NUM_CHANNELS;
		}

		if (j < NUM_CHANNELS || fb->rotation->motion_morphpos[channel_num] != 0) {

			// Set filter_num and scale_num to the Morph sources
			if (j < NUM_CHANNELS) {
				filter_num = fb->note[channel_num];
				scale_num  = fb->scale[channel_num];
			} else {
				// Set filter_num and scale_num to the Morph dests
				filter_num = fb->rotation->motion_fadeto_note[channel_num];
				scale_num  = fb->rotation->motion_fadeto_scale[channel_num];
			}

			nudge_filter_num = filter_num + 1;
			if (nudge_filter_num > NUM_FILTS) {
				nudge_filter_num = NUM_FILTS;
			}

			// Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
			if (fb->io->HICPUMODE) {
				c0 = 1.0f - exp_4096[(uint32_t)(fb->q->qval[channel_num] / 1.4f) + 200] / 10.0; //exp[200...3125]
			} else {
				c0 = 1.0f - exp_4096[(uint32_t)(fb->q->qval[channel_num] / 1.4f) + 200] / 5.0; //exp[200...3125]
			}

			// FREQ: c1 = 2 * pi * freq / samplerate
			c1 = *(fb->c_hiq[channel_num] + (scale_num * NUM_SCALENOTES) + filter_num);
			c1 *= fb->tuning->freq_nudge[channel_num] * fb->tuning->freq_shift[channel_num];

			if (fb->io->HICPUMODE) {
				if (c1 > 1.30899581f) {
					c1 = 1.30899581f; //hard limit at 20k
				}
			} else {
				if (c1 > 1.9f) {
					c1 = 1.9f; //hard limit at 20k
				}
			}

			// Set VOCT output
			if (j < NUM_CHANNELS) { // Starting v/oct for gliss calc comes from first pass
				fb->envelope->envout_preload_voct[channel_num] = c1;
			} else {
				destvoct[channel_num] = c1;
			}

			// AMPLITUDE: Boost high freqs and boost low resonance
			c2  = (0.003f * c1) - (0.1f * c0) + 0.102f;
			c2 *= ((4096.0f - fb->q->qval[channel_num]) / 1024.0f) + 1.04f;

			for (int i = 0; i < NUM_SAMPLES; i++) {

				tmp = fb->io->in[channel_num][i];

				if (tmp >= INPUT_LED_CLIP_LEVEL) {
					fb->io->INPUT_CLIP = true;
				}

				buf[channel_num][scale_num][filter_num][2] = (c0 * buf[channel_num][scale_num][filter_num][1] + c1 * buf[channel_num][scale_num][filter_num][0]) - c2 * tmp;
				iir = buf[channel_num][scale_num][filter_num][0] - (c1 * buf[channel_num][scale_num][filter_num][2]);
				buf[channel_num][scale_num][filter_num][0] = iir;

				buf[channel_num][scale_num][filter_num][1] = buf[channel_num][scale_num][filter_num][2];
				filter_out[j][i] = buf[channel_num][scale_num][filter_num][1];
			}
	
			// VOCT output with glissando
			// must run on second set of channels as target V/oct comes from second pass, then interp
			if (fb->io->GLIDE_SWITCH && (j >= NUM_CHANNELS)) { 
				fb->envelope->envout_preload_voct[channel_num] = 
					(fb->envelope->envout_preload_voct[channel_num] * (1.0f - fb->rotation->motion_morphpos[channel_num])) + 
					(destvoct[channel_num] * fb->rotation->motion_morphpos[channel_num]);
			}
		}
	}
}

//Calculate filter_out[]
//filter_out[0-5] are the note[]/scale[]/scale_bank[] filters. filter_out[6-11] are the morph destination values
void Filter::filter_bpre(FilterBank *fb, float **filter_out) { 

	uint8_t filter_num;
	uint8_t channel_num;
	uint8_t scale_num;
	uint8_t nudge_filter_num;

	float a0, a1, a2;
	float c0, c1, c2;
	float tmp;
	float iir;
	float fir;

	float var_q;
	float inv_var_q;
	float var_f;
	float inv_var_f;

	float destvoct[6];

	fb->io->INPUT_CLIP = false;

	for (int j = 0; j < NUM_CHANNELS * 2; j++) {

		if (j < NUM_CHANNELS) {
			channel_num = j;
		} else {
			channel_num = j - NUM_CHANNELS;
		}

		if (j < NUM_CHANNELS || fb->rotation->motion_morphpos[channel_num] != 0.0f) {

			// Set filter_num and scale_num to the Morph sources
			if (j < NUM_CHANNELS) {
				filter_num = fb->note[channel_num];
				scale_num  = fb->scale[channel_num];

			// Set filter_num and scale_num to the Morph dests
			} else {
				filter_num = fb->rotation->motion_fadeto_note[channel_num];
				scale_num  = fb->rotation->motion_fadeto_scale[channel_num];
			}

			//Q vector
			var_f = fb->tuning->freq_nudge[channel_num];
			if (var_f < 0.002f) {
				var_f = 0.0f;
			}
			if (var_f > 0.998f) {
				var_f = 1.0f;
			}
			inv_var_f = 1.0f - var_f;

			//Freq nudge vector
			nudge_filter_num = filter_num + 1;
			if (nudge_filter_num > NUM_FILTS) {
				nudge_filter_num = NUM_FILTS;
			}

			// Set VOCT output
			if (j < NUM_CHANNELS) {
				fb->envelope->envout_preload_voct[channel_num] = 
					*(fb->bpretuning[channel_num] + (scale_num * NUM_SCALENOTES) + filter_num);
			} else {
				destvoct[channel_num] = 
					*(fb->bpretuning[channel_num] + (scale_num * NUM_SCALENOTES) + filter_num);
			}

			a0 =* (fb->c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 0)*var_f + *(fb->c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 0)*inv_var_f;
			a1 =* (fb->c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 1)*var_f + *(fb->c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 1)*inv_var_f;
			a2 =* (fb->c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 2)*var_f + *(fb->c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 2)*inv_var_f;

			c0 =* (fb->c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 0)*var_f + *(fb->c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 0)*inv_var_f;
			c1 =* (fb->c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 1)*var_f + *(fb->c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 1)*inv_var_f;
			c2 =* (fb->c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 2)*var_f + *(fb->c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 2)*inv_var_f;

			//Q vector
			if (fb->q->qval[channel_num] > 4065) {
				var_q	 = 1.0f;
				inv_var_q = 0.0f;
			} else {
				var_q	 = log_4096[fb->q->qval[channel_num]];
				inv_var_q = 1.0f - var_q;
			}

			c0 = c0 * var_q + a0 * inv_var_q;
			c1 = c1 * var_q + a1 * inv_var_q;
			c2 = c2 * var_q + a2 * inv_var_q;

			for (int i = 0; i < NUM_SAMPLES; i++){

				tmp = buf[channel_num][scale_num][filter_num][0];
				buf[channel_num][scale_num][filter_num][0] = buf[channel_num][scale_num][filter_num][1];

				//Odd input (left) goes to odd filters (1/3/5)
				//Even input (right) goes to even filters (2/4/6)

				int32_t pTmp = fb->io->in[channel_num][i];

				if (pTmp >= INPUT_LED_CLIP_LEVEL) {
					fb->io->INPUT_CLIP = true;
				}

				iir = pTmp * c0;

				iir -= c1 * tmp;
				fir = -tmp;
				iir -= c2 * buf[channel_num][scale_num][filter_num][0];
				fir += iir;
				buf[channel_num][scale_num][filter_num][1] = iir;

				filter_out[j][i] = fir;

			}

			// VOCT output with glissando
			// must run on second set of channels as target V/oct comes from second pass, then interp
			if (fb->io->GLIDE_SWITCH && (j >= NUM_CHANNELS)) { 
				fb->envelope->envout_preload_voct[channel_num] = 
					(fb->envelope->envout_preload_voct[channel_num] * (1.0f - fb->rotation->motion_morphpos[channel_num])) + 
					(destvoct[channel_num] * fb->rotation->motion_morphpos[channel_num]);
			}
		}
	}
}

void Filter::reset_buffer(int i, bool twopass) {

	float *ff = (float *)buf[i];
	for (int j = 0; j < (NUM_SCALES * NUM_FILTS); j++) {
		*(ff+j)		= 0.0f;
		*(ff+j+1)	= 0.0f;
		*(ff+j+2)	= 0.0f;
	}

	if (twopass) {
		float *ffa = (float *)buf_a[i];
		for (int j = 0; j < (NUM_SCALES * NUM_FILTS); j++) {
			*(ffa+j)        = 0.0f;
			*(ffa+j+1)      = 0.0f;
			*(ffa+j+2)      = 0.0f;
		}
	}

}

