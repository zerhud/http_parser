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
	enum class state_t { simple, chunked, chunked_progress };
	std::pmr::memory_resource* mem;
	Container headers;
	Container head;
	methods cur_method = methods::get;
	mutable state_t cur_state = state_t::simple;

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
	requires( !std::is_pointer_v<T> && !std::is_same_v<std::decay_t<T>, Container> )
	void append(Container& con, T val) const
	{
		con.push_back((typename Container::value_type)val);
	}

	template<typename T>
	requires( std::is_same_v<std::decay_t<T>, Container> )
	void append(Container& con, const T& val) const
	{
		for(auto& v:val) con.push_back(v);
	}

	auto create_headers() const
	{
		Container ret{mem};
		for(auto& h:head) ret.push_back(h);
		for(auto& h:headers) ret.push_back(h);
		return ret;
	}

	template< typename Src >
	Container create_simple_body(Src cnt) const
	{
		Container ret = create_headers();
		if(cnt.size() != 0) {
			std::array<char, std::numeric_limits<std::size_t>::digits10 + 1> str;
			auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), cnt.size(), 10);
			assert(ec == std::errc());
			std::string_view len(str.data(), ptr);
			append(ret, "Content-Length: ", len, "\r\n\r\n", cnt);
		}
		append(ret, "\r\n");
		return ret;
	}

	template< typename Src >
	Container create_chunked_body(Src cnt) const
	{
		Container ret{ mem };
		if(cnt.size() == 0) {
			append(ret, "0\r\n");
		} else {
			std::array<char, std::numeric_limits<std::size_t>::digits10 + 1> str;
			auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), cnt.size(), 16);
			assert(ec == std::errc());
			std::string_view len(str.data(), ptr);
			append(ret, len, "\r\n", cnt);
		}
		append(ret, "\r\n");
		return ret;
	}

	template< typename Src >
	Container create_body(Src cnt) const
	{
		if(cur_state == state_t::simple)
			return create_simple_body(std::forward<Src>(cnt));
		if(cur_state == state_t::chunked) {
			auto ret = create_headers();
			if(cnt.size() !=0 )
				append(ret, "\r\n", create_chunked_body(std::forward<Src>(cnt)));
			else append(ret, "\r\n");
			cur_state = state_t::chunked_progress;
			return ret;
		}
		if(cur_state == state_t::chunked_progress)
			return create_chunked_body(std::forward<Src>(cnt));
		assert(false);
		throw std::logic_error("inner error (not all state supported)");
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
		append(head, " HTTP/1.1\r\nHost: ");
		append(head, prs.host());
		append(head, 0x0D, 0x0A);
		return *this;
	}

	basic_request_generator& header(StringView name, StringView val)
	{
		append(headers, name, 0x3A, 0x20, val, 0x0D, 0x0A);
		return *this;
	}

	Container body(StringView cnt) const
	{
		return create_body(cnt);
	}

	Container body(Container cnt) const
	{
		return create_body(cnt);
	}

	bool chunked() const
	{
		return cur_state == state_t::chunked || cur_state == state_t::chunked_progress;
	}

	basic_request_generator& make_chunked()
	{
		if(!chunked())
			header("Transfer-Encoding", "chunked");
		cur_state = state_t::chunked;
		return *this;
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
