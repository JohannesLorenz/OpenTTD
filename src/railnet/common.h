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

#include "json_static.h"

namespace comm
{

class s_end {};

template<char L, class N>
class s
{
};
enum s_id
{
	s_unit_number,
	s_rev_unit_no,
	s_is_cycle,
	s_is_bicycle,
	s_min_station,
	s_cargo,
	s_stations,
	s_real_stations,
	s_name,
	s_x,
	s_y,
	s_order_lists,
	s_cargo_names,
	s_version,
	s_filetype,
	s_label, // TODO: required?
	s_fwd, // TODO: req?
	s_rev, // TODO: req?
	s_slice,
	s_railnet,
	s_mimetype,
	s_size
};

const char* string_no(std::size_t id);

struct cargo_info
{
//	smem<CargoLabel, s_label> label;
	smem<bool, s_fwd> fwd;
	smem<bool, s_rev> rev;
	smem<int, s_slice> slice;
/*	bool operator<(const cargo_info& rhs) const {
		return (slice == rhs.slice) ? label < rhs.label
	    : slice < rhs.slice }*/
};

struct order_list
{
	smem<UnitID, s_unit_number> unit_number;
	smem<UnitID, s_rev_unit_no> rev_unit_no;
	smem<bool, s_is_cycle> is_cycle;
	smem<bool, s_is_bicycle> is_bicycle; //! at least two trains that drive in opposite cycles
//	smem<StationID, s_min_station> min_station;
	smem<std::map<CargoLabel, cargo_info>, s_cargo> cargo; // cargo order and amount does not matter
	int next_cargo_slice;
	smem<std::vector<std::pair<StationID, bool> >, s_stations> stations;
//	smem<std::size_t, s_real_stations> real_stations;
	order_list() : is_cycle(false), is_bicycle(false), next_cargo_slice(1) /*,
		min_station(std::numeric_limits<StationID>::max()),
		real_stations(0)*/
	{
	}

}; // TODO: subclass order_list_sortable? avoid serializing min_station

struct station_info
{
	smem<std::string, s_name> name;
	smem<float, s_x> x;
	smem<float, s_y> y;

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
struct railnet_file_info
{
	static const std::string hdr;
	static const uint version;
			default: throw "expected x, y or name";
		}
	}
};*/

struct railnet_file_info
{
	smem<std::string, s_mimetype> hdr;
	smem<int, s_version> version;
	smem<std::list<order_list>, s_order_lists> order_lists;
	smem<std::map<StationID, station_info>, s_stations> stations;
	/*FEATURE: char suffices, but need short to print it*/
	smem<std::map<char, CargoLabel>, s_cargo_names> cargo_names;

	railnet_file_info() : hdr("openttd/railnet"), version(0) {}
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
inline void serialize(const int& i, std::ostream& o) { dtl::wrt(i, o); }
inline void serialize(const char& c, std::ostream& o) { dtl::wrt(c, o); }
inline void serialize(const float& f, std::ostream& o) { dtl::wrt(f, o); }

inline void deserialize(byte& b, std::istream& i) { dtl::rd(b, i); }
inline void deserialize(uint16& i, std::istream& is) { dtl::rd(i, is); }
inline void deserialize(uint32& i, std::istream& is) { dtl::rd(i, is); }
inline void deserialize(bool& b, std::istream& i) { dtl::rd(b, i); }
inline void deserialize(int& i, std::istream& is) { dtl::rd(i, is); }
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

//! sfinae based container deserializer
template<class Container>
void deserialize(Container& c, std::istream& is,
	const typename Container::const_iterator* = NULL)
{
	uint32 sz;
	deserialize(sz, is);
	for(uint32 i = 0; i<sz; ++i)
	{
		typename detail::noconst_value_type_of<Container>::type val;
		deserialize(val, is);
		detail::push_back(c, val);
	}
}

template<class T1, class T2>
inline void serialize(const std::pair<T1, T2>& p, std::ostream& o) {
	serialize(p.first, o); serialize(p.second, o); }

template<class T1, class T2>
inline void deserialize(std::pair<T1, T2>& p, std::istream& i) {
	deserialize(p.first, i); deserialize(p.second, i); }

template<class T, std::size_t S>
inline void serialize(const smem<T, S>& s, std::ostream& o) { serialize(s.get(), o); }

template<class T, std::size_t S>
inline void deserialize(smem<T, S>& s, std::istream& i) { deserialize(s.get(), i); }

void serialize(const cargo_info& ci, std::ostream& o);
void serialize(const order_list& ol, std::ostream& o);
void serialize(const station_info& si, std::ostream& o);
void serialize(const railnet_file_info& file, std::ostream& o);

void deserialize(cargo_info& ci, std::istream& i);
void deserialize(order_list& ol, std::istream& i);
void deserialize(station_info& si, std::istream &i);
void deserialize(railnet_file_info& file, std::istream &i);

class json_ofile
{
	template<class T>
	struct _raw
	{
		const T* ptr;
		_raw(const T& ref) : ptr(&ref) {}
	};

	json_ofile(json_ofile& other) = delete;

protected:
	class _struct_depth
	{
		char depth;
		bool _first;
	public:
		_struct_depth& operator++() { return ++depth, _first = true, *this; }
		_struct_depth& operator--() { return --depth, _first = false, *this; }
		_struct_depth() : depth(0), _first(true) {

		}

		operator char() const { return depth; }

		bool first() {
			bool res = _first; _first = false; return res;
		}
	} struct_depth;

	class struct_guard
	{
		json_ofile* j;
	public:
		struct_guard(json_ofile& _j) : j(&_j) {
			++j->struct_depth;
			(*j) << raw("{\n") << ident();
		}
		~struct_guard() { --j->struct_depth; (*j) << raw('\n') << ident() << raw("}"); }
	};

	std::ostream* const os;
public:
	json_ofile(std::ostream& os) : os(&os) {}

	json_ofile& operator<<(const bool& b) { *os << (b ? "true" : "false"); return *this; }
	json_ofile& operator<<(const byte& b) { *os << +b; return *this; }
	json_ofile& operator<<(const uint16& i) { *os << i; return *this; }
	json_ofile& operator<<(const uint32& i) { *os << i; return *this; }
	json_ofile& operator<<(const int& i) { *os << i; return *this; }
	json_ofile& operator<<(const std::size_t& i) { *os << i; return *this; }
	json_ofile& operator<<(const char& c) { *os << '"' << c << '"'; return *this; }
	json_ofile& operator<<(const float& f) { *os << f; return *this; }
	json_ofile& operator<<(const char* s) { *os << '"' << s << '"'; return *this; }
	json_ofile& operator<<(const std::string& s) { *os << '"' << s << '"'; return *this; }

	struct ident {};
	json_ofile& operator<<(const ident&)
	{
		for(char i = 0; i < struct_depth; ++i) *this << raw("  ");
		return *this;
	}

	template<class Cont>
	json_ofile& operator<<(const Cont& v)
	{
		if(v.empty())
		 return *this << raw("[]");

		*this << /*raw('\n') << ident() <<*/ raw("[\n");
		++struct_depth;
		typename Cont::const_iterator it = v.begin();
		*this << ident() << *(it++);
		for(; it != v.end(); ++it)
		{
			*this << raw(",\n") << ident() << *it;
		}
		--struct_depth;
		*this << raw('\n') << ident() << raw(']');
		return *this;
	}

	template<class T> static _raw<T> raw(const T& ref) { return _raw<T>(ref); }

	template<class T>
	json_ofile& operator<<(const _raw<T>& r)
	{
		return ((*os) << *r.ptr), *this;
	}

	// pairs are typically in containers
	// in order to not write the same struct member names 100 times,
	// we serialize pairs as arrays of size 2
	template<class T1, class T2>
	json_ofile& operator<<(const std::pair<T1, T2>& p)
	{
		*this << /*raw('\n') << ident() <<*/ raw("[\n");
		++struct_depth;
			*this << ident() << p.first << raw(",\n") <<
			ident() << p.second << raw("\n");
		--struct_depth;
		return *this << ident() << raw(']');
	}

	json_ofile& operator<<(const order_list& ol);
	json_ofile& operator<<(const station_info& si);
	json_ofile& operator<<(const cargo_info& ci);
	json_ofile& operator<<(const railnet_file_info& file);

	json_ofile& operator<<(const std::pair<const char, CargoLabel>& pr);

	template<class T, std::size_t StringId>
	json_ofile& operator<<(const smem<T, StringId>& _smem)
	{
		if(!struct_depth.first())
		  *this << raw(",\n") << ident();
		*this << string_no(StringId) << raw(": ") << (const T&)_smem;
		//*os << '\n';
		return *this;
	}
};

class json_ifile
{
	template<char c> class must_read {};
	bool hit_end;

	template<class T>
	bool rdnum(T& number)
	{
		char tmp;
		*this >> read_raw(tmp);
		bool neg = false;
		char comma_pos = -1;
		if(chk_end(tmp))
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
					if(! rdnum(exponent) )
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
	struct _read_raw
	{
		T* ptr;
		_read_raw(T& ref) : ptr(&ref) {}
	};

	bool once(order_list& ol);
	bool once(station_info& si);
	bool once(cargo_info& ci);
	bool once(railnet_file_info& fi);

	template<class S>
	class _struct_dict
	{
		S* _ptr;
	public:
		S* the_struct() { return _ptr; }
		_struct_dict(S& ref) : _ptr(&ref) {}
	};

protected:
	std::istream* const is;

	bool chk_end(char c) {
		return (c == ']' || c == '}') ? (hit_end = true) : false;
	}
	bool end_hit() { bool r = hit_end; hit_end = false; return r; }

	template<class T, std::size_t S>
	bool _try(smem<T, S>& s)
	{
		if(recent.empty())
		 *this >> recent;
		if(recent == string_no(S))
		{
	//		std::cerr << "key: " << recent << std::endl;
			recent.clear();
			*this >> must_read<':'>() >> s.get();
			return true;
		}
		else return false;
	}

	std::string recent;

	template<class S>
	static _struct_dict<S> struct_dict(S& s) {
		return _struct_dict<S>(s);
	}

	template<class S>
	json_ifile& operator>>(_struct_dict<S> d) {
		char tmp;
		*this >> read_raw(tmp);
		if(!chk_end(tmp))
		{
			if(tmp != '{')
			 throw "expected '{' at beginning of struct.";
			do {
				once(*d.the_struct());
				*this >> read_raw(tmp);
				if(tmp != ',' && tmp != '}')
				 throw "expecting ',' or '}' inside struct";
			} while(tmp == ',');
		}
		return *this;
	}

public:
	json_ifile(std::istream& is) : hit_end(false), is(&is) {}

	template<char c>
	json_ifile& operator>>(const must_read<c>)
	{
		char tmp;
		*is >> tmp;
		if(tmp != c) throw "parse error: inexpected char";
		return *this;
	}

	json_ifile& operator>>(bool& b);
	json_ifile& operator>>(byte& b) { rdnum(b); return *this; }
	json_ifile& operator>>(uint16& i) { rdnum(i); return *this; }
	json_ifile& operator>>(uint32& i) { rdnum(i); return *this; }
	json_ifile& operator>>(int& i) { rdnum(i); return *this; }
	json_ifile& operator>>(std::size_t& i) { rdnum(i); return *this; }
	static void assert_q(char c) { if(c!='"') throw "parse error, expected '\"'"; }
	json_ifile& operator>>(char& c) { char tmp; *is >> tmp;
		if(!chk_end(tmp)) { assert_q(tmp); *is >> c; *is >> tmp; assert_q(tmp); }
		return *this; }
	json_ifile& operator>>(float& f) { rdnum(f); return *this; }
	json_ifile& operator>>(std::string& s);

	template<class T> _read_raw<T> read_raw(T& ref) {
		return _read_raw<T>(ref);
	}

	template<class T>
	json_ifile& operator>>(_read_raw<T> r)
	{
		(*is) >> *r.ptr;
		return *this;
	}

	template<class Cont>
	json_ifile& operator>>(Cont& v)
	{
		char tmp;
		*this >> read_raw(tmp);
		if(!chk_end(tmp))
		{
			if(tmp != '[')
			 throw "expected [ at beginning of container";

			typedef typename detail::noconst_value_type_of<Cont>::type value_type;
			{
				value_type tmp;
				*this >> tmp;
				detail::push_back(v, tmp);
			}
			if(!end_hit())
			{
				while(true)
				{
					char sep;
					*this >> read_raw(sep);
					if(sep == ',') {
						value_type tmp;
						*this >> tmp;
						detail::push_back(v, tmp);
					}
					else if(sep == ']') break;
					else throw "expected separator: , or ]";
				}
			}
		}
		return *this;
	}

	template<class T1, class T2>
	json_ifile& operator>>(std::pair<T1, T2>& p)
	{
		char tmp;
		*this >> read_raw(tmp);
		if(!chk_end(tmp))
		{
			if(tmp != '[')
			 throw "expected [ at beginning of pair";
			*this >> p.first >> must_read<','>()
				>> p.second >> must_read<']'>();
		}
		return *this;
	}
	json_ifile& operator>>(std::pair<char, CargoLabel>& pr);

	json_ifile& operator>>(order_list& ol) { return *this >> struct_dict(ol); }
	json_ifile& operator>>(station_info& si) { return *this >> struct_dict(si); }
	json_ifile& operator>>(cargo_info& ci) { return *this >> struct_dict(ci); }
	json_ifile& operator>>(railnet_file_info& file) { return *this >> struct_dict(file); }
};

} // namespace comm

#endif

