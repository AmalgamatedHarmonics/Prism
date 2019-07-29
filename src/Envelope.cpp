/*
 * envout_pwm.c - PWM output for the channel ENV OUT jacks
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

void Envelope::configure(IO *_io, Levels *_levels) {
	levels	= _levels;
	io		= _io;
}

void Envelope::initialise(void) {
	for (int i = 0; i < NUM_CHANNELS; i++) {
		io->env_out[i]  = 0.0f;
		io->voct_out[i] = 0.0f;
	}
}

void Envelope::update(void) {

	if (env_update_ctr++ > ENV_UPDATE_RATE) {
		env_update_ctr = 0;

		// VOCT calc
		for (int j = 0; j < NUM_CHANNELS; j++) {
			//multiply the preload float value by 8192 so we can do a series of integer comparisons in FreqCoef_to_PWMval()
			//it turns out integer comparisons are faster than float comparisons, and we do a lot of them in FreqCoef_to_PWMval()
			uint32_t k = envout_preload_voct[j] * 8192;	
												
			io->voct_out[j] = freqCoeftoVOct(k, envout_preload_voct[j]);
		}

		if (env_track_mode == ENV_SLOW || env_track_mode == ENV_FAST) {

			for (int j = 0; j < NUM_CHANNELS; j++) {
				//Apply LPF
				if(envelope[j] < envout_preload[j]) {
					envelope[j] *= envspeed_attack;
					envelope[j] += (1.0f - envspeed_attack) * envout_preload[j];
				} else {
					envelope[j] *= envspeed_decay;
					envelope[j] += (1.0f - envspeed_decay) * envout_preload[j];
				}

				//Pre-CV (input) or Post faders (quieter)
				//To-Do: Attenuate by a global system parameter
				if (env_prepost_mode == Pre) {
					io->env_out[j] = envelope[j] / ENV_SCALE;
				} else {
					io->env_out[j] = envelope[j] * levels->channel_level[j]  / ENV_SCALE;
				}
				if (io->env_out[j] > 1.0f) {
					io->env_out[j] = 1.0f;
				}
			}
		} else { //trigger mode
			for (int j = 0; j < NUM_CHANNELS; j++) {

				//Pre-CV (input) or Post-CV (output)
				if (env_prepost_mode == Pre) {
					if (stored_trigger_level[j] < 0.002f) {
						envout_preload[j] *= 0.5f;
					} else {
						envout_preload[j] *= stored_trigger_level[j];
					}
				} else {
					envout_preload[j] *= levels->channel_level[j];
					stored_trigger_level[j] = levels->channel_level[j];
				}

				if (env_trigout[j]) { //keep the trigger high for about 100ms, ignoring the input signal
					env_trigout[j]--;
				} else {
					if (((uint32_t)envout_preload[j]) > 0x02000000) { //about 12.5% of max, or 1V envelope output
						env_low_ctr[j] = 0;
						env_trigout[j] = 50; //about 100ms
						io->env_out[j]  = 10.0f;
					} else {
						if (++env_low_ctr[j] >= 50) { //only set the output low if the input signal has stayed low for about 100ms
							io->env_out[j] = 0.0f;
						}
					}
				}
			}
		}
	}
}

float Envelope::freqCoeftoVOct(uint32_t k, float v) {
	float t, b, tval, bval;

	if (k <= 14) return(MIN_VOCT);
	else if (k<=15) {t=0.001906896677806f;		b=0.001799870791119f;  	tval=-3.166666667f;	bval=-3.25f;}
	else if (k<=16) {t=0.002020286654891f;		b=0.001906896677806f;  	tval=-3.083333333f;	bval=-3.166666667f;}
	else if (k<=17) {t=0.002140419150884f;		b=0.002020286654891f;  	tval=-3.0f;			bval=-3.083333333f;}
	else if (k<=18) {t=0.002267695096821f;		b=0.002140419150884f;  	tval=-2.916666667f;	bval=-3.0f;}
	else if (k<=19) {t=0.002402539264342f;		b=0.002267695096821f;  	tval=-2.833333333f;	bval=-2.916666667f;}
	else if (k<=20) {t=0.002545401683319f;		b=0.002402539264342f;  	tval=-2.75f;		bval=-2.833333333f;}
	else if (k<=22) {t=0.002696759143797f;		b=0.002545401683319f;  	tval=-2.666666667f;	bval=-2.75f;}
	else if (k<=23) {t=0.002857116787229f;		b=0.002696759143797f;  	tval=-2.583333333f;	bval=-2.666666667f;}
	else if (k<=24) {t=0.003027009792343f;		b=0.002857116787229f;  	tval=-2.5f;			bval=-2.583333333f;}
	else if (k<=26) {t=0.003207005161252f;		b=0.003027009792343f;  	tval=-2.416666667f;	bval=-2.5f;}
	else if (k<=27) {t=0.003397703611766f;		b=0.003207005161252f;  	tval=-2.333333333f;	bval=-2.416666667f;}
	else if (k<=29) {t=0.003599741582238f;		b=0.003397703611766f;  	tval=-2.25f;		bval=-2.333333333f;}
	else if (k<=31) {t=0.003813793355611f;		b=0.003599741582238f;  	tval=-2.166666667f;	bval=-2.25f;}
	else if (k<=33) {t=0.004040573309783f;		b=0.003813793355611f;  	tval=-2.083333333f;	bval=-2.166666667f;}
	else if (k<=35) {t=0.004280838301768f;		b=0.004040573309783f;  	tval=-2.0f;			bval=-2.083333333f;}
	else if (k<=37) {t=0.004535390193643f;		b=0.004280838301768f;  	tval=-1.916666667f;	bval=-2.0f;}
	else if (k<=39) {t=0.004805078528684f;		b=0.004535390193643f;  	tval=-1.833333333f;	bval=-1.916666667f;}
	else if (k<=41) {t=0.005090803366639f;		b=0.004805078528684f;  	tval=-1.75;			bval=-1.833333333f;}
	else if (k<=44) {t=0.005393518287594f;		b=0.005090803366639f;  	tval=-1.666666667f;	bval=-1.75f;}
	else if (k<=46) {t=0.005714233574458f;		b=0.005393518287594f;  	tval=-1.583333333f;	bval=-1.666666667f;}
	else if (k<=49) {t=0.006054019584687f;		b=0.005714233574458f;  	tval=-1.5f;			bval=-1.583333333f;}
	else if (k<=52) {t=0.006414010322504f;		b=0.006054019584687f;  	tval=-1.416666667f;	bval=-1.5f;}
	else if (k<=55) {t=0.006795407223532f;		b=0.006414010322504f;  	tval=-1.333333333f;	bval=-1.416666667f;}
	else if (k<=58) {t=0.007199483164475f;		b=0.006795407223532f;  	tval=-1.25f;		bval=-1.333333333f;}
	else if (k<=62) {t=0.007627586711222f;		b=0.007199483164475f;  	tval=-1.166666667f;	bval=-1.25f;}
	else if (k<=66) {t=0.008081146619566f;		b=0.007627586711222f;  	tval=-1.083333333f;	bval=-1.166666667f;}
	else if (k<=70) {t=0.008561676603536f;		b=0.008081146619566f;  	tval=-1.0f;			bval=-1.083333333f;}
	else if (k<=74) {t=0.009070780387286f;		b=0.008561676603536f;  	tval=-0.916666667f;	bval=-1.0f;}
	else if (k<=78) {t=0.009610157057368f;		b=0.009070780387286f;  	tval=-0.833333333f;	bval=-0.916666667f;}
	else if (k<=83) {t=0.010181606733277f;		b=0.009610157057368f;  	tval=-0.75f;		bval=-0.833333333f;}
	else if (k<=88) {t=0.010787036575188f;		b=0.010181606733277f;  	tval=-0.666666667f;	bval=-0.75f;}
	else if (k<=93) {t=0.011428467148915f;		b=0.010787036575188f;  	tval=-0.583333333f;	bval=-0.666666667f;}
	else if (k<=99) {t=0.012108039169373f;		b=0.011428467148915f;  	tval=-0.5f;			bval=-0.583333333f;}
	else if (k<=105) {t=0.012828020645008f;		b=0.012108039169373f;	tval=-0.416666667f;	bval=-0.5f;}
	else if (k<=111) {t=0.013590814447065f;		b=0.012828020645008f; 	tval=-0.333333333f;	bval=-0.416666667f;}
	else if (k<=117) {t=0.01439896632895f;		b=0.013590814447065f;  	tval=-0.25f;		bval=-0.333333333f;}
	else if (k<=124) {t=0.015255173422445f;		b=0.01439896632895f;  	tval=-0.166666667f;	bval=-0.25f;}
	else if (k<=132) {t=0.016162293239131f;		b=0.015255173422445f; 	tval=-0.083333333f;	bval=-0.166666667f;}
	else if (k<=140) {t=0.017123353207072f;		b=0.016162293239131f; 	tval=0.0f;			bval=-0.083333333f;}
	else if (k<=148) {t=0.018141560774572f;		b=0.017123353207072f; 	tval=0.083333333f;	bval=0.0f;}
	else if (k<=157) {t=0.019220314114735f;		b=0.018141560774572f; 	tval=0.166666667f;	bval=0.083333333f;}
	else if (k<=166) {t=0.020363213466555f;		b=0.019220314114735f; 	tval=0.25f;			bval=0.166666667f;}
	else if (k<=176) {t=0.021574073150375f;		b=0.020363213466555f; 	tval=0.333333333f;	bval=0.25f;}
	else if (k<=187) {t=0.02285693429783f;		b=0.021574073150375f;  	tval=0.416666667f;	bval=0.333333333f;}
	else if (k<=198) {t=0.024216078338746f;		b=0.02285693429783f;  	tval=0.5f;			bval=0.416666667f;}
	else if (k<=210) {t=0.025656041290015f;		b=0.024216078338746f; 	tval=0.583333333f;	bval=0.5f;}
	else if (k<=222) {t=0.027181628894129f;		b=0.025656041290015f; 	tval=0.666666667f;	bval=0.583333333f;}
	else if (k<=235) {t=0.0287979326579f;		b=0.027181628894129f;   tval=0.75f;			bval=0.666666667f;}
	else if (k<=249) {t=0.03051034684489f;		b=0.0287979326579f;    	tval=0.833333333f;	bval=0.75f;}
	else if (k<=264) {t=0.032324586478262f;		b=0.03051034684489f;  	tval=0.916666667f;	bval=0.833333333f;}
	else if (k<=280) {t=0.034246706414144f;		b=0.032324586478262f; 	tval=1.0f;			bval=0.916666667f;}
	else if (k<=297) {t=0.036283121549144f;		b=0.034246706414144f; 	tval=1.083333333f;	bval=1.0f;}
	else if (k<=314) {t=0.03844062822947f;		b=0.036283121549144f;  	tval=1.166666667f;	bval=1.083333333f;}
	else if (k<=333) {t=0.04072642693311f;		b=0.03844062822947f;   	tval=1.25f;			bval=1.166666667f;}
	else if (k<=353) {t=0.04314814630075f;		b=0.04072642693311f;   	tval=1.333333333f;	bval=1.25f;}
	else if (k<=374) {t=0.04571386859566f;		b=0.04314814630075f;   	tval=1.416666667f;	bval=1.333333333f;}
	else if (k<=396) {t=0.048432156677492f;		b=0.04571386859566f;  	tval=1.5f;			bval=1.416666667f;}
	else if (k<=420) {t=0.05131208258003f;		b=0.048432156677492f;  	tval=1.583333333f;	bval=1.5f;}
	else if (k<=445) {t=0.054363257788259f;		b=0.05131208258003f;  	tval=1.666666667f;	bval=1.583333333f;}
	else if (k<=471) {t=0.057595865315801f;		b=0.054363257788259f; 	tval=1.75f;			bval=1.666666667f;}
	else if (k<=499) {t=0.061020693689779f;		b=0.057595865315801f; 	tval=1.833333333f;	bval=1.75f;}
	else if (k<=529) {t=0.064649172956524f;		b=0.061020693689779f; 	tval=1.916666667f;	bval=1.833333333f;}
	else if (k<=561) {t=0.068493412828289f;		b=0.064649172956524f; 	tval=2.0f;			bval=1.916666667f;}
	else if (k<=594) {t=0.072566243098287f;		b=0.068493412828289f; 	tval=2.083333333f;	bval=2.0f;}
	else if (k<=629) {t=0.07688125645894f;		b=0.072566243098287f;  	tval=2.166666667f;	bval=2.083333333f;}
	else if (k<=667) {t=0.081452853866219f;		b=0.07688125645894f;  	tval=2.25f;			bval=2.166666667f;}
	else if (k<=706) {t=0.086296292601501f;		b=0.081452853866219f; 	tval=2.333333333f;	bval=2.25f;}
	else if (k<=748) {t=0.091427737191321f;		b=0.086296292601501f; 	tval=2.416666667f;	bval=2.333333333f;}
	else if (k<=793) {t=0.096864313354985f;		b=0.091427737191321f; 	tval=2.5f;			bval=2.416666667f;}
	else if (k<=840) {t=0.102624165160061f;		b=0.096864313354985f; 	tval=2.583333333f;	bval=2.5f;}
	else if (k<=890) {t=0.108726515576518f;		b=0.102624165160061f; 	tval=2.666666667f;	bval=2.583333333f;}
	else if (k<=943) {t=0.115191730631602f;		b=0.108726515576518f; 	tval=2.75f;			bval=2.666666667f;}
	else if (k<=999) {t=0.122041387379559f;		b=0.115191730631602f; 	tval=2.833333333f;	bval=2.75f;}
	else if (k<=1059) {t=0.129298345913049f;	b=0.122041387379559f;	tval=2.916666667f;	bval=2.833333333f;}
	else if (k<=1122) {t=0.136986825656577f;	b=0.129298345913049f;	tval=3.0f;			bval=2.916666667f;}
	else if (k<=1188) {t=0.145132486196575f;	b=0.136986825656577f;	tval=3.083333333f;	bval=3.0f;}
	else if (k<=1259) {t=0.153762512917881f;	b=0.145132486196575f;	tval=3.166666667f;	bval=3.083333333f;}
	else if (k<=1334) {t=0.162905707732438f;	b=0.153762512917881f;	tval=3.25f;			bval=3.166666667f;}
	else if (k<=1413) {t=0.172592585203002f;	b=0.162905707732438f;	tval=3.333333333f;	bval=3.25f;}
	else if (k<=1497) {t=0.182855474382642f;	b=0.172592585203002f;	tval=3.416666667f;	bval=3.333333333f;}
	else if (k<=1587) {t=0.19372862670997f;		b=0.182855474382642f; 	tval=3.5f;			bval=3.416666667f;}
	else if (k<=1681) {t=0.205248330320122f;	b=0.19372862670997f; 	tval=3.583333333f;	bval=3.5f;}
	else if (k<=1781) {t=0.217453031153035f;	b=0.205248330320122f;	tval=3.666666667f;	bval=3.583333333f;}
	else if (k<=1887) {t=0.230383461263203f;	b=0.217453031153035f;	tval=3.75f;			bval=3.666666667f;}
	else if (k<=1999) {t=0.244082774759117f;	b=0.230383461263203f;	tval=3.833333333f;	bval=3.75f;}
	else if (k<=2118) {t=0.258596691826098f;	b=0.244082774759117f;	tval=3.916666667f;	bval=3.833333333f;}
	else if (k<=2244) {t=0.273973651313155f;	b=0.258596691826098f;	tval=4.0f;			bval=3.916666667f;}
	else if (k<=2377) {t=0.290264972393149f;	b=0.273973651313155f;	tval=4.083333333f;	bval=4.0f;}
	else if (k<=2519) {t=0.307525025835761f;	b=0.290264972393149f;	tval=4.166666667f;	bval=4.083333333f;}
	else if (k<=2669) {t=0.325811415464877f;	b=0.307525025835761f;	tval=4.25f;			bval=4.166666667f;}
	else if (k<=2827) {t=0.345185170406003f;	b=0.325811415464877f;	tval=4.333333333f;	bval=4.25f;}
	else if (k<=2995) {t=0.365710948765283f;	b=0.345185170406003f;	tval=4.416666667f;	bval=4.333333333f;}
	else if (k<=3174) {t=0.387457253419939f;	b=0.365710948765283f;	tval=4.5f;			bval=4.416666667f;}
	else if (k<=3362) {t=0.410496660640243f;	b=0.387457253419939f;	tval=4.583333333f;	bval=4.5f;}
	else if (k<=3562) {t=0.43490606230607f;		b=0.410496660640243f;	tval=4.666666667f;	bval=4.583333333f;}
	else if (k<=3774) {t=0.460766922526406f;	b=0.43490606230607f;	tval=4.75f;			bval=4.666666667f;}
	else return(MAX_VOCT);

	return (((t - v) / (t - b)) * bval + ((v - b)/(t - b)) * tval);

}
