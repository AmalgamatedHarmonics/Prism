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

#include "Rainbow.hpp"

extern float exp_4096[4096];

using namespace rainbow;

void Inputs::configure(Rotation *_rotation, Envelope *_envelope, IO *_io, Filter *_filter, Tuning *_tuning, Levels *_levels) {
	rotation 	= _rotation;
	envelope 	= _envelope;
	io 			= _io;
	filter		= _filter;
	tuning		= _tuning;
	levels 		= _levels;
}

void Inputs::param_read_switches(void) {
	uint32_t lag_val;

    /*** Read Switches ***/
	envelope->env_prepost_mode = io->PREPOST_SWITCH;

	if (io->MOD246_SWITCH == Mod_6) {
		tuning->mod_mode_246 = 6;
	} else {
		tuning->mod_mode_246 = 246;
	}

	if (io->MOD135_SWITCH == Mod_1) {
		tuning->mod_mode_135 = 1;
	} else {
		tuning->mod_mode_135 = 135;
	}

	if (io->SCALEROT_SWITCH == RotateOn) {
		rotation->rotate_to_next_scale = 1;
	} else {
		rotation->rotate_to_next_scale = 0;
	}

	switch (io->FILTER_SWITCH) {
		case OnePass:
			filter->filter_mode = ONEPASS;
			filter->change_filter_type(MAXQ);
			break;
		case TwoPass:
			filter->filter_mode = TWOPASS;
			filter->change_filter_type(MAXQ);
			break;
		case Bpre:
			filter->filter_mode = ONEPASS;
			filter->change_filter_type(BPRE);
			break;
	}

	switch (io->ENV_SWITCH) {
		case Fast:
			envelope->env_track_mode = ENV_FAST;
			envelope->envspeed_attack = 0.9990;
			envelope->envspeed_decay  = 0.9991;
			break;
		case Slow:
			envelope->env_track_mode = ENV_SLOW;
			envelope->envspeed_attack = 0.9995;
			envelope->envspeed_decay  = 0.9999;
			break;
		case Trigger:
			envelope->env_track_mode = ENV_TRIG;
			envelope->envspeed_attack = 0.0;
			envelope->envspeed_decay  = 0.0;
			break;
	}

	if (io->SLEW_SWITCH != lastSlewSetting) { // switch changed
		lastSlewSetting = io->SLEW_SWITCH; // save switch value

		// CVLAG switch is flipped on, latch the current Morph adc value and use that to calculate LPF coefficients
		switch(io->SLEW_SWITCH) {
			case SlewMorph:
				lag_val = (io->MORPH_ADC / 2) + 137;
				if (lag_val > 4095) {
					lag_val = 4095;
				}
				levels->channel_level_lpf = 1.0 - exp_4096[lag_val];
				break;
			case SlewControl:
				lag_val = (io->SLEW_ADC / 2) + 137;
				if (lag_val > 4095) {
					lag_val = 4095;
				}
				levels->channel_level_lpf = 1.0 - exp_4096[lag_val];
				break;
			default:
				levels->channel_level_lpf = levels->CHANNEL_LEVEL_MIN_LPF;
		}
		if (io->SLEW_SWITCH == SlewMorph) {
			//Read from morph pot, and scale to 137..4095
			lag_val = (io->MORPH_ADC / 2) + 137;
			if (lag_val > 4095) {
                lag_val = 4095;
            }
			levels->channel_level_lpf = 1.0 - exp_4096[lag_val];
		} else {			
			levels->channel_level_lpf = levels->CHANNEL_LEVEL_MIN_LPF;
		}
	}
}

//Reads ADC, applies hysteresis correction and returns 1 if spread value has changed
int8_t Inputs::read_spread(void) {

	uint8_t  test_spread = 0;
    uint8_t  hys_spread  = 0;
	uint16_t temp_u16;

	// Hysteresis is used to ignore noisy ADC values.
	// We are checking if Spread ADC has changed values enough to warrant a change our "spread" variable.
	// Like a Schmitt trigger, we set a different threshold (temp_u16) depending on if the ADC value is rising or falling.

	test_spread = (io->SPREAD_ADC >> 8) + 1; //0-4095 to 1-16

	if (test_spread < rotation->spread) {
		if (io->SPREAD_ADC <= (4095 - SPREAD_ADC_HYSTERESIS)) {
			temp_u16 = io->SPREAD_ADC + SPREAD_ADC_HYSTERESIS;
        } else {
			temp_u16 = 4095;
        }

		hys_spread = (temp_u16 >> 8) + 1;
	} else if (test_spread > rotation->spread) {
		if (io->SPREAD_ADC > SPREAD_ADC_HYSTERESIS) {
			temp_u16 = io->SPREAD_ADC - SPREAD_ADC_HYSTERESIS;
        } else {
			temp_u16 = 0;
        }
		hys_spread = (temp_u16 >> 8) + 1;

	} else {
		hys_spread = 0xFF; //adc has not changed, do nothing
	}

	if (hys_spread == test_spread) {
		return(hys_spread); //spread has changed
	}
	else {
        return(-1); //spread has not changed
    }
}

void Inputs::process_rotateCV(void) {
	int32_t t = (int16_t)io->ROTCV_ADC - (int16_t)old_rotcv_adc;
	if (t < -100 || t > 100) {
		old_rotcv_adc = io->ROTCV_ADC;
		rot_offset    = io->ROTCV_ADC / 205; //0..19

		rotation->jump_rotate_with_cv(rot_offset - old_rot_offset);

		old_rot_offset = rot_offset;
	}
}

void Inputs::process_scaleCV(void) {
	// apply LPF to scale CV ADC readout	
	lpf_buf *= SCALECV_LPF;
	lpf_buf += (1 - SCALECV_LPF) * io->SCALE_ADC;

	// switch scales according to CV in
	t_scalecv = lpf_buf / 409; //0..10
	rotation->jump_scale_with_cv(t_scalecv - t_old_scalecv);
	t_old_scalecv = t_scalecv;
}
