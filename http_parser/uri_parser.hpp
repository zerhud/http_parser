#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <cassert>
#include <optional>
#include <memory_resource>

namespace http_parser {


template<typename StringView>
class uri_parser_machine {
	enum class state {
		scheme, only_path, scheme_end_1, scheme_end_2,
		user_pwd, dog,
		domain, port,
		path, query, anchor,
		finish };

	typedef void (uri_parser_machine::*parse_fnc)();
	typename StringView::value_type cur_symbol;
	state cur_state = state::scheme;
	StringView src;
	std::size_t begin=0;
	std::size_t user_pwd_colon_pos = StringView::npos;
	std::array<parse_fnc,12> parse_functions;
	void init_parser_functions()
	{
		parse_functions[static_cast<std::size_t>(state::scheme)] = &uri_parser_machine::pscheme;
		parse_functions[static_cast<std::size_t>(state::only_path)] = &uri_parser_machine::ponly_path;
		parse_functions[static_cast<std::size_t>(state::scheme_end_1)] = &uri_parser_machine::pscheme_end_1;
		parse_functions[static_cast<std::size_t>(state::scheme_end_2)] = &uri_parser_machine::pscheme_end_2;
		parse_functions[static_cast<std::size_t>(state::user_pwd)] = &uri_parser_machine::puser_pwd;
		parse_functions[static_cast<std::size_t>(state::dog)] = &uri_parser_machine::pdog;
		parse_functions[static_cast<std::size_t>(state::domain)] = &uri_parser_machine::pdomain;
		parse_functions[static_cast<std::size_t>(state::port)] = &uri_parser_machine::pport;
		parse_functions[static_cast<std::size_t>(state::path)] = &uri_parser_machine::ppath;
		parse_functions[static_cast<std::size_t>(state::query)] = &uri_parser_machine::pquery;
		parse_functions[static_cast<std::size_t>(state::anchor)] = &uri_parser_machine::panchor;
		parse_functions[static_cast<std::size_t>(state::finish)] = &uri_parser_machine::pfinish;
	}

	void pfinish() {}

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
		if(is_colon()) {
			user_pwd_colon_pos = begin;
			switch_state(state::scheme_end_1);
		} else if(!is_AZ() && !is_az()) {
			if(is_slash()) {
				if(begin==0) switch_state(state::only_path);
				else {
					begin += 2;
					to_state(state::user_pwd);
				}
			} else {
				switch_state(state::user_pwd);
			}
		}
	}

	void ponly_path()
	{
		if(!is_slash()) switch_state(state::path);
		else {
			++begin;
			to_state(state::user_pwd);
		}
	}
	void pscheme_end_1() { switch_state( is_slash() ? state::scheme_end_2 : state::user_pwd ); }
	void pscheme_end_2()
	{
		if(!is_slash()) switch_state(state::user_pwd);
		else {
			user_pwd_colon_pos = StringView::npos;
			scheme = src.substr(0, begin - 2);
			++begin;
			to_state(state::user_pwd);
		}
	}
	void puser_pwd()
	{
		if(is_colon()) user_pwd_colon_pos = begin;
		else if(is_dog()) {
			auto up = src.substr(0, begin);
			if( user_pwd_colon_pos == std::string::npos) {
				user = up;
			} else {
				user = up.substr(0, user_pwd_colon_pos);
				password = up.substr(user_pwd_colon_pos+1);
			}
			user_pwd_colon_pos=StringView::npos;
			to_state(state::dog);
		} else if(is_slash() || is_end()) {
			switch_state(state::domain);
			pdomain();
			user_pwd_colon_pos=StringView::npos;
		}
	}
	void pdog() { if(!is_dog()) to_state(state::domain); }
	void pdomain()
	{
		if(user_pwd_colon_pos != StringView::npos) {
			domain = src.substr(0, user_pwd_colon_pos);
			if(is_slash()) {
				port = src.substr(user_pwd_colon_pos+1, begin-user_pwd_colon_pos-1);
				to_state(state::path);
			} else if(is_end()) {
				port = src.substr(user_pwd_colon_pos+1);
				to_state(state::finish);
			}
		} else if(is_colon() || is_slash()) {
			domain = src.substr(0, begin);
			to_state(is_colon() ? state::port : state::path);
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
			to_state(state::path);
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

	inline bool is_AZ() const { return 0x41 <= cur_symbol && cur_symbol <= 0x5A; }
	inline bool is_az() const { return 0x61 <= cur_symbol && cur_symbol <= 0x7A; }
	inline bool is_slash() const { return cur_symbol == 0x2F; }
	inline bool is_colon() const { return cur_symbol == 0x3A; }
	inline bool is_dog() const { return cur_symbol == 0x40; }
	inline bool is_digit() const { return 0x30 <= cur_symbol && cur_symbol <= 0x39; }
	inline bool is_question() const { return cur_symbol == 0x3F; }
	inline bool is_number() const { return cur_symbol == 0x23; }
	inline bool is_end() const { return src.size() <= begin + 1; }
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
	}
public:
	uri_parser_machine& operator = (const uri_parser_machine&) =delete ;
	uri_parser_machine(const uri_parser_machine&) =delete ;
	uri_parser_machine& operator = (uri_parser_machine&&) =delete ;
	uri_parser_machine(uri_parser_machine&&) =delete ;

	uri_parser_machine()
	{
		init_parser_functions();
	}

	uri_parser_machine& operator()(StringView uri)
	{
		clear_state();
		src = uri;
		if(src.empty()) return *this;
		for(begin=0;begin<src.size();++begin) {
			cur_symbol = src[begin];
			parse_fnc fnc = parse_functions[static_cast<std::size_t>(cur_state)];
			(this->*fnc)();
		}
		if(cur_state != state::finish)
			(this->*(parse_functions[static_cast<std::size_t>(cur_state)]))();
		if(cur_state != state::finish)
			throw std::runtime_error("cannot parse uri");
		return *this;
	}

	std::size_t path_pos() const
	{
		return path.empty() ? 0 :
		        scheme.size()
		      + (scheme.empty() ? 0 : 3)
		      + (user.empty() ? 0 : (user.size() + 1 + password.size() + 1))
		      + (domain.size())
		      + (port.empty() ? 0 : port.size() + 1)
		      ;
	}

	StringView scheme;
	StringView user, password;
	StringView domain;
	StringView port;
	StringView path;
	StringView query;
	StringView anchor;
};

template<typename StringView>
inline bool operator == (
        const uri_parser_machine<StringView>& left,
        const uri_parser_machine<StringView>& right)
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


template<typename StringView>
class basic_uri_parser final {
	using CharType = typename StringView::value_type;

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


	StringView source;
	uri_parser_machine<StringView> parsed;
public:
	basic_uri_parser() =default ;
	basic_uri_parser(StringView url)
	    : source(url)
	{
		recalculate();
	}

	StringView uri() const { return source; }
	void uri(StringView nu)
	{
		source = nu;
		recalculate();
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
		static const std::array<CharType, 2> static_slash{ 0x2F, 0x00 };
		auto ret = parsed.path;
		if(ret.empty()) return StringView{&static_slash.front(), 1};
		return ret;
	}
	StringView request() const
	{
		std::size_t pos = parsed.path_pos();
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
			if(amp_pos==StringView::npos) amp_pos = data.size();
			std::size_t name_finish_pos = std::min(eq_pos, amp_pos);
			auto cur_name = data.substr(begin, name_finish_pos - begin);
			if(cur_name == name) {
				if(eq_pos == StringView::npos)
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

} // namespace http_parser
