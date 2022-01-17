/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include "response_parser.hpp"

#include <cassert>
#include <charconv>

using namespace std::literals;
using http_utils::response_parser;

response_parser::response_parser(std::function<void (http_utils::response_message)> cb)
    : response_parser(std::pmr::get_default_resource(), std::move(cb))
{
}

response_parser::response_parser(
        std::pmr::memory_resource* mr,
        std::function<void (http_utils::response_message)> cb
        )
    : mem(mr)
    , callback(std::move(cb))
    , result(mem)
{
}

response_parser& response_parser::operator()(std::string_view data)
{
	std::size_t size_before = parsing.size();
	std::ptrdiff_t starting_pos =
	          parsing.empty()
	        ? 0
	        : parsing.data() - result.data().data();
	result.data() += data;
	parsing = std::string_view(
	            result.data().data() + starting_pos,
	            size_before + data.size());
	for(cur_pos=0;cur_pos<parsing.size();++cur_pos) {
		cur_symbol = parsing[cur_pos];
		parse();
	}
	if(cur_state == state::end) parse();
	return *this;
}

void response_parser::parse()
{
	switch(cur_state) {
	case state::begin: pbegin(); break;
	case state::code: pcode(); break;
	case state::reason: preason(); break;
	case state::header_begin: phbegin(); break;
	case state::header_name: phname(); break;
	case state::header_sep: phsep(); break;
	case state::header_value: phval(); break;
	case state::content_begin: pcontent_begin(); break;
	case state::content: pcontent(); break;
	case state::finishing: pfinishing(); break;
	case state::end: exec_end(); break;
	}
}

void response_parser::to_state(state st)
{
	parsing = parsing.substr(cur_pos);
	cur_pos = 0;
	cur_state = st;
}

void response_parser::pop_back_parsing()
{
	assert(!parsing.empty());
	assert(cur_pos != 0);
	parsing = parsing.substr(1);
	cur_pos -= 1;
}

void response_parser::pbegin()
{
	if(cur_pos < 9) return;
	if(cur_pos == 9) {
		if(parsing.substr(0,9) == "HTTP/1.1 ") to_state(state::code);
		else pop_back_parsing();
	}
	assert( cur_pos == 0 || cur_pos == 9 );
}

void response_parser::pcode()
{
	if(0x29 < cur_symbol && cur_symbol < 0x3A) return;
	auto code = parsing.substr(0, cur_pos);
	auto cr = std::from_chars(code.data(), code.data() + code.size(), result.code);
	if(cr.ec != std::errc()) to_state(state::begin);
	else to_state(state::reason);
}

void response_parser::preason()
{
	if(cur_symbol == '\r') {
		result.reason = parsing.substr(1, cur_pos-1);
		to_state(state::header_begin);
	}
}

void response_parser::phbegin()
{
	if(cur_pos == 1) {
		if(parsing.substr(0,2) == "\r\n"sv)
			to_state(state::header_name);
		else to_state(state::begin);
	}
	assert(cur_pos < 2);
}

void response_parser::phname()
{
	if(cur_pos == 1 && parsing.substr(1,2)=="\r\n"sv)
		to_state(state::content_begin);
	else if(cur_symbol == ':') {
		result.add_header_name(parsing.substr(1,cur_pos-1));
		to_state(state::header_sep);
	}
}

void response_parser::phsep()
{
	assert( cur_pos != 0 || cur_symbol == ':' );
	if(0 < cur_pos && cur_symbol != ' ')
		to_state(state::header_value);
}

void response_parser::phval()
{
	if(cur_symbol == '\r') {
		assert(!result.headers_empty());
		result.last_header_value(parsing.substr(0, cur_pos));
		to_state(state::header_begin);
	}
}

void response_parser::pcontent_begin()
{
	if(cur_pos == 1 && parsing.substr(0,2)=="\r\n"sv) {
		auto len_val = result.find_header("Content-Length"sv);
		if(!len_val) {
			to_state(state::end);
			result.content_lenght = 0;
		} else {
			to_state(state::content);
			std::size_t len;
			auto cr = std::from_chars(
			            len_val->data(),
			            len_val->data() + len_val->size(),
			            len);
			if(cr.ec != std::errc()) to_state(state::begin);
			else result.content_lenght = len;
		}
	} else if(1 < cur_pos ) {
		to_state(state::end);
	}
}

void response_parser::pcontent()
{
	if(cur_pos == result.content_lenght+1) {
		result.content = parsing.substr(1, cur_pos-1);
		to_state(state::finishing);
	}
}

void response_parser::pfinishing()
{
	if(cur_pos == 3) to_state(state::end);
}

void response_parser::exec_end()
{
	assert(callback);
	response_message tmp(mem);
	std::swap(tmp, result);
	callback(tmp);
}