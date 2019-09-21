
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

void Q::configure(IO *_io) {
	io			= _io;
}

void Q::update(void) {

 	if (q_update_ctr++ > Q_UPDATE_RATE) { 
		q_update_ctr = 0;

		float lpf = io->HICPUMODE ? Q_LPF_96 : Q_LPF_48;

		//Check jack + LPF
		int32_t qg = io->GLOBAL_Q_LEVEL + io->GLOBAL_Q_CONTROL;
		if (qg < 0) {
			qg = 0;
		}
		if (qg > 4095) {
			qg = 4095;
		}
		
		global_lpf *= lpf;
		global_lpf += (1.0f - lpf) * qg;

		for (int i = 0; i < NUM_CHANNELS; i++){
			int32_t qc = io->CHANNEL_Q_LEVEL[i] + io->CHANNEL_Q_CONTROL[i];
			if (qc < 0) {
				qc = 0;
			}
			if (qc > 4095) {
				qc = 4095;
			}

			qlockpot_lpf[i] *= lpf;
			qlockpot_lpf[i] += (1.0f - lpf) * qc;

			prev_qval[i] = qval_goal[i];
			if (io->CHANNEL_Q_ON[i]) {
				qval_goal[i] = qlockpot_lpf[i];
			} else {
				qval_goal[i] = global_lpf;
			}
		}
 	}
 	
 	// SMOOTH OUT DATA BETWEEN ADC READS
	for (int i = 0; i < NUM_CHANNELS; i++) {
		qval[i] = (uint32_t)(prev_qval[i] + (q_update_ctr * (qval_goal[i] - prev_qval[i]) / 51.0f)); // Q_UPDATE_RATE + 1
 	}

}