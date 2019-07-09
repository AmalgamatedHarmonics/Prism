#!/usr/bin/env python3

import sys
import os
import re
import json
import xml.etree.ElementTree


# Version check
f"Python 3.6+ is required"


class UserException(Exception):
    pass


def find(f, array):
	for a in array:
		if f(a):
			return f

def input_default(prompt, default=""):
	str = input(f"{prompt} [{default}]: ")
	if str == "":
		return default
	return str


def is_valid_slug(slug):
	return re.match(r'^[a-zA-Z0-9_\-]+$', slug) != None


def slug_to_identifier(slug):
	if len(slug) == 0 or slug[0].isdigit():
		slug = "_" + slug
	slug = slug[0].upper() + slug[1:]
	slug = slug.replace('-', '_')
	return slug

def create_module(slug, panel_filename=None, source_filename=None):
	# Check slug
	if not is_valid_slug(slug):
		raise UserException("Slug must only contain ASCII letters, numbers, '-', and '_'.")

	# Check filenames
	if panel_filename and source_filename:
		if not os.path.exists(panel_filename):
			raise UserException(f"Panel not found at {panel_filename}.")

		print(f"Panel found at {panel_filename}. Generating source file.")

		if os.path.exists(source_filename):
			if input_default(f"{source_filename} already exists. Overwrite?", "n").lower() != "y":
				return

		# Read SVG XML
		tree = xml.etree.ElementTree.parse(panel_filename)

		components = panel_to_components(tree)
		print(f"Components extracted from {panel_filename}")

		# Write source
		source = components_to_source(components, slug)

		with open(source_filename, "w") as f:
			f.write(source)
		print(f"Source file generated at {source_filename}")


def panel_to_components(tree):
	ns = {
		"svg": "http://www.w3.org/2000/svg",
		"inkscape": "http://www.inkscape.org/namespaces/inkscape",
	}

	# Get components layer
	root = tree.getroot()
	groups = root.findall(".//svg:g[@inkscape:label='components']", ns)
	if len(groups) < 1:
		raise UserException("Could not find \"components\" layer on panel")

	# Get circles and rects
	components_group = groups[0]
	circles = components_group.findall(".//svg:circle", ns)
	rects = components_group.findall(".//svg:rect", ns)

	components = {}
	components['params'] = []
	components['inputs'] = []
	components['outputs'] = []
	components['lights'] = []
	components['widgets'] = []

	for el in circles + rects:

		c = {}
		# Get name
		name = el.get('{http://www.inkscape.org/namespaces/inkscape}label')
		if name is None:
			name = el.get('id')
		name = slug_to_identifier(name)
		c['name'] = name

		# Get color
		style = el.get('style')
		color_match = re.search(r'fill:\S*#(.{6});', style)
		color = color_match.group(1)
		c['color'] = color

		# Get position
		if el.tag == "{http://www.w3.org/2000/svg}rect":
			x = float(el.get('x'))
			y = float(el.get('y'))
			width = float(el.get('width'))
			height = float(el.get('height'))
			c['x'] = round(x, 3)
			c['y'] = round(y, 3)
			c['width'] = round(width, 3)
			c['height'] = round(height, 3)
			c['cx'] = round(x + width / 2, 3)
			c['cy'] = round(y + height / 2, 3)
		elif el.tag == "{http://www.w3.org/2000/svg}circle":
			cx = float(el.get('cx'))
			cy = float(el.get('cy'))
			c['cx'] = round(cx, 3)
			c['cy'] = round(cy, 3)

		if color == 'ff0000':
			components['params'].append(c)
		if color == '00ff00':
			components['inputs'].append(c)
		if color == '0000ff':
			components['outputs'].append(c)
		if color == 'ff00ff':
			components['lights'].append(c)
		if color == 'ffff00':
			components['widgets'].append(c)

	# Sort components
	top_left_sort = lambda w: (w['cy'], w['cx'])
	components['params'] = sorted(components['params'], key=top_left_sort)
	components['inputs'] = sorted(components['inputs'], key=top_left_sort)
	components['outputs'] = sorted(components['outputs'], key=top_left_sort)
	components['lights'] = sorted(components['lights'], key=top_left_sort)
	components['widgets'] = sorted(components['widgets'], key=top_left_sort)

	print(f"Found {len(components['params'])} params, {len(components['inputs'])} inputs, {len(components['outputs'])} outputs, {len(components['lights'])} lights, and {len(components['widgets'])} custom widgets.")
	return components


def components_to_source(components, slug):
	identifier = slug_to_identifier(slug)
	source = ""

	# Params
	if len(components['params']) > 0:
		source += "\n"
	for c in components['params']:
		vars = c['name'].split(',', 2)
		if len(vars) == 1:
			label = vars[0]
			ui = 'RoundBlackKnob'
		else:
			label = vars[0]
			ui = vars[1]
		print(ui)
		if 'x' in c:
			source += f"""
		addParam(createParam<{ui}>(mm2px(Vec({c['x']}, {c['y']})), module, {identifier}::{label}));"""
		else:
			source += f"""
		addParam(createParamCentered<{ui}>(mm2px(Vec({c['cx']}, {c['cy']})), module, {identifier}::{label}));"""

	# Inputs
	if len(components['inputs']) > 0:
		source += "\n"
	for c in components['inputs']:
		if 'x' in c:
			source += f"""
		addInput(createInput<PJ301MPort>(mm2px(Vec({c['x']}, {c['y']})), module, {identifier}::{c['name']}));"""
		else:
			source += f"""
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec({c['cx']}, {c['cy']})), module, {identifier}::{c['name']}));"""

	# Outputs
	if len(components['outputs']) > 0:
		source += "\n"
	for c in components['outputs']:
		if 'x' in c:
			source += f"""
		addOutput(createOutput<PJ301MPort>(mm2px(Vec({c['x']}, {c['y']})), module, {identifier}::{c['name']}));"""
		else:
			source += f"""
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec({c['cx']}, {c['cy']})), module, {identifier}::{c['name']}));"""

	# Lights
	if len(components['lights']) > 0:
		source += "\n"
	for c in components['lights']:
		if 'x' in c:
			source += f"""
		addChild(createLight<MediumLight<RedLight>>(mm2px(Vec({c['x']}, {c['y']})), module, {identifier}::{c['name']}));"""
		else:
			source += f"""
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec({c['cx']}, {c['cy']})), module, {identifier}::{c['name']}));"""

	# Widgets
	if len(components['widgets']) > 0:
		source += "\n"
	for c in components['widgets']:
		if 'x' in c:
			source += f"""
		// mm2px(Vec({c['width']}, {c['height']}))
		addChild(createWidget<Widget>(mm2px(Vec({c['x']}, {c['y']}))));"""
		else:
			source += f"""
		addChild(createWidgetCentered<Widget>(mm2px(Vec({c['cx']}, {c['cy']}))));"""

	return source


def usage(script):
	text = f"""VCV Rack Plugin Helper Utility

Usage: {script} <command> ...
Commands:

createplugin <slug> [plugin dir]

	A directory will be created and initialized with a minimal plugin template.
	If no plugin directory is given, the slug is used.

createmanifest <slug> [plugin dir]

	Creates a `plugin.json` manifest file in an existing plugin directory.
	If no plugin directory is given, the current directory is used.

createmodule <module slug> [panel file] [source file]

	Adds a new module to the plugin manifest in the current directory.
	If a panel and source file are given, generates a template source file initialized with components from a panel file.
	Example:
		{script} createmodule MyModule res/MyModule.svg src/MyModule.cpp

	See https://vcvrack.com/manual/PanelTutorial.html for creating SVG panel files.
"""
	print(text)


def parse_args(args):
	script = args.pop(0)
	if len(args) == 0:
		usage(script)
		return

	cmd = args.pop(0)
	if cmd == 'createplugin':
		create_plugin(*args)
	elif cmd == 'createmodule':
		create_module(*args)
	elif cmd == 'createmanifest':
		create_manifest(*args)
	else:
		print(f"Command not found: {cmd}")


if __name__ == "__main__":
	try:
		parse_args(sys.argv)
	except KeyboardInterrupt:
		pass
	except UserException as e:
		print(e)
		sys.exit(1)
