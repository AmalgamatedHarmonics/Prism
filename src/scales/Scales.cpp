#include "Scales.hpp"

std::vector<Scale> buildScale() {
	std::vector<Scale> s;
	s.push_back(major);
	s.push_back(minor);
	s.push_back(et_intervals);
	s.push_back(et_triads);
	s.push_back(et_chromatic);
	s.push_back(et_wholestep);
	s.push_back(ji_intervals);
	s.push_back(ji_triads);
	s.push_back(ji_wholestep);
	s.push_back(indian_penta);
	s.push_back(indian_shrutis);
	s.push_back(mesopotamian);
	s.push_back(gamelan);
	s.push_back(wc_alpha2);
	s.push_back(wc_alpha1);
	s.push_back(wc_gamma);
	s.push_back(seventeen);
	s.push_back(bp);
	s.push_back(buchla296);
	s.push_back(userscale);
	s.push_back(gamma_notused);
	s.push_back(video_notused);
	return s;
}
