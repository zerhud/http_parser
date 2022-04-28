#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

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
private:
	traits_type* traits;
	enum state_t { start, wait, http1, http2, websocket };

	state_t cur_state = state_t::start;
	DataContainer data;

	void create_container()
	{
		if(cur_state != state_t::start) return;
		data = traits->create_data_container();
		cur_state = state_t::wait;
	}

	void determine()
	{
		;
	}
public:
	acceptor(traits_type* traits)
	    : traits(traits)
	    , data(traits->create_data_container())
	{}

	template<typename S>
	void operator()(S&& buf){
		create_container();
		for(auto& s:buf) { data.push_back((typename DataContainer::value_type) s); }
		if(cur_state == state_t::wait) determine();
	}
};

} // namespace http_parser
