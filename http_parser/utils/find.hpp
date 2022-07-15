#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <cstdint>

namespace http_parser {

template<typename S>
std::size_t find(const char* data, std::size_t size, S symbol)
{
	for(std::size_t i=0;i<size;++i)
		if(
		        (*((const S*)&data[i]) == symbol)
		     && (sizeof(S) + i <= size))
			return i;
	return size;
}

} // namespace http_parser
