/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file railnet.cpp Implementation of the railnet converter */

#include <cstring>
#include <map>
#include <iostream>
#include <stdexcept>
#include <getopt.h>
#include <cmath>
#include <cstdlib>

#include "common.h"
#include "railnet_node_list.h"
#include "railnet_options.h"

enum station_flag_t
{
	st_used = 1,
	st_passed = 2
};

void print_version()
{
	std::cerr << "version: <not specified yet>" << std::endl;
	std::cerr << "railnet file version: (currently not implemented)" << std::endl;
//		comm::railnet_file_info::version << std::endl;
}

void print_help()
{
	std::cerr << "railnet tool" << std::endl;
	std::cerr << "converts openttd railnet dumps into dot graphs" << std::endl;
	options::usage();
}

lbl_conv_t lbl_conv;

//! main routine of the railnet converter
int run(const options& opt)
{
	comm::railnet_file_info file;
//	deserialize(file, std::cin);
	comm::json_ifile(std::cin) >> file;
//	comm::json_ofile(std::cout) << file; // <- debugging only

	/*
		find out which cargo is used...
	*/
	std::set<int> cargo_ids;

	for(const auto& pr : file.cargo_names.get())
	{
		const char* str = lbl_conv.convert(pr.second);

		if(opt.command == options::cmd_list_cargo)
		 std::cout << str << std::endl;
		else if(strstr(opt.cargo.c_str(), str))
		 cargo_ids.insert(pr.second);
	}
	if(opt.command == options::cmd_list_cargo)
	 return 0;

	if(cargo_ids.size() != (opt.cargo.length()+1)/5)
	 throw "not all of your cargos are known";

	/*
		remove unwanted cargo from list
	*/
	{
	std::list<comm::order_list>::iterator itr, next;
	for(itr = next = file.order_lists.get().begin();
		itr != file.order_lists.get().end(); itr = next)
	{
		++next;
/*		bool cargo_found = false;
		for(std::map<cargo_label_t, comm::cargo_info>::const_iterator itr2 = itr->cargo.get().begin();
			!cargo_found && itr2 != itr->cargo.get().end(); ++itr2)
		 cargo_found = (cargo_ids.find(itr2->first) != cargo_ids.end());
		if(!cargo_found)
		 file.order_lists.get().erase(itr);*/
		std::map<cargo_label_t, comm::cargo_info>::const_iterator itr2, next2;
		for(itr2 = itr->cargo.get().begin();
			!cargo_found && itr2 != itr->cargo.get().end(); itr2 = next2)
		{
			++next2;
			if(cargo_ids.find(itr2->first) == cargo_ids.end())
			 itr->cargo().erase(itr2);
		}
		if(itr->cargo().empty())
		 file.order_lists().erase(itr);
	}
	}

	/*
		sort out subset or express trains
	*/
	node_list_t nl;
	for(const comm::order_list& ol : file.order_lists.get())
	 nl.init(ol);

	auto next = file.order_lists.get().begin();
	for(auto itr = file.order_lists.get().begin(); itr != file.order_lists.get().end(); itr = next)
	{
		++(next = itr);
		int type = nl.traverse(*itr, NULL, false, false);

		bool is_express = type & node_list_t::is_express_train,
			is_short = type & node_list_t::is_short_train;

		if((is_express && opt.hide_express)||(is_short && opt.hide_short_trains))
		{
			UnitID erased_no = itr->unit_number;
			std::cerr << "Erasing train: " << erased_no << ", reason: " <<
				((is_express && is_short)
					? "express + short"
					: is_express
						? "express"
						: "short")
				<< std::endl;
			file.order_lists.get().erase(itr);
		}
	}

	std::map<StationID, int> station_flags;
	const float passed_offset_x = 0.2, passed_offset_y = 0.2;

	/*
		find out which stations are actually being used...
	*/
	// only set flags
	for(std::list<comm::order_list>::const_iterator itr =
		file.order_lists.get().begin();
		itr != file.order_lists.get().end(); ++itr)
	if(itr->stations.get().size())
	{
		const comm::order_list& ol = *itr;
		for(std::vector<std::pair<StationID, bool> >::const_iterator
				itr = ol.stations.get().begin();
				itr != ol.stations.get().end(); ++itr)
		{
			station_flags[itr->first] |= itr->second ? st_used : st_passed;
		}
	}

	// this is required for the drawing algorithm
	for(std::list<comm::order_list>::const_iterator itr3 =
		file.order_lists.get().begin();
		itr3 != file.order_lists.get().end(); ++itr3)
	{
		// all in all, the whole for loop will not affect the order
		comm::order_list& ol = const_cast<comm::order_list&>(*itr3);
		ol.stations.get().push_back(ol.stations.get().front());
	}

	/*
		draw nodes
	*/
	std::cout <<	"digraph graphname\n" // FEATURE: get savegame name
		"{\n"
		"\tgraph[splines=line];\n"
		"\tnode[label=\"\", size=0.2, width=0.2, height=0.2];\n"
		"\tedge[penwidth=2];\n";

	// draw nodes if stations are used
	for(const auto& pr : file.stations.get())
	{
		std::map<StationID, int>::const_iterator itr = station_flags.find(pr.first);
		int flags = (itr == station_flags.end()) ? 0 : itr->second;
		{
			if(flags & st_used)
			std::cout << "\t" << pr.first << " [xlabel=\""
				<< pr.second.name.get() << "\", pos=\""
				<< (pr.second.x * opt.stretch)
				<< ", "
				<< (pr.second.y * opt.stretch)
				<< "!\"];" << std::endl;
			if(flags & st_passed)
			std::cout << "\tp" << pr.first << " [pos=\""
				<< ((pr.second.x-passed_offset_x) * opt.stretch)
				<< ", "
				<< ((pr.second.y-passed_offset_y) * opt.stretch)
				<< "!\" size=0.0, width=0.0, height=0.0];" << std::endl;
		}
	}

	float large_prime = 7919.0f;
	float large_prime_2 = 5417.0f;
	float order_list_step = ((float)large_prime) / file.order_lists.get().size();
	float order_list_step_2 = ((float)large_prime_2) / file.order_lists.get().size();
	float value = 0.0f, hue = 0.0f;
	std::cout.precision(3);

	/*
		draw edges
	*/
	for(std::list<comm::order_list>::const_iterator itr3 =
		file.order_lists.get().begin();
		itr3 != file.order_lists.get().end(); ++itr3)
	for(std::map<cargo_label_t, cargo_info>::const_iterator itr4 = itr3->cargo().begin();
		itr4 != itr3->cargo().end(); ++itr4)
	{
		hue = fmod(hue += order_list_step, 1.0f);
		value = fmod(value += order_list_step_2, 1.0f);

		if(itr3->stations.get().size())
		{
			const comm::order_list& ol = *itr3;
			const auto& cur_stations = ol.stations.get();
			bool only_double_edges = itr4->is_bicycle(); //ol.is_bicycle;
			// TODO: non-bicycles: iterate backwards?
			std::size_t double_edges = 0;
			std::size_t mid = cur_stations.size() >> 1;

			if(!only_double_edges && !(cur_stations.size() & 1))
			{
				for(std::size_t i = 1; i < mid; ++i)
					double_edges += (
						cur_stations[mid+i] == cur_stations[mid-i]);
				only_double_edges = (double_edges == mid - 1);
			}

			std::cout << "\t// order " << itr3->unit_number << " ("
				<< file.stations.get()[cur_stations[0].first].name.get() << " - "
				<< file.stations.get()[cur_stations[mid].first].name.get()
				<< ")" << std::endl;

#if 0
			std::cout << "\t// cargo: ";
			for(std::set<CargoID>::const_iterator itr2 = itr3->cargo.begin();
				itr2 != itr3->cargo.end(); ++itr2)
					std::cout << " " << +*itr2;
			std::cout << std::endl;
#endif
			for(std::vector<std::pair<StationID, bool> >::const_iterator
				itr = cur_stations.begin();
				itr != cur_stations.end(); ++itr)
			{
				std::cout
					<< ((itr == cur_stations.begin()) ? "\t// " : " - ")
					<< file.stations.get()[itr->first].name.get()
					<< (itr->second ? "" : "(p)");
			}
			std::cout << std::endl;

			auto print_end = [&](bool double_edge, const std::map<cargo_label_t, comm::cargo_info>& cargo)
			{
				// values below 50 get difficult to read
				float _value = 0.5f + (value/2.0f);
				std::cout << " [color=\"" << hue << ", 1.0, " << _value << "\"";
				if(double_edge)
				{
					std::cout << ", dir=none";
				}
				if(cargo_ids.size() > 1)
				{
					std::cout << ", label=\"";
					bool first = true;
					for(const std::pair<const CargoLabel, comm::cargo_info>& id : cargo)
					{
						std::cout << (first ? "" : ", ")
							<< lbl_conv.convert(id.first);
						first = false;
					}
					std::cout << "\"";
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
				itr = cur_stations.begin() + 1;
				itr != cur_stations.end(); ++itr)
			{
				const bool first = itr == cur_stations.begin();

				edge_type_t edge_type = edge_type_t::unique;

				if(itr4->is_bicycle())
					edge_type = edge_type_t::duplicate_first;
				else
				{
					for(std::vector<std::pair<StationID, bool>>::const_iterator
						itr2 = cur_stations.begin();
						!(int)edge_type && itr2 != itr; ++itr2)
						// itr2 < itr => itr2 + 1 is valid
						// itr == begin => for loop is not run => itr - 1 is valid
						if(*itr2 == *itr && (*(itr2 + 1)) == (*(itr - 1)) )
							edge_type = edge_type_t::duplicate_further;

					if(!first)
					for(std::vector<std::pair<StationID, bool>>::const_iterator
						itr2 = itr + 1;
						!(int)edge_type && itr2 != cur_stations.end(); ++itr2)
						// itr2 > itr => itr2 - 1 is valid
						// !first => itr - 1 is valid
						if(*(itr2 - 1) == *itr && *itr2 == (*(itr - 1)) )
						 edge_type = edge_type_t::duplicate_first;

					// both edge types
					// second for loop is skipped
					// => duplicate_further wins
				}

				if(last_edge_type != edge_type_t::duplicate_further && edge_type != last_edge_type)
				 print_end(last_edge_type == edge_type_t::duplicate_first, ol.cargo);

				if(edge_type != last_edge_type && edge_type != edge_type_t::duplicate_further)
				 print_station("\t", *(itr - 1));

				if(edge_type != edge_type_t::duplicate_further)
				 print_station(" -> ", *itr);

				if(file.stations.get().find(itr->first) == file.stations.get().end())
				{
					std::cerr << "Could not find station id " << itr->first << std::endl;
					throw std::runtime_error("invalid station id in order list");
				}

				last_edge_type = edge_type;
			}

			if(last_edge_type != edge_type_t::duplicate_further)
			 print_end(last_edge_type == edge_type_t::duplicate_first, ol.cargo);
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
		std::cerr << "Rest of file: ";
		char buf[1024];
		int count = 0;
		while(std::cin.getline(buf, 1024) && ++count < 8) {
			std::cerr << "* " << buf << std::endl;
		}
		std::cerr << "Exiting." << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
