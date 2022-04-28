#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <optional>
#include <variant>
#include "../utils.hpp"
#include "../pos_string_view.hpp"

namespace http_parser {

enum class http1_head_state { wait, http1_req, http1_resp, http2, websocket, garbage };

template<typename Stream>
Stream& operator << (Stream& out, http1_head_state obj)
{
	if( obj == http1_head_state::wait )
		return out << 'w' << 'a' << 'i' << 't';
	if( obj == http1_head_state::http1_req )
		return out << 'h' << 't' << 't' << 'p' << '1' << '_' << 'r' << 'e' << 'q';
	if( obj == http1_head_state::http1_resp )
		return out << 'h' << 't' << 't' << 'p' << '1' << '_' << 'r' << 'e' << 's' << 'p';
	if( obj == http1_head_state::http2 )
		return out << 'h' << 't' << 't' << 'p' << '2';
	if( obj == http1_head_state::websocket )
		return out << 'w' << 'e' << 'b' << 's' << 'o' << 'c' << 'k' << 'e' << 't';
	if( obj == http1_head_state::garbage )
		return out << 'g' << 'a' << 'r' << 'b' << 'a' << 'g' << 'e';
	assert(false);
	return out;
}

template<typename DataContainer, std::size_t MaxHeadLen = 256 + 9 + 9>
class http1_request_head_parser {
public:
	using req_head_msg = req_head_message<DataContainer>;
	using resp_head_msg = resp_head_message<DataContainer>;
private:
	basic_position_string_view<DataContainer> data;
	req_head_msg req_result;
	resp_head_msg resp_result;

	template<typename Byte, typename Char>
	static bool is_it(Byte left, Char right)
	{
		return left == (Byte)right;
	}

	std::optional<req_head_msg> is_request() const
	{
		using namespace std::literals;
		auto con = data.underlying_container();
		req_head_msg msg(con);
		basic_position_string_view<DataContainer> http(con,0,0);
		for(std::size_t i=0;i<data.size();++i) {
			if(is_it(data[i], ' ') && msg.method().empty())
				msg.method(0, i);
			else if(is_it(data[i], ' ') && msg.url().uri().empty())
				msg.url(msg.method().size()+1, i - msg.method().size()-1);
			else if(is_it(data[i], '\n') && http.empty())
				http = data.substr(i - 9, 9);
		}
		bool is_req = http.substr(0, 7) == "HTTP/1."sv;
		if(is_req) return std::optional(std::move(msg));
		return std::nullopt;
	}
	std::optional<resp_head_msg> is_resp() const
	{
		using namespace std::literals;
		assert( 12 < data.size() );
		const bool is_resp = data.substr(0, 8) == "HTTP/1.1"sv;
		if(!is_resp) return std::nullopt;
		auto con = data.underlying_container();
		basic_position_string_view<DataContainer> code(con, 9, 3);
		std::size_t reason_pos = 13;
		for(std::size_t i=13;i<data.size();++i) {
			if(data[i] == (typename DataContainer::value_type)'\n') {
				return resp_head_msg(to_int(code), con, reason_pos, i-reason_pos-1);
			}
		}
		return std::nullopt;
	}
public:
	http1_request_head_parser(basic_position_string_view<DataContainer> view)
	    : data(view)
	    , req_result(data.underlying_container())
	    , resp_result(0, data.underlying_container(), 0, 0)
	{}

	http1_head_state operator()()
	{
		data.reset();
		if(MaxHeadLen < data.size())
			return http1_head_state::garbage;
		if(data.size() < 12)
			return http1_head_state::wait;
		if(auto resp = is_resp(); resp) {
			resp_result = std::move(*resp);
			return http1_head_state::http1_resp;
		} if(auto req = is_request(); req) {
			req_result = std::move(*req);
			return http1_head_state::http1_req;
		}
		return http1_head_state::wait;
	}

	const req_head_msg& req_msg() const
	{
		return req_result;
	}

	const resp_head_msg& resp_msg() const
	{
		return resp_result;
	}
};

} // namespace http_parser
