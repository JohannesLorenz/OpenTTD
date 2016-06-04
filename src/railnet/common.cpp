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
	"rev_unit_no",
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
	"slice",
	"railnet",
	"mimetype",
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
//	serialize(ol.unit_number, o);
	serialize(ol.is_cycle, o);
	serialize(ol.is_bicycle, o);
//	serialize(ol.min_station, o);
	serialize(ol.cargo, o);
	serialize(ol.stations, o);
	serialize(ol.unit_number, o);
	serialize(ol.rev_unit_no, o);
}

void deserialize(order_list &ol, std::istream &i)
{
//	deserialize(ol.unit_number, i);
	deserialize(ol.is_cycle, i);
	deserialize(ol.is_bicycle, i);
//	deserialize(ol.min_station, i);
	deserialize(ol.cargo, i);
	deserialize(ol.stations, i);
	deserialize(ol.unit_number, i);
	deserialize(ol.rev_unit_no, i);
}

json_ifile&json_ifile::operator>>(bool& b)
{
	char tmp[6];
	tmp[5]=tmp[4]=0;
	*is >> tmp[0];
	if(!chk_end(*tmp))
	{
		*is >> tmp[1];
		*is >> tmp[2];
		*is >> tmp[3];
		if(!strncmp(tmp, "true", 4))
			b = true;
		else
		{
			*is >> tmp[4];
			if(!strncmp(tmp, "false", 5))
				b = false;
			else throw "boolean must be 'true' or 'false'";
		}
	}
	return *this;
}

json_ifile&json_ifile::operator>>(std::string& s)
{
	char tmp; *is >> tmp;
	if(!chk_end(tmp)) {
		assert_q(tmp);
		do { is->get(tmp); if(tmp != '\"') s.push_back(tmp); } while (tmp != '\"');
	}
	return *this;
}

json_ifile&json_ifile::operator>>(char* s)
{
	char tmp; *is >> tmp;
	if(!chk_end(tmp)) {
		assert_q(tmp);
		do { is->get(tmp); if(tmp != '\"') *(s++) = tmp; } while (tmp != '\"');
		*s = 0;
	}
	return *this;
}

bool json_ifile::once(order_list& ol)
{
	bool ret =	   _try(ol.is_cycle)
			|| _try(ol.is_bicycle)
			//		|| _try(ol.min_station)
			|| _try(ol.cargo)
			|| _try(ol.stations)
			|| _try(ol.unit_number)
			|| _try(ol.rev_unit_no);
	recent.clear();

	if(ret)
		return true;
	else
	{
		if(recent == "}")
			recent.clear();
		else
			throw "error";
		return false;		}
}

json_ofile& json_ofile::operator<<(const order_list& ol)
{
	struct_guard _(*this);
	return *this
		<< ol.is_cycle
		<< ol.is_bicycle
//		<< ol.min_station
		<< ol.cargo
		<< ol.stations
		<< ol.unit_number
		<< ol.rev_unit_no;
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
	recent.clear();
	if(ret)
	 return true;
	else
	{
		if(recent == "}")
		 recent.clear();
		else
		 throw "error";
		return false;	}
}

bool json_ifile::once(cargo_info& ci)
{
	bool ret = /*_try(ci.label) || */_try(ci.fwd) || _try(ci.rev)
		|| _try(ci.slice) /* || _try(ci.unit_number)*/;
	// TODO: just operator|| ?
	recent.clear();
	if(ret)
	 return true;
	else
	{
		if(recent == "}")
		 recent.clear();
		else
		 throw "error";
		return false;
	}
}

/*json_ifile& json_ifile::operator>>(std::pair<char, CargoLabel>& pr)
{
	static lbl_conv_t lbl_conv;
	std::pair<byte, char[5]> p2;
	*this >> p2;
	pr.first = p2.first;
	pr.second = lbl_conv.convert(p2.second);
	return *this;
}*/

json_ifile& json_ifile::operator>>(cargo_label_t& c)
{
	static lbl_conv_t lbl_conv;
	char tmp[5];
	*this >> tmp;
	c = lbl_conv.convert(tmp);
	return *this;
}

json_ofile& json_ofile::operator<<(const station_info& si)
{
	struct_guard _(*this);
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
	assert(found_hdr=="openttd/railnet");
	uint version;
	deserialize(version, i);
	assert(version==0);
	deserialize(file.order_lists, i);
	deserialize(file.stations, i);
	deserialize(file.cargo_names, i);
}

bool json_ifile::once(railnet_file_info& fi)
{
	// TODO: check version + filetype
	smem<std::string, s_mimetype> tmp_hdr;
	smem<int, s_version> tmp_version = -1;

	bool ret = _try(tmp_hdr)
		|| _try(tmp_version)
		|| _try(fi.order_lists)
		|| _try(fi.stations)
		|| _try(fi.cargo_names);
	recent.clear();
	if(ret)
	{
		// FEATURE: put this into operator<< ? i.e.:
		//	tmp = read..., if... throw? instead of
		//	*this = read
		if(tmp_version() != -1 && tmp_version != fi.version)
			throw "version mismatch";
		else if(tmp_hdr().length() && tmp_hdr() != fi.hdr())
			throw "header signature does not match";
		else return true;
	}
	else
	{
		if(recent == "}")
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
	struct_guard _(*this);
	return *this << fi.hdr
		<< fi.version
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
//	serialize(ci.unit_number, o);
}

void deserialize(cargo_info& ci, std::istream& i)
{
//	deserialize(ci.label, i);
	deserialize(ci.fwd, i);
	deserialize(ci.rev, i);
	deserialize(ci.slice, i);
//	deserialize(ci.unit_number, i);
}

json_ofile& json_ofile::operator<<(const cargo_info& ci)
{
	struct_guard _(*this);
	return *this <</* ci.label <<*/ ci.fwd << ci.rev << ci.slice;
	//<< ci.unit_number;
}
/*
json_ofile& json_ofile::operator<<(const std::pair<const char, CargoLabel>& pr)
{
	// TODO: avoid things like "short", they may not fit the 16bit scheme
	static lbl_conv_t lbl_conv;
	std::pair<unsigned short, char[5]> p2;
	p2.first = pr.first;
	lbl_conv.convert(pr.second, p2.second);
	return *this << p2;
}*/

json_ofile& json_ofile::operator<<(const cargo_label_t& c)
{
	static lbl_conv_t lbl_conv;
	return *this << lbl_conv.convert(c);
}

void prechecks(const railnet_file_info& )
{
	// FEATURE, not done yet
/*	for(const order_list& ol : file.order_lists())
	{
		bool fwd, rev;
		if(ol.cargo().size() > 1)
		{
			fwd = ol.cargo().begin()->second.fwd;
			rev = ol.cargo().begin()->second.rev;
			for(auto itr = ol.cargo().begin(); itr != ol.cargo().end(); ++itr)
			{
				if(itr->second.fwd != fwd || itr->second.rev != rev)
				 throw "precheck error: two cargos in slice with different fwd/rev flags";
			}
		}
	}*/
}

/*
json_ofile& json_ofile::operator<<(const std::pair<CargoLabel, cargo_info>& pr)
{
	static lbl_conv_t lbl_conv;
	std::pair<char[5], cargo_info> p2;
	p2.first = pr.first;
	lbl_conv.convert(pr.first, p2.first);
	return *this << p2;
}*/

}

