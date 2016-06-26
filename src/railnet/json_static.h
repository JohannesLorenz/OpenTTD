/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file json_static.h defines classes to read and write generic
 * json files
 */

#ifndef JSON_STATIC_H
#define JSON_STATIC_H

#include <cstdint>
#include <cstring>
#include <cmath>
#include <map>
#include <iostream>

namespace detail {

	template<class T, T v>
	struct integral_constant {
		static const T value = v;
		typedef T value_type;
	};

	typedef integral_constant<bool, true> true_type;
	typedef integral_constant<bool, false> false_type;

	template<class T, class U>
	struct is_same : false_type {};

	template<class T>
	struct is_same<T, T> : true_type {};

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

//! class to encapsulate struct members,
//! such that they can be used for json files
template<class T, std::size_t Str>
class SMem
{
	T val;
public:
	T& Get() { return val; }
	const T& Get() const { return val; }
	operator T& () { return Get(); }
	operator const T& () const { return Get(); }
	T& operator()() { return Get(); }
	const T& operator()() const { return Get(); }
	SMem() {}
	SMem(const T& val) : val(val) {}
};

//! Class to write a JSON formatted file
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
		_StructDepth() : depth(0), _first(true) {}

		operator char() const { return depth; }

		bool First() {
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
	Crtp& operator<<(const std::uint8_t& b) { *os << +b; return m; }
	Crtp& operator<<(const std::uint16_t& i) { *os << i; return m; }
	Crtp& operator<<(const std::uint32_t& i) { *os << i; return m; }
	Crtp& operator<<(const std::int32_t& i) { *os << i; return m; }
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

	/* start of unused code */
	const static int linewidth = 79;
	const static int max_keylength = 24;
	template<class T>
	static int MWidth(const T& num) {
		return ::logf(num) + 1 + (num < 0); // FEATURE: use non float algorithm
	}

	static int Est(const bool& b) { return b ? 4 : 5; }
	static int Est(const std::uint8_t& b) { return MWidth(b); } // FEATURE: log10
	static int Est(const std::uint16_t& i) { return MWidth(i); }
	static int Est(const std::uint32_t& i) { return MWidth(i); }
	static int Est(const std::int32_t& i) { return MWidth(i); }
	static int Est(const std::size_t& i) { return MWidth(i); }
	static int Est(const char& ) { return 3; }
	static int Est(const float& ) { return 20; }
	static int Est(const char* s) { return 2 + strlen(s); }
	static int Est(const std::string& s) { return 2 + s.length(); }
	template<class T1, class T2>
	static int Est(const std::pair<T1, T2>& p) { return 4 + Est(p.first) + Est(p.second); }
	template<class T>
	static int Est(const T& ) { return linewidth << 1; }
	int remain;
	int used;
	/* end of unused code */

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
		int remain2 = remain - Est(v.begin());

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
			remain2 = remain - Est(*it);
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
		if(!struct_depth.First())
		  m << raw(",\n") << ident();
		m << m.StringNo(StringId) << raw(": ");
		return m << ((const T&)_SMem);
	}
};

//! Non-template version of JsonIfile. Can have functions inside cpp files
class JsonIfileBase
{
	bool hit_end = false;
protected:
	static void assert_q(char c) { if(c!='"') throw "parse error, expected '\"'"; }

	//! Read "true" or "false" from @a is and stores it in @a b
	void ReadBool(bool& b, std::istream* is);
	//! Reads a string and stores it in @a s
	void ReadString(std::string& s, std::istream* is);
	//! Reads a string and stores it in a preallocated c string @a s
	void ReadCStr(char* s, std::istream* is);

	//! Checks if the end of an array or struct was read
	bool ChkEnd(char c) {
		return (c == ']' || c == '}') ? (hit_end = true) : false;
	}
	bool EndHit() { bool r = hit_end; hit_end = false; return r; }
};

//! Class to read from a JSON formatted file
template<class Crtp>
class JsonIfile : JsonIfileBase
{
	template<char c> class MustRead {};

	//! reads a json formatted number
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

	//! Checks if the recent key was matching the string value of @a s
	//! If it does, the value is stored in @a s
	//! @return if the key matched
	template<class T, std::size_t S>
	bool Try(SMem<T, S>& s)
	{
		if(recent.empty())
		 *this >> recent;
		if(recent == m.StringNo(S))
		{
	//		std::cerr << "key: " << recent << std::endl;
			recent.clear();
			m >> MustRead<':'>() >> s.Get();
			return true;
		}
		else return false;
	}

	//! the recently red key, if any
	std::string recent;

	template<class S>
	static _StructDict<S> StructDict(S& s) {
		return _StructDict<S>(s);
	}

	//! This function is called once for each new key found
	//! it calls m.Once, which needs to match the key
	//! to the correct struct members
	template<class T>
	bool _Once(T& obj)
	{
		bool ret = m.Once(obj);
		recent.clear();
		if(ret)
		{
			return true;
		}
		else
		{
			if(recent == "}")
			 recent.clear();
			else
			 throw "error";
			return false;
		}
	}

	//! this function shall be called to read any struct
	template<class S>
	Crtp& operator>>(_StructDict<S> d) {
		char tmp;
		*this >> ReadRaw(tmp);
		if(!ChkEnd(tmp))
		{
			if(tmp != '{')
			 throw "expected '{' at beginning of struct.";
			do {
				_Once(*d.the_struct());
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
	Crtp& operator>>(std::uint8_t& b) { RdNum(b); return m; }
	Crtp& operator>>(std::uint16_t& i) { RdNum(i); return m; }
	Crtp& operator>>(std::uint32_t& i) { RdNum(i); return m; }
	Crtp& operator>>(std::int32_t& i) { RdNum(i); return m; }
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
};

#endif // JSON_STATIC_H

