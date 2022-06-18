#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <vector>
#include <string>
#include <memory_resource>

#include "inner_static_vector.hpp"

namespace http_parser {

struct pmr_vector_factory {
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();
	template<typename T>
	std::pmr::vector<T> operator()() const
	{ return std::pmr::vector<T>{}; }
};

template<std::size_t N>
struct static_vector_factory {
	template<typename T>
	auto operator()() const
	{
		return inner_static_vector<T, N>{};
	}
};

struct pmr_string_factory {
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();
	std::pmr::string operator()() const
	{ return std::pmr::string{mem}; }
};

template<typename T>
struct pmr_vector_t_factory {
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();
	std::pmr::vector<T> operator()() const
	{ return std::pmr::vector<T>{mem}; }
};

} // namespace http_parser

