#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include "http_parser/request_generator.hpp"
#include "http_parser/response_parser.hpp"
#include "http_parser/uri_parser.hpp"

namespace http_parser {

namespace pmr_vec {
using data_type = std::pmr::vector<std::byte>;
using uri_parser = http_parser::basic_uri_parser<std::string_view>;
using response_parser = http_parser::basic_response_parser<data_type>;
using request_generator = http_parser::basic_request_generator<data_type, std::string_view>;
} // namespace pmr_vec

namespace pmr_str {
using data_type = std::pmr::string;
using uri_parser = http_parser::basic_uri_parser<std::string_view>;
using uri_wparser = http_parser::basic_uri_parser<std::wstring_view>;
using response_parser = http_parser::basic_response_parser<data_type>;
using request_generator = http_parser::basic_request_generator<data_type, std::string_view>;
} // namespace pmr_str

using pmr_vec::data_type;
using pmr_vec::uri_parser;
using pmr_vec::response_parser;
using pmr_vec::request_generator;

} // namespace http_parser
