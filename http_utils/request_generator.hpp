#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <string>
#include <memory_resource>

namespace http_utils {

struct uri {
	uri(std::string_view u) : u(u) {}
	std::string_view u;
};

struct header {
	header(std::string_view n, std::string_view v) : n(n), v(v) {}
	std::string_view n, v;
};

class request_generator {
	std::pmr::memory_resource* mem;
	std::pmr::string headers;
	std::pmr::string head;
public:
	request_generator();
	request_generator(std::pmr::memory_resource* mem);

	request_generator& uri(std::string_view u);
	request_generator& header(std::string_view name, std::string_view val);
	std::pmr::string body(std::string_view cnt) const ;
};

inline request_generator& operator << (request_generator& left, const uri& right)
{
	return left.uri(right.u);
}

inline request_generator& operator << (request_generator& left, const header& right)
{
	return left.header(right.n, right.v);
}

} // namespace http_utils
