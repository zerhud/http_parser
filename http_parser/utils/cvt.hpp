#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <bit>
#include <iterator>

namespace http_parser {

template<typename S>
inline std::int8_t ascii_to_int(S s)
{
	//TODO: use unordered map or even vector for speed up
	//      assert(s < vec.size()); return vec[s];
	if((S)'0' <= s && s <= (S)'9') return (int)s - 48;
	if((S)'a' <= s && s <= (S)'z') return (int)s - 87;
	if((S)'A' <= s && s <= (S)'Z') return (int)s - 55;
	assert(false);
	return 0;
}

template<typename StringView>
inline std::int64_t to_int(StringView src, std::uint8_t base=10)
{
	std::int64_t ret = 0;
	std::int8_t sign = 1;
	if(!src.empty()) {
		std::size_t i=0;
		if(src[i]==(typename StringView::value_type)'-') sign = -1, ++i;
		for(;i<src.size();++i)
			ret = ret * base + ascii_to_int(src[i]);
	}
	return sign * ret;
}

template<typename S, typename I>
inline void to_str16(I src, S& to)
{
	static_assert(
	    std::endian::native == std::endian::big || std::endian::native == std::endian::little,
		"we cannot convert integer to hex string on current arch");
	if(src < 0) {
		to.push_back((typename S::value_type)'-');
		src *= -1;
	}
	static const std::array<char,16> symbols = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	std::byte* walker = (std::byte*)&src;
	bool leading_zero = true;
	if constexpr (std::endian::native == std::endian::big) {
		for(std::size_t i=0;i<sizeof(I);++i) {
			auto a = (typename S::value_type)symbols[std::size_t(walker[i] & (std::byte)0x0F)];
			auto b = (typename S::value_type)symbols[std::size_t(walker[i] >> 4)];
			leading_zero = leading_zero && a == '0';
			if(!leading_zero) to.push_back(a);
			leading_zero = leading_zero && b == '0';
			if(!leading_zero) to.push_back(b);
		}
	} else if constexpr (std::endian::native == std::endian::little) {
		for(std::int32_t i=sizeof(I)-1;0 <= i;--i) {
			auto a = (typename S::value_type)symbols[std::size_t(walker[i] >> 4)];
			auto b = (typename S::value_type)symbols[std::size_t(walker[i] & (std::byte)0x0F)];
			leading_zero = leading_zero && a == '0';
			if(!leading_zero) to.push_back(a);
			leading_zero = leading_zero && b == '0';
			if(!leading_zero) to.push_back(b);
		}
	}

	if(to.empty()) to.push_back((typename S::value_type)'0');
}

template<typename S, typename I, typename ... Args>
inline S to_str16c(I src, Args... args)
{
	S ret(std::forward<Args>(args)...);
	to_str16(src, ret);
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

