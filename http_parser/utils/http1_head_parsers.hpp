#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <optional>

#include "pos_string_view.hpp"
#include "../utils.hpp"

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
	std::size_t pos=0;
	http1_head_state status = http1_head_state::wait;

	template<typename Byte, typename Char>
	static bool is_it(Byte left, Char right)
	{
		return left == (Byte)right;
	}

	std::optional<req_head_msg> is_request()
	{
		using namespace std::literals;
		auto con = data.underlying_container();
		req_head_msg msg(con);
		basic_position_string_view<DataContainer> http(con,0,0);
		std::size_t end = std::min(data.size(), MaxHeadLen);
		for(pos=0;pos<end;++pos) {
			if(is_it(data[pos], ' ')) {
				if( msg.method().empty() ) msg.method(0, pos);
				else {
					msg.url(msg.method().size()+1, pos - msg.method().size()-1);
					http = data.substr(pos+1, 10);
					pos += 10;
					break;
				}
			}
		}
		if(end <= pos) pos = end-1;
		bool is_req = http.size() == 10 && http.substr(0, 7) == "HTTP/1."sv;
		if(is_req) return std::optional(std::move(msg));
		if(http.size() == 10 && !is_it(http[9], '\n'))
			status = http1_head_state::garbage;
		return std::nullopt;
	}
	std::optional<resp_head_msg> is_resp()
	{
		using namespace std::literals;
		assert( 12 < data.size() );
		const bool is_resp = data.substr(0, 8) == "HTTP/1.1"sv;
		if(!is_resp) return std::nullopt;
		auto con = data.underlying_container();
		basic_position_string_view<DataContainer> code(con, 9, 3);
		std::size_t reason_pos = 13;
		for(pos=13;pos<data.size();++pos) {
			if(data[pos] == (typename DataContainer::value_type)'\n') {
				return resp_head_msg(to_int(code), con, reason_pos, pos-reason_pos-1);
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

	std::size_t end_position() const { return pos+1; }

	http1_head_state operator()()
	{
		data.reset();
		status = http1_head_state::wait;
		if(data.size() < 12)
			return status;
		if(auto resp = is_resp(); resp) {
			resp_result = std::move(*resp);
			return http1_head_state::http1_resp;
		}
		if(auto req = is_request(); req) {
			req_result = std::move(*req);
			return http1_head_state::http1_req;
		}
		if(MaxHeadLen < data.size())
			status = http1_head_state::garbage;
		return status;
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
