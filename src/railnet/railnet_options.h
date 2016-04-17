/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file railnet_options.h Struct defining the @a options class */

#ifndef MKGRAPH_OPTIONS_H
#define MKGRAPH_OPTIONS_H

#include <string>

//! struct holding options (e.g. commandline parameters) for the
//! railnet converter
struct options
{
	enum command_t
	{
		cmd_print_version,
		cmd_print_help,
		cmd_list_cargo,
		cmd_create_graph
	} command = cmd_create_graph;

	std::string cargo; //! csv-string for cargo filter
	bool hide_express = false,
		hide_short_trains = false;
	float stretch = 1.0f; //! stretch factor for the map

	options(int arg, char** argv);
	static void usage();
};

#endif // MKGRAPH_OPTIONS_H

