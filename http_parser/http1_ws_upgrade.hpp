#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include "parser.hpp"

namespace http_parser {

template<typename Head, typename DataContainer>
struct http1_ws_acceptor : public http1_parser_acceptor<Head, DataContainer> {
};


} // namespace http_parser
