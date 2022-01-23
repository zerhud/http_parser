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
	advance_parsing(data);
	for(cur_pos=0;cur_pos<parsing.size();++cur_pos) {
		cur_symbol = parsing[cur_pos];
		parse();
	}
	if(cur_state == state::end) parse();
	return *this;
}

void response_parser::advance_parsing(std::string_view data)
{
	assert((parsing.empty() && result.data().empty()) || result.data().size() != 0);
	std::size_t size_before = parsing.size();
	std::size_t starting_pos =
	          parsing.empty()
	        ? 0
	        : result.data().size() - parsing.size();
	result.data() += data;
	parsing = std::string_view(
	            result.data().data() + starting_pos,
	            size_before + data.size());
	assert(parsing.empty() || &parsing.back() == &result.data().back());
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
	case state::end: exec_end(); break;
	}
}

void response_parser::to_state(state st)
{
	parsing = parsing.substr(cur_pos);
	cur_pos = 0;
	cur_state = st;
	assert(parsing.empty() || parsing.back() == result.data().back());
}

void response_parser::pop_back_parsing()
{
	assert(!parsing.empty());
	assert(cur_pos != 0);
	parsing.remove_prefix(1);
	cur_pos = 0;
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
	assert(0 < result.content_lenght);
	if(cur_pos < result.content_lenght)
		cur_pos = result.content_lenght-1;
	else if(cur_pos == result.content_lenght) {
		result.content = parsing.substr(1, cur_pos);
		to_state(state::end);
	}
	else assert(false);
}

void response_parser::exec_end()
{
	assert(callback);
	assert(!parsing.empty());
	assert(!result.data().empty());
	std::size_t tail_size = parsing.size() - 1;
	response_message tmp(std::move(result));
	result = response_message(mem);
	cur_pos = 0;
	assert(tail_size <= tmp.data().size());
	if(tail_size != 0)
		result.data() += tmp.data().substr(tmp.data().size() - tail_size);
	parsing = std::string_view(result.data().data(), result.data().size());
	to_state(state::begin);
	callback(std::move(tmp));
}

void http_utils::response_message::move_headers(response_message& other)
{
	assert(headers_.empty());
	headers_.reserve(other.headers_.size());
	for(auto& h:other.headers_) {
		auto& cur = headers_.emplace_back(&data_);
		cur.name.assign(h.name);
		cur.value.assign(h.value);
	}
}

http_utils::response_message::response_message(std::pmr::memory_resource* mem)
    : mem(mem)
    , data_(mem)
    , reason(&data_)
    , content(&data_)
{}

http_utils::response_message::response_message(response_message&& other)
    : mem(other.mem)
    , data_(std::move(other.data_))
    , headers_(mem)
    , code(other.code)
    , content_lenght(other.content_lenght)
    , reason(&data_, other.reason)
    , content(&data_, other.content)
{
	move_headers(other);
}

http_utils::response_message& http_utils::response_message::operator =(response_message&& other)
{
	mem = other.mem;
	data_ = std::move(other.data_);
	headers_ = std::pmr::vector<header_view>(mem);
	move_headers(other);
	content_lenght = other.content_lenght;
	reason.assign(&data_, other.reason);
	content.assign(&data_, other.content);
	return *this;
}
