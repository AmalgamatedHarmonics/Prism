#include <math.h>
#include <vector>

#include "../Rainbow.hpp"

using namespace rainbow;

struct Scale {
	std::string name;
	std::string description;
	std::string scalename[NUM_SCALES];
	std::string notedesc[NUM_BANKNOTES];
	float c_maxq[NUM_BANKNOTES];
	float c_bpre_hi[NUM_BANKNOTES][3];
	float c_bpre_lo[NUM_BANKNOTES][3];
};

