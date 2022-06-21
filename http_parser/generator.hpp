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

namespace details {

template<class,template<class>class>
struct is_instance : std::false_type {};
template<class T, template<class>class U>
struct is_instance<U<T>, U> : std::true_type {};

} // namespace details

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

template<
        typename DataContainerFactory
      , typename StringView = std::string_view
      , typename DataContainer = decltype(std::declval<DataContainerFactory>()())
      >
class basic_generator {
	template<typename T>
	struct cvt_int{ T value; int base = 10; };

	enum class state_t { simple, chunked, chunked_progress };
	DataContainerFactory dcf;
	DataContainer headers;
	DataContainer head;
	methods cur_method = methods::get;
	mutable state_t cur_state = state_t::simple;

	void append(DataContainer& con, StringView tail) const
	{
		for(auto& c:tail) append(con, (typename DataContainer::value_type)c);
	}

	template<std::size_t Cnt>
	void append(DataContainer& con, char(&str)[Cnt]) const
	{
		for(std::size_t i=0;i<Cnt;++i)
			append(con, DataContainer::value_type(str[i]));
	}

	template<typename Arg, typename... Args>
	requires( sizeof...(Args) != 0 )
	void append(DataContainer& con, Arg arg, Args... args) const
	{
		append(con, arg);
		if constexpr (sizeof...(Args) != 0) append(con, args...);
	}

	template<typename T>
	requires(
	        !std::is_pointer_v<T>
	     && !std::is_same_v<std::decay_t<T>, DataContainer>
	     && !details::is_instance<T, cvt_int>::value
	        )
	void append(DataContainer& con, T val) const
	{
		con.push_back((typename DataContainer::value_type)val);
	}

	template<typename T>
	requires( std::is_same_v<std::decay_t<T>, DataContainer> )
	void append(DataContainer& con, const T& val) const
	{
		for(auto& v:val) con.push_back(v);
	}

	template<typename T>
	void append(DataContainer& con, cvt_int<T> num) const {
		std::array<char, std::numeric_limits<T>::digits10 + 1> str;
		auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), num.value, num.base);
		assert(ec == std::errc());
		std::string_view str_num(str.data(), ptr);
		append(con, str_num);
	}

	auto create_headers() const
	{
		DataContainer ret = dcf();
		for(auto& h:head) ret.push_back(h);
		for(auto& h:headers) ret.push_back(h);
		return ret;
	}

	template<typename C>
	C& set_content_length(C& con, std::size_t len) const
	{
		if(con.size() != 0)
			append(con, "Content-Length: ", cvt_int{ len }, "\r\n\r\n");
		return con;
	}

	template< typename Src >
	DataContainer create_simple_body(Src cnt) const
	{
		DataContainer ret = create_headers();
		if(cnt.size() == 0) append(ret, "\r\n");
		else append(set_content_length(ret, cnt.size()), cnt);
		return ret;
	}

	template<typename Con>
	Con& create_chunked_body(Con& con, std::size_t sz) const
	{
		if(cur_state == state_t::chunked) {
			append(con, "\r\n");
			cur_state = state_t::chunked_progress;
		}
		append(con, cvt_int{sz, 16}, "\r\n");
		return con;
	}

	template< typename Src >
	DataContainer create_chunked_body(Src cnt) const
	{
		DataContainer ret = dcf();
		if(cnt.size() == 0) append(ret, "0\r\n");
		else append(ret, cvt_int{ cnt.size(), 16 }, "\r\n", cnt);
		append(ret, "\r\n");
		return ret;
	}

	template< typename Src >
	DataContainer create_body(Src cnt) const
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
	basic_generator() requires std::is_default_constructible_v<DataContainerFactory>
	    : basic_generator(DataContainerFactory{}) {}
	basic_generator(DataContainerFactory&& dcf)
	    : dcf(std::move(dcf))
	    , headers(this->dcf())
	    , head(this->dcf())
	{
	}

	basic_generator& method(methods m)
	{
		cur_method = m;
		return *this;
	}

	basic_generator& response(int code, StringView r)
	{
		if(code < 100 || 999 < code)
			throw std::runtime_error("this code are not allowed in response");
		append(head, "HTTP/1.1 ", cvt_int{code}, ' ', r, 0x0d, 0x0a);
		return *this;
	}

	basic_generator& uri(StringView u)
	{
		head.clear();
		basic_uri_parser<StringView> prs(u);
		append(head, to_string_view(cur_method));
		append(head, (typename DataContainer::value_type)0x20); // space
		append(head, prs.request().empty() ? prs.path() : prs.request());
		append(head, " HTTP/1.1\r\nHost: ");
		append(head, prs.host());
		append(head, 0x0D, 0x0A);
		return *this;
	}

	basic_generator& header(StringView name, StringView val)
	{
		append(headers, name, 0x3A, 0x20, val, 0x0D, 0x0A);
		return *this;
	}

	DataContainer body(StringView cnt) const
	{
		return create_body(cnt);
	}

	DataContainer body(DataContainer cnt) const
	{
		return create_body(cnt);
	}

	bool chunked() const
	{
		return cur_state == state_t::chunked || cur_state == state_t::chunked_progress;
	}

	basic_generator& make_chunked()
	{
		if(!chunked())
			header("Transfer-Encoding", "chunked");
		cur_state = state_t::chunked;
		return *this;
	}

	DataContainer body(std::size_t sz)
	{
		auto ret = cur_state == state_t::chunked_progress ? dcf() : create_headers();
		if(!chunked()) set_content_length(ret, sz);
		else create_chunked_body(ret, sz);
		return ret;
	}
};

template<typename C, typename S>
inline basic_generator<C,S>&
operator << (basic_generator<C,S>& left, const uri<S>& right)
{
	return left.uri(right.u);
}

template<typename C, typename S>
inline basic_generator<C,S>&
operator << (basic_generator<C,S>& left, const header<S>& right)
{
	return left.header(right.n, right.v);
}

} // namespace http_parser
