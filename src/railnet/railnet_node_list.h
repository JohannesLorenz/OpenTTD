/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file railnet_node_list.h defines the node_list_t class
 */

#include <map>
#include "common.h"

#ifdef COMPILE_RAILNET
#ifndef RAILNET_NODE_LIST_H
#define RAILNET_NODE_LIST_H

//! a list that hold for each node: for each order list passing,
//! it's the n'th stop
//! used for subset detection
class NodeListT
{
	using times_t = unsigned short;
	using NodeInfoT = std::multimap<UnitID, times_t>;

	//! @node: C++11 guarantees that equivalent keys remain
	//! in the insertion order. we insert them sorted by key...
	std::map<StationID, NodeInfoT> nodes;
	std::map<UnitID, times_t> lengths;
	std::map<UnitID, std::set<CargoLabel>> cargo;

	void Visit(UnitID u, StationID s, times_t nth);
public:
	void InitNodes(const comm::OrderList& ol);

	// to call this, the stations and cargo must be known already
	void InitRest(const comm::OrderList& ol);

	void Init(const comm::OrderList& ol);


	enum DirectionT
	{
		DIR_UPWARDS,
		DIR_DOWNWARDS,
		DIR_NONE
	};

	struct SuperInfoT
	{
		//! the stored value in the first node (first relative to the current line)
		times_t station_no;
		DirectionT upwards; // TODO: remove that?
		std::size_t last_station_no;
		bool can_be_short_train;
		std::size_t length;
	};

	//! @param value the value that is stored inside the node for this train
	std::size_t value_of(const std::multimap<UnitID, SuperInfoT>& supersets,
		const UnitID& train, const times_t& value, bool neg) const;

	enum SupersetType
	{
		NO_SUPERSETS = 0,
		IS_EXPRESS_TRAIN = 1,
		IS_SHORT_TRAIN = 2,
		IS_EXPRESS_AND_SHORT_TRAIN = 4,
		IS_SAME_TRAIN = 8
	};

	//! single step of @a traverse
	//! @param itr the current station for which we need to find an equivalent
	//! @param supersets the supersets set
	//! @param itr_1 the previous station
	bool FollowToNode(const std::vector<StationID>::const_iterator& itr,
		std::multimap<UnitID, SuperInfoT>& supersets,
		const std::vector<StationID>::const_iterator& itr_1,
		bool neg) const;

	//! traverses one order list, keeping track of all possible
	//! superset order lists
	//! @param ol the order list
	//! @param matches the resulting matches will be stored here if non NULL
	//! @param neg whether to negate the direction of @a ol
	//! @return subset type of @a ol
	int Traverse(const comm::OrderList& ol, std::map<UnitID, SupersetType>* matches,
		bool neg, bool ignore_cargo) const;
};

#endif // RAILNET_NODE_LIST_H
#endif

