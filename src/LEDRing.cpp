/*
 * led_ring.c - handles interfacing the RGB LED ring
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

#include "Rainbow.hpp"

using namespace rainbow;

void LEDRing::configure(IO *_io, Rotation *_rotation, Envelope *_envelope, FilterBank *_filterbank, Q *_q) {
	rotation	= _rotation;
	envelope	= _envelope;
	io			= _io;
	filterbank	= _filterbank;
	q			= _q;
}

void LEDRing::calculate_envout_leds() {

	for (int chan = 0; chan < NUM_CHANNELS; chan++) {

		float f = (io->voct_out[chan] - envelope->MIN_VOCT) / envelope->VOCT_RANGE;
		
		// Here we are going to use HSL
		// 0V = Green = 120
		// MIN_VOCT = Blue = 240
		// MAX_VOCT = Red = 0
		io->tuning_out_leds[chan][0] = f * hslRange;
		io->tuning_out_leds[chan][1] = 1.0f;
		io->tuning_out_leds[chan][2] = 0.5f;

		// Level leds
		float qval = q->qval_goal[chan] / 4095.0f;
		io->q_leds[chan][0] = channel_led_colors[chan][0] * qval;
		io->q_leds[chan][1] = channel_led_colors[chan][1] * qval;
		io->q_leds[chan][2] = channel_led_colors[chan][2] * qval;

		if (io->q_leds[chan][0] > 1.0f) {
			io->q_leds[chan][0] = 1.0f;
		}
		if (io->q_leds[chan][1] > 1.0f) {
			io->q_leds[chan][1] = 1.0f;
		}
		if (io->q_leds[chan][2] > 1.0f) {
			io->q_leds[chan][2] = 1.0f;
		}

		// Envelope
		io->envelope_leds[chan][0] = channel_led_colors[chan][0] * io->env_out[chan];
		io->envelope_leds[chan][1] = channel_led_colors[chan][1] * io->env_out[chan];
		io->envelope_leds[chan][2] = channel_led_colors[chan][2] * io->env_out[chan];

		if (io->envelope_leds[chan][0] > 1.0f) {
			io->envelope_leds[chan][0] = 1.0f;
		}
		if (io->envelope_leds[chan][1] > 1.0f) {
			io->envelope_leds[chan][1] = 1.0f;
		}
		if (io->envelope_leds[chan][2] > 1.0f) {
			io->envelope_leds[chan][2] = 1.0f;
		}
	}
}

void LEDRing::display_filter_rotation() {

	float inv_fade[NUM_CHANNELS];
	float fade[NUM_CHANNELS];

	for (int i = 0; i < NUM_FILTS; i++) {
		io->ring[i][0] = 0.0f;
		io->ring[i][1] = 0.0f;
		io->ring[i][2] = 0.0f;
	}

	// Set the brightness of each LED in the ring:
	// --if it's unlocked, then brightness corresponds to the slider+cv level. Keep a minimum of 5% so that it doesn't go totally off
	// --if it's locked, brightness flashes between 100% and 0%.
	// As we rotate morph between two LEDs in the ring:
	// --fade[chan] is the brightness of the end point LED
	// --inv_fade[chan] is the brightness of the start point LED

	if (filter_flash_ctr++ > 16) {
		filter_flash_ctr = 0;
	}

	for (int chan = 0; chan < NUM_CHANNELS; chan++) {
		if (!io->LOCK_ON[chan]) {
			inv_fade[chan] = (1.0f - rotation->motion_morphpos[chan]);
			fade[chan]	 = rotation->motion_morphpos[chan];
		} else {
			fade[chan] = 0.0f;
			if (filter_flash_ctr) {
				inv_fade[chan] = 1.0f;
			} else {
				inv_fade[chan] = 0.0f;
			}
		}
	}

	for (int i = 0; i < NUM_FILTS; i++) {
		for (int chan = 0; chan < NUM_CHANNELS; chan++) {
			int next_i = rotation->motion_fadeto_note[chan];

			if (filterbank->note[chan] == i) {
				// PROCESS REST OF LED RING
				if (inv_fade[chan] > 0.0f) {
					if (io->ring[i][0] + io->ring[i][1] + io->ring[i][2] == 0.0f) {
						io->ring[i][0] = channel_led_colors[chan][0] * inv_fade[chan];
						io->ring[i][1] = channel_led_colors[chan][1] * inv_fade[chan];
						io->ring[i][2] = channel_led_colors[chan][2] * inv_fade[chan];
					} else {
						io->ring[i][0] += channel_led_colors[chan][0] * inv_fade[chan];
						io->ring[i][1] += channel_led_colors[chan][1] * inv_fade[chan];
						io->ring[i][2] += channel_led_colors[chan][2] * inv_fade[chan];
					}

					if (io->ring[i][0] > 1.0f) {
						io->ring[i][0] = 1.0f;
					}
					if (io->ring[i][1] > 1.0f) {
						io->ring[i][1] = 1.0f;
					}
					if (io->ring[i][2] > 1.0f) {
						io->ring[i][2] = 1.0f;
					}
				}
				if (fade[chan] > 0.0f) {
					if (io->ring[next_i][0] + io->ring[next_i][1] + io->ring[next_i][2] == 0.0f) {
						io->ring[next_i][0] = channel_led_colors[chan][0] * fade[chan];
						io->ring[next_i][1] = channel_led_colors[chan][1] * fade[chan];
						io->ring[next_i][2] = channel_led_colors[chan][2] * fade[chan];
					} else {
						io->ring[next_i][0] += channel_led_colors[chan][0] * fade[chan];
						io->ring[next_i][1] += channel_led_colors[chan][1] * fade[chan];
						io->ring[next_i][2] += channel_led_colors[chan][2] * fade[chan];
					}

					if (io->ring[next_i][0] > 1.0f) {
						io->ring[next_i][0] = 1.0f;
					}
					if (io->ring[next_i][1] > 1.0f) {
						io->ring[next_i][1] = 1.0f;
					}
					if (io->ring[next_i][2] > 1.0f) {
						io->ring[next_i][2] = 1.0f;
					}
				}
				chan = 6; //break;
			}
		}
	}
}

void LEDRing::display_scale() {
	//There's probably a more efficient way of calculating this!
	uint8_t elacs[NUM_SCALES][NUM_CHANNELS];
	uint8_t elacs_num[NUM_SCALES];

	if (flash_ctr++ > 3) {
		flash_ctr = 0;
	}

	//Destination of fade:
	// --Blank out the reverse-hash scale table
	for (int i = 0; i < NUM_SCALES; i++) {
		elacs_num[i] = 0;
		elacs[i][0]  = 99;
		elacs[i][1]  = 99;
		elacs[i][2]  = 99;
		elacs[i][3]  = 99;
		elacs[i][4]  = 99;
		elacs[i][5]  = 99;
	}

	// --Each entry in elacs[][] equals the number of channels
	for (int i = 0; i < NUM_CHANNELS; i++) {
		elacs[rotation->motion_scale_dest[i]][elacs_num[rotation->motion_scale_dest[i]]] = i;
		elacs_num[rotation->motion_scale_dest[i]]++;
	}

	for (int i = 0; i < NUM_SCALES; i++) {

		if (flash_ctr == 0) {
			elacs_ctr[i]++;
			if (elacs_ctr[i] >= elacs_num[i]) {
				elacs_ctr[i] = 0;
			}
		}

		// --Blank out the channel if there are no entries
		if (elacs[i][0] == 99) {
			io->scale[i][0] = 0.05f;
			io->scale[i][1] = 0.05f;
			io->scale[i][2] = 0.05f;
		} else {
			io->scale[i][0] = channel_led_colors[ elacs[i][ elacs_ctr[i] ] ][0];
			io->scale[i][1] = channel_led_colors[ elacs[i][ elacs_ctr[i] ] ][1];
			io->scale[i][2] = channel_led_colors[ elacs[i][ elacs_ctr[i] ] ][2];
		}
	}
}

void LEDRing::update_led_ring() {

	if (io->UI_UPDATE || io->FORCE_RING_UPDATE) {

		io->FORCE_RING_UPDATE 	= false;

		display_scale();
		display_filter_rotation();
		calculate_envout_leds();

	}
}
