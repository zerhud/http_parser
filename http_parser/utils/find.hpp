#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <cstdint>

namespace http_parser {

std::size_t find(std::uint64_t* data, std::size_t size, std::uint8_t symbol)
{
	std::uint64_t mask1 = symbol;
	std::uint64_t mask2 = mask1 << 8;
	std::uint64_t mask3 = mask2 << 8;
	std::uint64_t mask4 = mask3 << 8;
	std::uint64_t mask5 = mask4 << 8;
	std::uint64_t mask6 = mask5 << 8;
	std::uint64_t mask7 = mask6 << 8;
	std::uint64_t mask8 = mask7 << 8;
	std::size_t dsize = size/8;
	if(size%8!=0) ++dsize;
	for(std::size_t i=0;i<dsize;++i) {
		if((data[i] & mask1) == mask1) return (i*8);
		if((data[i] & mask2) == mask2) return (i*8)+1;
		if((data[i] & mask3) == mask3) return (i*8)+2;
		if((data[i] & mask4) == mask4) return (i*8)+3;
		if((data[i] & mask5) == mask5) return (i*8)+4;
		if((data[i] & mask6) == mask6) return (i*8)+5;
		if((data[i] & mask7) == mask7) return (i*8)+6;
		if((data[i] & mask8) == mask8) return (i*8)+7;
	}
	return size;
}

} // namespace http_parser
