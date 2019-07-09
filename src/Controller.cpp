/*
 * main.c - Spectral Mulitband Resonator v1.0
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

Controller::Controller(void) {

    rotation    = new Rotation();
    envelope    = new Envelope();
    ring        = new LEDRing();
    filter      = new Filter();
    io          = new IO();
	q			= new Q();
	tuning		= new Tuning();
	levels		= new Levels();	
	input		= new Inputs();
	state		= new State();

    rotation->configure(filter, io);
    envelope->configure(levels, io);
    ring->configure(rotation, envelope, io, filter, tuning, levels);
    filter->configure(rotation, envelope, q, tuning, io, levels);
	q->configure(io);
	tuning->configure(filter, io);
	levels->configure(io);
	input->configure(rotation, envelope, io, filter, tuning, levels);

}

void Controller::initialise(void) {

	set_default_param_values();
	filter->set_default_user_scalebank();

	rotation->spread = (io->SPREAD_ADC >> 8) + 1;
	rotation->update_spread(1);

	tuning->initialise();

	envelope->initialise();

} 

void Controller::prepare(void) {

    input->param_read_switches();

    tuning->update();

    ring->update_led_ring();
	
    // was in IRQ
    rotation->update_motion();

	envelope->update();

    int32_t t_spread = input->read_spread();
    if (t_spread != -1) {
        rotation->update_spread(t_spread);
    }

    filter->process_bank_change();

    if (io->ROTUP_TRIGGER || io->ROTUP_BUTTON) {
        rotation->rotate_up();
    }

    if (io->ROTDOWN_TRIGGER || io->ROTDOWN_BUTTON) {
        rotation->rotate_down();
    }

    if (io->SCALEUP_BUTTON) {
		rotation->change_scale_up();
    }

    if (io->SCALEDOWN_BUTTON) {
		rotation->change_scale_down();
    }

    input->process_rotateCV();
    input->process_scaleCV();

	levels->update();

	populate_state();

}

void Controller::process_audio(void) {

	filter->process_audio_block(io->in, io->out);

}

void Controller::set_default_param_values(void) {
	
	//Set default parameter values
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		filter->note[i]  					= i + 8;
		filter->scale[i] 					= 0;
		rotation->motion_fadeto_scale[i] 	= filter->scale[i];
		rotation->motion_scale_dest[i]   	= filter->scale[i];
		filter->scale_bank[i] 				= 0;
		rotation->motion_spread_dir[i]  	= 0;
		rotation->motion_spread_dest[i] 	= filter->note[i];
		rotation->motion_fadeto_note[i] 	= filter->note[i];

		rotation->motion_morphpos[i]        = 0;
		tuning->freq_shift[i]     			= 0;
		rotation->motion_scalecv_overage[i] = 0;
	}

	rotation->motion_notejump 	= 0;
	rotation->motion_rotate   	= 0;

	filter->filter_type = MAXQ;
	filter->filter_mode = TWOPASS;

	state->initialised = true;

}

void Controller::load_from_state(void) {

	if(state != NULL && state->initialised) {

		//Set default parameter values
		for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
			filter->note[i]  					= state->note[i];
			filter->scale[i] 					= state->scale[i];
			rotation->motion_fadeto_scale[i] 	= filter->scale[i];
			rotation->motion_scale_dest[i]   	= filter->scale[i];
			filter->scale_bank[i] 				= state->scale_bank[i];
			rotation->motion_spread_dir[i]  	= 0;
			rotation->motion_spread_dest[i] 	= filter->note[i];
			rotation->motion_fadeto_note[i] 	= filter->note[i];

			rotation->motion_morphpos[i]        = 0;
			tuning->freq_shift[i]     			= 0;
			rotation->motion_scalecv_overage[i] = 0;
		}

		rotation->motion_notejump 	= 0;
		rotation->motion_rotate   	= 0;

		state->initialised = true;

	}

}

void Controller::populate_state(void) {

	if(state != NULL && state->initialised) {
		for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
			state->note[i] 						= filter->note[i];
			state->scale[i]						= filter->scale[i];
			state->scale_bank[i]				= filter->scale_bank[i];
		}
	}
}
