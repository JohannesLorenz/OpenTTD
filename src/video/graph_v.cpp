#include "../stdafx.h"

#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

#include "../vehicle_base.h"
#include "../station_base.h"
#include "../order_base.h"
#include "../strings_func.h"
#include "graph_v.h"

/** Factory for the graph video driver. */
static FVideoDriver_Graph iFVideoDriver_Graph;

VideoDriver_Graph::VideoDriver_Graph()
{
}

//template<class T> ... (function)

void VideoDriver_Graph::MainLoop()
{
	//uint i;
	//for (i = 0; i < this->ticks; i++) {
		GameLoop();
		UpdateWindows();
	//}

	struct order_list
	{
		bool is_cycle;
		bool is_bicycle; //! at least two trains that drive in opposite cycles
		StationID min_station;
		std::set<CargoID> cargo; // cargo order and amount does not matter
		std::vector<StationID> stations;
		bool operator<(const order_list& other) const {
			return (min_station == other.min_station)
				? stations.size() < other.stations.size()
				: min_station < other.min_station; }
	};

	std::multiset<order_list> order_lists;

	/*FOR_ALL_STATIONS(st) {
		SetDParam(0, st->index); GetString(buf, STR_STATION_NAME, lastof(buf));
		std::cout << buf << std::endl;
	}*/

	std::vector<bool> stations_used;
	stations_used.resize(_station_pool.size, false);

	const OrderList* _ol;
	FOR_ALL_ORDER_LISTS(_ol) {

		order_list new_ol;

		/*
			basically fill new order list
		*/

		for (Vehicle *v = _ol->GetFirstSharedVehicle(); v != NULL; v = v->NextShared()) {
			new_ol.cargo.insert(v->cargo_type);
		}
		for (Order *order = _ol->GetFirstOrder(); order != NULL; order = order->next)
		{
			if (order->IsType(OT_GOTO_STATION) || order->IsType(OT_IMPLICIT))
			{
				StationID sid = (StationID)order->GetDestination();
				// rewrite A->B->B->C as A->B->C
				if(new_ol.stations.empty() || sid != new_ol.stations.back())
				 new_ol.stations.push_back(sid);
				new_ol.min_station = std::min(new_ol.min_station, sid);
			}
		}
		if(new_ol.stations.size())
		 if(new_ol.stations.back() == new_ol.stations.front())
		  new_ol.stations.pop_back();

		typedef std::multiset<order_list>::iterator it;
		std::pair<it, it> itrs = order_lists.equal_range(new_ol);
		bool this_is_a_new_line = true;
		if(new_ol.stations.empty())
		 this_is_a_new_line = false; // consider this already added
		else for (it itr=itrs.first; itr!=itrs.second; ++itr)
		{
			// from the multiset, we can assume:
			// min_station == other.min_station
			// station.size() == other.station_size()

			// g++ disabled this because modifying the element might
			// change the correct order. however, we do only changes
			// for the cycle bits, they have no effect.
			order_list& already_added = const_cast<order_list&>(*itr);

			// if the cargo types are no subsets, then it is another line
			bool added_is_subset = std::includes(new_ol.cargo.begin(), new_ol.cargo.end(),
				already_added.cargo.begin(), already_added.cargo.end());
			bool new_is_subset = std::includes(new_ol.cargo.begin(), new_ol.cargo.end(),
				already_added.cargo.begin(), already_added.cargo.end());

			if(added_is_subset || new_is_subset)
			{
				std::size_t start_found = 0;
				for(std::vector<StationID>::const_iterator sitr
					= already_added.stations.begin();
						sitr != already_added.stations.end() &&
						*sitr != new_ol.stations.front()
						; ++sitr, ++start_found)

				if(sitr != already_added.stations.end())
				{

					bool same_order = true;

					std::size_t sz = new_ol.stations.size();
					for(std::size_t i = 0; i < sz; ++i)
					 same_order = same_order && (new_ol.stations[i]
						== already_added.stations[start_found+i%sz]);

					if(same_order)
					{
						// case 1: order + direction are the same,
						// just the starting point is maybe different
						// nothing to do
						this_is_a_new_line = false;
					}
					else
					{
						// case 2: the next stations of the new line and of the old line
						//	sum up to *one* bidirectional circle
						// don't insert, but add bidirectional edges (if required)
						if(already_added.is_bicycle || already_added.is_cycle)
						{
							// must be either the same direction
							// (then same_order was already true)
							// or the opposite direction
							// => it must be the opposite direction
							std::size_t cur_old = start_found,
								cur_new = 0;
							bool equal = true;
							for(; cur_new < already_added.stations.size();
								++cur_new,
								cur_old = (cur_old == 0) ? (already_added.stations.size() - 1) : (cur_old - 1))
								equal = equal && (already_added.stations[cur_old] == new_ol.stations[cur_new]);

							if(equal)
							{
								if(already_added.is_cycle) {
									already_added.is_cycle = false;
									already_added.is_bicycle = true;
								}
								this_is_a_new_line = false;
							}
						}

					}
				}
			}

		} // for all already added order lists

		if(this_is_a_new_line)
		{
			// add more information
			bool unique = true;
			// O(n^2), but less (0) mallocs than sort
			// and earlier abort in many cases
			for(std::vector<StationID>::const_iterator sitr
				= new_ol.stations.begin();
					sitr != new_ol.stations.end() && unique; ++sitr)
			{
				stations_used[*sitr] = true;
				for(std::vector<StationID>::const_iterator sitr2 = sitr;
					sitr2 != new_ol.stations.end() && unique; ++sitr2)
						unique = unique && (*sitr != *sitr2);
			}

			new_ol.is_cycle = unique;
			// bi-cycle at this point is yet forbidden
			// you could have a train running A->B->C->D->A->D->C->B,
			// however, passengers wanting to go C->B might get
			// confused that their train turns around

			order_lists.insert(new_ol);
		}

	} // for all order lists


	std::cout <<	"digraph graphname\n" // TODO: get savegame name
			"{\n"
			"\tgraph[splines=line];\n"
			"\tnode[label=\"\", shape=none, size=0.1, width=1.0, height=0.1];\n";
	{
	const Station* st;
	char buf[256];

	std::size_t idx = 0;
	float scale = 25.0f;
	FOR_ALL_STATIONS(st) {

		if(stations_used[idx++])
		{
			const TileIndex& center = st->xy; // TODO: better algorithm to find center
			SetDParam(0, st->index); GetString(buf, STR_STATION_NAME, lastof(buf));
			std::cout << "\t" << st->index << " [xlabel=\"" << buf << "\", pos=\""
				<< (TileX(center)/scale)
				<< ", "
				<< (TileY(center)/scale)
				<< "!\"];" << std::endl;
		}

	}

	const std::size_t num_colors = 4;
	const char* colors[num_colors] = { "gray", "red", "green", "blue" };
	idx = 0;
	for(std::multiset<order_list>::const_iterator itr = order_lists.begin();
		itr != order_lists.end(); ++itr, ++idx)
	{
		if(itr->stations.size())
		{
			const order_list& ol = *itr;
			std::vector<StationID>::const_iterator recent;
			std::cout << ol.stations.front();
			for(std::vector<StationID>::const_iterator itr = ol.stations.begin();
				itr != ol.stations.end(); ++itr)
			{
				if(itr != ol.stations.begin())
				{
					std::cout << " -> "
						<< *itr;
				}
				recent = itr;
			}
			std::cout << " -> " << ol.stations.front();
			std::cout << " [color=\"" << colors[idx%num_colors] << "\"];" << std::endl;
		}
	}

	}
	std::cout <<	"}";
}

