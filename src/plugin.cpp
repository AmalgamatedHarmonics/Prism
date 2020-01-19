#include "plugin.hpp"

// The plugin-wide instance of the Plugin class
Plugin *pluginInstance;

void init(rack::Plugin *p) {
	pluginInstance = p;

	// For each module, specify the ModuleWidget subclass, manufacturer slug (for saving in patches), manufacturer human-readable name, module slug, 
	// and module name
	p->addModel(modelRainbow);
	p->addModel(modelRainbowScaleExpander);

	p->addModel(modelDroplet);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
