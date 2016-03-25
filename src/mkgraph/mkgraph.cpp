/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstring>
#include <map>
#include <iostream>
#include <stdexcept>
#include <getopt.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

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

union lbl_to_str_t
{
	int lbl[2];
	char str[8];
	lbl_to_str_t() : lbl{0,0} {}
	const char* convert(int value)
	{
		lbl[0] = value;
		std::reverse(str, str + 4);
		return str;
	}
} lbl_to_str;

struct node_list_t
{
	using times_t = unsigned short;
	using node_info_t = std::multimap<UnitID, times_t>;

	//! @node: C++11 guarantees that equivalent keys remain
	//! in the insertion order. we insert them sorted by key...
	std::map<StationID, node_info_t> nodes;
	std::map<UnitID, times_t> lengths;
	std::map<UnitID, std::set<CargoLabel>> cargo;

private:
	void visit(UnitID u, StationID s, times_t nth) {
		nodes[s].emplace(u, nth);
	}
public:
	void init_nodes(const order_list& ol)
	{
		std::size_t nth = 0;
		UnitID unit_no = ol.unit_number;
		for(const auto& pr : ol.stations)
		 if(pr.second) /* if train stops */
		  visit(unit_no, pr.first, nth++);
		lengths[unit_no] = nth;
		for(const auto& c : ol.cargo)
		 cargo[unit_no].insert(c);
	}

	enum direction_t
	{
		dir_upwards,
		dir_downwards,
		dir_none
	};

	struct super_info_t
	{
		times_t station_no;
		direction_t upwards;
		std::size_t last_station_no;
		bool can_be_short_train;
	};

	std::size_t value_of(const std::multimap<UnitID, super_info_t>& supersets,
		const UnitID& train, const times_t& value) const
	{
		std::size_t length = lengths.at(train);
		const super_info_t& sinf = supersets.find(train)->second;

		// actually, it's just (value - pr.first % length),
		// but lenght is added to avoid % on negative numbers
		return ((length + value) - sinf.station_no) % length;
	}

	enum superset_type
	{
		no_supersets = 0,
		is_express_train = 1,
		is_short_train = 2,
		is_express_and_short_train = 4
	};

	void follow_to_node(const std::vector<StationID>::const_iterator& itr,
		std::multimap<UnitID, super_info_t>& supersets,
		const std::vector<StationID>::const_iterator& itr_1,
		UnitID train) const
	{
		(void)train;

		const node_info_t& info = nodes.at(*itr);

		auto next = supersets.begin();
		for(auto sitr = supersets.begin(); sitr != supersets.end(); sitr = next)
		{
			++(next = sitr);

			UnitID cur_line = sitr->first;
			std::size_t& last_station_no = sitr->second.last_station_no;
			std::size_t new_last_station_no = std::numeric_limits<std::size_t>::max();

			const auto in_nodes = info.equal_range(cur_line);
			std::size_t value_of_last_station = last_station_no; // TODO: useless variable
			std::size_t tmp = lengths.at(cur_line);
			for(auto in_node = in_nodes.first;
				in_node != in_nodes.second; ++in_node)
			{
				tmp = value_of(supersets, cur_line, in_node->second);
				if(tmp == 0)
				 tmp = lengths.at(cur_line);
				if(tmp >= value_of_last_station && // is it a future node?
					tmp < new_last_station_no /* better than previous result? */ )
				{
					new_last_station_no = tmp;
				}
			}

			if(new_last_station_no == std::numeric_limits<std::size_t>::max())
			 supersets.erase(sitr);
			else
			{
				// was the last node visited twice before we got here?
				// if so, this train can still be a short train
				const auto& last_in_nodes =
					nodes.at(*itr_1).equal_range(cur_line);

				bool no_express = true;
				// TODO: < value_of(new_last...)?
				if(value_of_last_station < new_last_station_no - 1) // some nodes between...
				{
					no_express = false;
					for(auto last_in_node = last_in_nodes.first;
						(!no_express) && last_in_node != last_in_nodes.second; ++last_in_node)
					{
						std::size_t value_found_in_last = value_of(supersets, cur_line, last_in_node->second);
						if(value_of_last_station < value_found_in_last
							// TODO: < value_of(new_last...)?
							&& value_found_in_last < new_last_station_no)
						{
							// ok, it was just a slope we've left out
							no_express = true;
						}
					}
				}

				sitr->second.can_be_short_train =
					sitr->second.can_be_short_train && no_express;

				// advance
				last_station_no = new_last_station_no;
			}
		}
	}

	int traverse(const order_list& ol) const
	{
		std::vector<StationID> stations;
		for(const auto& pr : ol.stations)
		 if(pr.second)
		  stations.push_back(pr.first);

		UnitID train = ol.unit_number;

		enum class masks
		{
			m_same = 1,
			m_opposite = 2,
			m_remove = 4,
			m_two_directions = 3
		};

		// times_t: offset, char: direction mask
		std::multimap<UnitID, super_info_t> supersets;

		// find first node where the train stops
		const node_info_t *station_0 = &nodes.at(stations[0]);

		// fill map for the first node
		for(const auto& pr : *station_0)
		if(pr.first != train)
		{
			const auto& c_oth = cargo.at(pr.first);
			const auto& c_this = cargo.at(train);
			if(std::includes(c_oth.begin(), c_oth.end(), c_this.begin(), c_this.end()))
			 supersets.emplace(pr.first, super_info_t { pr.second, dir_none, 0, true });
		}

		for(const auto& pr : *station_0)
		if(pr.first != train)
		{
			const auto& c_oth = cargo.at(pr.first);
			const auto& c_this = cargo.at(train);
			if(std::includes(c_oth.begin(), c_oth.end(), c_this.begin(), c_this.end()))
			if(value_of(supersets, pr.first, station_0->find(pr.first)->second) != 0)
			 throw "Internal error: invalid value computation";
		}

		// follow the order list, find monotonically increasing superset line
		for(auto itr = stations.begin() + 1; itr != stations.end(); ++itr)
		 follow_to_node(itr, supersets, itr-1, train);
		// last node: (end-1) -> (begin)
		// follow_to_node(stations.begin(), supersets, stations.end() - 1, train);
		// TODO: short or express for last part...


		int result = no_supersets;
		for(const auto& pr : supersets)
		{
			std::cerr << "train " << train
				<< ", other: " << pr.first
				<< " -> short? " << pr.second.can_be_short_train << std::endl;
			result |= (pr.second.can_be_short_train
				? is_short_train : is_express_train);
		}

		return result;
	}
};

int run(const options& opt)
{
	railnet_file_info file;
	deserialize(file, std::cin);

	/*
		find out which cargo is used...
	*/
	std::set<int> cargo_ids;

	for(const auto& pr : file.cargo_names)
	{
		const char* str = lbl_to_str.convert(pr.second);

		if(opt.command == options::cmd_list_cargo)
		 std::cout << str << std::endl;
		else if(strstr(opt.cargo.c_str(), str))
		 cargo_ids.insert(pr.second);
	}
	if(opt.command == options::cmd_list_cargo)
	 return 0;

	if(cargo_ids.size() != (opt.cargo.length()+1)/5)
	 throw "not all of your cargos are known";

	std::map<StationID, int> station_flags;

	// TODO: remove wrong cargo from order list already here!


	/*
		sort out subset or express trains
	*/
	node_list_t nl;
	for(const order_list& ol : file.order_lists)
	 nl.init_nodes(ol);

	auto next = file.order_lists.begin();
	for(auto itr = file.order_lists.begin(); itr != file.order_lists.end(); itr = next)
	{
		++(next = itr);
		int type = nl.traverse(*itr);

		if(((type & node_list_t::is_express_train) && opt.hide_express)
			|| ((type & node_list_t::is_short_train) && opt.hide_short_trains) )
		{
			UnitID erased_no = itr->unit_number;
			file.order_lists.erase(itr);
			std::cerr << "Would erase: " << erased_no << ", reason: " << type << std::endl;
		}
	}

	/*
		find out which stations are actually being used...
	*/
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

	// this is required for the drawing algorithm
	for(std::multiset<order_list>::const_iterator itr3 = file.order_lists.begin();
		itr3 != file.order_lists.end(); ++itr3)
	{
		// all in all, the whole for loop will not affect the order
		order_list& ol = const_cast<order_list&>(*itr3);
		ol.stations.push_back(ol.stations.front());
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

	/*
		draw edges
	*/
	// TODO: container that visits begin() before end()
	for(std::multiset<order_list>::const_iterator itr3 = file.order_lists.begin();
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
			const order_list& ol = *itr3; // TODO: no const (see for loop)
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
			for(std::vector<std::pair<StationID, bool> >::const_iterator
				itr = ol.stations.begin();
				itr != ol.stations.end(); ++itr)
			{
				std::cout
					<< ((itr == ol.stations.begin()) ? "\t// " : " - ")
					<< file.stations[itr->first].name
					<< (itr->second ? "" : "(p)");
			}
			std::cout << std::endl;

			auto print_end = [&](bool double_edge, std::size_t i, const std::set<CargoLabel>& cargo)
			{
				// values below 50 get difficult to read
				float _value = 0.5f + (value/2.0f);
				std::cout << " [color=\"" << hue << ", 1.0, " << _value << "\"";
				if(double_edge)
				{
					std::cout << ", dir=none";
				}
				std::cerr << i << std::endl;
				if(cargo_ids.size() > 1)
				{
					std::cout << ", label=\"";
					bool first = true;
					for(const int& id : cargo)
					{
						std::cout << (first ? "" : ", ")
							<< lbl_to_str.convert(id);
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

			std::size_t count = 0;
			for(std::vector<std::pair<StationID, bool>>::const_iterator
				itr = ol.stations.begin() + 1;
				itr != ol.stations.end(); ++itr, ++count)
			{
				const bool first = itr == ol.stations.begin();

				edge_type_t edge_type = edge_type_t::unique;

				if(ol.is_bicycle)
					edge_type = edge_type_t::duplicate_first;
				else
				{
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
				}

				if(last_edge_type != edge_type_t::duplicate_further && edge_type != last_edge_type)
				 print_end(last_edge_type == edge_type_t::duplicate_first, count, ol.cargo);

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
			 print_end(last_edge_type == edge_type_t::duplicate_first, count, ol.cargo);
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
