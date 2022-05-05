#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include "../message.hpp"

namespace http_parser {

template<typename DataContainer, template<class> class Container>
class headers_parser final {
public:
	using container_view = basic_position_string_view<DataContainer>;
	using result_type = header_message<Container, DataContainer>;
private:
	typedef void (headers_parser::*parse_fnc)();
	enum state_t { name, space, value, finish } ;

	result_type result;
	state_t cur_state = state_t::name;
	std::size_t cur_pos = 0;
	std::size_t switch_pos = 0;
	container_view source;
	std::array<parse_fnc, 4> funcs;

	void silent_to_state(state_t ns) { switch_pos = cur_pos; cur_state = ns; }
	void init_srates() {
		funcs[static_cast<std::size_t>(state_t::name)] = &headers_parser::pname;
		funcs[static_cast<std::size_t>(state_t::space)] = &headers_parser::pspace;
		funcs[static_cast<std::size_t>(state_t::value)] = &headers_parser::pvalue;
		funcs[static_cast<std::size_t>(state_t::finish)] = &headers_parser::pfinish;
	}

	inline void parse()
	{
		(this->*(funcs[static_cast<std::size_t>(cur_state)]))();
	}

	bool is_end() { return source.size() <= cur_pos + 1; }
	bool is_r() const { return source[cur_pos] == 0x0D; }
	bool is_n() const { return source[cur_pos] == 0x0A; }
	bool is_space() const { return source[cur_pos] == 0x20; }
	std::size_t find_colon()
	{
		assert(switch_pos <= cur_pos);
		for(std::size_t i=cur_pos;i<source.size();++i) if(source[i] == 0x3A) return i;
		return 0;
	}
	std::size_t find_r()
	{
		assert(switch_pos <= cur_pos);
		for(std::size_t i=cur_pos;i<source.size();++i) if(source[i] == 0x0D) return i;
		return 0;
	}

	void pname()
	{
		if(is_n()) silent_to_state(state_t::finish);
		else {
			auto pos = find_colon();
			if(pos != 0) {
				result.add_header_name(switch_pos, pos-switch_pos);
				cur_pos = pos;
				silent_to_state(state_t::space);
			}
		}
	}
	void pspace()
	{
		if(!is_space()) {
			silent_to_state(state_t::value);
			pvalue();
		}
	}
	void pvalue()
	{
		auto pos = find_r();
		if(pos != 0) {
			result.last_header_value(switch_pos, pos-switch_pos);
			cur_pos = pos + 2;
			silent_to_state(state_t::name);
		}
	}
	void pfinish()
	{
	}
public:
	template<typename ... Args>
	headers_parser(container_view data, Args... args)
	    : result(data.underlying_container(), std::forward<Args>(args)...)
	    , source(data)
	{
		init_srates();
	}

	void skip_first_bytes(std::size_t count)
	{
		cur_pos += count;
		switch_pos += count;
	}

	bool is_finished() const
	{
		return cur_state == state_t::finish;
	}

	std::size_t finish_position() const
	{
		return cur_pos;
	}

	std::size_t operator()()
	{
		source.advance_to_end();
		for(;cur_pos<source.size() && !is_finished();++cur_pos) parse();
		return cur_pos;
	}

	[[nodiscard]]
	result_type extract_result() { return result; }
};

} // namespace http_parser
