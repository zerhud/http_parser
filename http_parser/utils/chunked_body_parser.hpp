#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include "pos_string_view.hpp"

namespace http_parser {

template<typename Container>
class chunked_body_parser {
	basic_position_string_view<Container> src, body;
public:
	chunked_body_parser(basic_position_string_view<Container> data)
	    : src(data.underlying_container())
	    , body(data)
	{}

	bool operator()()
	{
		src.advance_to_end();
		return false;
	}

	bool waiting() const
	{
		return false;
	}

	basic_position_string_view<Container> result() const
	{
		return body;
	}
};

} // namespace http_parser
