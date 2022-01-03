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

class request_generator {
public:
	std::pmr::string as_string() const ;
};

} // namespace http_utils
