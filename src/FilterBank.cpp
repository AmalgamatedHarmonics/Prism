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

void FilterBank::configure(IO *_io, Rotation *_rotation, Envelope *_envelope, Q *_q, Tuning *_tuning, Levels *_levels) {
	rotation	= _rotation;
	envelope	= _envelope; 
	q			= _q;
	tuning		= _tuning;
	io			= _io;
	levels		= _levels;

	filter_out = new float *[NUM_FILTS]; // FIXME should be NUM_CHANNELS * 2
	for (int i = 0; i < NUM_FILTS; i++) {
	    filter_out[i] = new float[NUM_SAMPLES];
	}

}

FilterBank::~FilterBank() {
	for (int i = 0; i < NUM_FILTS; i++) {
	    delete[] filter_out[i];
	}

	delete[] filter_out;
}

void FilterBank::set_default_user_scalebank(void) {
	for (uint8_t j = 0; j < NUM_BANKNOTES; j++) {
		userscale_bank96[j] = scales.presets[NUM_SCALEBANKS - 1]->c_maxq96000[j];
		userscale_bank48[j] = scales.presets[NUM_SCALEBANKS - 1]->c_maxq48000[j];
	}
}

void FilterBank::process_bank_change(void) {
	if (io->CHANGED_BANK) {
		for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
			if (!io->LOCK_ON[i]) {
				scale_bank[i] = io->NEW_BANK; //Set all unlocked scale_banks to the same value
			}
		}
	}
}

void FilterBank::process_user_scale_change() {
	if (io->USERSCALE_CHANGED) {
		for (uint8_t i = 0; i < NUM_BANKNOTES; i++) {
			userscale_bank96[i] = io->USERSCALE96[i];
			userscale_bank48[i] = io->USERSCALE48[i];
 		}
	}
}

void FilterBank::change_filter(FilterTypes new_type, FilterModes new_mode) {

	if (filter_mode != new_mode) {
		filter_mode_changed = true;
		new_filter_mode = new_mode;
	}

	if (filter_type != new_type) {
		filter_type_changed = true;
		new_filter_type = new_type;
	}
}

void FilterBank::prepare_scale_bank(void) {
	// Determine the coef tables we're using for the active filters (Lo-Q and Hi-Q) for each channel
	// Also clear the buf[] history if we changed scales or banks, so we don't get artifacts
	// To-Do: move this somewhere else, so it runs on a timer

	if (filter_type_changed) {
		filter_type = new_filter_type;
	}

	if (filter_mode_changed) {
		filter_mode = new_filter_mode;
	}

	for (int i = 0; i < NUM_CHANNELS; i++) {

		if (scale_bank[i] >= NUM_SCALEBANKS) {
			scale_bank[i] = NUM_SCALEBANKS - 1;
		}

		if (scale[i] >= NUM_SCALES) {
			scale[i] = NUM_SCALES - 1;
		}

		if (scale_bank[i] != old_scale_bank[i] || io->READCOEFFS || io->USERSCALE_CHANGED ) {

			old_scale_bank[i] = scale_bank[i];

			if (filter_type == MAXQ) {

				maxq[i].reset(this);

				if (scale_bank[i] == NUM_SCALEBANKS - 1) {
					if (io->HICPUMODE) {
						c_hiq[i] = (float *)(userscale_bank96); 
					} else {
						c_hiq[i] = (float *)(userscale_bank48); 
					}
				} else {
					if (io->HICPUMODE) {
						c_hiq[i] = (float *)(scales.presets[scale_bank[i]]->c_maxq96000);
					} else {
						c_hiq[i] = (float *)(scales.presets[scale_bank[i]]->c_maxq48000);
					}
				}	

			} else {

				bpre[i].reset(this);

				if (io->HICPUMODE) {
					c_hiq[i] 		= (float *)(scales.presets[scale_bank[i]]->c_bpre9600080040);
					c_loq[i] 		= (float *)(scales.presets[scale_bank[i]]->c_bpre9600022);
					bpretuning[i]	= (float *)(scales.presets[scale_bank[i]]->c_maxq96000); // Filter tuning, no exact tracking
				} else {
					c_hiq[i] 		= (float *)(scales.presets[scale_bank[i]]->c_bpre4800080040);
					c_loq[i] 		= (float *)(scales.presets[scale_bank[i]]->c_bpre4800022);
					bpretuning[i]	= (float *)(scales.presets[scale_bank[i]]->c_maxq48000); // Filter tuning, no exact tracking
				}
			}
		}	// new scale bank or filter type changed
	}	// channels
}

void FilterBank::process_audio_block() {

	float f_blended;

	// Populate the filter coefficients
	prepare_scale_bank();

	// UPDATE QVAL
	q->update();

	// Reset filter_out
	for (uint8_t filter = 0; filter < NUM_FILTS; filter++) {
		for (uint8_t sample = 0; sample < NUM_SAMPLES; sample++) {
			filter_out[filter][sample] = 0.0f;
		}
	}

	for (uint8_t chan = 0; chan < NUM_CHANNELS; chan++) {
		if (filter_type == MAXQ) {
			maxq[chan].filter(this, chan, filter_out);
		} else {
			bpre[chan].filter(this, chan, filter_out);
		}
	}

	rotation->update_morph();

	// Since process_audio_block is called half as frequently in 48Khz mode as in 96Khz mode
	// We must call update_morph twice, instead of once, to compensate
	if (!io->HICPUMODE) { 
		rotation->update_morph();
	}

	// MORPHING
	for (int i = 0; i < NUM_SAMPLES; i++) {
		
		for (int j = 0; j < NUM_CHANNELS; j++) {
		
			if (rotation->motion_morphpos[j] == 0.0f) {
				f_blended = filter_out[j][i];
			} else {
				f_blended = (filter_out[j][i] * (1.0f - rotation->motion_morphpos[j])) + (filter_out[j + NUM_CHANNELS][i] * rotation->motion_morphpos[j]); // filter blending
			}

			io->out[j][i] = (f_blended * levels->channel_level[j]);

		}
	}

	for (int j = 0; j < NUM_CHANNELS; j++) {
		f_blended = (filter_out[j][0] * (1.0f - rotation->motion_morphpos[j])) + (filter_out[j + NUM_CHANNELS][0] * rotation->motion_morphpos[j]);

		io->channelLevel[j] = (f_blended * levels->channel_level[j]) / CLIP_LEVEL;
		
		if (f_blended > 0.0f) { // Envelope does not take into account channel level
			envelope->envout_preload[j] = f_blended;
		} else {
			envelope->envout_preload[j] = -1.0f * f_blended;
		}
	}

	// Completed pass, so reset flags
	filter_type_changed = false;
	filter_mode_changed = false;
	io->USERSCALE_CHANGED = false;
	io->READCOEFFS = false;
	
}

