#pragma once

#include <vector>
#include <string>

struct Scale {
	std::string name;
	std::string description;
	std::string scalename[11];
	std::string notedesc[231];
	float c_maxq48000[231];
	float c_maxq96000[231];
	float c_bpre4800022[231][3];
	float c_bpre9600022[231][3];
	float c_bpre4800080040[231][3];
	float c_bpre9600080040[231][3];
};

struct ScaleSet {

	std::vector<Scale *> presets;
	std::vector<Scale *> full;
	ScaleSet();
	
};

extern Scale et_major;
extern Scale et_minor;
extern Scale et_intervals;
extern Scale et_triads;
extern Scale et_chromatic;
extern Scale et_wholestep;
extern Scale ji_intervals;
extern Scale ji_triads;
extern Scale ji_wholestep;
extern Scale indian_penta;
extern Scale indian_shrutis;
extern Scale mesopotamian;
extern Scale gamelan;
extern Scale wc_alpha1;
extern Scale wc_alpha2;
extern Scale wc_gamma;
extern Scale seventeen;
extern Scale bohlenpierce;
extern Scale buchla296;
extern Scale userscale;
extern Scale gamma_notused;
extern Scale video_notused;
