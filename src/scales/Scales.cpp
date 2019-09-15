#include "Scales.hpp"

ScaleSet::ScaleSet() {

	presets.push_back(&et_major);
	presets.push_back(&et_minor);
	presets.push_back(&et_intervals);
	presets.push_back(&et_triads);
	presets.push_back(&et_chromatic);
	presets.push_back(&et_wholestep);
	presets.push_back(&ji_intervals);
	presets.push_back(&ji_triads);
	presets.push_back(&ji_wholestep);
	presets.push_back(&indian_penta);
	presets.push_back(&indian_shrutis);
	presets.push_back(&mesopotamian);
	presets.push_back(&gamelan);
	presets.push_back(&wc_alpha2);
	presets.push_back(&wc_alpha1);
	presets.push_back(&wc_gamma);
	presets.push_back(&seventeen);
	presets.push_back(&bohlenpierce);
	presets.push_back(&buchla296);
	presets.push_back(&userscale);

	full.push_back(&et_major);
	full.push_back(&et_minor);
	full.push_back(&et_intervals);
	full.push_back(&et_triads);
	full.push_back(&et_chromatic);
	full.push_back(&et_wholestep);
	full.push_back(&ji_intervals);
	full.push_back(&ji_triads);
	full.push_back(&ji_wholestep);
	full.push_back(&indian_penta);
	full.push_back(&indian_shrutis);
	full.push_back(&mesopotamian);
	full.push_back(&gamelan);
	full.push_back(&wc_alpha2);
	full.push_back(&wc_alpha1);
	full.push_back(&wc_gamma);
	full.push_back(&seventeen);
	full.push_back(&bohlenpierce);
	full.push_back(&buchla296);
	full.push_back(&userscale);
	full.push_back(&gamma_notused);
	full.push_back(&video_notused);

}
