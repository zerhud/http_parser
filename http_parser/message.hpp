#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <optional>
#include "uri_parser.hpp"
#include "utils/pos_string_view.hpp"

namespace http_parser {

template<typename Con>
struct header_view {
	header_view(const Con* src) : name(src, 0, 0), value(src, 0, 0) {}

	basic_position_string_view<Con> name;
	basic_position_string_view<Con> value;
};

template<template<class> class Container, typename DataContainer>
class header_message {
public:
	using headers_container = Container<header_view<DataContainer>>;
	using pos_view = basic_position_string_view<DataContainer>;
private:
	const DataContainer* data_;
	headers_container headers_;
public:
	template<typename ... Args>
	header_message(const DataContainer* d, Args... args)
	    : data_(d)
	    , headers_(std::forward<Args>(args)...)
	{}

	headers_container headers() const { return headers_; }
	void add_header_name(std::string_view n)
	{
		headers_.emplace_back(data_).name = n;
	}
	void last_header_value(std::string_view v)
	{
		headers_.back().value = v;
	}

	bool empty() const { return headers_.empty(); }
	auto find_header(std::string_view v) const
	{
		using hv_type = header_view<DataContainer>;
		auto pos = std::find_if(headers_.begin(), headers_.end(),
		                        [v](const hv_type& hv){ return hv.name == v; });
		return pos == headers_.end() ? std::nullopt : std::make_optional(pos->value);
	}
};

template<typename DataContainer>
class req_head_message {
public:
	using container_view = basic_position_string_view<DataContainer>;
	using inner_string_view = typename container_view::std_string_view;
	using url_view = basic_uri_parser<inner_string_view>;
private:
	const DataContainer* data_;
	container_view method_;
	url_view url_;
public:
	req_head_message(const DataContainer* d) : data_(d), method_(data_, 0, 0) {}

	const url_view& url() const  { return url_; }
	auto& url(std::size_t pos, std::size_t size)
	{
		container_view tmp_view(data_, pos, size);
		url_.uri( (inner_string_view)tmp_view );
		return *this;
	}

	container_view method() const { return method_; }
	auto& method(std::size_t pos, std::size_t size)
	{
		method_.assign(pos, size);
		return *this;
	}

};

template<typename DataContainer>
struct resp_head_message {
	resp_head_message(std::uint16_t c, const DataContainer* d, std::size_t pos, std::size_t size)
	    : code(c)
	    , reason(d, pos, size)
	{}

	std::uint16_t code=200;
	basic_position_string_view<DataContainer> reason;
};

} // namespace http_parser
