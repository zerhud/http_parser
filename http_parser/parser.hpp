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
	virtual void on_message(const head_t& head, const data_view& body, std::size_t tail) {}
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
	DataContainer data;
	basic_position_string_view<DataContainer> body_view;
	std::size_t big_body_pos = 0;

	message_t result_msg;

	headers_parser<DataContainer, ContainerFactory> parser_hdrs;

	void parse_head() {
		assert(traits);
		basic_position_string_view<DataContainer> head_view(&data, 0, 0);
		http1_request_head_parser prs(head_view);
		if(base_acceptor_t::parser_head_base(result_msg, prs)) {
			cur_state = state_t::head;
			parser_hdrs.skip_first_bytes(prs.end_position());
		}
	}
	void parse_headers() {
		parser_hdrs();
		if(!parser_hdrs.is_finished()) return;
		result_msg.headers() = parser_hdrs.result();
		cur_state = state_t::headers;
	}
	void headers_ready() {
		body_view.assign(parser_hdrs.finish_position(), 0);
		const bool body_exists = result_msg.headers().body_exists();
		if(body_exists) traits->on_head(result_msg);
		else traits->on_message( result_msg, body_view, 0 );
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
		if(data.size() - 1 <= actual_pos)
			data.resize(parser_hdrs.finish_position());
		else {
			auto body = df();
			for(std::size_t i=actual_pos;i<data.size();++i)
				body.push_back(data[i]);
			data.resize(parser_hdrs.finish_position());
			for(auto& s:body) data.push_back(s);
		}
		body_view.assign(parser_hdrs.finish_position(), 0);
	}

	void remove_parsed_data()
	{
		std::size_t start_pos = parser_hdrs.finish_position() + body_view.size();
		auto ndata = df();
		for(std::size_t i=start_pos;i<data.size();++i)
			ndata.push_back(data[i]);
		data.clear();
		body_view.reset();
		big_body_pos = 0;
		data = std::move(ndata);
	}

	void parse_single_body(std::size_t size)
	{
		const bool ready = size <= body_view.size();
		const bool overflow = max_body_size < size;
		const bool on_big_body = overflow && max_body_size <= body_view.size();
		const bool on_big_tail = overflow && size <= (big_body_pos + body_view.size()) ;
		body_view.resize(size);
		if(ready) {
			traits->on_message(result_msg, body_view, 0);
			cur_state = state_t::finish;
		} else if(on_big_body || on_big_tail) {
			big_body_pos += body_view.size();
			traits->on_message(result_msg, body_view, size - big_body_pos);
			if(size <= big_body_pos) cur_state = state_t::finish;
		}
		if(ready || on_big_body || on_big_tail)
			clean_body(parser_hdrs.finish_position() + body_view.size());
	}

	void parse_chunked_body()
	{
		chunked_body_parser prs(body_view);
		while(prs()) {
			if(prs.error()) traits->on_error(result_msg, body_view);
			else if(prs.ready()) traits->on_message(result_msg, prs.result(), 0);
		}
		clean_body(prs.end_pos() + parser_hdrs.finish_position());
		if(prs.finish()) cur_state = state_t::finish;
	}

	void parse_finish()
	{
		remove_parsed_data();
		parser_hdrs = decltype(parser_hdrs){&data, cf};
		cur_state = state_t::ready;
	}

	template<typename S>
	void copy_buf(std::size_t pos, S&& buf)
	{
		for(std::size_t i=pos;i<buf.size();++i)
			data.push_back(buf[i]);
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
	    , body_view(&data, 0, 0)
	    , result_msg(&data, this->cf)
	    , parser_hdrs(&data, this->cf)
	{
		using namespace std::literals;
		body_view.assign(&data, other.body_view);
		(*this)(""sv);
	}


	http1_parser(traits_type* traits) : http1_parser(traits, DataContainerFactory{}, ContainerFactory{}) {}
	http1_parser(traits_type* traits, DataContainerFactory df, ContainerFactory cf)
	    : df(std::move(df)), cf(std::move(cf))
	    , cur_state(state_t::wait)
	    , traits(traits)
	    , data(this->df())
	    , body_view(&data, 0, 0)
	    , result_msg(&data, this->cf)
	    , parser_hdrs(&data, this->cf)
	{
	}

	template<typename S>
	void operator()(S&& buf) {
		using namespace std::literals;
		assert( cur_state <= state_t::finish );
		if(cur_state == state_t::finish) parse_finish();
		copy_buf(0, std::forward<S>(buf));
		do {
			if(cur_state == state_t::ready) cur_state = state_t::wait;
			if(cur_state == state_t::wait) parse_head();
			if(cur_state == state_t::head) parse_headers();
			if(cur_state == state_t::headers) headers_ready();
			if(cur_state == state_t::body) parse_body();
			if(cur_state == state_t::finish) parse_finish();
		} while(cur_state == state_t::ready);
	}
};

template<
        typename ContainerFactory
      , typename DataContainerFactory
      , std::size_t max_body_size = 4 * 1024
        >
using http1_req_parser = http1_parser<
    ContainerFactory, DataContainerFactory,
    http1_req_base_parser,
    max_body_size
>;

template<
        typename ContainerFactory
      , typename DataContainerFactory
      , std::size_t max_body_size = 4 * 1024
        >
using http1_resp_parser = http1_parser<
    ContainerFactory, DataContainerFactory,
    http1_resp_base_parser,
    max_body_size
>;

} // namespace http_parser
