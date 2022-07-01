#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <string_view>
#include "concepts.hpp"

namespace http_parser {

template<iteratible S>
auto md5(const S& src)
{
	using namespace std::literals;
	return ""sv;
}

} // namespace http_parser
