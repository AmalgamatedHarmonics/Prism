/*
 * lpf.c - Low Pass Filter utilities
 *
 * Author: Dan Green (danngreen1@gmail.com)
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

uint32_t diff(uint32_t a, uint32_t b) {
	return (a > b) ? (a - b) : (b - a);
}

void LPF::setup_fir_filter() {

	//range check the lpf filter size
	if (fir_lpf_size > MAX_FIR_LPF_SIZE) {
		fir_lpf_size = MAX_FIR_LPF_SIZE;
	}

	//initialize the moving average buffer
	for (int i = 0; i < MAX_FIR_LPF_SIZE; i++) {
		fir_lpf[i] = 0.0f;
	}

	//set inital values
	lpf_val = raw_val = bracketed_val = fir_lpf[0];
}

void LPF::apply_fir_lpf() {
	float old_value, new_value;

	old_value = fir_lpf[fir_lpf_i]; 	//oldest value: remove this from the array
	new_value = raw_val;				//new value: insert this into the array

	fir_lpf[fir_lpf_i] = new_value;

	//Increment the index, wrapping around the whole buffer
	if (++fir_lpf_i >= fir_lpf_size) {
		fir_lpf_i = 0;
	}

	//Calculate the arithmetic average (FIR LPF)
	lpf_val = ((lpf_val * fir_lpf_size) - old_value + new_value) / fir_lpf_size;

}

//
// Bracketing
//
void LPF::apply_bracket() {

	float diff = lpf_val - bracketed_val;

	if (diff > bracket_size)	{
		bracketed_val = lpf_val - bracket_size;
	} else if (diff < (-1*(bracket_size))) {
		bracketed_val = lpf_val + bracket_size;
	}

}

