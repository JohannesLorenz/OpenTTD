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

constexpr int RailnetFileInfo::_version;

const char* strings[S_SIZE] =
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

const char* StringNo(std::size_t id) { return strings[id]; }

/*bool order_list::operator<(const order_list &other) const
{
	return (min_station == other.min_station)
		? real_stations < other.real_stations
		: min_station < other.min_station;
}*/

void JsonIfileBase::ReadBool(bool& b, std::istream* is)
{
	char tmp[6];
	tmp[5]=tmp[4]=0;
	*is >> tmp[0];
	if(!ChkEnd(*tmp))
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
}

void JsonIfileBase::ReadString(std::string& s, std::istream* is)
{
	char tmp; *is >> tmp;
	if(!ChkEnd(tmp)) {
		assert_q(tmp);
		do { is->get(tmp); if(tmp != '\"') s.push_back(tmp); } while (tmp != '\"');
	}
}

void JsonIfileBase::ReadCStr(char* s, std::istream* is)
{
	char tmp; *is >> tmp;
	if(!ChkEnd(tmp)) {
		assert_q(tmp);
		do { is->get(tmp); if(tmp != '\"') *(s++) = tmp; } while (tmp != '\"');
		*s = 0;
	}
}

bool RailnetIfile::Once(OrderList& ol)
{
	bool ret =	   Try(ol.is_cycle)
			|| Try(ol.is_bicycle)
			//		|| Try(ol.min_station)
			|| Try(ol.cargo)
			|| Try(ol.stations)
			|| Try(ol.unit_number)
			|| Try(ol.rev_unit_no);
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

RailnetOfile& RailnetOfile::operator<<(const OrderList& ol)
{
	StructGuard _(*this);
	return	*this << ol.is_cycle
		<< ol.is_bicycle
//		<< ol.min_station
		<< ol.cargo
		<< ol.stations
		<< ol.unit_number
		<< ol.rev_unit_no;
}

bool RailnetIfile::Once(StationInfo& si)
{
	bool ret = Try(si.name)
		|| Try(si.x)
		|| Try(si.y);
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

bool RailnetIfile::Once(CargoInfo& ci)
{
	bool ret = /*Try(ci.label) || */Try(ci.fwd) || Try(ci.rev)
		|| Try(ci.slice) /* || Try(ci.unit_number)*/;
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

/*JsonIfile& JsonIfile::operator>>(std::pair<char, CargoLabel>& pr)
{
	static LblConvT lbl_conv;
	std::pair<byte, char[5]> p2;
	*this >> p2;
	pr.first = p2.first;
	pr.second = lbl_conv.Convert(p2.second);
	return *this;
}*/

RailnetIfile& RailnetIfile::operator>>(CargoLabelT& c)
{
	static LblConvT lbl_conv;
	char tmp[5];
	*this >> tmp;
	c = lbl_conv.Convert(tmp);
	return *this;
}

RailnetOfile& RailnetOfile::operator<<(const StationInfo& si)
{
	StructGuard _(*this);
	//return *this << si.name
	//	     << si.x
	//	     << si.y;
	return *this << si.name
	<< si.x
	<< si.y;
}

bool RailnetIfile::Once(RailnetFileInfo& fi)
{
	// TODO: check version + filetype
	SMem<std::string, S_MIMETYPE> tmp_hdr;
	SMem<int, S_VERSION> tmp_version = -1;

	bool ret = Try(tmp_hdr)
		|| Try(tmp_version)
		|| Try(fi.order_lists)
		|| Try(fi.stations)
		|| Try(fi.cargo_names);
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
	else throw "no key for RailnetFileInfo matched the found key"; */
}

RailnetOfile& RailnetOfile::operator<<(const RailnetFileInfo& fi)
{
	StructGuard _(*this);
	/*return *this << fi.hdr
		<< fi.version
		<< fi.order_lists
		<< fi.stations
		<< fi.cargo_names;*/
	return *this << fi.hdr
		<< fi.version
		<< fi.order_lists
		<< fi.stations
		<< fi.cargo_names;
}

RailnetOfile& RailnetOfile::operator<<(const CargoInfo& ci)
{
	StructGuard _(*this);
	return *this << ci.fwd
		<< ci.rev
		<< ci.slice;
#if 0
	return *this <</* ci.label <<*/ ci.fwd << ci.rev << ci.slice;
#endif
	//<< ci.unit_number;
}
/*
railnet_writer& railnet_writer::operator<<(const std::pair<const char, CargoLabel>& pr)
{
	// TODO: avoid things like "short", they may not fit the 16bit scheme
	static LblConvT lbl_conv;
	std::pair<unsigned short, char[5]> p2;
	p2.first = pr.first;
	lbl_conv.Convert(pr.second, p2.second);
	return *this << p2;
}*/

RailnetOfile& RailnetOfile::operator<<(const CargoLabelT& c)
{
	static LblConvT lbl_conv;
//	return *this << lbl_conv.Convert(c);
	return *this << lbl_conv.Convert(c);
}

void prechecks(const RailnetFileInfo& )
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
railnet_writer& railnet_writer::operator<<(const std::pair<CargoLabel, CargoInfo>& pr)
{
	static LblConvT lbl_conv;
	std::pair<char[5], CargoInfo> p2;
	p2.first = pr.first;
	lbl_conv.Convert(pr.first, p2.first);
	return *this << p2;
}*/

}

