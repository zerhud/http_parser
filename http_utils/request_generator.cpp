/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include "request_generator.hpp"

using http_utils::request_generator;

request_generator& request_generator::uri(std::string_view u)
{
	return *this;
}

request_generator& request_generator::header(std::string_view name, std::string_view val)
{
	return *this;
}

request_generator& request_generator::body(std::string_view cnt)
{
	return *this;
}

std::pmr::string http_utils::request_generator::as_string() const
{
	return "";
}
