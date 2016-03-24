#ifndef MKGRAPH_OPTIONS_H
#define MKGRAPH_OPTIONS_H

#include <string>

struct options
{
	enum command_t
	{
		cmd_print_version,
		cmd_print_help,
		cmd_list_cargo,
		cmd_create_graph
	} command = cmd_create_graph;

	std::string cargo;
	bool hide_express = false,
		hide_short_trains = false;

	options(int arg, char** argv);
	static void usage();
};

#endif // MKGRAPH_OPTIONS_H

