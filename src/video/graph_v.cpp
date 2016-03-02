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
#include "../vehicle_func.h"
#include "../depot_base.h"

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

struct location_data
{
	TileIndex tile;
	Trackdir dir;
	location_data() : tile(INVALID_TILE), dir(INVALID_TRACKDIR) {}
	location_data(TileIndex tile, Trackdir dir) :
		tile(tile), dir(dir) {}
};

struct st_node_t : public location_data
{
	st_node_t* child;
	StationID sid;
//	TileIndex tile;
//	Trackdir dir;
};

class visited_path_t
{
	//! this struct contains pointers, better don't copy it
	visited_path_t(const visited_path_t& ) {}
public:
	typedef std::map<std::pair<TileIndex, Trackdir>, st_node_t> path_t;
	path_t path;
	st_node_t *first, *target;
	visited_path_t() : target(NULL) {}
};

template<class Ftor>
void for_all_stations_to(visited_path_t& visited, Ftor& ftor)
{
	std::size_t max = visited.path.size() << 1;
	st_node_t* from = visited.first;
	bool go_on = true;
	for(; go_on; from = from->child)
	{
		static char buf[256];
		SetDParam(0, from->sid); GetString(buf, STR_STATION_NAME, lastof(buf));
		std::cerr << " for_all: " << buf << ", tiles:" << std::endl;

		ftor(*from);
		if(!--max)
		 throw "Cycle detected.";

		go_on = (from != visited.target);
	}
}

bool _ForAllStationsTo(const Train *train, location_data location, const Order& order,
	visited_path_t* visited_path, int best_cost = std::numeric_limits<int>::max());

class GraphFtor : public StationFtor
{
	visited_path_t* visited_path;
	static st_node_t* last_node;
	bool last_station;
	virtual void OnStationTile(const TileIndex &t, const Trackdir& td, int cost)
	{
		if(last_station)
		 last_node = NULL;

		Station* station = Station::GetByTile(t);
		// if it's a station, and not the same one as last time
		if(station && (!last_node || last_node->sid != station->index))
		{
			StationID sid = station->index;

			st_node_t& st_node = visited_path->path[std::make_pair(t, td)];
			if(last_station)
			{
#if 0
				loc.tile = t;
				loc.dir = td;
				loc.cost = cost;
#endif
				max_cost = cost;
				visited_path->target = &st_node;
				std::cerr << "RECOMPUTED PATH" << std::endl;
				std::cerr << "tile/trackdir/cost: " << t << ", " << td << ", " << cost << std::endl;
			}
			else
			{
				// assume that this is the best path
				// if this will overwritten in the future: no problem
				st_node.child = last_node;
			}

			st_node.sid = sid;
			st_node.dir = td;
			st_node.tile = t;

			location_data cur_st_loc(t, ReverseTrackdir(td));

			// denote our station as the first station
			// can be overwritten by subsequent calls
			visited_path->first = &st_node;

			last_node = &st_node;

			static char buf[256];


			SetDParam(0, sid); GetString(buf, STR_STATION_NAME, lastof(buf));
			std::cerr << " GraphFtor: " << buf << ", tiles:" << std::endl;

			fprintf(stderr, "    %d, %d\n", TileX(t), TileY(t));

			// try from this station into the reverse direction
			if(last_station /*|| cost <= 0*/) {
				std::cerr << "->NOCOMP: " << cost << std::endl;
				// yapf allows "negative pentalties" for the starting tile
				// so, we must disallow loops
			}
			else {
				_ForAllStationsTo(train, cur_st_loc, order,
					visited_path, max_cost - std::max(cost, 0));
			}

			std::cerr << " <- GraphFtor: " << buf << std::endl;

			last_station = false;
		}
	}
public:
//	location_data loc; //!< where + how the target is being reached
	int max_cost; // TODO: no class variable
	const Train* train;
	const Order& order;
	GraphFtor(visited_path_t* v, const Train* train, const Order& order) :
		visited_path(v),
		last_station(true),
		max_cost(0),
		train(train),
		order(order) {}
};

st_node_t* GraphFtor::last_node = NULL;

bool _ForAllStationsTo(const Train* train, location_data location, const Order& order,
	visited_path_t* visited_path, int best_cost)
{
	bool path_found;
	PBSTileInfo target;

	GraphFtor graphf(visited_path, train, order);

	YapfTrainStationsToTarget(train, path_found, &target, graphf,
		location.tile, location.dir, order, best_cost);

	return path_found;
}

bool ForAllStationsTo(const Train* train, location_data location, const Order& order,
	visited_path_t* visited_path, int best_cost = std::numeric_limits<int>::max())
{
	if(!_ForAllStationsTo(train, location, order, visited_path, best_cost))
	{
		// we only reverse on GraphFtor::OnStationTile, so if
		// GraphFtor::OnStationTile never gets reached, we have to try reversing
		// at the starting position

		if(location.dir == INVALID_TRACKDIR)
		{
			PBSTileInfo origin = FollowTrainReservation(train);
			location.tile = origin.tile;
			location.dir = ReverseTrackdir(origin.trackdir);
		}
		else
		 location.dir = ReverseTrackdir(location.dir);

		if(!_ForAllStationsTo(train, location, order, visited_path, best_cost))
		{
			return false;
		}
	}
	return true;
}

struct DumpStation
{
	void operator()(const st_node_t& node) const
	{
		if(node.tile == INVALID_TILE || node.dir == INVALID_TRACKDIR)
		 throw "Internal error";
		if(!Station::GetByTile(node.tile))
		 throw "Internal error";

		static char buf[256];
		SetDParam(0, node.sid); GetString(buf, STR_STATION_NAME, lastof(buf));
		std::cerr << " (-> Station): " << buf << ", tiles:" << std::endl;

		fprintf(stderr, "    %d, %d\n", TileX(node.tile), TileY(node.tile));
	}
} dump_station;

struct AddStation : DumpStation
{
	OrderNonStopFlags nst;
	const visited_path_t& vp;
	order_list* new_ol;

	void add_node(const st_node_t& node) const {
		const StationID sid = node.sid;
		if(new_ol->stations.empty() || sid != new_ol->stations.back())
		 new_ol->stations.push_back(sid);
		new_ol->min_station = std::min(new_ol->min_station, sid);
	}

	void operator()(const st_node_t& node) const
	{
		DumpStation::operator ()(node);

	/*	if(&node == vp.target)
		switch(nst)
		{
			case ONSF_STOP_EVERYWHERE:
			case ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS:
				add_node(node);
			default: break;
		}
		else
		switch(nst)
		{
			case ONSF_STOP_EVERYWHERE:
			case ONSF_NO_STOP_AT_DESTINATION_STATION:
				add_node(node);
			default: break;
		}*/
		add_node(node);
	}

	AddStation(OrderNonStopFlags nst, const visited_path_t& vp,
		order_list* new_ol) : nst(nst), vp(vp), new_ol(new_ol) {}
};

void VideoDriver_Graph::SaveOrderList(railnet_file_info& file, /*const OrderList* _ol,*/const Train* train,
	std::vector<bool>& stations_used) const
{
	order_list new_ol;

	if(!train->orders.list)
	 return;

	/*
		basically fill new order list
	*/
//	std::cerr << "cargo: ";
	for (Vehicle *v = train->orders.list->GetFirstSharedVehicle(); v != NULL; v = v->Next())
	{
		if(v->cargo_cap) {
			new_ol.cargo.insert(v->cargo_type);
//			std::cerr << " " << +v->cargo_type;
		}
	}
//	std::cerr << std::endl;

#if 0
	for (Order *order = _ol->GetFirstOrder(); order != NULL; order = order->next)
	{
		if (order->IsType(OT_GOTO_STATION) /*|| order->IsType(OT_IMPLICIT)*/)
		{
			StationID sid = (StationID)order->GetDestination();

			DoTrainPathfind()

			// rewrites A->B->B->C as A->B->C
/*			if(new_ol.stations.empty() || sid != new_ol.stations.back())
			 new_ol.stations.push_back(sid);*/
			new_ol.min_station = std::min(new_ol.min_station, sid);
		}
	}
#endif
	std::set<const OrderList*> order_lists_done;

	//const Train* train;
	/*FOR_ALL_TRAINS(train)*/ {

		bool path_found;

	//	if(train->orders.list != _ol)
	//	 continue;

		const OrderList* cur_ol = train->orders.list;
		if(!cur_ol)
		 return;
		std::cerr << "NEXT TRAIN: " << train->unitnumber << std::endl;
		if(	train->IsFrontEngine()
			&& cur_ol->GetFirstOrder() != NULL // don't add empty orders
			&& order_lists_done.find(cur_ol) == order_lists_done.end())
		{
			bool ok = true;
			static char buf[256];
			Order* first_order = NULL;

			// how did the train enter the recently passed (e.g. current) station?
			location_data recent_loc;
			visited_path_t visited_path;

			path_found = false;
			for (Order *order = train->orders.list->GetFirstOrder();
				order != NULL && !path_found;
				order = order->next)
			{
				if (order->IsType(OT_GOTO_STATION) /*|| order->IsType(OT_IMPLICIT)*/)
				{

					StationID sid = (StationID)order->GetDestination();
					SetDParam(0, sid); GetString(buf, STR_STATION_NAME, lastof(buf));
					std::cerr << "Pre-Heading (trying) for station: " << buf << std::endl;

					path_found = ForAllStationsTo(train, location_data(),
						*order, &visited_path);
					if(path_found)
					{
						recent_loc = *visited_path.target;
						std::cerr << "Found: " << recent_loc.tile << " / "
							<< recent_loc.dir << std::endl;
						StationID sid2 = (StationID)order->GetDestination();
						SetDParam(0, sid2); GetString(buf, STR_STATION_NAME, lastof(buf));
						std::cerr << "Found: " << buf << std::endl;

						first_order = order;
					}
				}
			}

			if(!first_order) // no real stations found
			 return; // => nothing to draw

			// virtually drive once around to get the right track direction
			{
				std::cerr << "Still: " << recent_loc.tile
						<< " / " << recent_loc.dir << std::endl;
				for (Order *order = first_order->next; order != NULL; order = order->next)
				if (order->IsType(OT_GOTO_STATION))
				{
					StationID sid = (StationID)order->GetDestination();
					SetDParam(0, sid); GetString(buf, STR_STATION_NAME, lastof(buf));
					std::cerr << "Pre-Heading for station: " << buf << std::endl;
					std::cerr << "Recent loc: " << recent_loc.tile
						<< " / " << recent_loc.dir << std::endl;
					path_found = ForAllStationsTo(train, recent_loc, *order,
						&visited_path);
					for_all_stations_to(visited_path, dump_station);
					std::cerr << "path found? " << path_found << std::endl;
					recent_loc = *visited_path.target;
				}
			}

			for (Order *order = train->orders.list->GetFirstOrder(); order != NULL; order = order->next)
			{

				if (order->IsType(OT_GOTO_STATION)
/*					|| order->IsType(OT_GOTO_WAYPOINT)
					|| order->IsType(OT_GOTO_DEPOT)*/)
				{
					StationID sid = (StationID)order->GetDestination();
					SetDParam(0, sid); GetString(buf, STR_STATION_NAME, lastof(buf));
					std::cerr << "Heading for station: " << buf << std::endl;

					path_found = ForAllStationsTo(train, recent_loc,
						*order, &visited_path);

					AddStation add_stations(order->GetNonStopType(), visited_path, &new_ol);
					for_all_stations_to(visited_path, add_stations);
					recent_loc = *visited_path.target;
					std::cerr << "path found? " << path_found << std::endl;


					ok = ok && path_found;
				}

				if(!path_found)
				 break;

#if 0
				if (order->IsType(OT_GOTO_STATION) /*|| order->IsType(OT_IMPLICIT)*/)
				{
					StationID sid = (StationID)order->GetDestination();

					// rewrites A->B->B->C as A->B->C
					if(new_ol.stations.empty() || sid != new_ol.stations.back())
					 new_ol.stations.push_back(sid);
					new_ol.min_station = std::min(new_ol.min_station, sid);
				}
#endif
			}

			if(!path_found) {
				std::cerr << "Warning: path for train #" << train->unitnumber << " not found" << std::endl;
				return;
			}


			order_lists_done.insert(cur_ol);
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

		new_ol.unit_number = train->unitnumber;

		file.order_lists.insert(new_ol);
	}
}

const float scale = 25.0f;

void VideoDriver_Graph::SaveStation(struct railnet_file_info& file, const struct Station* st,
	const std::vector<bool> &stations_used) const
{
	static char buf[256];
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

void VideoDriver_Graph::SaveCargoSpec(railnet_file_info &file, const CargoSpec *carg) const
{
	static char buf[256];
	if(carg->IsValid())
	{
		SetDParam(0, 1 << carg->Index()); GetString(buf, STR_JUST_CARGO_LIST, lastof(buf));
		file.cargo_names.insert(std::make_pair(carg->Index(), buf));
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

//	const OrderList* _ol;
//	FOR_ALL_ORDER_LISTS(_ol) { SaveOrderList(file, _ol, stations_used); }
	const Train* train;
	FOR_ALL_TRAINS(train) { SaveOrderList(file, train, stations_used); }

	const Station* st;
	FOR_ALL_STATIONS(st) { SaveStation(file, st, stations_used); }

	const CargoSpec* carg;
	FOR_ALL_CARGOSPECS(carg) { SaveCargoSpec(file, carg); }

	serialize(file, std::cout);
}

