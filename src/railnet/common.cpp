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
	"rev_unit_no"
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
	"cargo_names",
	"version",
	"filetype",
	"label",
	"fwd",
	"rev",
	"slice"
};

const char* string_no(std::size_t id) { return strings[id]; }

/*bool order_list::operator<(const order_list &other) const
{
	return (min_station == other.min_station)
		? real_stations < other.real_stations
		: min_station < other.min_station;
}*/

void serialize(const order_list &ol, std::ostream &o)
{
	serialize(ol.unit_number, o);
	serialize(ol.is_cycle, o);
	serialize(ol.is_bicycle, o);
//	serialize(ol.min_station, o);
	serialize(ol.cargo, o);
	serialize(ol.stations, o);
}

void deserialize(order_list &ol, std::istream &i)
{
	deserialize(ol.unit_number, i);
	deserialize(ol.is_cycle, i);
	deserialize(ol.is_bicycle, i);
//	deserialize(ol.min_station, i);
	deserialize(ol.cargo, i);
	deserialize(ol.stations, i);
}

bool json_ifile::once(order_list& ol)
{
	bool ret = _try(ol.unit_number)
		|| _try(ol.is_cycle)
		|| _try(ol.is_bicycle)
//		|| _try(ol.min_station)
		|| _try(ol.cargo)
		|| _try(ol.stations);
	if(ret)
	 return true;
	else
	{
		if(recent == "[")
		 recent.clear();
		else
		 throw "error";
		return false;		}
}

json_ofile& json_ofile::operator<<(const order_list& ol)
{
	return *this << ol.unit_number
		<< ol.is_cycle
		<< ol.is_bicycle
//		<< ol.min_station
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

bool json_ifile::once(station_info& si)
{
	bool ret = _try(si.name)
		|| _try(si.x)
		|| _try(si.y);
	if(ret)
	 return true;
	else
	{
		if(recent == "[")
		 recent.clear();
		else
		 throw "error";
		return false;	}
}

bool json_ifile::once(cargo_info& ci)
{
	bool ret = /*_try(ci.label) || */_try(ci.fwd) || _try(ci.rev)
		|| _try(ci.slice); // TODO: just operator|| ?
	if(ret)
	 return true;
	else
	{
		if(recent == "[")
		 recent.clear();
		else
		 throw "error";
		return false;	}
}

json_ofile& json_ofile::operator<<(const station_info& si)
{
	return *this << si.name
		<< si.x
		<< si.y;
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
bool json_ifile::once(railnet_file_info& fi)
{
	smem<std::string, s_filetype> filetype;
	smem<uint, s_version> version;
	// TODO: check version + filetype	
	bool ret = _try(filetype)
		|| _try(version)
		|| _try(fi.order_lists)
		|| _try(fi.stations)
		|| _try(fi.cargo_names);
	if(ret)
	 return true;
	else
	{
		if(recent == "[")
		 recent.clear();
		else
		 throw "error";
		return false;
	}
/*	if(ret) return *this;
	else throw "no key for railnet_file_info matched the found key"; */
}

json_ofile& json_ofile::operator<<(const railnet_file_info& fi)
{
	return *this << railnet_file_info::hdr
		<< railnet_file_info::version
		<< fi.order_lists
		<< fi.stations
		<< fi.cargo_names;
}

void serialize(const cargo_info& ci, std::ostream& o)
{
//	serialize(ci.label, o);
	serialize(ci.fwd, o);
	serialize(ci.rev, o);
	serialize(ci.slice, o);
}

void deserialize(cargo_info& ci, std::istream& i)
{
//	deserialize(ci.label, i);
	deserialize(ci.fwd, i);
	deserialize(ci.rev, i);
	deserialize(ci.slice, i);
}

json_ofile&json_ofile::operator<<(const cargo_info& ci)
{
	return *this <</* ci.label <<*/ ci.fwd << ci.rev << ci.slice;
}

}

