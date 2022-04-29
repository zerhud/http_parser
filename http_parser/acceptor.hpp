#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <variant>
#include "message.hpp"
#include "utils/http1_head_parsers.hpp"

namespace http_parser {

template<typename DataContainer>
struct acceptor_traits {
	virtual ~acceptor_traits() noexcept =default ;
	virtual void on_http1_response() {}
	virtual DataContainer create_data_container() =0 ;
};

template<
        typename DataContainer,
        template<class> class Container
        >
class acceptor final {
public:
	using traits_type = acceptor_traits<DataContainer>;
	using request_head = req_head_message<DataContainer>;
	using response_head = resp_head_message<DataContainer>;
private:
	traits_type* traits;
	enum state_t { start, wait, http1, http2, websocket };

	state_t cur_state = state_t::start;
	DataContainer data;
	std::variant<request_head, response_head> result_head;

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
		}
	}
public:
	acceptor(traits_type* traits)
	    : traits(traits)
	    , data(traits->create_data_container())
	    , result_head(request_head{&data})
	{}

	template<typename S>
	void operator()(S&& buf){
		create_container();
		for(auto& s:buf) { data.push_back((typename DataContainer::value_type) s); }
		if(cur_state == state_t::wait) determine();
	}
};

} // namespace http_parser
