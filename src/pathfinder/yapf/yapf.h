/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf.h Entry point for OpenTTD to YAPF. */

#ifndef YAPF_H
#define YAPF_H

#include "../../direction_type.h"
#include "../../track_type.h"
#include "../../vehicle_type.h"
#include "../pathfinder_type.h"

/**
 * Finds the best path for given ship using YAPF.
 * @param v        the ship that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the ship is about to enter)
 * @param enterdir diagonal direction which the ship will enter this new tile from
 * @param tracks   available tracks on the new tile (to choose from)
 * @param path_found [out] Whether a path has been found (true) or has been guessed (false)
 * @return         the best trackdir for next turn or INVALID_TRACK if the path could not be found
 */
Track YapfShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found);

/**
 * Returns true if it is better to reverse the ship before leaving depot using YAPF.
 * @param v the ship leaving the depot
 * @return true if reversing is better
 */
bool YapfShipCheckReverse(const Ship *v);

/**
 * Finds the best path for given road vehicle using YAPF.
 * @param v         the RV that needs to find a path
 * @param tile      the tile to find the path from (should be next tile the RV is about to enter)
 * @param enterdir  diagonal direction which the RV will enter this new tile from
 * @param trackdirs available trackdirs on the new tile (to choose from)
 * @param path_found [out] Whether a path has been found (true) or has been guessed (false)
 * @return          the best trackdir for next turn or INVALID_TRACKDIR if the path could not be found
 */
Trackdir YapfRoadVehicleChooseTrack(const RoadVehicle *v, TileIndex tile, DiagDirection enterdir, TrackdirBits trackdirs, bool &path_found);

/**
 * Finds the best path for given train using YAPF.
 * @param v        the train that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the train is about to enter)
 * @param enterdir diagonal direction which the RV will enter this new tile from
 * @param tracks   available trackdirs on the new tile (to choose from)
 * @param path_found [out] Whether a path has been found (true) or has been guessed (false)
 * @param reserve_track indicates whether YAPF should try to reserve the found path
 * @param target   [out] the target tile of the reservation, free is set to true if path was reserved
 * @return         the best track for next turn
 */
Track YapfTrainChooseTrack(const Train *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, bool reserve_track, struct PBSTileInfo *target);

/**
 * Used when user sends road vehicle to the nearest depot or if road vehicle needs servicing using YAPF.
 * @param v            vehicle that needs to go to some depot
 * @param max_penalty  max distance (in pathfinder penalty) from the current vehicle position
 *                     (used also as optimization - the pathfinder can stop path finding if max_penalty
 *                     was reached and no depot was seen)
 * @return             the data about the depot
 */
FindDepotData YapfRoadVehicleFindNearestDepot(const RoadVehicle *v, int max_penalty);

/**
 * Used when user sends train to the nearest depot or if train needs servicing using YAPF.
 * @param v            train that needs to go to some depot
 * @param max_distance max distance (int pathfinder penalty) from the current train position
 *                     (used also as optimization - the pathfinder can stop path finding if max_penalty
 *                     was reached and no depot was seen)
 * @return             the data about the depot
 */
FindDepotData YapfTrainFindNearestDepot(const Train *v, int max_distance);

/**
 * Returns true if it is better to reverse the train before leaving station using YAPF.
 * @param v the train leaving the station
 * @return true if reversing is better
 */
bool YapfTrainCheckReverse(const Train *v);

/**
 * Try to extend the reserved path of a train to the nearest safe tile using YAPF.
 *
 * @param v    The train that needs to find a safe tile.
 * @param tile Last tile of the current reserved path.
 * @param td   Last trackdir of the current reserved path.
 * @param override_railtype Should all physically compatible railtypes be searched, even if the vehicle can't run on them on its own?
 * @return True if the path could be extended to a safe tile.
 */
bool YapfTrainFindNearestSafeTile(const Train *v, TileIndex tile, Trackdir td, bool override_railtype);

//! @a a functor that can be called on stations, waypoints and depots
class StationFtor
{
	virtual void OnTile(const struct Station& st, const TileIndex& t, const Trackdir& td, int cost) = 0;
	virtual void OnTile(const struct Waypoint& wp, const TileIndex& t, const Trackdir& td, int cost) = 0;
	virtual void OnTile(const struct Depot& dp, const TileIndex& t, const Trackdir& td, int cost) = 0;
public:
	template<class T>
	void operator()(const T& building, const TileIndex& t, const Trackdir& td, int cost) {
		OnTile(building, t, td, cost); }
	virtual ~StationFtor() = 0;
};

inline StationFtor::~StationFtor() {}

/**
 * Finds the best path for given train using YAPF and
 * executes a @a StationFtor for each station to target, in reverse order
 * @param v             the train that needs to find a path
 * @param path_found    [out] Whether a path has been found (true) or not (false)
 * @param target        [out] the target tile of the reservation, free is set to true if path was reserved
 * @param ftor          station functor to be executed along the best path in reverse order
 * @param orig          origin, or INVALID_TILE if search should start at @a v
 * @param orig_dir      direction to start at @a orig. ignored if @a orig == INVALID_TILE
 * @param current_order order, containing the destination
 * @param best_cost     functor will only be called if computed cost < best_cost
 * @param try_reverse   whether to try in the reverse direction of @a orig_dir if no path was found
 */
void YapfTrainStationsToTarget(const Train *v, bool &path_found, struct PBSTileInfo *target,
	StationFtor& ftor, TileIndex orig, Trackdir orig_dir, const struct Order &current_order,
	int best_cost, bool try_reverse = true);

#endif /* YAPF_H */
