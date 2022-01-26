#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <string>
#include <charconv>
#include <memory_resource>
#include "uri_parser.hpp"

namespace http_parser {

enum class methods { get, head, post, put, delete_method, connect, trace, patch };
inline std::string_view to_string_view(methods m)
{
	using namespace std::literals;
	if(m == methods::get) return "GET"sv;
	if(m == methods::head) return "HEAD"sv;
	if(m == methods::post) return "POST"sv;
	if(m == methods::put) return "PUT"sv;
	if(m == methods::delete_method) return "DELETE"sv;
	if(m == methods::connect) return "CONNECT"sv;
	if(m == methods::trace) return "TRACE"sv;
	if(m == methods::patch) return "PATCH"sv;
	assert(false);
	return ""sv;
}

template<typename StringView>
struct uri {
	uri(StringView u) : u(u) {}
	StringView u;
};
uri(const char*) -> uri<std::string_view>;

template<typename StringView>
struct header {
	header(StringView n, StringView v) : n(n), v(v) {}
	StringView n, v;
};
header(const char*, const char*) -> header<std::string_view>;

template<typename Container, typename StringView>
class basic_request_generator {
	std::pmr::memory_resource* mem;
	Container headers;
	Container head;
	methods cur_method = methods::get;

	void append(Container& con, StringView tail) const
	{
		for(auto& c:tail) append(con, (typename Container::value_type)c);
	}

	template<std::size_t Cnt>
	void append(Container& con, char(&str)[Cnt]) const
	{
		for(std::size_t i=0;i<Cnt;++i)
			append(con, Container::value_type(str[i]));
	}

	template<typename Arg, typename... Args>
	requires( sizeof...(Args) != 0 )
	void append(Container& con, Arg arg, Args... args) const
	{
		append(con, arg);
		if constexpr (sizeof...(Args) != 0) append(con, args...);
	}

	template<typename T>
	requires( !std::is_pointer_v<T> )
	void append(Container& con, T val) const
	{
		con.push_back((typename Container::value_type)val);
	}
public:
	basic_request_generator()
		: basic_request_generator(std::pmr::get_default_resource()) {}
	basic_request_generator(std::pmr::memory_resource* mem)
		: mem(mem)
		, headers(mem)
		, head(mem)
	{
		if(!mem) throw std::runtime_error(
			"cannot create generator without memory resource");
	}

	basic_request_generator& method(methods m)
	{
		cur_method = m;
		return *this;
	}

	basic_request_generator& uri(StringView u)
	{
		head.clear();
		basic_uri_parser<StringView> prs(u);
		append(head, to_string_view(cur_method));
		append(head, (typename Container::value_type)0x20); // space
		append(head, prs.request().empty() ? prs.path() : prs.request());
		append(head, " HTTP/1.1\r\nHost:");
		append(head, prs.host());
		append(head, 0x0D, 0x0A);
		return *this;
	}

	basic_request_generator& header(StringView name, StringView val)
	{
		append(headers, name, 0x3A, val, 0x0D, 0x0A);
		return *this;
	}

	Container body(StringView cnt) const
	{
		Container ret{mem};
		for(auto& h:head) ret.push_back(h);
		for(auto& h:headers) ret.push_back(h);
		if(cnt.size() != 0) {
			std::array<char, std::numeric_limits<std::size_t>::digits10 + 1> str;
			auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), cnt.size());
			assert(ec == std::errc());
			std::string_view len(str.data(), ptr);
			append(ret, "Content-Length:", len, "\r\n\r\n", cnt);
		}
		append(ret, "\r\n");
		return ret;
	}
};

template<typename C, typename S>
inline basic_request_generator<C,S>&
operator << (basic_request_generator<C,S>& left, const uri<S>& right)
{
	return left.uri(right.u);
}

template<typename C, typename S>
inline basic_request_generator<C,S>&
operator << (basic_request_generator<C,S>& left, const header<S>& right)
{
	return left.header(right.n, right.v);
}

} // namespace http_parser
