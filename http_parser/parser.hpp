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
#include "utils/chunked_body_parser.hpp"

namespace http_parser {


template<typename Head, typename DataContainer>
struct http1_parser_traits {
	using data_view = basic_position_string_view<DataContainer>;

	virtual ~http1_parser_traits () noexcept =default ;

	virtual DataContainer create_data_container() =0 ;
	virtual typename Head::headers_container create_headers_container() =0 ;

	virtual void on_head(const Head& head) {}
	virtual void on_message(const Head& head, const data_view& body) {}
	virtual void on_error(const Head& head, const data_view& body) {}
};


template<
        template<class> class Container,
        typename DataContainer
        >
class http1_req_base_parser {
public:
	using message_t = http1_message<Container,  req_head_message, DataContainer>;
protected:
	template<typename HeadParser>
	bool parser_head_base(message_t& msg, HeadParser& prs) const
	{
		auto st = prs();
		if(st == http1_head_state::wait) return false;
		if(st != http1_head_state::http1_req)
			throw std::runtime_error("request was await");
		msg.head() = prs.req_msg();
		return true;
	}
};

template<
        template<class> class Container,
        typename DataContainer
        >
class http1_resp_base_parser {
public:
	using message_t = http1_message<Container,  resp_head_message, DataContainer>;
protected:
	template<typename HeadParser>
	bool parser_head_base(message_t& msg, HeadParser& prs) const
	{
		auto st = prs();
		if(st == http1_head_state::wait) return false;
		if(st != http1_head_state::http1_resp)
			throw std::runtime_error("response was await");
		msg.head() = prs.resp_msg();
		return true;
	}
};

template<
        template<class> class Container,
        typename DataContainer,
        template<template<class> class,class> class BaseAcceptor,
        std::size_t max_body_size = 4 * 1024,
        std::size_t max_head_size = 1 * 1024
        >
class http1_parser final : protected BaseAcceptor<Container, DataContainer> {
	using base_acceptor_t = BaseAcceptor<Container, DataContainer>;
public:
	using message_t = base_acceptor_t::message_t;
	using traits_type = http1_parser_traits<message_t, DataContainer>;
private:
	enum class state_t { wait, head, headers, body, finish };

	state_t cur_state = state_t::wait;
	traits_type* traits;
	DataContainer data, body_data;
	basic_position_string_view<DataContainer> head_view;
	basic_position_string_view<DataContainer> body_view;

	message_t result_msg;

	headers_parser<DataContainer, Container> parser_hrds;

	void parse_head() {
		assert(traits);
		head_view.reset();
		http1_request_head_parser prs(head_view);
		if(base_acceptor_t::parser_head_base(result_msg, prs)) {
			cur_state = state_t::head;
			parser_hrds.skip_first_bytes(prs.end_position());
		}
	}
	void parse_headers() {
		parser_hrds();
		require_head_limit(parser_hrds.finish_position());
		if(!parser_hrds.is_finished()) return;
		result_msg.headers() = parser_hrds.result();
		cur_state = state_t::headers;
	}
	void headers_ready() {
		const bool body_exists = result_msg.headers().body_exists();
		move_to_body_container(parser_hrds.finish_position());
		if(body_exists) traits->on_head(result_msg);
		else traits->on_message( result_msg, body_view );
		cur_state = body_exists ? state_t::body : state_t::finish;
	}
	void parse_body() {
		body_view.advance_to_end();
		if(auto size=result_msg.headers().content_size(); size)
			parse_single_body(*size);
		else if(result_msg.headers().is_chunked())
			parse_chunked_body();
	}

	void clean_body(std::size_t actual_pos)
	{
		auto body = traits->create_data_container();
		for(std::size_t i=actual_pos;i<body_data.size();++i)
			body.push_back(body_data[i]);
		body_data = std::move(body);
		body_view.reset();
	}

	void parse_single_body(std::size_t size)
	{
		if(size <= body_view.size()) {
			body_view.resize(size);
			traits->on_message(result_msg, body_view);
			cur_state = state_t::finish;
		}
	}

	void parse_chunked_body()
	{
		chunked_body_parser prs(body_view);
		while(prs()) {
			if(prs.error()) traits->on_error(result_msg, body_view);
			else if(prs.ready()) traits->on_message(result_msg, prs.result());
		}
		clean_body(prs.end_pos());
		if(prs.finish()) cur_state = state_t::finish;
	}

	void move_to_body_container(std::size_t start)
	{
		for(std::size_t i=start;i<data.size();++i)
			body_data.push_back(data[i]);
		body_view.advance_to_end();
	}

	void require_limits(std::size_t size_to_add)
	{
		using namespace std::literals;
		if(cur_state == state_t::body) {
			if(max_body_size < body_data.size() + size_to_add)
				throw std::out_of_range("maximum body size reached"s);
		} else {
			require_head_limit(data.size() + size_to_add);
		}
	}

	void require_head_limit(std::size_t size_now) const
	{
		using namespace std::literals;
		if(max_head_size < size_now)
			    throw std::out_of_range("maximum head size reached"s);
	}
public:
	http1_parser(traits_type* traits)
	    : traits(traits)
	    , data(traits->create_data_container())
	    , body_data(traits->create_data_container())
	    , head_view(&data, 0, 0)
	    , body_view(&body_data, 0, 0)
	    , result_msg(&data, traits->create_headers_container())
	    , parser_hrds(&data, traits->create_headers_container())
	{}

	template<typename S>
	void operator()(S&& buf) {
		assert( cur_state <= state_t::finish );
		if(!data.empty()) require_limits( buf.size() );
		if(cur_state == state_t::body)
			for(auto& s:buf) body_data.push_back((typename DataContainer::value_type) s);
		else for(auto& s:buf) data.push_back((typename DataContainer::value_type) s);
		if(cur_state == state_t::wait) parse_head();
		if(cur_state == state_t::head) parse_headers();
		if(cur_state == state_t::headers) headers_ready();
		if(cur_state == state_t::body) parse_body();
	}
};

template<
        template<class> class Container,
        typename DataContainer,
        std::size_t max_body_size = 4 * 1024,
        std::size_t max_head_size = 1 * 1024
        >
using http1_req_parser = http1_parser<Container, DataContainer, http1_req_base_parser, max_body_size, max_head_size>;

template<
        template<class> class Container,
        typename DataContainer,
        std::size_t max_body_size = 4 * 1024,
        std::size_t max_head_size = 1 * 1024
        >
using http1_resp_parser = http1_parser<Container, DataContainer, http1_resp_base_parser, max_body_size, max_head_size>;

} // namespace http_parser
