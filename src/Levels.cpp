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

#include "Rainbow.hpp"

using namespace rainbow;

void Levels::configure(IO *_io) {
	io =	_io;
}

void Levels::update(void) {

	if (level_update_ctr++ > LEVEL_UPDATE_RATE) { 
		level_update_ctr = 0;

		for (int j = 0; j < NUM_CHANNELS; j++) {

			float level_lpf = io->LEVEL[j];
			if (level_lpf <= SLIDER_LPF_MIN) {
				level_lpf = 0.0f;
			}
			if (level_lpf > 1.0f) {
				level_lpf = 1.0f;
			}

			prev_level[j] = level_goal[j];

			level_goal[j] *= channel_level_lpf;
			level_goal[j] += (1.0f - channel_level_lpf) * level_lpf;

			level_inc[j] = (level_goal[j] - prev_level[j]) / LEVEL_RATE;
			channel_level[j] = prev_level[j];
		} 

	} else { // SMOOTH OUT DATA BETWEEN ADC READS
		for (int j = 0; j < NUM_CHANNELS; j++) {
			channel_level[j] += level_inc[j];
			io->OUTLEVEL[j] = channel_level[j]; 
		}
	}
}
