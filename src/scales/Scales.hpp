#pragma once

#include <vector>
#include <string>

struct Scale {
	std::string name;
	std::string description;
	std::string scalename[11];
	std::string notedesc[231];
	float c_maxq[231];
	float c_bpre_hi[231][3];
	float c_bpre_lo[231][3];
};

struct ScaleSet {

	std::vector<Scale *> presets;
	std::vector<Scale *> full;
	ScaleSet();
	
};

extern Scale major;
extern Scale minor;
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
extern Scale bp;
extern Scale buchla296;
extern Scale userscale;
extern Scale gamma_notused;
extern Scale video_notused;
