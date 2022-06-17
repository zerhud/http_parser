#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

namespace http_parser {

template<typename T>
concept Buffer = requires(const T& buf)
{
	buf.size();
	buf[0];
};

} // namespace http_parser
