/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file railnet_node_list.h implementation of the node_list_t class
 */

#include "railnet_node_list.h"

void NodeListT::Visit(UnitID u, StationID s, NodeListT::times_t nth) {
	nodes[s].emplace(u, nth);
}

void NodeListT::InitNodes(const comm::OrderList &ol)
{
	std::size_t nth = 0;
	UnitID unit_no = ol.unit_number;

	for(const auto& pr : ol.stations.Get())
	if(pr.second) /* if train stops */
		Visit(unit_no, pr.first, nth++);

	if(ol.rev_unit_no != no_unit_no)
	{
		nth = 0;
		unit_no = ol.rev_unit_no;
		for(std::vector<std::pair<StationID, bool>>::const_reverse_iterator
			r = ol.stations.Get().rbegin();
			r != ol.stations.Get().rend(); ++r)
		{
			if(r->second) // if train stops
			 Visit(unit_no, r->first, nth++);
		}
	}
}

void NodeListT::InitRest(const comm::OrderList &ol)
{
	// assumption: same slice => same unit number/same reverse unit number
	UnitID unit_no = ol.unit_number;
	lengths[unit_no] = ol.stations().size();

	for(const auto& ci : ol.cargo())
		if(ci.second.fwd)
			cargo[unit_no].insert(ci.first);

	if(ol.rev_unit_no != no_unit_no)
	{
		unit_no = ol.rev_unit_no;
		lengths[unit_no] = ol.stations().size();
		for(const std::pair<CargoLabel, comm::CargoInfo>& c :
			ol.cargo())
		if(c.second.rev)
			cargo[unit_no].insert(c.first);
	}
}

void NodeListT::Init(const comm::OrderList &ol) {
	InitNodes(ol);
	InitRest(ol);
}

std::size_t NodeListT::value_of(const std::multimap<UnitID, NodeListT::SuperInfoT> &supersets, const UnitID &train, const NodeListT::times_t &value, bool neg) const
{
	std::size_t length = lengths.find(train)->second;
	const SuperInfoT& sinf = supersets.find(train)->second;

	// actually, it's just (value - sinf.station_no % length),
	// but lenght is added to avoid % on negative numbers
	// the associate braces avoid getting negative temporaries
	return (neg)
		? (((length + sinf.station_no) - value) % length)
		: (((length + value) - sinf.station_no) % length);
}

bool NodeListT::FollowToNode(const std::vector<StationID>::const_iterator &itr,
	std::multimap<UnitID, NodeListT::SuperInfoT> &supersets,
	const std::vector<StationID>::const_iterator &itr_1,
	bool neg) const
{
	const auto node_itr = nodes.find(*itr);
	if(node_itr == nodes.end())
		return false;
	const NodeInfoT* info = &node_itr->second;

	auto next = supersets.begin();
	for(auto sitr = supersets.begin(); sitr != supersets.end(); sitr = next)
	{
		++(next = sitr);

		UnitID cur_line = sitr->first;
		std::size_t& last_station_no = sitr->second.last_station_no;
		std::size_t new_last_station_no = std::numeric_limits<std::size_t>::max();

		const auto in_nodes = info->equal_range(cur_line);
		std::size_t tmp = lengths.at(cur_line);
		for(auto in_node = in_nodes.first;
			in_node != in_nodes.second; ++in_node)
		{
			tmp = value_of(supersets, cur_line, in_node->second, neg);
			if(tmp == 0)
				tmp = lengths.at(cur_line);
			if(tmp >= last_station_no && // is it a future node?
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
			if(last_station_no < new_last_station_no - 1) // some nodes between...
			{
				no_express = false;
				for(auto last_in_node = last_in_nodes.first;
					(!no_express) && last_in_node != last_in_nodes.second;
					++last_in_node)
				{
					std::size_t value_found_in_last = value_of(supersets, cur_line, last_in_node->second, neg);
					if(last_station_no < value_found_in_last
							&& value_found_in_last == (new_last_station_no-1))
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
	return true;
}

int NodeListT::Traverse(const comm::OrderList &ol, std::map<UnitID, NodeListT::SupersetType> *matches, bool neg, bool ignore_cargo) const
{
	if(matches)
		matches->clear();

	std::vector<StationID> stations;
	for(const auto& pr : ol.stations.Get())
		if(pr.second)
			stations.push_back(pr.first);

	UnitID train = ol.unit_number;

	enum class Masks
	{
		M_SAME = 1,
		M_OPPOSITE = 2,
		M_REMOVE = 4,
		M_TWO_DIRECTIONS = 3
	};

	std::multimap<UnitID, SuperInfoT> supersets;

	// find first node where the train stops
	const NodeInfoT* station_0;
	const auto itr_0 = nodes.find(stations[0]);
	if(itr_0 == nodes.end())
		return NO_SUPERSETS;
	else
		station_0 = &itr_0->second;

	// fill map for the first node
	for(const auto& pr : *station_0)
	if(pr.first != train)
	{
		const auto& c_oth = cargo.at(pr.first);
		const auto& c_this = cargo.at(train);
		if(ignore_cargo
			|| std::includes(c_oth.begin(), c_oth.end(), c_this.begin(), c_this.end()))
			supersets.emplace(pr.first, SuperInfoT { pr.second, DIR_NONE, 0, true, lengths.find(pr.first)->second });
	}

	const auto& c_this = cargo.at(train);

	for(const auto& pr : *station_0)
	if(pr.first != train)
	{
		const auto& c_oth = cargo.at(pr.first);
		if(ignore_cargo
			|| std::includes(c_oth.begin(), c_oth.end(), c_this.begin(), c_this.end()))
		if(value_of(supersets, pr.first, station_0->find(pr.first)->second, neg) != 0)
		 throw "Internal error: invalid value computation";
	}

	// follow the order list, find monotonically increasing superset line
	for(auto itr = stations.begin() + 1; itr != stations.end(); ++itr)
	if(! FollowToNode(itr, supersets, itr-1, neg) )
	 return NO_SUPERSETS;
	// last node: (end-1) -> (begin)
	FollowToNode(stations.begin(), supersets, stations.end() - 1, neg);

	int result = NO_SUPERSETS;
	std::size_t m_length = lengths.find(ol.unit_number)->second;
	for(const auto& pr : supersets)
	{
#ifdef DEBUG_EXPRESS_TRAINS
		std::cerr << "train " << train
			  << ", other: " << pr.first
			  << " -> short? " << pr.second.can_be_short_train << std::endl;
#endif
		int tmp_mask = (pr.second.can_be_short_train
				? IS_SHORT_TRAIN : IS_EXPRESS_TRAIN);
		if(pr.second.length == m_length) {
			const auto& c_oth = cargo.at(pr.first);
			if(ignore_cargo ||
				std::includes(c_this.begin(), c_this.end(), c_oth.begin(), c_oth.end()))
			 tmp_mask |= IS_SAME_TRAIN;
		}

		if(tmp_mask)
		{
			if(matches)
				(*matches)[pr.first] = (SupersetType)tmp_mask;
			result |= tmp_mask;
		}
	}

	return result;
}
