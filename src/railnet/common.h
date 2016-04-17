/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file common.h Common data structures shared by
 *	the railnet video driver (ingame) and the railnet utility
 */

#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <vector>
#include <set>
#include <iosfwd>
#include <limits>

#ifdef EXPORTER
#include <cstdint>
typedef uint8_t byte;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint16 StationID;
typedef uint16 UnitID; // TODO: remove or type assertion
typedef uint32 CargoLabel;
#else
#include "../station_base.h"
#include "../transport_type.h"
#endif

namespace comm
{

struct order_list
{
	UnitID unit_number;
	bool is_cycle;
	bool is_bicycle; //! at least two trains that drive in opposite cycles
	StationID min_station;
	std::set<CargoLabel> cargo; // cargo order and amount does not matter
	std::vector<std::pair<StationID, bool> > stations;
	std::size_t real_stations;
	bool operator<(const order_list& other) const;
	order_list() : is_cycle(false), is_bicycle(false),
		min_station(std::numeric_limits<StationID>::max()),
		real_stations(0)
	{
	}
};

struct station_info
{
	std::string name;
	float x, y;
};

struct railnet_file_info
{
	static const std::string hdr;
	static const uint version;
	std::multiset<order_list> order_lists;
	std::map<StationID, station_info> stations;
	std::map<char, CargoLabel> cargo_names;
};


/*
	IO primitives:
*/

namespace dtl
{

void _wrt(std::ostream& o, const char* raw, std::size_t sz);
void _rd(std::istream& i, char* raw, std::size_t sz);

template<class T>
inline void wrt(const T& raw, std::ostream& o) {
	_wrt(o, reinterpret_cast<const char*>(&raw), sizeof(raw));
}

template<class T>
inline void rd(T& raw, std::istream& i) {
	_rd(i, reinterpret_cast<char*>(&raw), sizeof(raw));
}

}
inline void serialize(const byte& b, std::ostream& o) { dtl::wrt(b, o); }
inline void serialize(const uint16& i, std::ostream& o) { dtl::wrt(i, o); }
inline void serialize(const uint32& i, std::ostream& o) { dtl::wrt(i, o); }
inline void serialize(const bool& b, std::ostream& o) { dtl::wrt(b, o); }
inline void serialize(const char& c, std::ostream& o) { dtl::wrt(c, o); }
inline void serialize(const float& f, std::ostream& o) { dtl::wrt(f, o); }

inline void deserialize(byte& b, std::istream& i) { dtl::rd(b, i); }
inline void deserialize(uint16& i, std::istream& is) { dtl::rd(i, is); }
inline void deserialize(uint32& i, std::istream& is) { dtl::rd(i, is); }
inline void deserialize(bool& b, std::istream& i) { dtl::rd(b, i); }
inline void deserialize(char& c, std::istream& i) { dtl::rd(c, i); }
inline void deserialize(float& f, std::istream& i) { dtl::rd(f, i); }

/*
	IO containers/structs:
*/

template<class T1, class T2>
inline void serialize(const std::pair<T1, T2>& p, std::ostream& o);

template<class T1, class T2>
inline void deserialize(std::pair<T1, T2>& p, std::istream& i);

//! sfinae based container serializer
template<class Container>
void serialize(const Container& c, std::ostream& o,
	const typename Container::const_iterator* = NULL) {
	uint32_t sz = c.size();
	serialize(sz, o);
	for(typename Container::const_iterator itr = c.begin();
		itr != c.end(); ++itr)
		serialize(*itr, o);
}

namespace dtl {

//! generic push back function for all types of containers
template<class Cont> void push_back(Cont& c,
	const typename Cont::value_type& val) {
	c.insert(c.end(), val);
}

template<class Container>
struct noconst_value_type_of {
	typedef typename Container::value_type type;
};

template<class T1, class T2>
struct noconst_value_type_of<std::map<T1, T2> > {
	// this fixes that map's value type is not std::pair<const T1, T2>
	typedef typename std::pair<T1, T2> type;
};

}

//! sfinae based container deserializer
template<class Container>
void deserialize(Container& c, std::istream& is,
	const typename Container::const_iterator* = NULL)
{
	uint32 sz;
	deserialize(sz, is);
	for(uint32 i = 0; i<sz; ++i)
	{
		typename dtl::noconst_value_type_of<Container>::type val;
		deserialize(val, is);
		dtl::push_back(c, val);
	}
}

template<class T1, class T2>
inline void serialize(const std::pair<T1, T2>& p, std::ostream& o) {
	serialize(p.first, o); serialize(p.second, o); }

template<class T1, class T2>
inline void deserialize(std::pair<T1, T2>& p, std::istream& i) {
	deserialize(p.first, i); deserialize(p.second, i); }

void serialize(const order_list& ol, std::ostream& o);
void serialize(const station_info& si, std::ostream& o);
void serialize(const railnet_file_info& file, std::ostream& o);

void deserialize(order_list& ol, std::istream& i);
void deserialize(station_info& si, std::istream &i);
void deserialize(railnet_file_info& file, std::istream &i);

} // namespace comm

#endif // COMMON_H

