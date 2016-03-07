/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#include <map>
#include <iostream>
#include <stdexcept>
#include <getopt.h>
#include <cmath>
#include <cstdlib>

#include "common.h"
#include "mkgraph_options.h"

enum station_flag_t
{
	st_used = 1,
	st_passed = 2
};

void print_version()
{
	std::cerr << "version: <not specified yet>" << std::endl;
	std::cerr << "railnet file version: " << railnet_file_info::version << std::endl;
}

void print_help()
{
	std::cerr << "mkgraph tool" << std::endl;
	std::cerr << "converts openttd railnet dumps into dot graphs" << std::endl;
	options::usage();
}
#include <cstring> // TODO
#include <algorithm>
int run(const options& opt)
{
	railnet_file_info file;
	deserialize(file, std::cin);

	std::set<int> cargo_ids;

	{
		union
		{
			int lbl[2];
			char str[8];
		} lbl_to_str;
		lbl_to_str.lbl[1] = 0;

		for(const auto& pr : file.cargo_names)
		{
			lbl_to_str.lbl[0] = pr.second;
			std::reverse(lbl_to_str.str, lbl_to_str.str + 4);

			if(opt.command == options::cmd_list_cargo)
			 std::cout << lbl_to_str.str << std::endl;
			else if(strstr(opt.cargo.c_str(), lbl_to_str.str))
			 cargo_ids.insert(pr.second);
		}
		if(opt.command == options::cmd_list_cargo)
		 return 0;
	}

	if(cargo_ids.size() != (opt.cargo.length()+1)/5)
	 throw "not all of your cargos are known";

	std::map<StationID, int> station_flags;

	std::cout <<	"digraph graphname\n" // TODO: get savegame name
		"{\n"
		"\tgraph[splines=line];\n"
		"\tnode[label=\"\", size=0.2, width=0.2, height=0.2];\n"
		"\tedge[penwidth=2];\n";

	// find out which stations are actually being used...

	// only set flags
	for(std::multiset<order_list>::iterator itr = file.order_lists.begin();
		itr != file.order_lists.end(); ++itr)
	{
		{
			bool cargo_found = false;
			for(std::set<CargoLabel>::const_iterator itr2 = itr->cargo.begin();
				!cargo_found && itr2 != itr->cargo.end(); ++itr2)
			 cargo_found = (cargo_ids.find(*itr2) != cargo_ids.end());
			if(!cargo_found)
			 continue;
		}

		if(itr->stations.size())
		{
			order_list& ol = const_cast<order_list&>(*itr); // TODO: no const (see for loop)
			for(std::vector<std::pair<StationID, bool> >::const_iterator
					itr = ol.stations.begin();
					itr != ol.stations.end(); ++itr)
			{
				station_flags[itr->first] |= itr->second ? st_used : st_passed;
			}
		}

	}

	// draw nodes if stations are used
	for(const auto& pr : file.stations)
	{
		std::map<StationID, int>::const_iterator itr = station_flags.find(pr.first);
		int flags = (itr == station_flags.end()) ? 0 : itr->second;
		{
			if(flags & st_used)
			std::cout << "\t" << pr.first << " [xlabel=\""
				<< pr.second.name << "\", pos=\""
				<< pr.second.x
				<< ", "
				<< pr.second.y
				<< "!\"];" << std::endl;
			if(flags & st_passed)
			std::cout << "\tp" << pr.first << " [pos=\""
				<< pr.second.x-0.2f
				<< ", "
				<< pr.second.y-0.2f
				<< "!\" size=0.0, width=0.0, height=0.0];" << std::endl;
		}
	}

	float large_prime = 7919.0f; // algebra ftw!
	float large_prime_2 = 5417.0f;
	float order_list_step = ((float)large_prime) / file.order_lists.size();
	float order_list_step_2 = ((float)large_prime_2) / file.order_lists.size();
	float value = 0.0f, hue = 0.0f;
	std::cout.precision(3);

	// TODO: const_iterator and container that visits begin() before end()
	for(std::multiset<order_list>::iterator itr3 = file.order_lists.begin();
		itr3 != file.order_lists.end(); ++itr3)
	{
		{
			bool cargo_found = false;
	/*		std::cerr << "cargo: ";
			for(std::set<CargoID>::const_iterator itr2 = itr3->cargo.begin();
				itr2 != itr3->cargo.end(); ++itr2)
					std::cerr << " " << +*itr2;
			std::cerr << std::endl;*/

			for(std::set<CargoLabel>::const_iterator itr2 = itr3->cargo.begin();
				!cargo_found && itr2 != itr3->cargo.end(); ++itr2)
			 cargo_found = (cargo_ids.find(*itr2) != cargo_ids.end());

			std::cerr << "order: " << itr3->unit_number << std::endl;
			if(!cargo_found)
			 std::cerr << "not found" << std::endl;
			if(!cargo_found)
			 continue;
		}

		hue = fmod(hue += order_list_step, 1.0f);
		value = fmod(value += order_list_step_2, 1.0f);

		if(itr3->stations.size())
		{
			order_list& ol = const_cast<order_list&>(*itr3); // TODO: no const (see for loop)
			bool only_double_edges = ol.is_bicycle;
			std::size_t double_edges = 0;
			std::size_t mid = ol.stations.size() >> 1;

			if(!only_double_edges && !(ol.stations.size() & 1))
			{
				for(std::size_t i = 1; i < mid; ++i)
					double_edges += (
						ol.stations[mid+i] == ol.stations[mid-i]);
				only_double_edges = (double_edges == mid - 1);
			}

			std::cerr << "only double edges? " << only_double_edges << std::endl;

			// make printing easier:
			if(ol.stations.front().first != ol.stations.back().first)
			{
				ol.stations.push_back(ol.stations.front());
			}

			std::cout << "\t// order " << itr3->unit_number << " ("
				<< file.stations[ol.stations[0].first].name << " - "
				<< file.stations[ol.stations[mid].first].name
				<< ")" << std::endl;

#if 0
			std::cout << "\t// cargo: ";
			for(std::set<CargoID>::const_iterator itr2 = itr3->cargo.begin();
				itr2 != itr3->cargo.end(); ++itr2)
					std::cout << " " << +*itr2;
			std::cout << std::endl;
#endif
			std::cout << "\t//";
			for(std::vector<std::pair<StationID, bool> >::const_iterator
				itr = ol.stations.begin();
				itr != ol.stations.end(); ++itr)
			{
				std::cout << " - " << file.stations[itr->first].name <<
					(itr->second ? "" : "(p)");
			}
			//std::cout << " " << *itr;
			std::cout << std::endl;

			auto print_end = [&](bool double_edge)
			{
				// values below 50 get difficult to read
				float _value = 0.5f + (value/2.0f);
				std::cout << " [color=\"" << hue << ", 1.0, " << _value << "\"";
				if(double_edge)
				{
					std::cout << ", dir=none";
				}
				std::cout << "];" << std::endl;
			};

			auto print_station = [&](const char* before, const std::pair<StationID, bool>& pr)
			{
				std::cout << before << (pr.second ? "" : "p") << pr.first;
			};

			enum class edge_type_t
			{
				unique,
				duplicate_first,
				duplicate_further
			};
			edge_type_t last_edge_type = edge_type_t::duplicate_further;

			for(std::vector<std::pair<StationID, bool>>::const_iterator
				itr = ol.stations.begin() + 1;
				itr != ol.stations.end(); ++itr)
			{
				const bool first = itr == ol.stations.begin();

				edge_type_t edge_type = edge_type_t::unique;

				for(std::vector<std::pair<StationID, bool>>::const_iterator
					itr2 = ol.stations.begin();
					!(int)edge_type && itr2 != itr; ++itr2)
					// itr2 < itr => itr2 + 1 is valid
					// itr == begin => for loop is not run => itr - 1 is valid
					if(*itr2 == *itr && (*(itr2 + 1)) == (*(itr - 1)) )
						edge_type = edge_type_t::duplicate_further;

				if(!first)
				for(std::vector<std::pair<StationID, bool>>::const_iterator
					itr2 = itr + 1;
					!(int)edge_type && itr2 != ol.stations.end(); ++itr2)
					// itr2 > itr => itr2 - 1 is valid
					// !first => itr - 1 is valid
					if(*(itr2 - 1) == *itr && *itr2 == (*(itr - 1)) )
					 edge_type = edge_type_t::duplicate_first;

				// both edge types
				// second for loop is skipped
				// => duplicate_further wins

				if(last_edge_type != edge_type_t::duplicate_further && edge_type != last_edge_type)
				 print_end(last_edge_type == edge_type_t::duplicate_first);

				if(edge_type != last_edge_type && edge_type != edge_type_t::duplicate_further)
				 print_station("\t", *(itr - 1));

				if(edge_type != edge_type_t::duplicate_further)
				 print_station(" -> ", *itr);

				if(file.stations.find(itr->first) == file.stations.end())
				{
					std::cerr << "Could not find station id " << itr->first << std::endl;
					throw std::runtime_error("invalid station id in order list");
				}

				last_edge_type = edge_type;
			}

			if(last_edge_type != edge_type_t::duplicate_further)
			 print_end(last_edge_type == edge_type_t::duplicate_first);
		}
	}

	std::cout << '}' << std::endl;

	return 0;
}

int main(int argc, char** argv)
{
	try
	{
		setlocale(LC_ALL, "en_US.UTF-8");
		options opt(argc, argv);
		switch(opt.command)
		{
			case options::cmd_print_help:
				print_help();
				break;
			case options::cmd_print_version:
				print_version();
				break;
			default:
				run(opt);
		}
	} catch(const char* s)
	{
		std::cerr << "Error: " << s << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
