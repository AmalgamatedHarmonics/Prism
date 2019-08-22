#pragma once

#include <iostream>
#include <bitset>

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

struct PrismKnobSnap : RoundKnob {
	PrismKnobSnap() {
		snap = true;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismKnob.svg")));
		shadow->opacity = 0.0f;
	}
};

struct PrismKnobNoSnap : RoundKnob {
	PrismKnobNoSnap() {
		snap = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismKnob.svg")));
		shadow->opacity = 0.0f;
	}
};

struct PrismLargeKnobNoSnap : RoundKnob {
	PrismLargeKnobNoSnap() {
		snap = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibraryPrismLargeKnob.svg")));
	}
};

struct PrismLargeKnobSnap : RoundKnob {
	PrismLargeKnobSnap() {
		snap = true;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibraryPrismLargeKnob.svg")));
	}
};

struct PrismTrimpotSnap : RoundKnob {
	PrismTrimpotSnap() {
		snap = true;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismTrimpot.svg")));
	}
};

struct PrismTrimpotNoSnap : RoundKnob {
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
		maxHandlePos = app::mm2px(math::Vec(0.0f, 0.0f).plus(math::Vec(1.0f, 0.0f)));
		minHandlePos = app::mm2px(math::Vec(0.0f, 22.078f).plus(math::Vec(1.0f, 0.0f)));
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSliderHandle.svg")));
	}
};

struct PrismLEDIndicator : LEDSlider {
	PrismLEDIndicator() {
		maxHandlePos = app::mm2px(math::Vec(0.0f, 0.0f).plus(math::Vec(1.0f, 0.0f)));
		minHandlePos = app::mm2px(math::Vec(0.0f, 22.078f).plus(math::Vec(1.0f, 0.0f)));
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismSliderNop.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismIndicator.svg")));
	}
};

struct PrismReadoutParam : app::ParamWidget {

	widget::FramebufferWidget *fb;
	widget::SvgWidget *sw;
	std::shared_ptr<Font> font;

	bool isActive = true;
	std::string title = "";

	PrismReadoutParam() {
		fb = new widget::FramebufferWidget;
		addChild(fb);

		sw = new widget::SvgWidget;
		fb->addChild(sw);

		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/BarlowCondensed-Bold.ttf"));
	}

	void setSvg(std::shared_ptr<Svg> svg) {
		sw->setSvg(svg);
		fb->box.size = sw->box.size;
		box.size = sw->box.size;
	}

	void onChange(const event::Change &e) override {
		if (paramQuantity) {
			fb->dirty = true;
		}
		ParamWidget::onChange(e);
	}

	void onHover(const event::Hover &e) override {
		math::Vec c = box.size.div(2);
		float dist = e.pos.minus(c).norm();
		if (dist <= c.x) {
			ParamWidget::onHover(e);
		}
	}

	void onButton(const event::Button &e) override {
		math::Vec c = box.size.div(2);
		float dist = e.pos.minus(c).norm();
		if (dist <= c.x) {
			ParamWidget::onButton(e);
		}
	}

	void reset() override {
		if (paramQuantity && paramQuantity->isBounded()) {
			paramQuantity->reset();
		}
	}

	void randomize() override {
		if (paramQuantity && paramQuantity->isBounded()) {
			float value = math::rescale(random::uniform(), 0.f, 1.f, paramQuantity->getMinValue(), paramQuantity->getMaxValue());
			paramQuantity->setValue(value);
		}
	}

	void draw(const DrawArgs &ctx) override {

		ParamWidget::draw(ctx);

		if (paramQuantity) {

			Vec pos = Vec(5, 15);

			nvgFontSize(ctx.vg, 17.0f);
			nvgFontFaceId(ctx.vg, font->handle);

			char text[128];
			snprintf(text, sizeof(text), "%.3f", paramQuantity->getValue());
			nvgText(ctx.vg, pos.x, pos.y, text, NULL);
		}
	}

};

struct FloatReadout : PrismReadoutParam {

	FloatReadout() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismFloatReadout.svg")));
	}

	void draw(const DrawArgs &ctx) override {

		ParamWidget::draw(ctx);

		if (paramQuantity) {

			Vec pos = Vec(5, 15);

			nvgFontSize(ctx.vg, 17.0f);
			nvgFontFaceId(ctx.vg, font->handle);

			char text[128];

			snprintf(text, sizeof(text), "%s", title.c_str());
			nvgText(ctx.vg, pos.x, pos.y, text, NULL);

			if (!isActive) {
				nvgFillColor(ctx.vg, nvgRGBA(0x80, 0x80, 0x80, 0xFF));
			}

			snprintf(text, sizeof(text), "%.3f", paramQuantity->getValue());
			nvgText(ctx.vg, pos.x, pos.y + 17, text, NULL);
		}
	}

};

struct IntegerReadout : PrismReadoutParam {

	IntegerReadout() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/PrismIntegerReadout.svg")));
	}

	void draw(const DrawArgs &ctx) override {

		ParamWidget::draw(ctx);

		if (paramQuantity) {

			Vec pos = Vec(5, 15);

			nvgFontSize(ctx.vg, 17.0f);
			nvgFontFaceId(ctx.vg, font->handle);

			char text[128];
			snprintf(text, sizeof(text), "%d", (int)paramQuantity->getValue());
			nvgText(ctx.vg, pos.x, pos.y, text, NULL);
		}
	}
	
};


} // namespace gui

} // namespace ah

inline math::Vec ink2vcv(float x, float y) {
	return Vec(x * 1.5f, (128.5f - y) * 1.5f);
}
