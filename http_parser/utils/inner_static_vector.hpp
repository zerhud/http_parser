#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <array>
#include <exception>

namespace http_parser {

template<typename T>
concept static_arraible = std::is_trivially_constructible_v<T> && std::is_trivially_destructible_v<T>;

template<static_arraible T, std::size_t L>
class inner_static_vector {
	std::size_t cur_size = 0;
	std::array<T, L> con;
public:
	using value_type = T;

	template<typename ... Args>
	value_type& emplace_back(Args... args) {
		if(cur_size == L)
			throw std::out_of_range("cannot add to static vector any more");
		return *(new (&con[cur_size++]) T ( std::forward<Args>(args)... ));
	}

	std::size_t size() const { return cur_size; }

	const T& operator[](std::size_t ind) const {
		return con[ind];
	}

	T& operator[](std::size_t ind) {
		return con[ind];
	}

	bool empty() const { return cur_size == 0; }
	T& back() { return con[cur_size-1]; }
	const T& back() const { return con[cur_size-1]; }

	auto begin() { return con.begin(); }
	auto end() { return begin() + cur_size; }
};

} // namespace http_parser

