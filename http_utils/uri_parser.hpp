#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <regex>
#include <cassert>
#include <optional>
#include <charconv>
#include <memory_resource>

namespace http_utils {


struct pmr_narrow_traits {
	using string_type = std::pmr::string;
	using string_view_type = std::string_view;
	using regex_type = std::regex;
	using match_type = std::pmr::smatch;

	static constexpr string_view_type slash = "/";

	static inline char ascii_to_char_t(char v) {return v;}
	static inline bool ascii_eq(string_view_type left, std::string_view ascii_v) {
		return left == ascii_v;
	}

	static inline bool regex_match(const string_type& str, match_type& m, const regex_type& e)
	{
		return std::regex_search(str, m, e);
	}

	static inline auto create_extended(std::pmr::memory_resource*, const char* r)
	{
		return std::regex(r, std::regex::extended);
	}
};
struct pmr_wide_traits {
	using string_type = std::pmr::wstring;
	using string_view_type = std::wstring_view;
	using regex_type = std::wregex;
	using match_type = std::pmr::wsmatch;

	static constexpr string_view_type slash = L"/";

	static inline wchar_t ascii_to_char_t(char v) {return v;}
	static inline bool ascii_eq(string_view_type left, std::string_view ascii_v) {
		if(left.size() == ascii_v.size()) {
			for(std::size_t i=0;i<left.size();++i)
				if(left[i] != ascii_to_char_t(ascii_v[i]))
					return false;
			return true;
		}
		return false;
	}

	static inline bool regex_match(const string_type& str, match_type& m, const regex_type& e)
	{
		return std::regex_search(str, m, e);
	}

	static inline auto create_extended(std::pmr::memory_resource* mem, std::string_view r)
	{
		string_type expr(mem);
		for(auto& c:r) expr += ascii_to_char_t(c);
		return std::wregex(expr, std::regex::extended);
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
		auto uri_parser = Traits::create_extended(mem,
		        "^((([^:/?#]+):)?" // http
		        "(//((([^:@]+)(:([^@]+))?@))?([^/:?#]*))" // user:pass@ya.com
		        "(:([0-9]+))?" // :80
		        "([^?#]*)" // /path
		        "(\\?([^#]*))?" // ?param
		        "(#(.*))?)?$" // #anchor
		        );
		bool result = Traits::regex_match(source, parsed, uri_parser);
		result = result && !parsed.empty();
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
	    : mem(url.get_allocator().resource())
	    , source(std::move(url))
	{
		recalculate();
	}

	basic_uri_parser(std::pmr::memory_resource* mem)
	    : basic_uri_parser(mem, "") {}
	basic_uri_parser(std::pmr::memory_resource* mem, typename Traits::string_view_type url)
	    : mem(mem)
	    , source(url, mem)
	    , parsed(mem)
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
		} else if(Traits::ascii_eq(scheme(), "https")) {
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
	std::optional<typename Traits::string_view_type>
	param(typename Traits::string_view_type name) const
	{
		auto data = params();
		for(std::size_t begin=0;begin<data.size();++begin) {
			std::size_t eq_pos = data.find(Traits::ascii_to_char_t('='), begin);
			std::size_t amp_pos = data.find(Traits::ascii_to_char_t('&'), begin);
			if(amp_pos==Traits::string_type::npos) amp_pos = data.size();
			std::size_t name_finish_pos = std::min(eq_pos, amp_pos);
			auto cur_name = data.substr(begin, name_finish_pos - begin);
			if(cur_name == name) {
				if(eq_pos == Traits::string_type::npos)
					return typename Traits::string_view_type{};
				return data.substr(
				            name_finish_pos + 1,
				            amp_pos == name_finish_pos
				            ? 0
				            : amp_pos - name_finish_pos - 1);
			}
			begin = amp_pos;
		}
		return std::nullopt;
	}

	typename Traits::string_view_type user() const { return extract_str(7); }
	typename Traits::string_view_type pass() const { return extract_str(9); }
};

using uri_parser = basic_uri_parser<pmr_narrow_traits>;
using uri_wparser = basic_uri_parser<pmr_wide_traits>;

} // namespace http_utils
