
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

		//Check jack + LPF
		int32_t t = io->QVAL_ADC;
		qcv_lpf *= QCV_LPF;
		qcv_lpf += (1.0f - QCV_LPF) * t;

		//Check pot + LPF
		t = io->QPOT_ADC;
		qpot_lpf *= QPOT_LPF;
		qpot_lpf += (1.0f - QPOT_LPF) * t;

		for (int i = 0; i < NUM_CHANNELS; i++){
			t = io->CHANNEL_Q_ADC[i];
			qlockpot_lpf[i] *= QPOT_LPF;
			qlockpot_lpf[i] += (1.0f - QPOT_LPF) * t;

			prev_qval[i] = qval_goal[i];
			if (io->CHANNEL_Q_ON[i]) {
				qval_goal[i] = qlockpot_lpf[i];
			} else {
				qval_goal[i] = qcv_lpf + qpot_lpf;
				if (qval_goal[i] > 4095.0f) {
                    qval_goal[i] = 4095.0f;
                }
			}
		}
 	}
 	
 	// SMOOTH OUT DATA BETWEEN ADC READS
	for (int i = 0; i < NUM_CHANNELS; i++) {
		qval[i] = (uint32_t)(prev_qval[i] + (q_update_ctr * (qval_goal[i] - prev_qval[i]) / 16.0));
 	}

}