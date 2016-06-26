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
#include <cstring> // TODO: cpp?
#include <list>
#include <string>
#include <cmath>
#include <algorithm>

#ifdef EXPORTER
#include <cstdint>
typedef uint8_t byte;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint16 StationID;
typedef uint16 UnitID; // TODO: remove or type assertion
typedef uint32 CargoLabel;
typedef byte CargoID;
#else
#include "../station_base.h"
#include "../transport_type.h"
#endif

#include "json_static.h"

const UnitID no_unit_no = std::numeric_limits<UnitID>::max();

//! Converter to convert between CargoLabels and C strings
union LblConvT
{
private:
	int lbl[2];
	char str[8];
public:
	LblConvT() : lbl{0,0} {}
	const char* Convert(int value)
	{
		lbl[0] = value;
		std::reverse(str, str + 4);
		return str;
	}
	void Convert(int value, char* res)
	{
		std::copy_n(Convert(value), 5, res);
	}
	int Convert(const char* value)
	{
		std::copy(value, value+4, str);
		std::reverse(str, str+4);
		return *lbl;
	}
};

/**
 * Wrapper around CargoLabel class, in order to treat it diferently
 * in the json file.
 */
struct CargoLabelT
{
	CargoLabel lbl;
	operator const CargoLabel&() const { return lbl; }
	operator CargoLabel&() { return lbl; }
	CargoLabelT() = default;
	CargoLabelT(CargoLabel lbl) : lbl(lbl) {}
};

namespace comm
{

/** string IDs, as used by the SMem class */
enum StrId
{
	S_UNIT_NUMBER,
	S_REV_UNIT_NO,
	S_IS_CYCLE,
	S_MIN_STATION,
	S_CARGO,
	S_STATIONS,
	S_REAL_STATIONS,
	S_NAME,
	S_X,
	S_Y,
	S_ORDER_LISTS,
	S_CARGO_NAMES,
	S_VERSION,
	S_FILETYPE,
	S_FWD,
	S_REV,
	S_SLICE,
	S_MIMETYPE,
	S_SIZE
};

struct CargoInfo
{
//	SMem<CargoLabel, s_label> label;
	SMem<bool, S_FWD> fwd;
	SMem<bool, S_REV> rev;
	SMem<int, S_SLICE> slice;
	bool is_bicycle() const { return fwd && rev; }
//	SMem<UnitID, s_unit_number> unit_number;
//	SMem<UnitID, s_unit_number> rev_unit_no;
/*	bool operator<(const CargoInfo& rhs) const {
		return (slice == rhs.slice) ? label < rhs.label
	    : slice < rhs.slice }*/
};

struct OrderList
{
	SMem<UnitID, S_UNIT_NUMBER> unit_number;
	SMem<UnitID, S_REV_UNIT_NO> rev_unit_no;
	SMem<bool, S_IS_CYCLE> is_cycle;
//	SMem<bool, S_IS_BICYCLE> is_bicycle; //! at least two trains that drive in opposite cycles
	SMem<std::map<CargoLabelT, CargoInfo>, S_CARGO> cargo; // cargo order and amount does not matter
	int next_cargo_slice;
	SMem<std::vector<std::pair<StationID, bool> >, S_STATIONS> stations;
	OrderList() :
		unit_number(no_unit_no),
		rev_unit_no(no_unit_no),
		is_cycle(false) /*, is_bicycle(false)*/, next_cargo_slice(1) /*,
		min_station(std::numeric_limits<StationID>::max()),
		real_stations(0)*/
	{
	}

}; // TODO: subclass OrderList_sortable? avoid serializing min_station

struct StationInfo
{
	SMem<std::string, S_NAME> name;
	SMem<float, S_X> x;
	SMem<float, S_Y> y;

	template<class T>
	T& rd(const char* name, T& rd) {
		switch(*name) {
			case 'x': return rd >> x;
			case 'y': return rd >> y;
			case 'n': return rd >> name;
			default: throw "expected x, y or name";
		}
	}
};

/*
struct RailnetFileInfo
{
	static const std::string hdr;
	static const uint version;
			default: throw "expected x, y or name";
		}
	}
};*/

//! The whole information of a railnet file
struct RailnetFileInfo
{
	static constexpr int _version = 0;
	SMem<std::string, S_MIMETYPE> hdr;
	SMem<int, S_VERSION> version;
	SMem<std::list<OrderList>, S_ORDER_LISTS> order_lists;
	SMem<std::map<StationID, StationInfo>, S_STATIONS> stations;
	/*FEATURE: char suffices, but need short to print it*/
	SMem<std::map<unsigned char, CargoLabelT>, S_CARGO_NAMES> cargo_names;

	RailnetFileInfo() : hdr("openttd/railnet"), version(_version) {}
};

/** Class assigning string to SMem ids */
struct RailnetStrings
{
	const char* StringNo(std::size_t id);
};

class RailnetOfile : public JsonOfile<RailnetOfile>, public RailnetStrings
{
//	using self=JsonOfile<railnet_writer>;
	using self=RailnetOfile;
public:
	using JsonOfile<RailnetOfile>::operator<<;

	self& operator<<(const OrderList& ol);
	self& operator<<(const StationInfo& si);
	self& operator<<(const CargoInfo& ci);
	self& operator<<(const RailnetFileInfo& file);

	self& operator<<(const CargoLabelT& c);
/*	JsonOfile& operator<<(const std::pair<const char, CargoLabel>& pr);
	JsonOfile& operator<<(const std::pair<CargoLabel, CargoInfo>& pr);*/
	RailnetOfile(std::ostream& os) : JsonOfile(*this, os) {}
};

class RailnetIfile : public JsonIfile<RailnetIfile>, public RailnetStrings
{
//	using self=JsonOfile<railnet_writer>;
	using self=RailnetIfile;
	friend class JsonIfile<RailnetIfile>;
	using JsonIfile<RailnetIfile>::operator>>;

	bool Once(OrderList& ol);
	bool Once(StationInfo& si);
	bool Once(CargoInfo& ci);
	bool Once(RailnetFileInfo& fi);
public:
	self& operator>>(OrderList& ol) { return *this >> StructDict(ol); }
	self& operator>>(StationInfo& si) { return *this >> StructDict(si); }
	self& operator>>(CargoInfo& ci) { return *this >> StructDict(ci); }
	self& operator>>(RailnetFileInfo& file) { return *this >> StructDict(file); }

	self& operator>>(CargoLabelT& c);
//	Crtp& operator>>(std::pair<char, CargoLabel>& pr);
	RailnetIfile(std::istream& is) : JsonIfile(*this, is) {}
};

//! @a yet unused
void Prechecks(const RailnetFileInfo& file);

} // namespace comm

#endif

