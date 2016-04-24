/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef EXPORTER
#include <cassert>
#else
#include "../stdafx.h"
#endif

#include <iostream>
#include "common.h"

namespace comm {

const std::string railnet_file_info::hdr = "openttd/railnet";
const uint railnet_file_info::version = 0;

namespace dtl {
	void _wrt(std::ostream& o, const char* raw, std::size_t sz) {
		o.write(raw, sz);
	}

	void _rd(std::istream& i, char* raw, std::size_t sz) {
		i.read(raw, sz);
	}
}

const char* strings[s_size] =
{
	"unit_number",
	"is_cycle",
	"is_bicycle",
	"min_station",
	"cargo",
	"stations",
	"real_stations",
	"name",
	"x",
	"y",
	"order_lists",
	"cargo_names"
};

const char* string_no(std::size_t id) { return strings[id]; }

bool order_list::operator<(const order_list &other) const
{
	return (min_station == other.min_station)
		? real_stations < other.real_stations
		: min_station < other.min_station;
}

void serialize(const order_list &ol, std::ostream &o)
{
	serialize(ol.unit_number, o);
	serialize(ol.is_cycle, o);
	serialize(ol.is_bicycle, o);
	serialize(ol.min_station, o);
	serialize(ol.cargo, o);
	serialize(ol.stations, o);
}

void deserialize(order_list &ol, std::istream &i)
{
	deserialize(ol.unit_number, i);
	deserialize(ol.is_cycle, i);
	deserialize(ol.is_bicycle, i);
	deserialize(ol.min_station, i);
	deserialize(ol.cargo, i);
	deserialize(ol.stations, i);
}

json_ifile& json_ifile::once(order_list& ol)
{
	bool ret = _try(ol.unit_number)
		|| _try(ol.is_cycle)
		|| _try(ol.is_bicycle)
		|| _try(ol.min_station)
		|| _try(ol.cargo)
		|| _try(ol.stations);
	if(ret) return *this;
	else throw "no key for order_list matched the found key";
}

json_ofile& json_ofile::operator<<(const order_list& ol)
{
	return *this << ol.unit_number
		<< ol.is_cycle
		<< ol.is_bicycle
		<< ol.min_station
		<< ol.cargo
		<< ol.stations;
}

void serialize(const station_info &si, std::ostream &o)
{
	serialize(si.name, o);
	serialize(si.x, o);
	serialize(si.y, o);
}

void deserialize(station_info &si, std::istream &i)
{
	deserialize(si.name, i);
	deserialize(si.x, i);
	deserialize(si.y, i);
}

void serialize(const railnet_file_info &file, std::ostream &o)
{
	serialize(file.hdr, o);
	serialize(file.version, o);
	serialize(file.order_lists, o);
	serialize(file.stations, o);
	serialize(file.cargo_names, o);
}

void deserialize(railnet_file_info &file, std::istream &i)
{
	std::string found_hdr;
	deserialize(found_hdr, i);
	assert(found_hdr==file.hdr);
	uint version;
	deserialize(version, i);
	assert(version==file.version);
	deserialize(file.order_lists, i);
	deserialize(file.stations, i);
	deserialize(file.cargo_names, i);
}

}

