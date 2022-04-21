#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include "../message.hpp"

namespace http_parser {

std::size_t find(std::uint64_t* data, std::size_t size, std::uint8_t symbol)
{
	std::uint64_t mask1 = symbol;
	std::uint64_t mask2 = mask1 << 8;
	std::uint64_t mask3 = mask2 << 8;
	std::uint64_t mask4 = mask3 << 8;
	std::uint64_t mask5 = mask4 << 8;
	std::uint64_t mask6 = mask5 << 8;
	std::uint64_t mask7 = mask6 << 8;
	std::uint64_t mask8 = mask7 << 8;
	std::size_t dsize = size/8;
	if(size%8!=0) ++dsize;
	for(std::size_t i=0;i<dsize;++i) {
		if((data[i] & mask1) == mask1) return (i*8);
		if((data[i] & mask2) == mask2) return (i*8)+1;
		if((data[i] & mask3) == mask3) return (i*8)+2;
		if((data[i] & mask4) == mask4) return (i*8)+3;
		if((data[i] & mask5) == mask5) return (i*8)+4;
		if((data[i] & mask6) == mask6) return (i*8)+5;
		if((data[i] & mask7) == mask7) return (i*8)+6;
		if((data[i] & mask8) == mask8) return (i*8)+7;
	}
	return size;
}

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
				result.add_header_name(source.substr(switch_pos, pos-switch_pos));
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
			result.last_header_value(source.substr(switch_pos, pos-switch_pos));
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
