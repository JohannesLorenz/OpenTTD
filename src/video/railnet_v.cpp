/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../stdafx.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <set>
#include <algorithm>
#include <limits>

#include "../vehicle_base.h"
#include "../station_base.h"
#include "../order_base.h"
#include "../strings_func.h"
#include "railnet_v.h"
#include "../railnet/common.h"
#include "../railnet/railnet_node_list.h"
#include "../vehicle_func.h"
#include "../depot_base.h"

// for finding paths
#include "../pbs.h"
#include "../pathfinder/npf/npf_func.h"
#include "../pathfinder/yapf/yapf.hpp"

/** Factory for the graph video driver. */
static FVideoDriver_Railnet iFVideoDriver_Railnet;

VideoDriver_Railnet::VideoDriver_Railnet()
{
}

//template<class T> ... (function)

//! tile + trackdir
struct location_data {
	TileIndex tile;
	Trackdir dir;
	location_data() : tile(INVALID_TILE), dir(INVALID_TRACKDIR) {}
	location_data(TileIndex tile, Trackdir dir) :
		tile(tile), dir(dir) {}
};

//! station node for A* paths
struct st_node_t : public location_data {
	st_node_t* child;
	StationID sid;
};

class visited_path_t {
	//! this struct contains pointers, better don't copy it
	visited_path_t(const visited_path_t& ) {}
public:
	typedef std::map<std::pair<TileIndex, Trackdir>, st_node_t> path_t;
	path_t path;
	st_node_t *first, *target;
	visited_path_t() : target(NULL) {}
};

template<class Ftor>
void ForAllStationsOnPath(visited_path_t& visited, Ftor& ftor)
{
	std::size_t max = visited.path.size() << 1;
	st_node_t* from = visited.first;
	bool go_on = true;
	for (; go_on; from = from->child) {
		static char buf[256];
		SetDParam(0, from->sid); GetString(buf, STR_STATION_NAME, lastof(buf));
#ifdef DEBUG_GRAPH_YAPF
		std::cerr << " for_all: " << buf << ", tiles:" << std::endl;
#endif

		ftor(*from);
		if (!--max)
		 throw "Cycle detected.";

		go_on = (from != visited.target);
	}
}

bool ForAllStationsTo(const Train *train, location_data location, const Order& order,
	visited_path_t* visited_path, int best_cost = std::numeric_limits<int>::max(),
	bool try_reverse = true);

class GraphFtor : public StationFtor {
	visited_path_t* visited_path;
	static st_node_t* last_node;
	bool first_run;

	enum BuildingType {
		BT_DEPOT,
		BT_STATION,
		BT_WAYPOINT
	};

	virtual void OnTile(const Depot&, const TileIndex &, const Trackdir&, int) {}

	virtual void OnTile(const Waypoint& wp, const TileIndex &t, const Trackdir& td, int cost)
	{
		_OnTile(wp.index, t, td, cost, BT_WAYPOINT);
	}

	virtual void OnTile(const Station& st, const TileIndex &t, const Trackdir& td, int cost)
	{
		_OnTile(st.index, t, td, cost, BT_STATION);
	}

	template<class T>
	void _OnTile(const T& index, const TileIndex &t, const Trackdir& td, int cost,
		BuildingType bt)
	{
		if (first_run)
		 last_node = NULL;

		if (!last_node || last_node->sid != index) {
			st_node_t& st_node = visited_path->path[std::make_pair(t, td)];
			if (first_run) {
#if 0
				loc.tile = t;
				loc.dir = td;
				loc.cost = cost;
#endif
				max_cost = cost;
				visited_path->target = &st_node;
#ifdef DEBUG_GRAPH_YAPF
				std::cerr << "RECOMPUTED PATH" << std::endl;
				std::cerr << "tile/trackdir/cost: " << t << ", " << td << ", " << cost << std::endl;
#endif
			} else {
				// assume that this is the best path
				// if this will be overwritten in the future: no problem
				st_node.child = last_node;
			}

			// only real stations should be displayed in the end
			// others only give a positional information
			st_node.sid = (bt == BT_DEPOT) ? INVALID_STATION : index;

			st_node.dir = td;
			st_node.tile = t;

			// denote our station as the first station
			// can be overwritten by subsequent calls
			visited_path->first = &st_node;

			last_node = &st_node;

			static char buf[256];
			if (bt != BT_DEPOT) { // print station
				SetDParam(0, index); GetString(buf, STR_STATION_NAME, lastof(buf));
#ifdef DEBUG_GRAPH_YAPF
				std::cerr << " GraphFtor: " << buf << ", tiles:" << std::endl;
				std::cerr << "    " << TileX(t) << ", " << TileY(t) << std::endl;
#endif
			}

			// simulate turnaround in stations
			if (bt == BT_STATION) {
				// try from this station into the reverse direction
				if (first_run /*|| cost <= 0*/) {
					// note: if there are ever problems with
					// negative penalties, the algorithm must go here
				} else {
					ForAllStationsTo(train,
						location_data(t, ReverseTrackdir(td)), order,
						visited_path, max_cost - std::max(cost, 0), false);
				}
			}
#ifdef DEBUG_GRAPH_YAPF
			if (bt != BT_DEPOT)
			 std::cerr << " <- GraphFtor: " << buf << std::endl;
#endif

			first_run = false;
		}
	}
public:
//	location_data loc; //!< where + how the target is being reached
	int max_cost; // FEATURE: make this a static variable??
	const Train* train;
	const Order& order;
	GraphFtor(visited_path_t* v, const Train* train, const Order& order) :
		visited_path(v),
		first_run(true),
		max_cost(0),
		train(train),
		order(order) {
			//last_node = NULL;
		}
};

st_node_t* GraphFtor::last_node = NULL;

bool ForAllStationsTo(const Train* train, location_data location, const Order& order,
	visited_path_t* visited_path, int best_cost, bool try_reverse)
{
	bool path_found;
	PBSTileInfo target;

	GraphFtor graphf(visited_path, train, order);
	YapfTrainStationsToTarget(train, path_found, &target, graphf,
		location.tile, location.dir, order, best_cost, try_reverse);

	return path_found;
}

struct DumpStation {
	void operator()(const st_node_t& node) const
	{
		if (node.tile == INVALID_TILE || node.dir == INVALID_TRACKDIR)
		 throw "Internal error";

#ifdef DEBUG_GRAPH_YAPF
		if (node.sid != INVALID_STATION) {
			static char buf[256];
			SetDParam(0, node.sid); GetString(buf, STR_STATION_NAME, lastof(buf));
			std::cerr << " (-> Station): " << buf << ", tiles:" << std::endl;
			srd::cerr << "    " << TileX(node.tile) << ", " << TileY(node.tile) << std::endl;
		}
#endif
	}
} dump_station;

class AddStation : DumpStation {
	OrderNonStopFlags nst;
	const visited_path_t& vp;
	comm::order_list* new_ol;
	void addNode (const st_node_t& node, bool stops) const {
		const StationID sid = node.sid;
		if (new_ol->stations().empty() || sid != new_ol->stations().back().first) {
			new_ol->stations().push_back(std::make_pair(sid, stops));
/*			if (stops) {
				new_ol->min_station() = std::min(new_ol->min_station(), sid);
			}*/
		}
	}
public:
	void operator()(const st_node_t& node) const
	{
		DumpStation::operator ()(node);

		bool stops = false;

		if (Waypoint::GetByTile(node.tile)) {
			// don't stop at waypoints
		} else if (&node == vp.target) switch(nst) {
			case ONSF_STOP_EVERYWHERE:
			case ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS:
				stops = true;
			default: break;
		} else switch(nst) {
			case ONSF_STOP_EVERYWHERE:
			case ONSF_NO_STOP_AT_DESTINATION_STATION:
				stops = true;
			default: break;
		}

		addNode(node, stops);
	}

	AddStation(OrderNonStopFlags nst, const visited_path_t& vp,
		comm::order_list* new_ol) : nst(nst), vp(vp), new_ol(new_ol) {}
};

void VideoDriver_Railnet::SaveOrderList(comm::railnet_file_info& file, const Train* train,
	std::vector<bool>& stations_used, std::set<CargoLabel>& cargo_used,
	std::set<const OrderList*>& order_lists_done, node_list_t& node_list) const
{
	comm::order_list new_ol;

	if (!train->orders.list)
	 return;

#ifdef DEBUG_GRAPH
	std::cerr << "NEXT TRAIN: " << train->unitnumber << std::endl;
#endif

	/*
		basically fill new order list
	*/
#ifdef DEBUG_GRAPH_CARGO
	std::cerr << "cargo: ";
#endif
#if 0 // cargo is being filled below....
	for (Vehicle *v = train->orders.list->GetFirstSharedVehicle(); v != NULL; v = v->Next()) {
		if (v->cargo_cap) {
			if (CargoSpec::Get(v->cargo_type)->IsValid()) {
				CargoLabel lbl = CargoSpec::Get(v->cargo_type)->label;
				new_ol.cargo().insert(cargo_info{ lbl, } );
#ifdef DEBUG_GRAPH_CARGO
				std::cerr << " " << (char)(lbl >> 24)
					<< (char)((lbl >> 16) & 0xFF)
					<< (char)((lbl >> 8) & 0xFF)
					<< (char)(lbl & 0xFF)
					;
#endif
			}
		}
	}
#endif
#ifdef DEBUG_GRAPH_CARGO
	std::cerr << std::endl;
#endif

	{
		bool path_found;

		const OrderList* cur_ol = train->orders.list;
		if (!cur_ol)
		 return;

		if (	train->IsFrontEngine()
			&& cur_ol->GetFirstOrder() != NULL // don't add empty orders
			&& order_lists_done.find(cur_ol) == order_lists_done.end()) {
			bool ok = true;
			static char buf[256];
			const Order *first_order = NULL, *first_found_order = NULL;

			// how did the train enter the recently passed (e.g. current) station?
			location_data recent_loc;
			visited_path_t visited_path;

			path_found = false;
			for (const Order *order = train->orders.list->GetFirstOrder();
				order != NULL && !path_found;
				order = order->next) {
				if (order->IsType(OT_GOTO_STATION)) {
					first_order = order;

					StationID sid = (StationID)order->GetDestination();
					SetDParam(0, sid); GetString(buf, STR_STATION_NAME, lastof(buf));
#ifdef DEBUG_GRAPH_YAPF
					std::cerr << "Pre-Heading (trying) for station: " << buf << std::endl;
#endif
					path_found = ForAllStationsTo(train, location_data(),
						*order, &visited_path);
					if (path_found) {
						recent_loc = *visited_path.target;

						StationID sid2 = (StationID)order->GetDestination(); // TODO: function st_name()
						SetDParam(0, sid2); GetString(buf, STR_STATION_NAME, lastof(buf));
#ifdef DEBUG_GRAPH_YAPF
						std::cerr << "  Found: " << buf << ": "
							<< recent_loc.tile << " / "
							<< recent_loc.dir <<<<  std::endl;
#endif
						first_found_order = order;
					}
				}
			}

			if (!first_found_order) // no real stations found
			 return; // => nothing to draw

			// virtually drive once around to get the right track direction
			{
				bool go_on = true;
				std::size_t round = 0;
				for (const Order *order = cur_ol->GetNext(first_found_order); go_on; order = cur_ol->GetNext(order))
				if (order->IsType(OT_GOTO_STATION)) {
					StationID sid = (StationID)order->GetDestination();
					SetDParam(0, sid); GetString(buf, STR_STATION_NAME, lastof(buf));
#ifdef DEBUG_GRAPH_YAPF
					std::cerr << "Pre-Heading for station: " << buf << std::endl;
					std::cerr << "Recent loc: " << recent_loc.tile
						<< " / " << recent_loc.dir << std::endl;
#endif
					path_found = ForAllStationsTo(train, recent_loc, *order,
						&visited_path);
					ForAllStationsOnPath(visited_path, dump_station);
#ifdef DEBUG_GRAPH_YAPF
					std::cerr << "path found? " << path_found << std::endl;
#endif
					recent_loc = *visited_path.target;

					if (order == first_order && round)
					 go_on = false;
					++round;
				}
			}
			{
			bool go_on = true;
			std::size_t round = 0;
			if (!first_order->next) { // less than two stations?
				return;
			}
			for (const Order *order = first_order->next; go_on; order = cur_ol->GetNext(order)) {

				if (order->IsType(OT_GOTO_STATION)
/*					|| order->IsType(OT_GOTO_WAYPOINT)
					|| order->IsType(OT_GOTO_DEPOT)*/) {
					StationID sid = (StationID)order->GetDestination();
					SetDParam(0, sid); GetString(buf, STR_STATION_NAME, lastof(buf));
#ifdef DEBUG_GRAPH_YAPF
					std::cerr << "Heading for station: " << buf << std::endl;
#endif
					path_found = ForAllStationsTo(train, recent_loc,
						*order, &visited_path);

					AddStation add_stations(order->GetNonStopType(), visited_path, &new_ol);
					ForAllStationsOnPath(visited_path, add_stations);
					recent_loc = *visited_path.target;
#ifdef DEBUG_GRAPH_YAPF
					std::cerr << "path found? " << path_found << std::endl;
#endif

					ok = ok && path_found;
				}

				if (!path_found)
				 break;

				if (order == first_order && round)
				 go_on = false;
				++round;

			} // for
			} // scope

			if (!path_found) {
				// must be left out... warning will be printed in run()
				return;
			}

			if (new_ol.stations().size()) {
				if (new_ol.stations().front().first != new_ol.stations().back().first) {
				 throw "first and last stations differ!";
				} else {
					// we do this for cycle detection etc
					new_ol.stations().front().second = new_ol.stations().back().second;
					new_ol.stations().pop_back();
				}
			}
			else return;

			order_lists_done.insert(cur_ol);
		}
		else return; // only useless cases
	}

	bool this_is_a_new_line = true;
	if (new_ol.stations().empty()) {
		this_is_a_new_line = false; // consider this already added
	} else {
		std::map<UnitID, node_list_t::superset_type> matches;

		comm::order_list* match = NULL;

		new_ol.unit_number = train->unitnumber;

		// add all cargo from this train
		for (Vehicle *v = train->orders.list->GetFirstSharedVehicle(); v != NULL; v = v->Next()) {
			if (v->cargo_cap) {
				if (CargoSpec::Get(v->cargo_type)->IsValid()) {
					CargoLabel lbl = CargoSpec::Get(v->cargo_type)->label;
					new_ol.cargo().insert(std::make_pair(lbl, comm::cargo_info {true, false, 0/*,
						train->unitnumber, no_unit_no*/} ));
#ifdef DEBUG_GRAPH_CARGO
					std::cerr << " " << (char)(lbl >> 24)
						<< (char)((lbl >> 16) & 0xFF)
						<< (char)((lbl >> 8) & 0xFF)
						<< (char)(lbl & 0xFF)
						;
#endif
				}
			}
		}

		node_list.init_rest(new_ol);

		// add new_ol to the other order lists from node_list
		// if new_ol will be discarded, this will not harm (TODO: but may harm runtime...?)
		bool is_same = node_list.traverse(new_ol, &matches, false, true) & node_list_t::is_same_train,
			is_rev = false;
		if(!is_same)
			is_rev = node_list.traverse(new_ol, &matches, true, true) & node_list_t::is_same_train;

		if(is_same || is_rev)
		{
			// duplicates are put together
			// bicycles are represented by only one line
			this_is_a_new_line = false;

			UnitID the_match = std::numeric_limits<UnitID>::max();
			for(const std::pair<const UnitID, node_list_t::superset_type> pr : matches)
			if(pr.second & node_list_t::is_same_train)
			{
				if(the_match == std::numeric_limits<UnitID>::max())
				 the_match = pr.first;
				else
				 throw "expected only one match!";
			}

			if(the_match == std::numeric_limits<UnitID>::max())
			 throw "expected some match!";

			// get the order list with the ID from matches
			// unfortunately, we need to look it up by scanning the whole order list...
			for(std::list<comm::order_list>::iterator it = file.order_lists().begin();
				it != file.order_lists().end() && !match; ++it)
			// for(auto it2 = it->cargo().begin(); it2 != it->cargo().end(); ++it2)
			{
				if(the_match == it->unit_number)
				 match = &*it;
			}
			if(!match)
			 throw "train ID not found, logical programmer error";
		}

/*		bool added_is_subset = std::includes(new_ol.cargo().begin(), new_ol.cargo().end(),
			match->cargo().begin(), match->cargo().end());
		bool new_is_subset = std::includes(match->cargo().begin(), match->cargo().end(),
			new_ol.cargo().begin(), new_ol.cargo().end());

		if(is_same)*/
		if(is_same || is_rev){
			// case 1: order + direction are the same,
			// (just the starting point may be different)

			// trains with the same path and the same cargo are being treated as one line
			// reasons: 1. easier to overview
			//          2. if you have 3 trains, one carrying A, one B and one A and B,
			//             do you want 3 lines? probably not...

			//	 1 1 1   2 2   3 3     5 5 4 4 5  <- match
			//	 0   0 0   0 0 0   0 0 0   0 0 0  <- new_ol
			//	 6 1 6 7 2 8 7 9 3 7 7 A 5 B B A  <- result
			//	=> everything in the new list gets a new number
			//	algorithm:
			//	(1) all new cargo get a separate number
			//	       7     7     7 7
			//	(2) all other cargo get inverted iff they are in the new list:
			//	-1 1-1   2-2  -3 3    -5 5-4-4-5
			//	(3) renumber the negated cargo
			//

			// step (1) + (2)
			int reserved_for_new = match->next_cargo_slice++;
			for(std::map<cargo_label_t, comm::cargo_info>::const_iterator new_i = new_ol.cargo().begin();
				new_i != new_ol.cargo().end(); ++new_i)
			{
//				std::set<comm::cargo_info>::const_iterator in_match = match->cargo().find(new_i->first);
				std::map<cargo_label_t, comm::cargo_info>::iterator in_match
					= match->cargo().find(new_i->first);
				if(in_match == match->cargo().end())
				{ // step (1)
					match->cargo().insert(std::make_pair(
						new_i->first,
						comm::cargo_info {is_same, !is_same, reserved_for_new/*,
							is_same ? train->unitnumber : no_unit_no,
							is_same ? no_unit_no : train->unitnumber*/
							}
						));
				}
				else
				{ // step (2)
			//		in_match->second.slice = -in_match->second.slice;
					match->next_cargo_slice = std::max
					(match->next_cargo_slice,
						1 +
					 (in_match->second.slice += reserved_for_new));
					if(is_rev) {
						in_match->second.rev = true;
						if(match->rev_unit_no == no_unit_no)
						 match->rev_unit_no = train->unitnumber;
					}
				}
			}
	/*		// step (3)
			int recent_slice = -1;
			for(std::set<cargo_info>::const_iterator match_i = match->cargo.begin();
				match_i != match->cargo.end(); ++match_i)
			{
				if(recent_slice != match->second.slice &&
					recent_slice != -match->second.slice)
				{
					++match->next_cargo_slice;
					recent_slice = match->second.slice;
				}
				if(match_i->se
				 match_i->second.slice = match->next_cargo_slice;
			}*/
#if 0
			// all cargo not found => one new id
			// all cargo found => one new id
			for(std::set<cargo_info>::const_iterator match_i = match->cargo.begin(),
				new_i = new_ol.cargo.begin();
				match_i != match->cargo.end() && new_i != new_ol.cargo.end();
				)
			{
				if(new_i->slice < match_i->slice)
				{
					// match does not
				}
			}
			match->
			++match->next_cargo_slice;

			added.cargo().insert(new_ol.cargo().begin(), new_ol.cargo().end());
			cargo_used.insert(new_ol.cargo().begin(), new_ol.cargo().end());
#endif
		}
#if 0
		else if(added_is_subset && new_is_subset && already_added.is_cycle && is_rev) {
			// why do we summarize cycles to bicycles? one reason is that trains of either direction
			// usually have the same line

			already_added.is_cycle = false;
			already_added.is_bicycle = true;
			//already_added.rev_unit_no = cur_ol.unit_number;
			// for bicycle, no need to add another
		} // for all already added order lists
#endif
	}

	if (this_is_a_new_line) {
		// add more information
		bool unique = true;
		// O(n^2), but less (0) mallocs than sort
		// and earlier abort in many cases
		for (std::vector<std::pair<StationID, bool> >::const_iterator sitr
			= new_ol.stations().begin();
				sitr != new_ol.stations().end(); ++sitr) {
			stations_used[sitr->first] = true;
			for (std::vector<std::pair<StationID, bool> >::const_iterator sitr2 = sitr;
				sitr2 != sitr && unique; ++sitr2) {
				unique = unique && (sitr->first != sitr2->first);
			}
		}

		for (std::map<cargo_label_t, comm::cargo_info>::const_iterator citr = new_ol.cargo().begin();
			citr != new_ol.cargo().end(); ++citr) {
			cargo_used.insert(citr->first);
		}

		new_ol.is_cycle = (new_ol.stations().size() > 2) && unique;
		// bi-cycle at this point is yet forbidden
		// you could have a train running A->B->C->D->A->D->C->B,
		// however, passengers wanting to go C->B might get
		// confused that their train turns around
		
		node_list.init_nodes(new_ol);
		file.order_lists().push_back(new_ol);
	}
}

float coord_of(uint orig)
{
	const float scale = 25.0f;
	const uint offset = 2.0f;
	return orig / scale + offset;
}

void VideoDriver_Railnet::SaveStation(comm::railnet_file_info& file, const struct BaseStation* st,
	const std::vector<bool> &stations_used) const
{
	static char buf[256];
	if (stations_used[st->index]) {
		const TileIndex& center = st->train_station.GetCenterTile();
		SetDParam(0, st->index); GetString(buf, STR_STATION_NAME, lastof(buf));

		comm::station_info tmp_station;
		tmp_station.name.get() = buf;
		tmp_station.x.get() = coord_of(MapSizeX() - TileX(center));
		tmp_station.y.get() = coord_of(MapSizeY() - TileY(center));
		file.stations().insert(std::make_pair(st->index, tmp_station));
	}
}

void VideoDriver_Railnet::SaveCargoLabels(comm::railnet_file_info &file, std::set<CargoLabel>& s) const
{
	int count = 0;
	for (std::set<CargoLabel>::const_iterator itr = s.begin(); itr != s.end(); ++itr)
	 file.cargo_names().insert(std::make_pair(++count, *itr));
}

void VideoDriver_Railnet::MainLoop()
{
	{
		GameLoop();
		UpdateWindows();
	}

	if (!detail::is_same<CargoLabel, uint32>::value ||
		!detail::is_same<StationID, uint16>::value ||
		!detail::is_same<UnitID, uint16>::value) {
		std::cerr << "Error! Types changed in OpenTTD. "
			"Programmers must fix this here." << std::endl;
		assert(false);
	}

	comm::railnet_file_info file;

	std::vector<bool> stations_used;
	std::set<CargoLabel> cargo_used;
	stations_used.resize(_station_pool.size, false);

	const Train* train;
	std::size_t n_trains = 0, cur_train = 0;
	FOR_ALL_TRAINS(train) {
		(void) train;
		++ n_trains;
	}

	std::set<const OrderList*> order_lists_done;
	std::cerr << "Calculating order lists... ";
	node_list_t node_list;
	FOR_ALL_TRAINS(train) {
		std::cerr << std::setw(3) << cur_train*100/n_trains << "%";
		SaveOrderList(file, train, stations_used, cargo_used, order_lists_done, node_list);
		std::cerr << "\b\b\b\b";
		++cur_train;
	}
	std::cerr << "100%" << std::endl;

	// did we forget any order list?
	bool any_problems = false;
	FOR_ALL_TRAINS(train) {
		if (train->IsFrontEngine() && train->orders.list &&
			order_lists_done.find(train->orders.list) == order_lists_done.end()) {
				std::cerr << "Warning: Could not compute order list for train #"
					<< train->unitnumber << std::endl;
				any_problems = true;
		}
	}
	if (any_problems) {
		std::cerr << "Warning: Some trains' order lists could not be computed" << std::endl
		<< "  and will be left out. This often means that" << std::endl
		<< "  a train's order list contains stations A and B" << std::endl
		<< "  where B could not be found reachable from A." << std::endl
		<< "  Hint: make sure that all stations *requiring* turnarounds" << std::endl
		<< "  have *explicit* orders." << std::endl;
	}

	const BaseStation* st;
	std::cerr << "Calculating stations..." << std::endl;
	FOR_ALL_BASE_STATIONS(st) { SaveStation(file, st, stations_used); }

	std::cerr << "Calculating cargo labels..." << std::endl;
/*	for(const comm::order_list& ol : file.order_lists())
	for(const auto& pr : ol.cargo())
	{
		//cargo_
	}*/
	SaveCargoLabels(file, cargo_used);

	std::cerr << "Serializing " << file.order_lists.get().size() << " order lists..." << std::endl;
	//serialize(file, std::cout);
	//comm::json_ofile(std::cout) << comm::smem<comm::railnet_file_info, comm::s_railnet>(file);

	comm::prechecks(file);
	comm::json_ofile(std::cout) << file;
}

