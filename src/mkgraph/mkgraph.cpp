#include <map>
#include <iostream>
#include <stdexcept>
#include <getopt.h>
#include <cmath>
#include "common.h"

int main(int argc, char** argv)
{
	railnet_file_info file;
	deserialize(file, std::cin);

	std::cout <<	"digraph graphname\n" // TODO: get savegame name
		"{\n"
		"\tgraph[splines=line];\n"
		"\tnode[label=\"\", shape=none, size=0.1, width=1.0, height=0.1];\n"
		"\tedge[penwidth=2];\n";

	for(const auto& pr : file.stations)
	{
		std::cout << "\t" << pr.first << " [xlabel=\""
			<< pr.second.name << "\", pos=\""
			<< pr.second.x
			<< ", "
			<< pr.second.y
			<< "!\"];" << std::endl;
	}

	float large_prime = 7919.0f; // algebra ftw!
	float large_prime_2 = 5417.0f;
	float order_list_step = ((float)large_prime) / file.order_lists.size();
	float order_list_step_2 = ((float)large_prime_2) / file.order_lists.size();
	float value = 0.0f, hue = 0.0f;
	std::cout.precision(3);

	std::set<CargoID> used_cargo_ids = { 0 };

	// TODO: const_iterator and container that visits begin() before end()
	for(std::multiset<order_list>::iterator itr = file.order_lists.begin();
		itr != file.order_lists.end(); ++itr)
	{
		{
			bool cargo_found = false;
			std::cerr << "cargo: ";
			for(std::set<CargoID>::const_iterator itr2 = itr->cargo.begin();
				itr2 != itr->cargo.end(); ++itr2)
					std::cerr << " " << +*itr2;
			std::cerr << std::endl;

			for(std::set<CargoID>::const_iterator itr2 = itr->cargo.begin();
				!cargo_found && itr2 != itr->cargo.end(); ++itr2)
			 cargo_found = (used_cargo_ids.find(*itr2) != used_cargo_ids.end());
			std::cerr << "order: " << itr->unit_number << std::endl;
			if(!cargo_found)
			 std::cerr << "not found" << std::endl;
			if(!cargo_found)
			 continue;
		}

		hue = fmod(hue += order_list_step, 1.0f);
		value = fmod(value += order_list_step_2, 1.0f);

		if(itr->stations.size())
		{
			order_list& ol = const_cast<order_list&>(*itr); // TODO: no const (see for loop)
			std::vector<StationID>::const_iterator recent;
			bool only_double_edges = ol.is_bicycle;
			std::size_t double_edges = 0;
			std::size_t mid = ol.stations.size() >> 1;

			for(std::vector<StationID>::const_iterator itr = ol.stations.begin();
				itr != ol.stations.end(); ++itr)
			{
				if(*itr == 502)
				 std::cerr << "502" << std::endl;
			}

			if(!only_double_edges && !(ol.stations.size() & 1))
			{
				for(std::size_t i = 1; i < mid; ++i) {
					double_edges += (
						ol.stations[mid+i] == ol.stations[mid-i]);
					if(ol.stations[mid+i] == 502 || ol.stations[mid-i] == 502)
					{
						std::cerr << "502" << std::endl;
					}
				}
				only_double_edges = (double_edges == mid - 1);
			}

			std::cerr << "only double edges? " << only_double_edges << std::endl;

			// make printing easier:
			ol.stations.push_back(ol.stations.front());

			std::cout << "\t// order " << itr->unit_number << " ("
				<< file.stations[ol.stations[0]].name << " - "
				<< file.stations[ol.stations[mid]].name
				<< ")" << std::endl;

			std::cout << "\t// cargo: ";
			for(std::set<CargoID>::const_iterator itr2 = itr->cargo.begin();
				itr2 != itr->cargo.end(); ++itr2)
					std::cout << " " << +*itr2;
			std::cout << std::endl;

			std::cout << "\t//";
			for(std::vector<StationID>::const_iterator itr = ol.stations.begin();
				itr != ol.stations.end(); ++itr)
			{
				std::cout << " " << file.stations[*itr].name;
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

			enum class edge_type_t
			{
				unique,
				duplicate_first,
				duplicate_further
			};
			edge_type_t last_edge_type = edge_type_t::duplicate_further;

			for(std::vector<StationID>::const_iterator itr = ol.stations.begin() + 1;
				itr != ol.stations.end(); ++itr)
			{
				const bool first = itr == ol.stations.begin();

				edge_type_t edge_type = edge_type_t::unique;

				for(std::vector<StationID>::const_iterator itr2 = ol.stations.begin();
					!(int)edge_type && itr2 != itr; ++itr2)
					// itr2 < itr => itr2 + 1 is valid
					// itr == begin => for loop is not run => itr - 1 is valid
					if(*itr2 == *itr && (*(itr2 + 1)) == (*(itr - 1)) )
						edge_type = edge_type_t::duplicate_further;

				if(!first)
				for(std::vector<StationID>::const_iterator itr2 = itr + 1;
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
				 std::cout << "\t" << *(itr - 1);

				if(edge_type != edge_type_t::duplicate_further)
				 std::cout << " -> " << *itr;

				if(file.stations.find(*itr) == file.stations.end())
				{
					std::cerr << "Could not find station id " << *itr << std::endl;
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
