#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <variant>
#include <type_traits>
#include "message.hpp"
#include "utils/headers_parser.hpp"
#include "utils/http1_head_parsers.hpp"
#include "utils/chunked_body_parser.hpp"

namespace http_parser {

struct pmr_string_factory {
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();
	std::pmr::string operator()() const
	{ return std::pmr::string{mem}; }
};

template<typename T>
struct pmr_vector_t_factory {
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();
	std::pmr::vector<T> operator()() const
	{ return std::pmr::vector<T>{mem}; }
};

template<typename Head, typename DataContainer>
struct http1_parser_traits {
	using head_t = Head;
	using data_view = basic_position_string_view<DataContainer>;

	virtual ~http1_parser_traits () noexcept =default ;

	virtual void on_head(const head_t& head) {}
	virtual void on_message(const head_t& head, const data_view& body) {}
	virtual void on_error(const head_t& head, const data_view& body) {}
};


template<
        typename DataContainer
      , typename ContainerFactory
        >
class http1_req_base_parser {
public:
	using message_t = http1_message<req_head_message, DataContainer, ContainerFactory>;
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
        typename DataContainer
      , typename ContainerFactory
        >
class http1_resp_base_parser {
public:
	using message_t = http1_message<resp_head_message, DataContainer, ContainerFactory>;
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
        typename ContainerFactory
      , typename DataContainerFactory
      , template<class,class> class BaseAcceptor
      , std::size_t max_body_size = 4 * 1024
      , std::size_t max_head_size = 1 * 1024
      , typename DataContainer = decltype(std::declval<DataContainerFactory>()())
        >
class http1_parser final : protected BaseAcceptor<DataContainer, ContainerFactory> {
	using base_acceptor_t = BaseAcceptor<DataContainer, ContainerFactory>;
public:
	using message_t = base_acceptor_t::message_t;
	using traits_type = http1_parser_traits<message_t, DataContainer>;
private:
	enum class state_t { ready, wait, head, headers, body, finish };

	DataContainerFactory df;
	ContainerFactory cf;

	state_t cur_state = state_t::ready;
	traits_type* traits;
	DataContainer data, body_data;
	basic_position_string_view<DataContainer> body_view;

	message_t result_msg;

	headers_parser<DataContainer, ContainerFactory> parser_hrds;

	void parse_head() {
		assert(traits);
		basic_position_string_view<DataContainer> head_view(&data, 0, 0);
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
		auto body = df();
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

	void parse_finish()
	{
		data.clear();
		move_tail_to_head();
		parser_hrds = decltype(parser_hrds){&data, cf};
		cur_state = state_t::ready;
	}

	void move_to_body_container(std::size_t start)
	{
		for(std::size_t i=start;i<data.size();++i)
			body_data.push_back(data[i]);
		body_view.advance_to_end();
	}

	void move_tail_to_head()
	{
		for(std::size_t i=body_view.size();i<body_data.size();++i)
			data.push_back(body_data[i]);
		body_data.clear();
		body_view.reset();
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
	http1_parser(const http1_parser&) =delete ;
	http1_parser& operator = (const http1_parser&) =delete ;
	http1_parser& operator = (http1_parser&& other) =delete ;

	http1_parser(http1_parser&& other)
	    : df(std::move(other.df))
	    , cf(std::move(other.cf))
	    , cur_state(state_t::wait)
	    , traits(other.traits)
	    , data(std::move(other.data))
	    , body_data(std::move(other.body_data))
	    , body_view(&body_data, 0, 0)
	    , result_msg(&data, this->cf)
	    , parser_hrds(&data, this->cf)
	{
		using namespace std::literals;
		body_view.assign(&body_data, other.body_view);
		(*this)(""sv);
	}


	http1_parser(traits_type* traits) : http1_parser(traits, DataContainerFactory{}, ContainerFactory{}) {}
	http1_parser(traits_type* traits, DataContainerFactory df, ContainerFactory cf)
	    : df(std::move(df)), cf(std::move(cf))
	    , cur_state(state_t::wait)
	    , traits(traits)
	    , data(this->df())
	    , body_data(this->df())
	    , body_view(&body_data, 0, 0)
	    , result_msg(&data, this->cf)
	    , parser_hrds(&data, this->cf)
	{
	}

	template<typename S>
	void operator()(S&& buf) {
		using namespace std::literals;
		assert( cur_state <= state_t::finish );
		if(!data.empty()) require_limits( buf.size() );
		if(cur_state == state_t::finish) parse_finish();
		if(cur_state == state_t::body)
			for(auto& s:buf) body_data.push_back((typename DataContainer::value_type) s);
		else for(auto& s:buf) data.push_back((typename DataContainer::value_type) s);
		do {
			if(cur_state == state_t::ready) cur_state = state_t::wait;
			if(cur_state == state_t::wait) parse_head();
			if(cur_state == state_t::head) parse_headers();
			if(cur_state == state_t::headers) headers_ready();
			if(cur_state == state_t::body) parse_body();
			if(cur_state == state_t::finish) (*this)(""sv);
		} while(cur_state == state_t::ready);
	}
};

template<
        typename ContainerFactory
      , typename DataContainerFactory
      , std::size_t max_body_size = 4 * 1024
      , std::size_t max_head_size = 1 * 1024
        >
using http1_req_parser = http1_parser<
    ContainerFactory, DataContainerFactory,
    http1_req_base_parser,
    max_body_size, max_head_size
>;

template<
        typename ContainerFactory
      , typename DataContainerFactory
      , std::size_t max_body_size = 4 * 1024
      , std::size_t max_head_size = 1 * 1024
        >
using http1_resp_parser = http1_parser<
    ContainerFactory, DataContainerFactory,
    http1_resp_base_parser,
    max_body_size, max_head_size
>;

} // namespace http_parser
