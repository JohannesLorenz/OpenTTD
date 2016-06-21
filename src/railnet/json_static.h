#ifndef JSON_STATIC_H
#define JSON_STATIC_H

#include <map>

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

template<class T, std::size_t Str>
class SMem
{
	T val;
public:
	T& get() { return val; }
	const T& get() const { return val; }
	operator T& () { return get(); }
	operator const T& () const { return get(); }
	T& operator()() { return get(); }
	const T& operator()() const { return get(); }
	SMem() {}
	SMem(const T& val) : val(val) {}
};

#endif // JSON_STATIC_H

