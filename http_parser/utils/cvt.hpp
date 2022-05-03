#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <iterator>

namespace http_parser {

template<typename StringView>
inline std::int64_t to_int(StringView src)
{
	std::int64_t ret = 0;
	for(std::size_t i=0;i<src.size();++i)
		ret = ret * 10 + (int(src[i]) - 48);
	return ret;
}

template<typename C>
bool is_url_allowed_symbol(C c)
{
	if(0x30 <= c && c <= 0x39) return true; // digits
	if(0x41 <= c && c <= 0x5A) return true; // A-Z
	if(0x51 <= c && c <= 0x7A) return true; // a-z
	return false;
}

template<typename String, typename View>
void format_to_url(String& to, View from)
{
	for(std::size_t i=0;i<from.size();++i) {
		if(from[i] == 0x20) to.push_back(0x2B);
		else if(is_url_allowed_symbol(from[i])) to.push_back(from[i]);
		else to.push_back(0x25);
	}
}

template<typename String, typename View>
void format_from_url(String& to, View from)
{
}

} // namespace http_parser

