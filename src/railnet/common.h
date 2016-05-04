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

class s_end {};

template<char L, class N>
class s
{
};
enum s_id
{
	s_unit_number,
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
	s_size
};

const char* string_no(std::size_t id);

template<class T, std::size_t Str>
class smem
{
	T val;
public:
	T& get() { return val; }
	const T& get() const { return val; }
	operator T& () { return get(); }
	operator const T& () const { return get(); }
	T& operator()() { return get(); }
	const T& operator()() const { return get(); }
	smem() {}
	smem(const T& val) : val(val) {}
};

struct order_list
{
	smem<UnitID, s_unit_number> unit_number;
	smem<bool, s_is_cycle> is_cycle;
	smem<bool, s_is_bicycle> is_bicycle; //! at least two trains that drive in opposite cycles
//	smem<StationID, s_min_station> min_station;
	smem<std::set<CargoLabel>, s_cargo> cargo; // cargo order and amount does not matter
	smem<std::vector<std::pair<StationID, bool> >, s_stations> stations;
//	smem<std::size_t, s_real_stations> real_stations;
	order_list() : is_cycle(false), is_bicycle(false)/*,
		min_station(std::numeric_limits<StationID>::max()),
		real_stations(0)*/
	{
	}
#if 0
	const char* name_of(const UnitID* ) const { return "unit_number"; }
	const char* name_of(const bool* b) const { return (b == &is_cycle) ? "is_cycle" : "is_bicycle" }
	const char* name_of(const StationID* ) const { return "min_station" };
	const char* name_of(const std::set<CargoLabel>* ) const { return "cargo"; }
	const char* name_of(const std::vector<std::pair<StationID, bool> >* ) const { return "stations"; }
	const char* name_of(const std::size_t* ) const { return "real_stations"; }
#endif
}; // TODO: subclass order_list_sortable? avoid serializing min_station

struct station_info
{
	smem<std::string, s_name> name;
	smem<float, s_x> x;
	smem<float, s_y> y;

/*	const char* name_of(const std::string*) { return "name"; }
	const char* name_of(const float* f) { return (f == &x) ? "x" : "y"; }
*/	template<class T>
	T& rd(const char* name, T& rd) {
		switch(*name) {
			case 'x': return rd >> x;
			case 'y': return rd >> y;
			case 'n': return rd >> name;
			default: throw "expected x, y or name";
		}
	}
};

struct railnet_file_info
{
	static const std::string hdr;
	static const uint version;
	smem<std::list<order_list>, s_order_lists> order_lists;
	smem<std::map<StationID, station_info>, s_stations> stations;
	smem<std::map<char, CargoLabel>, s_cargo_names> cargo_names;
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

template<class T, std::size_t S>
inline void serialize(const smem<T, S>& s, std::ostream& o) { serialize(s.get(), o); }

template<class T, std::size_t S>
inline void deserialize(smem<T, S>& s, std::istream& i) { deserialize(s.get(), i); }

void serialize(const order_list& ol, std::ostream& o);
void serialize(const station_info& si, std::ostream& o);
void serialize(const railnet_file_info& file, std::ostream& o);

void deserialize(order_list& ol, std::istream& i);
void deserialize(station_info& si, std::istream &i);
void deserialize(railnet_file_info& file, std::istream &i);

struct json_ofile
{
	std::ostream* os;

json_ofile(std::ostream& os) : os(&os) {}

json_ofile& operator<<(const bool& b) { *os << (b ? "true" : "false"); return *this; }
json_ofile& operator<<(const byte& b) { *os << +b; return *this; }
json_ofile& operator<<(const uint16& i) { *os << i; return *this; } 
json_ofile& operator<<(const uint32& i) { *os << i; return *this; }
json_ofile& operator<<(const std::size_t& i) { *os << i; return *this; }
json_ofile& operator<<(const char& c) { *os << '"' << c << '"'; return *this; }
json_ofile& operator<<(const float& f) { *os << f; return *this; }
json_ofile& operator<<(const char* s) { *os << '"' << s << '"'; return *this; }
json_ofile& operator<<(const std::string& s) { *os << '"' << s << '"'; return *this; }

template<class Cont>
json_ofile& operator<<(const Cont& v)
{
	*os << "[ ";
	typename Cont::const_iterator it = v.begin();
	*this << *(it++);
	for(; it != v.end(); ++it)
	{
		*this << ", " << *it;
	}
	*os << ']';
	return *this;
}

	// pairs are typically in containers
	// in order to not write the same struct member names 100 times,
	// we serialize pairs as arrays of size 2
	template<class T1, class T2>
	json_ofile& operator<<(const std::pair<T1, T2>& p)
	{
		return *this << "[ " << p.first << ", " << p.second << " ]";
	}

// TODO: -> cpp
	json_ofile& operator<<(const order_list& ol);
	json_ofile& operator<<(const station_info& si);
	json_ofile& operator<<(const railnet_file_info& file);

	template<class S, class M>
	struct _member
	{
		S* sptr;
		M* mptr;
	public:
		_member(S& sref, M& mref) : sptr(&sref), mptr(&mref) {}
	};
	
	template<class S, class M>
	_member<S, M> member(S& s, M& m) { return _member<S, M>(s, m); }
	
	
	template<class S, class M>
	json_ofile& operator<<(const _member<S, M>& m)
	{
		return *this << m.sptr->name_of(&m.mptr);
	}

	template<class T, std::size_t StringId>
	json_ofile& operator<<(const smem<T, StringId>& _smem)
	{
		return *this << '"' << string_no(StringId) << "\": " << (const T&)_smem << '\n';
	}

};

struct json_ifile
{
template<char c>
struct must_read
{
};

template<char c>
json_ifile& operator>>(const must_read<c>) {
	char tmp;
	*is >> tmp;
	if(tmp != c) throw "parse error: inexpected char";
	return *this;
}
	std::istream* is;
	std::size_t depth;

	json_ifile(std::istream& is) : is(&is) {}

json_ifile& operator>>(bool& b) { char tmp[6]; *is >> tmp; b = !strncmp(tmp, "true", 4); return *this; } /* TODO: vulnerability! */
json_ifile& operator>>(byte& b) { *is >> b; return *this; }
json_ifile& operator>>(uint16& i) { *is >> i; return *this; }
json_ifile& operator>>(uint32& i) { *is >> i; return *this; }
json_ifile& operator>>(std::size_t& i) { *is >> i; return *this; }
static void assert_q(char c) { if(c!='"') throw "parse error, expected '\"'"; }
json_ifile& operator>>(char& c) { char tmp; *is >> tmp; assert_q(tmp); *is >> c; *is >> tmp; assert_q(tmp); return *this; }
json_ifile& operator>>(float& f) { *this >> f; return *this; }
json_ifile& operator>>(std::string& s) { char tmp; *is >> tmp; assert_q(tmp); *is >> s; *is >> tmp; assert_q(tmp); return *this; }

template<class T>
struct read_raw
{
	T* ptr;
	read_raw(T& ref) : ptr(&ref) {}
};

bool once(order_list& ol);
bool once(station_info& si);
bool once(railnet_file_info& fi);

std::string recent;

template<class T, std::size_t S>
bool _try(smem<T, S> & s)
{
	if(recent.empty())
	 *this >> recent;
	if(recent == string_no(S))
	{
		*this >> must_read<':'>() >> s.get();
		return true;
	}
	else return false;
	
}

template<class T>
json_ifile& operator>>(read_raw<T>& r)
{
	return is >> *r.ptr;
}

template<class Cont>
json_ifile& operator>>(Cont& v)
{
	*this >> must_read<'['>();
	while(true)
	{
		char sep;
		*is >> sep;
		if(sep == ',') {
			typename dtl::noconst_value_type_of<Cont>::type tmp;
			*this >> tmp;
			dtl::push_back(v, tmp);
		}
		else if(sep == ']') break;
		else throw "expected serparator: , or ]";
	}
	return *this;
}


template<class T1, class T2>
json_ifile& operator>>(std::pair<T1, T2>& p)
{
	return *this >> must_read<'['>() >> p.first >> must_read<','>()
		>> p.second >> must_read<']'>();
}


json_ifile& operator>>(order_list& ol) { return *this >> struct_dict(ol); }
json_ifile& operator>>(station_info& si) { return *this >> struct_dict(si); }
json_ifile& operator>>(railnet_file_info& file) { return *this >> struct_dict(file); }

	template<class S>
	class _struct_dict
	{
		S* _ptr;
	public:
		S* the_struct() { return _ptr; }
		_struct_dict(S& ref) : _ptr(&ref) {}
	};



	template<class S> // TODO: static?
	_struct_dict<S> struct_dict(S& s) { return _struct_dict<S>(s); }

	template<class S>
	json_ifile& operator>>(_struct_dict<S> d) {
		*this >> must_read<'['>();
		while( once(*d.the_struct()) ) ;
		return *this;
	}
};

} // namespace comm

#endif

