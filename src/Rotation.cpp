/*
 * rotation.c - Calculate position of each filter in the scale, based on rotation, spread, rotation CV, and scale CV
 *
 * Author: Dan Green (danngreen1@gmail.com), Hugo Paris (hugoplho@gmail.com)
 * 2015
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

void Rotation::configure(IO *_io, Filter *_filter) {
	filter		= _filter;
	io			= _io;
}

void Rotation::change_scale_up(void) {
	for (int i = 0; i < NUM_CHANNELS; i++) {
		if (!io->LOCK_ON[i]) {
			if (motion_scale_dest[i] < (NUM_SCALES - 1)) {
				motion_scale_dest[i]++;
			}
		}
	}
}

void Rotation::change_scale_down(void) {
	for (int i = 0; i < NUM_CHANNELS; i++) {
		if (!io->LOCK_ON[i]) {
			if (motion_scale_dest[i] > 0) {
				motion_scale_dest[i]--;
			}
		}
	}
}

void Rotation::jump_rotate_with_cv(int8_t shift_amt) {
	motion_notejump += shift_amt;
}

void Rotation::jump_scale_with_cv(int8_t shift_amt) {
	for (int i = 0; i < NUM_CHANNELS; i++) {

		if (!io->LOCK_ON[i]) {
			int8_t this_shift_amt = shift_amt;

			if (this_shift_amt < 0 && motion_scalecv_overage[i] > 0) {
				if ((-1 * this_shift_amt) > motion_scalecv_overage[i]) {
					this_shift_amt += motion_scalecv_overage[i];
					motion_scalecv_overage[i] = 0;
				} else {
					motion_scalecv_overage[i] += this_shift_amt;
					this_shift_amt = 0;
				}
			}

			motion_scale_dest[i] += this_shift_amt;

			if (motion_scale_dest[i] < 0) {
				motion_scale_dest[i] = 0;
			}
			if (motion_scale_dest[i] > (NUM_SCALES - 1)) {
				motion_scalecv_overage[i] = motion_scale_dest[i] - (NUM_SCALES - 1);
				motion_scale_dest[i] = NUM_SCALES - 1;
			}
		}
	}
}

void Rotation::rotate_down(void) {
	//if we were rotating down, reverse direction. Otherwise add to current direction
	if (motion_rotate >= 0) {
		motion_rotate = -1;
	} else { 
		motion_rotate--;
	}
}

void Rotation::rotate_up(void) {
	if (motion_rotate <= 0) {
		motion_rotate = 1;
	} else {
		motion_rotate++;
	}
}

bool Rotation::is_spreading(void) {
	for (int i = 0; i < NUM_CHANNELS; i++) {
		if (motion_spread_dir[i] != 0) {
			return true;
		}
	}
	return false;
}

bool Rotation::is_morphing(void) {
	for (int i = 0; i < NUM_CHANNELS; i++) {
		if (motion_morphpos[0] != 0.0) {
			return true;
		}
	}
	return false;
}

void Rotation::update_spread(int8_t t_spread) {

	int32_t test_motion[NUM_CHANNELS] = {0, 0, 0, 0, 0, 0};

	float spread_out;
	spread = t_spread;
	if (spread > old_spread) {
		spread_out = 1;
	} else {
		spread_out = -1;
	}
	old_spread = spread;

	for (int ii = 0 ; ii < NUM_CHANNELS; ii++) {
		test_motion[ii] = 99;
	}

	int32_t base_note = motion_fadeto_note[2];

	for (int32_t i = 0; i < NUM_CHANNELS; i++) {

		if (io->LOCK_ON[i] || i == 2) {
			test_motion[i] = motion_fadeto_note[i];
		} else {
			//Set spread direction. Note spread_dir[2] remains 0 (stationary)
			if (i < 2) {
				motion_spread_dir[i] = -1 * spread_out;
			}
			if (i > 2) {
				motion_spread_dir[i] = spread_out;
			}

			//Find an open filter channel:
			//Our starting point is based on the location of channel 2, and spread outward from there
			int32_t offset = (((int32_t)i) - 2) * spread;
			int32_t test_spot = base_note + offset;

			//Set up test_spot, since the first thing we do is in the loop is test_spot += spread_dir[i]
			test_spot -= motion_spread_dir[i];

			//Now check to see if test_spot is open
			bool is_distinct = false;
			int k = 0;
			while (!is_distinct && (k < NUM_FILTS + 1)) {

				test_spot += motion_spread_dir[i];

				while (test_spot >= NUM_FILTS) {
					test_spot -= NUM_FILTS;
				}
				while (test_spot < 0) {
					test_spot += NUM_FILTS;
				}

				is_distinct = true;
				// Check to see if test_spot is a distinct value:
				for (int j = 0; j < NUM_CHANNELS; j++) {
					//...if the test spot is already assigned to a locked or stationary channel (2), or a channel we already spread'ed, or a blocked frequency then try again
					if ((i != j && ((test_spot == test_motion[j] && j < i) || (test_spot == motion_fadeto_note[j] && (io->LOCK_ON[j] || j == 2)))) || io->FREQ_BLOCK[test_spot]) {
						is_distinct = false;
					}
				}
				k++;
			}

			test_motion[i] = test_spot;

		}
	}

	for (int ii = 0 ; ii < NUM_CHANNELS; ii++) {
		motion_spread_dest[ii] = test_motion[ii];
	}

}

void Rotation::update_morph(void) {
	f_morph *= 0.999f;
	f_morph += 0.001f * (exp_4096[io->MORPH_ADC] / 16.0f);

	//if morph is happening, continue it
	//if it hits the limit, just hold it there until we can run update_motion()
	for (int chan = 0; chan < NUM_CHANNELS; chan++)	{
		if (motion_morphpos[chan] > 0.0f) {
			motion_morphpos[chan] += f_morph;
		}
		if (motion_morphpos[chan] >= 1.0f) {
			motion_morphpos[chan] = 1.0f;
		}
	}

}

void Rotation::update_motion(void) {

	if (rot_update_ctr++ > ROT_UPDATE_RATE) {
		rot_update_ctr = 0;

		bool is_distinct;

		for (int chan = 0; chan < NUM_CHANNELS; chan++)	{
			//if morph has reached the end, shift our present position to the (former) fadeto destination
			if (motion_morphpos[chan] >= 1.0f) {
				filter->note[chan]  = motion_fadeto_note[chan];
				filter->scale[chan] = motion_fadeto_scale[chan];

				if (motion_spread_dest[chan] == filter->note[chan]) {
					motion_spread_dir[chan] = 0;
				}
				motion_morphpos[chan] = 0.0f;

				io->FORCE_RING_UPDATE = true;
			}
		}

		//Initiate a motion if one is not happening
		if (!is_morphing()) {

			//Rotation CW
			if (motion_rotate > 0) {
				motion_rotate--;
				for (int chan = 0; chan < NUM_CHANNELS; chan++) {
					if (!io->LOCK_ON[chan]) {
						motion_spread_dir[chan] = 1;
						//Increment circularly
						if (motion_spread_dest[chan] >= (NUM_FILTS - 1)) {
							motion_spread_dest[chan] = 0;
						} else {
							motion_spread_dest[chan]++;
						}
					}
				}
			} else if (motion_rotate < 0) {
			//Rotation CCW
				motion_rotate++;
				for (int chan = 0; chan < NUM_CHANNELS; chan++) {
					if (!io->LOCK_ON[chan]) {
						motion_spread_dir[chan] = -1;
						//Dencrement circularly
						if (motion_spread_dest[chan] == 0) {
							motion_spread_dest[chan] = NUM_FILTS - 1;
						} else {
							motion_spread_dest[chan]--;
						}
					}
				}
			} else if (motion_notejump != 0) {
			//Rotate CV (jump)
				for (int chan = 0; chan < NUM_CHANNELS; chan++) {
					if (!io->LOCK_ON[chan]) {
						//Dec/Increment circularly
						motion_fadeto_note[chan] += motion_notejump;
						while (motion_fadeto_note[chan] < 0) {
							motion_fadeto_note[chan] += NUM_FILTS;
						}
						while (motion_fadeto_note[chan] >= NUM_FILTS) {
							motion_fadeto_note[chan] -= NUM_FILTS;
						}
						//Dec/Increment circularly
						motion_spread_dest[chan] += motion_notejump;
						while (motion_spread_dest[chan] < 0) {
							motion_spread_dest[chan] += NUM_FILTS;
						}
						while (motion_spread_dest[chan] >= NUM_FILTS) {
							motion_spread_dest[chan] -= NUM_FILTS;
						}

						//Check if the new destination is occupied by a locked channel, a channel we already notejump'ed or a blocked freq
						if (io->FREQ_BLOCK[motion_fadeto_note[chan]]) {
							is_distinct = false;
						} else {
							is_distinct = true;
							for (int test_chan = 0; test_chan < NUM_CHANNELS; test_chan++) {
								if (chan != test_chan && motion_fadeto_note[chan] == motion_fadeto_note[test_chan] && (io->LOCK_ON[test_chan] || test_chan < chan)) {
									is_distinct = false;
								}
							}
						}
						while (!is_distinct) {

							//Inc/decrement circularly
							if (motion_notejump > 0) {
								motion_fadeto_note[chan] = (motion_fadeto_note[chan] + 1) % NUM_FILTS;
							} else {
								if (motion_fadeto_note[chan] ==0 ) {
									motion_fadeto_note[chan] = NUM_FILTS - 1;
								} else {
									motion_fadeto_note[chan] = motion_fadeto_note[chan] - 1;
								}
							}

							//Check if the new destination is occupied by a locked channel, a channel we already notejump'ed or a blocked freq						
							if (io->FREQ_BLOCK[motion_fadeto_note[chan]]) {
								is_distinct = false;
							} else {
								is_distinct = true;
								for (int test_chan = 0;test_chan < NUM_CHANNELS; test_chan++) {
									if (chan != test_chan && motion_fadeto_note[chan] == motion_fadeto_note[test_chan] && (io->LOCK_ON[test_chan] || test_chan < chan)) {
										is_distinct = false;
									}
								}
							}
						}
						//Start the motion morph
						motion_morphpos[chan] = f_morph;
						motion_fadeto_scale[chan] = motion_scale_dest[chan];

					}
				}
				motion_notejump = 0;
			}
		}

		for (int chan = 0; chan < NUM_CHANNELS; chan++) {

			if (motion_morphpos[chan] == 0.0f) {

				//Spread

				//Problem: try spreading from 5 to 6, then 6 to 5.
				//channel 0 and 3 collide like this:
				//Start 18 x x 16 x x
				//End   16 x x 17 x x
				//So channel 0 starts to fade from 18 to 17
				//Channel 3 wants to fade to 17, but 17 is taken as a fadeto, so it fades to 18.
				//Next step:
				//Channel 0 fades to 16 and is done
				//Channel 3 fades from 18 to 19, 0, 1...17. Since its dir is set to +1, it has to go all the way around the long way from 18 to 17
				//Not big problem except that the scale is incremented on channel 3, and when spread is returned to 5, the scale does not decrement
				//Thus channel 3 will be in a different scale than when it started.
				//Solution ideas:
				//-ignore the issue, but force spread=1 to make all notes in the same scale (unless spanning 19/0)
				//-allow channels to fadeto the same spot, but not have the same motion_spread_dest
				// --ch0: 18->17 ch3: 16->17; ch0: 17->16 ch3: stay
				//-change the is_distinct code in fadeto to give priority to channels that are hitting their motion_spread_dest
				// --or somehow change it so ch0: 18->17, ch3:16->17 bumps ch0 18->16

				if (motion_spread_dest[chan] != filter->note[chan]) {

					// Check if the spread destination is still available
					if  (io->FREQ_BLOCK[motion_spread_dest[chan]]) {
						is_distinct = false;
					} else  {
						is_distinct = true;
						for (int test_chan = 0; test_chan < NUM_CHANNELS; test_chan++) {
							if (chan != test_chan && (motion_spread_dest[chan] == motion_spread_dest[test_chan]) && (io->LOCK_ON[test_chan] || test_chan < chan || motion_spread_dir[test_chan] == 0)) { 
								is_distinct = false;
							}
						}
					}
					while (!is_distinct) {

						//Inc/decrement circularly
						if (motion_spread_dir[chan] == 0) {
							motion_spread_dir[chan] = 1;
						}

						if (motion_spread_dir[chan] > 0) {
							motion_spread_dest[chan] = (motion_spread_dest[chan] + 1) % NUM_FILTS;
						} else {
							if (motion_spread_dest[chan] == 0) {
								motion_spread_dest[chan] = NUM_FILTS - 1;
							} else {
								motion_spread_dest[chan] = motion_spread_dest[chan] - 1;
							}
						}

						//Check that it's not already taken by a locked channel, channel with a higher priority, a blocked freq or a non-moving channel
						if (io->FREQ_BLOCK[motion_spread_dest[chan]]) {
							is_distinct = false;
						} else {
							is_distinct = true;
							for (int test_chan = 0; test_chan < NUM_CHANNELS; test_chan++) {
								if (chan != test_chan && (motion_spread_dest[chan] == motion_spread_dest[test_chan]) && (io->LOCK_ON[test_chan] || test_chan<chan || motion_spread_dir[test_chan] == 0)) {
									is_distinct = false;
								}
							}
						}
					}

					//Clockwise Spread
					if (motion_spread_dir[chan] > 0) {

						//Start the motion morph
						motion_morphpos[chan] = f_morph;

						// Shift the destination CW, wrapping it around
						is_distinct = false;
						while (!is_distinct) {

							//Increment circularly
							if (motion_fadeto_note[chan] >= (NUM_FILTS - 1)) {
								motion_fadeto_note[chan] = 0;

								// If scale rotation is on, increment the scale, wrapping it around
								if (rotate_to_next_scale) {
									motion_fadeto_scale[chan] = (motion_fadeto_scale[chan] + 1) % NUM_SCALES;
									motion_scale_dest[chan]   = (motion_scale_dest[chan] + 1) % NUM_SCALES;
								}
							} else {
								motion_fadeto_note[chan]++;
							}

							//Check that it's not already taken by a locked channel, channel with a higher priority, a blocked freq, or a non-moving channel
							if (io->FREQ_BLOCK[motion_fadeto_note[chan]]) {
								is_distinct = false;
							} else {
								is_distinct = true;
								for (int test_chan = 0; test_chan < NUM_CHANNELS; test_chan++) {
									if (chan != test_chan && (motion_fadeto_note[chan] == motion_fadeto_note[test_chan]) && (io->LOCK_ON[test_chan] || test_chan < chan)) {
										is_distinct = false;
									}
								}
							}
						}
					} else if (motion_spread_dir[chan]<0) {

					//Counter-clockwise Spread
				
						//Start the motion morph
						motion_morphpos[chan] = f_morph;

						// Shift the destination CCW, wrapping it around and repeating until we find a distinct value
						is_distinct = false;
						while (!is_distinct) {

							//Decrement circularly
							if (motion_fadeto_note[chan] == 0) {
								motion_fadeto_note[chan] = NUM_FILTS - 1;

								// If scale rotation is on, decrement the scale, wrapping it around
								if (rotate_to_next_scale) {
									if (motion_fadeto_scale[chan] == 0) {
										motion_fadeto_scale[chan] = NUM_SCALES - 1;
									} else {
										motion_fadeto_scale[chan]--;
									}
									if (motion_scale_dest[chan] == 0) {
										motion_scale_dest[chan] = NUM_SCALES - 1;
									} else {
										motion_scale_dest[chan]--;
									}
								}
							}
							else {
								motion_fadeto_note[chan]--;
							}

							//Check that it's not already taken by a locked channel, channel with a higher priority, a blocked freq, or a non-moving channel
							if (io->FREQ_BLOCK[motion_fadeto_note[chan]]) {
								is_distinct = false;
							} else {
								is_distinct = true;
								for (int test_chan = 0; test_chan < NUM_CHANNELS; test_chan++) {
									if (chan != test_chan && (motion_fadeto_note[chan] == motion_fadeto_note[test_chan]) && (io->LOCK_ON[test_chan] || test_chan < chan)) { 
										is_distinct = false;
									}
								}
							}
						}
					}
				}
				else if (motion_scale_dest[chan] != motion_fadeto_scale[chan]) {
				//Scale
					//Start the motion morph
					motion_morphpos[chan] = f_morph;
					motion_fadeto_scale[chan] = motion_scale_dest[chan];
				}
			}
		}
	}
}
