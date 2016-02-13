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
#include "../mkgraph/common.h"

// for finding paths
#include "../pbs.h"
#include "../pathfinder/npf/npf_func.h"
#include "../pathfinder/yapf/yapf.hpp"

/** Factory for the graph video driver. */
static FVideoDriver_Graph iFVideoDriver_Graph;

VideoDriver_Graph::VideoDriver_Graph()
{
}

//template<class T> ... (function)

namespace detail {

	template<class T, T v>
	struct integral_constant
	{
		static const T value = v;
		typedef T value_type;
	};

	typedef integral_constant<bool, true> true_type;
	typedef integral_constant<bool, false> false_type;

	template<class T, class U>
	struct is_same : false_type {};

	template<class T>
	struct is_same<T, T> : true_type {};
}

static Track DoTrainPathfind(const Train *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, bool do_track_reservation, PBSTileInfo *dest)
{
	switch (_settings_game.pf.pathfinder_for_trains) {
		case VPF_NPF: return NPFTrainChooseTrack(v, tile, enterdir, tracks, path_found, do_track_reservation, dest);
		case VPF_YAPF: return YapfTrainChooseTrack(v, tile, enterdir, tracks, path_found, do_track_reservation, dest);

		default: NOT_REACHED();
	}
}

void VideoDriver_Graph::MainLoop()
{
	//uint i;
	//for (i = 0; i < this->ticks; i++) {
		GameLoop();
		UpdateWindows();
	//}

	if(!detail::is_same<CargoID, byte>::value ||
		!detail::is_same<StationID, uint16>::value)
	{
		std::cerr << "Error! Types changed in OpenTTD. "
			"Programmers must fix this here." << std::endl;
		assert(false);
	}

	railnet_file_info file;

	std::vector<bool> stations_used;
	stations_used.resize(_station_pool.size, false);

	const OrderList* _ol;
	FOR_ALL_ORDER_LISTS(_ol) {

		order_list new_ol;

		/*
			basically fill new order list
		*/
//std::cerr << "cargo: ";
		for (Vehicle *v = _ol->GetFirstSharedVehicle(); v != NULL; v = v->Next())
		{
			if(v->cargo_cap) {
				new_ol.cargo.insert(v->cargo_type);
//				std::cerr << " " << +v->cargo_type;
			}
		}
//		std::cerr << std::endl;
		for (Order *order = _ol->GetFirstOrder(); order != NULL; order = order->next)
		{
			if (order->IsType(OT_GOTO_STATION) || order->IsType(OT_IMPLICIT))
			{
				StationID sid = (StationID)order->GetDestination();
				// rewrites A->B->B->C as A->B->C
				if(new_ol.stations.empty() || sid != new_ol.stations.back())
				 new_ol.stations.push_back(sid);
				new_ol.min_station = std::min(new_ol.min_station, sid);
			}
		}
		if(new_ol.stations.size())
		 if(new_ol.stations.back() == new_ol.stations.front())
		  new_ol.stations.pop_back();

		typedef std::multiset<order_list>::iterator it;
		std::pair<it, it> itrs = file.order_lists.equal_range(new_ol);
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
				std::vector<StationID>::const_iterator sitr
					= already_added.stations.begin();
				for( ;
					sitr != already_added.stations.end() &&
						*sitr != new_ol.stations.front()
					; ++sitr, ++start_found) ;

				if(sitr != already_added.stations.end())
				{ // start station found in already added order list?

					bool same_order = false;

					std::size_t sz = new_ol.stations.size();
					if(sz == already_added.stations.size())
					{
						same_order = true;
						for(std::size_t i = 0; i < sz; ++i)
						 same_order = same_order && (new_ol.stations[i]
							== already_added.stations[start_found+i%sz]);
					}

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
					sitr != new_ol.stations.end(); ++sitr)
			{
				stations_used[*sitr] = true;
				for(std::vector<StationID>::const_iterator sitr2 = sitr;
					sitr2 != new_ol.stations.end() && unique; ++sitr2)
						unique = unique && (*sitr != *sitr2);
			}

			new_ol.is_cycle = (new_ol.stations.size() > 2) && unique;
			// bi-cycle at this point is yet forbidden
			// you could have a train running A->B->C->D->A->D->C->B,
			// however, passengers wanting to go C->B might get
			// confused that their train turns around

			file.order_lists.insert(new_ol);
		}

	} // for all order lists

	{
	const Station* st;
	char buf[256];

	float scale = 25.0f;
	FOR_ALL_STATIONS(st) {

		if(stations_used[st->index])
		{
			const TileIndex& center = st->xy; // TODO: better algorithm to find center
			SetDParam(0, st->index); GetString(buf, STR_STATION_NAME, lastof(buf));

			station_info tmp_station;
			tmp_station.name = buf;
			tmp_station.x = (MapSizeX() - TileX(center))/scale;
			tmp_station.y = (MapSizeY() - TileY(center))/scale;
			file.stations.insert(std::make_pair(st->index, tmp_station));
		}

	}

	const CargoSpec* carg;
	FOR_ALL_CARGOSPECS(carg)
	{
		if(carg->IsValid())
		{
			SetDParam(0, 1 << carg->Index()); GetString(buf, STR_JUST_CARGO_LIST, lastof(buf));
			file.cargo_names.insert(std::make_pair(carg->Index(), buf));
		}

	}

	serialize(file, std::cout);

	}
}

