#include "Scales.hpp"

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

std::vector<Scale> scales = {
	major,
	minor,
	et_intervals,
	et_triads,
 	et_chromatic,
	et_wholestep,
	ji_intervals,
	ji_triads,
	ji_wholestep,
	indian_penta,
	indian_shrutis,
	mesopotamian,
	gamelan,
	wc_alpha2,
	wc_alpha1,
	wc_gamma,
	seventeen,
	bp,
	buchla296,
	userscale,
	gamma_notused,
	video_notused
};
