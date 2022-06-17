#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include "http_parser/utils/pos_string_view.hpp"
#include "http_parser/generator.hpp"
#include "http_parser/uri_parser.hpp"
#include "http_parser/parser.hpp"

namespace http_parser {

constexpr std::size_t default_max_body_size = 4 * 1024;

namespace pmr_vec {
using data_type = std::pmr::vector<std::byte>;
using uri_parser = http_parser::basic_uri_parser<std::string_view>;
using generator = http_parser::basic_generator<data_type, std::string_view>;

template<typename Head>
struct http1_acceptor_traits : http_parser::http1_parser_traits<Head, data_type> {
	std::pmr::memory_resource* mem;
	data_type create_data_container() override { return data_type{mem}; }
	std::pmr::vector<header_view<data_type>> create_headers_container() override
	{ return std::pmr::vector<header_view<data_type>>{mem}; }
};

template<std::size_t max_body_size = default_max_body_size>
using http1_req_parser = http_parser::http1_req_parser<pmr_vector_factory, pmr_vector_t_factory<std::byte>, max_body_size>;

template<std::size_t max_body_size = default_max_body_size>
using http1_resp_parser = http_parser::http1_resp_parser<pmr_vector_factory, pmr_vector_t_factory<std::byte>, max_body_size>;

} // namespace pmr_vec

namespace pmr_str {
using data_type = std::pmr::string;
using uri_parser = http_parser::basic_uri_parser<std::string_view>;
using uri_wparser = http_parser::basic_uri_parser<std::wstring_view>;
using generator = http_parser::basic_generator<data_type, std::string_view>;

template<typename Head>
struct http1_acceptor_traits : http_parser::http1_parser_traits<Head, data_type> {
	std::pmr::memory_resource* mem;
	data_type create_data_container() override { return data_type{mem}; }
	std::pmr::vector<header_view<data_type>> create_headers_container() override
	{ return std::pmr::vector<header_view<data_type>>{mem}; }
};

template<std::size_t max_body_size = default_max_body_size>
using http1_req_parser = http_parser::http1_req_parser<pmr_vector_factory, pmr_string_factory, max_body_size>;

template<std::size_t max_body_size = default_max_body_size>
using http1_resp_parser = http_parser::http1_resp_parser<pmr_vector_factory, pmr_string_factory, max_body_size>;

} // namespace pmr_str

using pmr_vec::data_type;
using pmr_vec::uri_parser;
using pmr_vec::generator;

using pos_string_view = basic_position_string_view<std::pmr::string>;
using pos_data_view = basic_position_string_view<std::pmr::vector<std::byte>>;

} // namespace http_parser
