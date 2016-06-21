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

enum StrId
{
	S_UNIT_NUMBER,
	S_REV_UNIT_NO,
	S_IS_CYCLE,
	S_IS_BICYCLE,
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
	S_LABEL, // TODO: REQUIRED?
	S_FWD, // TODO: REQ?
	S_REV, // TODO: REQ?
	S_SLICE,
	S_RAILNET,
	S_MIMETYPE,
	S_SIZE
};

const char* StringNo(std::size_t id);

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
	SMem<bool, S_IS_BICYCLE> is_bicycle; //! at least two trains that drive in opposite cycles
//	SMem<StationID, s_min_station> min_station;
	SMem<std::map<CargoLabelT, CargoInfo>, S_CARGO> cargo; // cargo order and amount does not matter
	int next_cargo_slice;
	SMem<std::vector<std::pair<StationID, bool> >, S_STATIONS> stations;
//	SMem<std::size_t, s_real_stations> real_stations;
	OrderList() :
		unit_number(no_unit_no),
		rev_unit_no(no_unit_no),
		is_cycle(false), is_bicycle(false), next_cargo_slice(1) /*,
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

template<class Crtp>
class JsonOfile
{

	template<class T>
	struct _raw
	{
		const T* ptr;
		_raw(const T& ref) : ptr(&ref) {}
	};

	//Crtp& m() { return static_cast<Crtp>(*this); }
	Crtp& m;

	JsonOfile(JsonOfile& other) = delete;

protected:
	class _StructDepth
	{
		char depth;
		bool _first;
	public:
		_StructDepth& operator++() { return ++depth, _first = true, *this; }
		_StructDepth& operator--() { return --depth, _first = false, *this; }
		_StructDepth() : depth(0), _first(true) {

		}

		operator char() const { return depth; }

		bool first() {
			bool res = _first; _first = false; return res;
		}
	} struct_depth;

	class StructGuard
	{
		JsonOfile* j;
	public:
		StructGuard(JsonOfile& _j) : j(&_j) {
			++j->struct_depth;
			j->m << raw("{\n") << ident();
		}
		~StructGuard() { --j->struct_depth; (*j) << raw('\n') << ident() << raw("}"); }
	};

	friend class StructGuard;

	std::ostream* const os;
public:
	JsonOfile(Crtp& base, std::ostream& os) : m(base), os(&os) {}

	Crtp& operator<<(const bool& b) { *os << (b ? "true" : "false"); return m; }
	Crtp& operator<<(const byte& b) { *os << +b; return m; }
	Crtp& operator<<(const uint16& i) { *os << i; return m; }
	Crtp& operator<<(const uint32& i) { *os << i; return m; }
	Crtp& operator<<(const int& i) { *os << i; return m; }
	Crtp& operator<<(const std::size_t& i) { *os << i; return m; }
	Crtp& operator<<(const char& c) { *os << '"' << c << '"'; return m; }
	Crtp& operator<<(const float& f) { *os << f; return m; }
	Crtp& operator<<(const char* s) { *os << '"' << s << '"'; return m; }
	Crtp& operator<<(const std::string& s) { *os << '"' << s << '"'; return m; }

	struct ident {};
	Crtp& operator<<(const ident&)
	{
		for(char i = 0; i < struct_depth; ++i) m << raw("  ");
		return m;
	}

	/*start*/
	const static int linewidth = 79;
	const static int max_keylength = 24;
	template<class T>
	static int mwidth(const T& num) {
		return ::logf(num) + 1 + (num < 0); // FEATURE: use non float algorithm
	}

	static int est(const bool& b) { return b ? 4 : 5; }
	static int est(const byte& b) { return mwidth(b); } // FEATURE: log10
	static int est(const uint16& i) { return mwidth(i); }
	static int est(const uint32& i) { return mwidth(i); }
	static int est(const int& i) { return mwidth(i); }
	static int est(const std::size_t& i) { return mwidth(i); }
	static int est(const char& ) { return 3; }
	static int est(const float& ) { return 20; }
	static int est(const char* s) { return 2 + strlen(s); }
	static int est(const std::string& s) { return 2 + s.length(); }
	template<class T1, class T2>
	static int est(const std::pair<T1, T2>& p) { return 4 + est(p.first) + est(p.second); }
	template<class T>
	static int est(const T& ) { return linewidth << 1; }
	int remain;
	int used;
	/*end*/

	template<class Cont>
	Crtp& operator<<(const Cont& v)
	{
		if(v.empty())
		 return m << raw("[]");

		int init_remain = linewidth -
			(struct_depth << 1) // ident
			- 1; // comma at the end
		remain =
			init_remain -
			max_keylength - // assumed key lenght
			4; // apostrophes for key, ': ' + comma at the end
		int remain2 = remain - est(v.begin());

		bool nextline = true; //remain2 < 0;
		const char* nsep = nextline ? "[\n" : "[";
		m << /*raw('\n') << ident() <<*/ raw(nsep);
		remain = nextline ? init_remain : remain2;
		++struct_depth;


		typename Cont::const_iterator it = v.begin();
		if(nextline) m << ident();
		else m << raw(' ');
		m << *(it++);
		for(; it != v.end(); ++it)
		{
			remain2 = remain - est(*it);
			nextline = true; /*remain2 < 0;*/
			if(nextline) m << raw(",\n") << ident();
			else m << raw(", ");
			m << *it;
			remain = nextline ? init_remain : remain2;
		}
		--struct_depth;
		return m << raw('\n') << ident() << raw(']');
	}

	template<class T> static _raw<T> raw(const T& ref) { return _raw<T>(ref); }

	template<class T>
	Crtp& operator<<(const _raw<T>& r)
	{
		(*os) << *r.ptr;
		return m;
	}

	// pairs are typically in containers
	// in order to not write the same struct member names 100 times,
	// we serialize pairs as arrays of size 2
	template<class T1, class T2>
	Crtp& operator<<(const std::pair<T1, T2>& p)
	{
		m << /*raw('\n') << ident() <<*/ raw("[\n");
		++struct_depth;
			m << ident() << p.first << raw(",\n") <<
			ident() << p.second << raw("\n");
		--struct_depth;
		return m << ident() << raw(']');
	}

	template<class T, std::size_t StringId>
	Crtp& operator<<(const SMem<T, StringId>& _SMem)
	{
		if(!struct_depth.first())
		  m << raw(",\n") << ident();
		m << StringNo(StringId) << raw(": ");
		return m << ((const T&)_SMem);
	}
};

class RailnetOfile : public JsonOfile<RailnetOfile>
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
/*
class RailnetOfile : public JsonOfile<railnet_writer> {
	using base=JsonOfile<railnet_writer>;
	using base::base;
};*/

class JsonIfileBase
{
	bool hit_end = false;
protected:
	static void assert_q(char c) { if(c!='"') throw "parse error, expected '\"'"; }

	void ReadBool(bool& b, std::istream* is);
	void ReadString(std::string& s, std::istream* is);
	void ReadCStr(char* s, std::istream* is);

	bool ChkEnd(char c) {
		return (c == ']' || c == '}') ? (hit_end = true) : false;
	}
	bool EndHit() { bool r = hit_end; hit_end = false; return r; }
};

template<class Crtp>
class JsonIfile : JsonIfileBase
{
	template<char c> class MustRead {};

	template<class T>
	bool RdNum(T& number)
	{
		char tmp;
		*this >> ReadRaw(tmp);
		bool neg = false;
		char comma_pos = -1;
		if(ChkEnd(tmp))
		 return false;
		number = 0;
		switch(tmp)
		{
			case ']':
			case '}':
				return false;
			case '-':
				neg = true;
				break;
			case '.':
				comma_pos = 0;
				break;
			default:
				if(isdigit(tmp))
				 (number *= 10 ) += (tmp - '0');
				else throw "not a number";
		}
		int i;
		int max = 16;
		signed int exponent = 0;
		for(i=1; i<max;++i)
		{
			//*this >> read_raw(tmp);
			tmp = std::char_traits<char>::to_char_type(is->peek());
			switch(tmp)
			{
				case '.':
					if(comma_pos == -1)
					 comma_pos = i;
					else
					 throw "two points not allowed";
					break;
				case 'e':
				case 'E':
					if(! RdNum(exponent) )
					 throw "expected exponent after 'e' or 'E'.";
				case ',':
				case '\n':
					max = i;
					break;
				default:
					if(isdigit(tmp))
					 (number *= 10 ) += (tmp-'0');
					else throw "not a number";
			}
			if(tmp != ',')
			 is->ignore(1);
		}

		int mov = 0;
		if(exponent)
		{
			if(detail::is_same<T, float>::value || exponent > 0)
			 mov += exponent;
			else throw "exponent found, but non-floating number expected (or exponent negative)";
		}
		if(comma_pos != -1)
		{
			if(detail::is_same<T, float>::value)
			 mov += comma_pos-i+2;
			else throw "point found, but non-floating number expected";
		}
		if(mov != 0)
		 number *= powf(10, mov);
		if(neg)
		 number *= -1;
		return true;
	}

	template<class T>
	struct _ReadRaw
	{
		T* ptr;
		_ReadRaw(T& ref) : ptr(&ref) {}
	};

	template<class S>
	class _StructDict
	{
		S* _ptr;
	public:
		S* the_struct() { return _ptr; }
		_StructDict(S& ref) : _ptr(&ref) {}
	};

	Crtp &m;

protected:
	std::istream* const is;

	template<class T, std::size_t S>
	bool Try(SMem<T, S>& s)
	{
		if(recent.empty())
		 *
			this >> recent;
		if(recent == StringNo(S))
		{
	//		std::cerr << "key: " << recent << std::endl;
			recent.clear();
			m >> MustRead<':'>() >> s.get();
			return true;
		}
		else return false;
	}

	std::string recent;

	template<class S>
	static _StructDict<S> StructDict(S& s) {
		return _StructDict<S>(s);
	}

	template<class S>
	Crtp& operator>>(_StructDict<S> d) {
		char tmp;
		*this >> ReadRaw(tmp);
		if(!ChkEnd(tmp))
		{
			if(tmp != '{')
			 throw "expected '{' at beginning of struct.";
			do {
				m.Once(*d.the_struct());
				*this >> ReadRaw(tmp);
				if(tmp != ',' && tmp != '}')
				 throw "expecting ',' or '}' inside struct";
			} while(tmp == ',');
		}
		return m;
	}

public:
	JsonIfile(Crtp& c, std::istream& is) : m(c), is(&is) {}

	template<char c>
	Crtp& operator>>(const MustRead<c>)
	{
		char tmp;
		*is >> tmp;
		if(tmp != c) throw "parse error: inexpected char";
		return m;
	}

	Crtp& operator>>(bool& b) { ReadBool(b, is); return m; }
	Crtp& operator>>(byte& b) { RdNum(b); return m; }
	Crtp& operator>>(uint16& i) { RdNum(i); return m; }
	Crtp& operator>>(uint32& i) { RdNum(i); return m; }
	Crtp& operator>>(int& i) { RdNum(i); return m; }
	Crtp& operator>>(std::size_t& i) { RdNum(i); return m; }
	Crtp& operator>>(char& c) { char tmp; *is >> tmp;
		if(!ChkEnd(tmp)) { assert_q(tmp); *is >> c; *is >> tmp; assert_q(tmp); }
		return m; }
	Crtp& operator>>(float& f) { RdNum(f); return m; }
	Crtp& operator>>(std::string& s) { ReadString(s, is); return m; }
	Crtp& operator>>(char* s) { ReadCStr(s, is); return m; }

	template<class T> _ReadRaw<T> ReadRaw(T& ref) {
		return _ReadRaw<T>(ref);
	}

	template<class T>
	Crtp& operator>>(_ReadRaw<T> r)
	{
		(*is) >> *r.ptr;
		return m;
	}

	template<class Cont>
	Crtp& operator>>(Cont& v)
	{
		char tmp;
		*this >> ReadRaw(tmp);
		if(!ChkEnd(tmp))
		{
			if(tmp != '[')
			 throw "expected [ at beginning of container";

			typedef typename detail::noconst_value_type_of<Cont>::type value_type;
			{
				value_type tmp;
				m >> tmp;
				detail::push_back(v, std::move(tmp));
			}
			if(!EndHit())
			{
				while(true)
				{
					char sep;
					*this >> ReadRaw(sep);
					if(sep == ',') {
						value_type tmp;
						m >> tmp;
						detail::push_back(v, tmp);
					}
					else if(sep == ']') break;
					else throw "expected separator: , or ]";
				}
			}
		}
		return m;
	}

	template<class T1, class T2>
	Crtp& operator>>(std::pair<T1, T2>& p)
	{
		char tmp;
		*this >> ReadRaw(tmp);
		if(!ChkEnd(tmp))
		{
			if(tmp != '[')
			 throw "expected [ at beginning of pair";
			m >> p.first >> MustRead<','>()
				>> p.second >> MustRead<']'>();
		}
		return m;
	}
//	Crtp& operator>>(std::pair<char, CargoLabel>& pr);
};

class RailnetIfile : public JsonIfile<RailnetIfile>
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
/*	JsonOfile& operator<<(const std::pair<const char, CargoLabel>& pr);
	JsonOfile& operator<<(const std::pair<CargoLabel, CargoInfo>& pr);*/
	RailnetIfile(std::istream& is) : JsonIfile(*this, is) {}
};

void prechecks(const RailnetFileInfo& file);

} // namespace comm

#endif

