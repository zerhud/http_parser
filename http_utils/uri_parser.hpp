#pragma once
      
/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <regex>
#include <optional>
#include <charconv>
#include <memory_resource>

namespace http_utils {


struct pmr_narrow_traits {
	using string_type = std::pmr::string;
	using string_view_type = std::string_view;
	using regex_type = std::regex;
	using match_type = std::pmr::smatch;

	static constexpr char eq_sign = '=';
	static constexpr char amp_sign = '&';
	static constexpr string_view_type slash = "/";
	static constexpr string_view_type https = "https";

	static inline bool regex_match(const string_type& str, match_type& m, const regex_type& e)
	{
		return std::regex_search(str, m, e);
	}

	static inline auto create_extended(const char* r)
	{
		return std::regex(r, std::regex::extended);
	}
};

template<typename Traits>
class basic_uri_parser final {
	std::pmr::memory_resource* def_mem()
	{
		return std::pmr::get_default_resource();
	}

	void recalculate()
	{
		static auto uri_parser = Traits::create_extended(
		        "^((([^:/?#]+):)?" // http
		        "(//((([^:@]+)(:([^@]+))?@))?([^/:?#]*))" // user:pass@ya.com
		        "(:([0-9]+))?" // :80
		        "([^?#]*)" // /path
		        "(\\?([^#]*))?" // ?param
		        "(#(.*))?)?$" // #anchor
		        );
		bool result = Traits::regex_match(source, parsed, uri_parser);
		result = result && parsed.ready() && !parsed.empty();
		if(!result) throw std::runtime_error("cannot parse uri");
	}

	typename Traits::string_view_type extract_str(std::size_t ind) const
	{
		assert(!parsed.empty());
		std::size_t pos = parsed.position(ind);
		std::size_t len = parsed.length(ind);
		assert(len == 0 || pos + len <= source.size());
		return typename Traits::string_view_type{ source.data() + pos, len};
	}

	std::pmr::memory_resource* mem;
	typename Traits::string_type source;
	typename Traits::match_type parsed;
public:
	basic_uri_parser() : basic_uri_parser(def_mem()) {}
	basic_uri_parser(typename Traits::string_type url)
	    : basic_uri_parser(def_mem(), url) {}
	basic_uri_parser(std::pmr::memory_resource* mem)
	    : basic_uri_parser(mem, typename Traits::string_type{}) {}
	basic_uri_parser(std::pmr::memory_resource* mem, typename Traits::string_type url)
	    : mem(mem)
	    , source(std::move(url))
	{
		recalculate();
	}

	typename Traits::string_view_type uri() const { return source; }
	void uri(typename Traits::string_view_type nu)
	{
		source = nu;
		recalculate();
	}

	std::pmr::memory_resource* mem_buffer() const
	{
		return mem;
	}

	int port() const
	{
		int port_=80;
		//TODO: use string_view here
		auto pstr = parsed[12].str();
		if(!pstr.empty()) {
			port_ = std::stoi(pstr);
		} else if(scheme() == Traits::https) {
			port_ = 443;
		}
		return port_;
	}
	typename Traits::string_view_type scheme() const { return extract_str(3); }
	typename Traits::string_view_type host() const { return extract_str(10); }
	typename Traits::string_view_type path() const
	{
		auto ret = extract_str(13);
		if(ret.empty()) return Traits::slash;
		return ret;
	}
	typename Traits::string_view_type request() const
	{
		std::size_t pos = parsed.position(13);
		auto params_len = parsed.length(15);
		std::size_t len =
		        parsed.length(13) + params_len +
		        (params_len == 0 ? 0 : 1); // 1 is a ? sign
		return typename Traits::string_view_type{ source.data() + pos, len };
	}
	typename Traits::string_view_type anchor() const { return extract_str(17); }
	typename Traits::string_view_type params() const { return extract_str(15); }
	std::optional<typename Traits::string_view_type> param(typename Traits::string_view_type name) const
	{
		return std::nullopt;
	}

	typename Traits::string_view_type user() const { return extract_str(7); }
	typename Traits::string_view_type pass() const { return extract_str(9); }
};

using uri_parser = basic_uri_parser<pmr_narrow_traits>;

} // namespace http_utils
