/*
 * params.c - Parameters
 *
 * Author: Dan Green (danngreen1@gmail.com), Hugo Paris (hugoplho@gmail.com)
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

#include <math.h>

#include "Rainbow.hpp"

using namespace rainbow;

extern float exp_1voct[4096];

void Tuning::configure(Filter * _filter, IO *_io) {
    filter      = _filter;
	io			= _io;
}

void Tuning::update(void) {
	// FREQ SHIFT
	//With the Maxq filter, the Freq Nudge pot alone adjusts the "nudge", and the CV jack is 1V/oct shift
	//With the BpRe filter, the Freq Nudge pot plus CV jack adjusts the "nudge", and there is no 1V/oct shift

	if (tuning_update_ctr++ > TUNING_UPDATE_RATE) {
		tuning_update_ctr = 0;
	
		if (filter->filter_type == MAXQ) {
			// Read buffer knob and normalize input: 0-1
			t_fo = (float)(io->FREQNUDGE1_ADC);
			t_fe = (float)(io->FREQNUDGE6_ADC);

			// Freq shift odds
			// is odds cv input Low-passed
			freq_jack_conditioning[0].raw_val = io->FREQCV1_ADC;
			freq_jack_conditioning[0].apply_fir_lpf();
			freq_jack_conditioning[0].apply_bracket();

			// Convert to 1VOCT
			f_shift_odds = exp_1voct[(uint32_t)freq_jack_conditioning[0].bracketed_val];

			// Freq shift evens
			// is odds cv input Low-passed
			freq_jack_conditioning[1].raw_val = io->FREQCV6_ADC;
			freq_jack_conditioning[1].apply_fir_lpf();
			freq_jack_conditioning[1].apply_bracket();

			// Convert to 1VOCT
			f_shift_evens = exp_1voct[(uint32_t)freq_jack_conditioning[1].bracketed_val];

			// FREQ NUDGE 
			// SEMITONE FINE TUNE	
			f_nudge_odds  = 1 + t_fo / 55000.0; // goes beyond semitone 
			f_nudge_evens = 1 + t_fe / 55000.0; 

			// 12-SEMITONE COARSE TUNE
			for (int i = 0; i < NUM_CHANNELS; i++) {

				switch(io->TRANS_DIAL[i]) {
					case 0:
						coarse_adj[i] = 1.0;
						break;
					case -6:
						coarse_adj[i] = 1.0 / 1.41421356237; 
						break;
					case -5:
						coarse_adj[i] = 1.0 / 1.33483985417; 
						break;
					case -4: 
						coarse_adj[i] = 1.0 / 1.25992104989; 
						break;
					case -3:
						coarse_adj[i] = 1.0 / 1.189207115;   
						break;
					case -2:
						coarse_adj[i] = 1.0 / 1.12246204831; 
						break;
					case -1:
						coarse_adj[i] = 1.0 / 1.05946309436; 
						break;
					case 1:
						coarse_adj[i] = 1.05946309436;       
						break;
					case 2:
						coarse_adj[i] = 1.12246204831;       
						break;
					case 3:
						coarse_adj[i] = 1.189207115; 	    
						break;
					case 4:
						coarse_adj[i] = 1.25992104989; 	    
						break;
					case 5:
						coarse_adj[i] = 1.33483985417;	    
						break;
					case 6:
						coarse_adj[i] = 1.41421356237;	    
						break;
					default:
						coarse_adj[i] = 1.0;                 
						break;
				}
			}

			// LOCK SWITCHES
			// nudge and shift always enabled on 1 and 6
			// ... and enabled on 3,5,2 and 4 based on the lock toggles
			// ODDS
			// enable freq nudge and shift for "135 mode"

			if (!io->LOCK_ON[0]) {
				freq_nudge[0] = f_nudge_odds * coarse_adj[0];
			}
			freq_shift[0] = f_shift_odds; // apply freq CV in

			if (mod_mode_135 == 135) {
				if (!io->LOCK_ON[2]) {
					freq_nudge[2] = f_nudge_odds * coarse_adj[2];
				}
				freq_shift[2] = f_shift_odds;

				if (!io->LOCK_ON[4]) {
					freq_nudge[4] = f_nudge_odds * coarse_adj[4];
				}
				freq_shift[4] = f_shift_odds;
			} 
			// disable freq nudge and shift on channel 3 and 5 when in "1 mode"
			else { 
				if (!io->LOCK_ON[2]) {
					freq_nudge[2] = coarse_adj[2];
				}
				freq_shift[2] = 1.0;

				if (!io->LOCK_ON[4]) {
					freq_nudge[4] = coarse_adj[4];
				}
				freq_shift[4] = 1.0;
			}

		//EVENS
			if (!io->LOCK_ON[5]) {
				freq_nudge[5] = f_nudge_evens * coarse_adj[5];
			}
			freq_shift[5] = f_shift_evens; 

			if (mod_mode_246 == 246){
				if (!io->LOCK_ON[1]) {
					freq_nudge[1] = f_nudge_evens * coarse_adj[1];
				} 
				freq_shift[1] = f_shift_evens;

				if (!io->LOCK_ON[3]) {
					freq_nudge[3] = f_nudge_evens * coarse_adj[3];
				}
				freq_shift[3] = f_shift_evens;
			} 
			// disable freq nudge and shift on channel 2 and 4 when in "6 mode"
			else {
				if (!io->LOCK_ON[3]) {
					freq_nudge[3] = coarse_adj[3];
				}
				freq_shift[3] = 1.0;

				if (!io->LOCK_ON[1]) {
					freq_nudge[1] = coarse_adj[1];
				}
				freq_shift[1] = 1.0;
			}

		} else { // BPRE Filter

			t_fo = (float)(io->FREQNUDGE1_ADC + io->FREQCV1_ADC) / 4096.0;
			if (t_fo > 1.0) {
				t_fo = 1.0;
			}

			t_fe = (float)(io->FREQNUDGE6_ADC + io->FREQCV6_ADC) / 4096.0;
			if (t_fe>1.0) {
				t_fe=1.0;
			}

			f_shift_odds  = 1.0;
			f_shift_evens = 1.0;
			
			f_nudge_odds  *= FREQNUDGE_LPF;
			f_nudge_odds  += (1.0f - FREQNUDGE_LPF) * t_fo;

			f_nudge_evens *= FREQNUDGE_LPF;
			f_nudge_evens += (1.0f - FREQNUDGE_LPF) * t_fe;

			if (!io->LOCK_ON[0]) {
				freq_nudge[0] = f_nudge_odds;
			}
			freq_shift[0] = f_shift_odds;

			if (mod_mode_135 == 135){
				if (!io->LOCK_ON[2]) {
					freq_nudge[2] = f_nudge_odds;
				}
				freq_shift[2] = f_shift_odds;

				if (!io->LOCK_ON[4]) {
					freq_nudge[4] = f_nudge_odds;
				}
				freq_shift[4] = f_shift_odds;
			} else {
				if (!io->LOCK_ON[2]) {
					freq_nudge[2] = 0.0;
				}
				freq_shift[2] = 1.0;

				if (!io->LOCK_ON[4]) {
					freq_nudge[4] = 0.0;
				}
				freq_shift[4] = 1.0;
			}

			if (!io->LOCK_ON[5]) {
				freq_nudge[5] = f_nudge_evens;
			}
			freq_shift[5] = f_shift_evens;

			if (mod_mode_246 == 246){
				if (!io->LOCK_ON[1]) {
					freq_nudge[1] = f_nudge_evens;
				}
				freq_shift[1] = f_shift_evens;

				if (!io->LOCK_ON[3]) {
					freq_nudge[3] = f_nudge_evens;
				}
				freq_shift[3] = f_shift_evens;
			} else {
				if (!io->LOCK_ON[1]) {
					freq_nudge[1] = 0.0;
				}
				freq_shift[1] = 1.0;

				if (!io->LOCK_ON[3]) {
					freq_nudge[3] = 0.0;
				}
				freq_shift[3] = 1.0;
			}
		}	
	}
}

void Tuning::initialise(void) {
	freq_jack_conditioning[0].polarity 		= AP_UNIPOLAR;
	freq_jack_conditioning[0].fir_lpf_size 	= 40;
	freq_jack_conditioning[0].iir_lpf_size	= 0;
	freq_jack_conditioning[0].bracket_size 	= 2;

	freq_jack_conditioning[1].polarity 		= AP_UNIPOLAR;
	freq_jack_conditioning[1].fir_lpf_size 	= 40;
	freq_jack_conditioning[1].iir_lpf_size	= 0;
	freq_jack_conditioning[1].bracket_size 	= 2;

	freq_jack_conditioning[0].setup_fir_filter();
	freq_jack_conditioning[1].setup_fir_filter();
}
