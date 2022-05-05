#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <array>
#include "cvt.hpp"
#include "find.hpp"
#include "pos_string_view.hpp"

namespace http_parser {

template<typename Container>
class chunked_body_parser {
	enum class state_t { wait, size, body, finish, error };
	typedef bool (chunked_body_parser::*parse_fnc)();

	basic_position_string_view<Container> src, body;
	bool wait = true, last_chunk = false;
	std::size_t last_pos=0, body_size = 0;

	state_t cur_state = state_t::size;

	std::array<parse_fnc, 5> parse_funcs;

	void init_parse()
	{
		parse_funcs[static_cast<std::size_t>(state_t::wait)] = &chunked_body_parser::pwait;
		parse_funcs[static_cast<std::size_t>(state_t::size)] = &chunked_body_parser::psize;
		parse_funcs[static_cast<std::size_t>(state_t::body)] = &chunked_body_parser::pbody;
		parse_funcs[static_cast<std::size_t>(state_t::finish)] = &chunked_body_parser::plast;
		parse_funcs[static_cast<std::size_t>(state_t::error)] = &chunked_body_parser::plast;
	}

	void parse()
	{
		parse_fnc fnc;
		do {
			fnc = parse_funcs[static_cast<std::size_t>(cur_state)];
		} while((this->*fnc)());
	}

	bool pwait() {
		cur_state = state_t::size;
		return true;
	}
	bool psize() {
		auto npos = find((std::uint64_t*)src.data(), src.size(), (std::uint8_t)'\n');
		if(npos == 0 || src.size() <= npos) return false;
		if(npos < 2) {
			cur_state = state_t::error;
			return false;
		}
		body_size = to_int(src.substr(0, npos-1));
		src = src.substr(npos);
		cur_state = state_t::body;
		return true;
	}
	bool pbody() {
		if(src.size() <= body_size) return false;
		body = src.substr(1, body_size);
		src = src.substr(body_size + 1);
		cur_state = body_size == 0 ? state_t::finish : state_t::wait;
		return false;
	}
	bool plast() {
		return false;
	}
public:
	chunked_body_parser(basic_position_string_view<Container> data)
	    : src(data.underlying_container())
	    , body(data)
	{
		init_parse();
	}

	bool operator()()
	{
		if(finish()) return false;
		body.resize(0);
		src.advance_to_end();
		parse();
		return ready();
	}

	bool ready() const
	{
		return cur_state == state_t::wait || state_t::finish <= cur_state ;
	}

	basic_position_string_view<Container> result() const
	{
		return body;
	}

	bool finish() const
	{
		return state_t::finish <= cur_state;
	}

	bool error() const
	{
		return cur_state == state_t::error;
	}
};

} // namespace http_parser
