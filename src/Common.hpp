#pragma once

#include <iostream>

#include "plugin.hpp"
#include "componentlibrary.hpp"

namespace prism {

namespace core {

const double PI = 3.14159265358979323846264338327950288;

struct PrismModule : rack::Module {

	PrismModule(int numParams, int numInputs, int numOutputs, int numLights = 0) {
		config(numParams, numInputs, numOutputs, numLights);
	}

	int stepX = 0;
	
	bool debugFlag = false;
	
	inline bool debugEnabled() {
		return debugFlag;
	}

	inline bool debugEnabled(int poll) {
		if (debugFlag && stepX % poll == 0) {
			return true;
		} else {
			return false;
		}
	}

	void step() override {
		stepX++;
	}
	
};

} // namespace core

namespace gui {

// Not going to monitor buttons
struct PrismButton : SVGSwitch {
	PrismButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismButton_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismButton_1.svg")));
	}	
};

struct PrismLargeButton : SVGSwitch {
	PrismLargeButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismLargeButton_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismLargeButton_1.svg")));
	}	
};


struct PrismKnob : RoundKnob { };

struct PrismKnobSnap : PrismKnob {
	PrismKnobSnap() {
		snap = true;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismKnob.svg")));
		shadow->opacity = 0.0f;
	}
};

struct PrismKnobNoSnap : PrismKnob {
	PrismKnobNoSnap() {
		snap = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismKnob.svg")));
		shadow->opacity = 0.0f;
	}
};

struct PrismLargeKnobNoSnap : PrismKnob {
	PrismLargeKnobNoSnap() {
		snap = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibraryPrismLargeKnob.svg")));
	}
};

struct PrismLargeKnobSnap : PrismKnob {
	PrismLargeKnobSnap() {
		snap = true;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibraryPrismLargeKnob.svg")));
	}
};

struct PrismTrimpotSnap : PrismKnob {
	PrismTrimpotSnap() {
		snap = true;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismTrimpot.svg")));
	}
};

struct PrismTrimpotNoSnap : PrismKnob {
	PrismTrimpotNoSnap() {
		snap = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismTrimpot.svg")));
	}
};

struct PrismSSwitch : app::SvgSwitch {
        PrismSSwitch() {
                addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSSwitch_0.svg")));
                addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSSwitch_2.svg")));
        }
};

struct PrismSSwitch3 : app::SvgSwitch {
        PrismSSwitch3 () {
                addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSSwitch_0.svg")));
                addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSSwitch_1.svg")));
                addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSSwitch_2.svg")));
        }
};

struct PrismLEDSlider : LEDSlider {
	PrismLEDSlider() {
		maxHandlePos = app::mm2px(math::Vec(0.0, 0.0).plus(math::Vec(1, 0)));
		minHandlePos = app::mm2px(math::Vec(0.0, 22.078).plus(math::Vec(1, 0)));
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSliderHandle.svg")));
	}
};

} // namespace gui

} // namespace ah