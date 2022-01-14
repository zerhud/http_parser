#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <cassert>
#include <optional>
#include <memory_resource>

namespace http_utils {


template<typename Char, typename StringView = std::basic_string_view<Char>>
class uri_parser_machine {
	enum class state {
		scheme, scheme_end_1, scheme_end_2,
		user_pwd, dog,
		domain, port,
		path_pos, path,
		query, anchor,
		finish };

	Char cur_symbol;
	state cur_state = state::scheme;
	StringView src;
	std::size_t begin=0;
	void parse()
	{
		switch(cur_state) {
		case state::scheme: pscheme(); break;
		case state::scheme_end_1: pscheme_end_1(); break;
		case state::scheme_end_2: pscheme_end_2(); break;
		case state::user_pwd: puser_pwd(); break;
		case state::dog: pdog(); break;
		case state::domain: pdomain(); break;
		case state::port: pport(); break;
		case state::path: ppath(); break;
		case state::path_pos: ppath_pos(); break;
		case state::query: pquery(); break;
		case state::anchor: panchor(); break;
		}
	}

	void to_state(state s)
	{
		cur_state = s;
		src = src.substr(begin);
		begin = 0;
	}

	void switch_state(state s)
	{
		cur_state = s;
	}

	void pscheme()
	{
		if(is_colon()) switch_state(state::scheme_end_1);
		else if(!is_AZ() && !is_az()) {
			if(is_slash()) {
				begin += 2;
				to_state(state::user_pwd);
			} else {
				switch_state(state::user_pwd);
			}
		}
	}

	void pscheme_end_1() { switch_state( is_slash() ? state::scheme_end_2 : state::user_pwd ); }
	void pscheme_end_2()
	{
		if(!is_slash()) switch_state(state::user_pwd);
		else {
			scheme = src.substr(0, begin - 2);
			++begin;
			to_state(state::user_pwd);
		}
	}
	void puser_pwd()
	{
		if(is_dog()) {
			auto up = src.substr(0, begin);
			auto colon_pos = up.find(0x3A);
			if(colon_pos == std::string::npos) {
				user = up;
			} else {
				user = up.substr(0, colon_pos);
				password = up.substr(colon_pos+1);
			}
			to_state(state::dog);
		} else if(is_slash() || is_end()) {
			begin = 0;
			switch_state(state::domain);
		}
	}
	void pdog() { if(!is_dog()) to_state(state::domain); }
	void pdomain()
	{
		if(is_colon() || is_slash()) {
			domain = src.substr(0, begin);
			to_state(is_colon() ? state::port : state::path_pos);
		} else if(is_end()) {
			domain = src;
			to_state(state::finish);
		}
	}
	void pport()
	{
		assert(begin!=0);
		if(is_end()) {
			port = src.substr(1);
			switch_state(state::finish);
		}
		else if(!is_digit()) {
			port = src.substr(1, begin-1);
			to_state(state::path_pos);
		}
	}
	void ppath()
	{
		if(is_question() || is_number()) {
			path = src.substr(0, begin);
			to_state(is_number() ? state::anchor : state::query);
		} else if(is_end()) {
			path = src.substr(0);
			switch_state(state::finish);
		}
	}
	void ppath_pos()
	{
		path_position =
		        scheme.size()
		      + (scheme.empty() ? 0 : 3)
		      + (user.empty() ? 0 : (user.size() + 1 + password.size() + 1))
		      + (domain.size())
		      + (port.empty() ? 0 : port.size() + 1)
		      ;
		switch_state(state::path);
		ppath();
	}
	void pquery()
	{
		if(is_number()) {
			if(begin !=0) query = src.substr(1, begin-1);
			to_state(state::anchor);
		} else if(is_end()) {
			query = src.substr(1);
			to_state(state::finish);
		}
	}
	void panchor()
	{
		if(is_end()) anchor = src.substr(1);
		to_state(state::finish);
	}

	bool is_AZ() const { return 0x41 <= cur_symbol && cur_symbol <= 0x5A; }
	bool is_az() const { return 0x61 <= cur_symbol && cur_symbol <= 0x7A; }
	bool is_slash() const { return cur_symbol == 0x2F; }
	bool is_colon() const { return cur_symbol == 0x3A; }
	bool is_dog() const { return cur_symbol == 0x40; }
	bool is_digit() const { return 0x30 <= cur_symbol && cur_symbol <= 0x39; }
	bool is_question() const { return cur_symbol == 0x3F; }
	bool is_number() const { return cur_symbol == 0x23; }
	bool is_end() const { return src.size() <= begin + 1; }
	void clear_state()
	{
		cur_state = state::scheme;
		scheme = StringView{};
		user = StringView{};
		password = StringView{};
		domain = StringView{};
		port = StringView{};
		path = StringView{};
		query = StringView{};
		anchor = StringView{};
		path_position=0;
	}
public:
	uri_parser_machine& operator()(StringView uri)
	{
		clear_state();
		src = uri;
		for(begin=0;begin<src.size();++begin) {
			cur_symbol = src[begin];
			parse();
		}
		if(cur_state != state::finish) parse();
		return *this;
	}

	StringView scheme;
	StringView user, password;
	StringView domain;
	StringView port;
	StringView path;
	StringView query;
	StringView anchor;
	std::size_t path_position=0;
};

template<typename Char, typename StringView>
inline bool operator == (
        const uri_parser_machine<Char,StringView>& left,
        const uri_parser_machine<Char,StringView>& right)
{
	return
	        left.scheme == right.scheme
	     && left.user == right.user
	     && left.password == right.password
	     && left.domain == right.domain
	     && left.port == right.port
	     && left.path == right.path
	     && left.query == right.query
	     && left.anchor == right.anchor
	     ;
}


template<typename String, typename StringView = std::basic_string_view<typename String::value_type>>
class basic_uri_parser final {
	using CharType = typename String::value_type;
	std::pmr::memory_resource* def_mem()
	{
		return std::pmr::get_default_resource();
	}

	void recalculate()
	{
		parsed(source);
	}

	static bool is_ascii_eq(StringView l, std::string_view r)
	{
		if(l.size() != r.size()) return false;
		for(std::size_t i=0;i<l.size();++i) if(l[i]!=r[i]) return false;
		return true;
	}

	static std::int64_t to_int(StringView src)
	{
		std::int64_t ret = 0;
		for(std::size_t i=0;i<src.size();++i)
			ret = ret * 10 + (int(src[i]) - 48);
		return ret;
	}


	std::pmr::memory_resource* mem;
	String source;
	uri_parser_machine<typename String::value_type, StringView> parsed;
public:
	basic_uri_parser() : basic_uri_parser(def_mem()) {}
	basic_uri_parser(String url)
	    : mem(url.get_allocator().resource())
	    , source(std::move(url))
	{
		recalculate();
	}

	basic_uri_parser(std::pmr::memory_resource* mem)
	    : basic_uri_parser(mem, "") {}
	basic_uri_parser(std::pmr::memory_resource* mem, StringView url)
	    : mem(mem)
	    , source(url, mem)
	{
		recalculate();
	}

	StringView uri() const { return source; }
	void uri(StringView nu)
	{
		source = nu;
		recalculate();
	}

	std::pmr::memory_resource* mem_buffer() const
	{
		return mem;
	}

	std::int64_t port() const
	{
		std::int64_t port_=80;
		auto pstr = parsed.port;
		//TODO: add more schemes
		if(!pstr.empty()) port_ = to_int(pstr);
		else if(is_ascii_eq(scheme(), "https")) {
			port_ = 443;
		}
		return port_;
	}
	StringView scheme() const { return parsed.scheme; }
	StringView host() const { return parsed.domain; }
	StringView path() const
	{
		auto ret = parsed.path;
		if(ret.empty()) return StringView{(const CharType*)"/", 1};
		return ret;
	}
	StringView request() const
	{
		std::size_t pos = parsed.path_position;
		auto params_len = parsed.query.size();
		std::size_t len =
		        parsed.path.size() + params_len +
		        (params_len == 0 ? 0 : 1); // 1 is a ? sign
		return StringView{ source.data() + pos, len };
	}
	StringView anchor() const { return parsed.anchor; }
	StringView params() const { return parsed.query; }
	std::optional<StringView> param(StringView name) const
	{
		auto data = params();
		for(std::size_t begin=0;begin<data.size();++begin) {
			std::size_t eq_pos = data.find(0x3D, begin);
			std::size_t amp_pos = data.find(0x26, begin);
			if(amp_pos==String::npos) amp_pos = data.size();
			std::size_t name_finish_pos = std::min(eq_pos, amp_pos);
			auto cur_name = data.substr(begin, name_finish_pos - begin);
			if(cur_name == name) {
				if(eq_pos == String::npos)
					return StringView{};
				return data.substr(
				            name_finish_pos + 1,
				            amp_pos == name_finish_pos
				            ? 0
				            : amp_pos - name_finish_pos - 1);
			}
			begin = amp_pos;
		}
		return std::nullopt;
	}

	StringView user() const { return parsed.user; }
	StringView pass() const { return parsed.password; }
};

using uri_parser = basic_uri_parser<std::pmr::string>;
using uri_wparser = basic_uri_parser<std::pmr::wstring>;

} // namespace http_utils
