/*
 * Idea for algoritm from https://www.kvraudio.com/forum/viewtopic.php?t=195315
 * Optimized for int32_t by Dan Green
 */

#include "Rainbow.hpp"

using namespace rainbow;

void Limiter::initialise(uint32_t max_sampleval_in, float threshold_percent_in) {
	max_sampleval   = max_sampleval_in;
	threshold_percent = threshold_percent_in;
	
	float m = (float)max_sampleval;
	threshold_compiled = m * m * threshold_percent * (1.0 - threshold_percent);
	threshold_value    = threshold_percent * max_sampleval;
}

int32_t Limiter::limit(int32_t val) {
	float tv = threshold_compiled / ((float)val); // value to be subtracted for incoming signal going above theshold 

	if (val > threshold_value) {
        return (max_sampleval - tv);
    } else if (val < -threshold_value) {
        return  (-max_sampleval - tv); 
    } else {
        return val;
    }
}