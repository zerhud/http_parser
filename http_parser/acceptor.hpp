#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <variant>
#include "message.hpp"
#include "utils/headers_parser.hpp"
#include "utils/http1_head_parsers.hpp"

namespace http_parser {

template<template<class> class Container, typename DataContainer>
struct acceptor_traits {
	virtual ~acceptor_traits() noexcept =default ;
	virtual void on_http1_request(
	        const http1_message<Container, req_head_message, DataContainer>& header,
	        const DataContainer& body) {}
	virtual void on_http1_response() {}
	virtual DataContainer create_data_container() =0 ;
};

template<
        template<class> class Container,
        typename DataContainer
        >
class acceptor final {
public:
	using traits_type = acceptor_traits<Container, DataContainer>;
	using request_head = req_head_message<DataContainer>;
	using response_head = resp_head_message<DataContainer>;

	using http1_message_t = http1_message<Container, req_head_message, DataContainer>;
private:
	traits_type* traits;
	enum class state_t { start, wait, http1, http2, websocket, headers, body };

	state_t cur_state = state_t::start;
	DataContainer data, body;
	basic_position_string_view<DataContainer> parsing;

	std::variant<request_head, response_head> result_head;
	http1_message_t result_request;
	header_message headers;

	headers_parser<DataContainer, Container> parser_headers;

	void create_container()
	{
		if(cur_state != state_t::start) return;
		data = traits->create_data_container();
		cur_state = state_t::wait;
	}

	void determine()
	{
		assert(traits);
		basic_position_string_view view(&data);
		http1_request_head_parser prs(view);
		auto st = prs();
		if(st == http1_head_state::http1_resp) {
			cur_state = state_t::http1;
			result_head = prs.resp_msg();
			traits->on_http1_response();
			cur_state = state_t::headers;
			parsing = parsing.substr(prs.end_position());
		}
		else if(st == http1_head_state::http1_req) {
			cur_state = state_t::http1;
			result_request.head() = prs.req_msg();
			traits->on_http1_request(result_request, body);
			cur_state = state_t::headers;
			parsing = parsing.substr(prs.end_position());
		}
	}

	void parse_headers()
	{
		parser_headers();
		if(parser_headers.is_finished())
	}
public:
	acceptor(traits_type* traits)
	    : traits(traits)
	    , data(traits->create_data_container())
	    , body(traits->create_data_container())
	    , parsing(&data)
	    , result_head(request_head{&data})
	    , result_request(&body)
	{}

	template<typename S>
	void operator()(S&& buf){
		create_container();
		for(auto& s:buf) { data.push_back((typename DataContainer::value_type) s); }
		if(cur_state == state_t::wait) determine();
		if(state_t::wait < cur_state) parser_headers();
	}
};

} // namespace http_parser
