#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <optional>
#include "uri_parser.hpp"
#include "utils/cvt.hpp"
#include "utils/pos_string_view.hpp"
#include "utils/inner_static_vector.hpp"

namespace http_parser {

template<typename Con>
struct header_view {
	//TODO: implement trivial ctor for use it with inner_static_array
	header_view(const Con* src) : name(src, 0, 0), value(src, 0, 0) {}

	basic_position_string_view<Con> name;
	basic_position_string_view<Con> value;
};

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

template<
        typename DataContainer
      , typename ContainerFactory
      , typename Container = decltype(std::declval<ContainerFactory>().template operator()<header_view<DataContainer>>())
      >
class header_message {
public:
	using header_view_t = header_view<DataContainer>;
	using headers_container = Container;
	using pos_view = basic_position_string_view<DataContainer>;
private:
	const DataContainer* data_;
	headers_container headers_;
public:
	template<typename ... Args>
	header_message(const DataContainer* d, const ContainerFactory& cf )
	    : data_(d)
	    , headers_(cf.template operator()<header_view_t>())
	{}

	header_message& operator = (const header_message& other)
	{
		data_ = other.data_;
		for(auto& h:other.headers_) headers_.emplace_back(h);
		return *this;
	}

	void add_header_name(std::size_t pos, std::size_t size)
	{
		headers_.emplace_back(data_).name.assign(pos, size);
	}
	void last_header_value(std::size_t pos, std::size_t size)
	{
		headers_.back().value.assign(pos, size);
	}

	std::size_t size() const { return headers_.size(); }
	bool empty() const { return headers_.empty(); }
	auto find_header(std::string_view v) const
	{
		using hv_type = header_view<DataContainer>;
		auto pos = std::find_if(headers_.begin(), headers_.end(),
		                        [v](const hv_type& hv){ return hv.name == v; });
		return pos == headers_.end() ? std::nullopt : std::make_optional(pos->value);
	}

	std::optional<std::size_t> content_size() const
	{
		auto header = find_header("Content-Length");
		if(!header.has_value()) return std::nullopt;
		return to_int(*header);
	}

	bool is_chunked() const
	{
		using namespace std::literals;
		auto header = find_header("Transfer-Encoding");
		return header && *header == "chunked"sv;
	}

	bool body_exists() const
	{
		auto size = content_size();
		return (size && *size != 0) || is_chunked();
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
	container_view url_src_;
	mutable url_view url_;
public:
	req_head_message(const DataContainer* d)
	    : data_(d)
	    , method_(data_, 0, 0)
	    , url_src_(data_, 0, 0)
	{}

	const url_view& url() const  {
		url_.uri( (inner_string_view)url_src_ );
		return url_;
	}
	auto& url(std::size_t pos, std::size_t size)
	{
		url_src_.assign(pos, size);
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
	resp_head_message(const DataContainer* d) : resp_head_message(200, d, 0, 0) {}
	resp_head_message(std::uint16_t c, const DataContainer* d, std::size_t pos, std::size_t size)
	    : code(c)
	    , reason(d, pos, size)
	{}

	std::uint16_t code=200;
	basic_position_string_view<DataContainer> reason;
};

template<template<class> class Head, typename DataContainer, typename ContainerFactory>
class http1_message {
public:
	using header_message_t = header_message<DataContainer, ContainerFactory>;
	using headers_container = header_message_t::headers_container;
private:
	const DataContainer* data;
	Head<DataContainer> head_;
	header_message_t headers_;
public:
	http1_message(const DataContainer* d, const ContainerFactory& cf)
	    : data(d)
	    , head_(d)
	    , headers_(d, cf)
	{}

	Head<DataContainer>& head() { return head_; }
	const Head<DataContainer>& head() const { return head_; }

	header_message_t& headers() { return headers_; }
	const header_message_t& headers() const { return headers_; }

	auto find_header(std::string_view v) const
	{
		return headers_.find_header(v);
	}
};

} // namespace http_parser
