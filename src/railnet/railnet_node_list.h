#include <map>
#include "common.h"

//! a list that hold for each node: for each order list passing,
//! it's the n'th stop
//! used for subset detection
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
		if(s)
		{
			std::cerr << "s" << std::endl;
		}
		if(u)
			std::cerr << "u" << std::endl;
		if(nth)
			std::cerr << "nth" << std::endl;
		if(nodes.size())
			std::cerr << "sz" << std::endl;
		nodes[s].emplace(u, nth);
	}
public:
	void init_nodes(const comm::order_list& ol)
	{
		std::size_t nth = 0;
		UnitID unit_no = ol.unit_number;

		for(const auto& pr : ol.stations.get())
		 if(pr.second) /* if train stops */
		  visit(unit_no, pr.first, nth++);

		if(ol.rev_unit_no != no_unit_no)
		{
			nth = 0;
			unit_no = ol.rev_unit_no;
			for(std::vector<std::pair<StationID, bool>>::const_reverse_iterator
				r = ol.stations.get().rbegin();
				r != ol.stations.get().rend(); ++r)
			 if(r->second) // if train stops
			  visit(unit_no, r->first, nth++);
		}
	}

	// to call this, the stations and cargo must be known already
	void init_rest(const comm::order_list& ol)
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
			for(const std::pair<CargoLabel, comm::cargo_info>& c :
				ol.cargo())
			 if(c.second.rev)
			  cargo[unit_no].insert(c.first);
		}
	}

	void init(const comm::order_list& ol) {
		init_nodes(ol);
		init_rest(ol);
	}


	enum direction_t
	{
		dir_upwards,
		dir_downwards,
		dir_none
	};

	struct super_info_t
	{
		//! the stored value in the first node (first relative to the current line)
		times_t station_no;
		direction_t upwards; // TODO: remove that?
		std::size_t last_station_no;
		bool can_be_short_train;
		std::size_t length;
	};

	//! @param value the value that is stored inside the node for this train
	std::size_t value_of(const std::multimap<UnitID, super_info_t>& supersets,
		const UnitID& train, const times_t& value, bool neg) const
	{
		std::size_t length = lengths.find(train)->second;
		const super_info_t& sinf = supersets.find(train)->second;

		// actually, it's just (value - sinf.station_no % length),
		// but lenght is added to avoid % on negative numbers
		// the associate braces avoid getting negative temporaries
		return (neg)
			? (((length + sinf.station_no) - value) % length)
			: (((length + value) - sinf.station_no) % length);
	}

	enum superset_type
	{
		no_supersets = 0,
		is_express_train = 1,
		is_short_train = 2,
		is_express_and_short_train = 4,
		is_same_train = 8
	};

	//! single step of @a traverse
	//! @param itr the current station for which we need to find an equivalent
	//! @param supersets the supersets set
	//! @param itr_1 the previous station
	bool follow_to_node(const std::vector<StationID>::const_iterator& itr,
		std::multimap<UnitID, super_info_t>& supersets,
		const std::vector<StationID>::const_iterator& itr_1,
		bool neg) const
	{
		const auto node_itr = nodes.find(*itr);
		if(node_itr == nodes.end())
		 return false;
		const node_info_t* info = &node_itr->second;

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
						(!no_express) && last_in_node != last_in_nodes.second; ++last_in_node)
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

	//! traverses one order list
	//! @param ol the order list
	//! @param matches the resulting matches will be stored here if non NULL
	//! @param neg whether to negate the direction of @a ol
	//! @return subset type of @a ol
	int traverse(const comm::order_list& ol, std::map<UnitID, superset_type>* matches,
		bool neg, bool ignore_cargo) const
	{
		if(matches)
		 matches->clear();

		std::vector<StationID> stations;
		for(const auto& pr : ol.stations.get())
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

		std::multimap<UnitID, super_info_t> supersets;

		// find first node where the train stops
		const node_info_t* station_0;
		const auto itr_0 = nodes.find(stations[0]);
		if(itr_0 == nodes.end())
		 return no_supersets;
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
			 supersets.emplace(pr.first, super_info_t { pr.second, dir_none, 0, true, lengths.find(pr.first)->second });
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
		 if(! follow_to_node(itr, supersets, itr-1, neg) )
		  return no_supersets;
		// last node: (end-1) -> (begin)
		follow_to_node(stations.begin(), supersets, stations.end() - 1, neg);

		int result = no_supersets;
		std::size_t m_length = lengths.find(ol.unit_number)->second;
		for(const auto& pr : supersets)
		{
#ifdef DEBUG_EXPRESS_TRAINS
			std::cerr << "train " << train
				<< ", other: " << pr.first
				<< " -> short? " << pr.second.can_be_short_train << std::endl;
#endif
			int tmp_mask = (pr.second.can_be_short_train
				? is_short_train : is_express_train);
			if(pr.second.length == m_length) {
				const auto& c_oth = cargo.at(pr.first);
				if(ignore_cargo ||
				std::includes(c_this.begin(), c_this.end(), c_oth.begin(), c_oth.end()))
				 tmp_mask |= is_same_train;
			}
			
			if(tmp_mask)
			{
				if(matches)
				 (*matches)[pr.first] = (node_list_t::superset_type)tmp_mask;
				result |= tmp_mask;
			}
		}

		return result;
	}
};

